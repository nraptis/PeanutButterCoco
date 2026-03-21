#include "Decode_ArchiveDecode.hpp"

#include <array>
#include <cstring>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "Decode_LogicalRecordDecoder.hpp"

namespace peanutbutter {
namespace {

using namespace memory_layout;

bool ShouldContinuePastDamagedBlock(const DecodeStageContextV2& pContext) {
  return DecodeIntentAllowsSalvageV2(pContext.Request().mIntent);
}

void SwitchToPessimistic(DecodeStageContextV2& pContext,
                         const std::string& pReason) {
  if (pContext.State().mDiscovery.mMode == DecodeModeV2::kPessimistic) {
    return;
  }
  pContext.State().mDiscovery.mMode = DecodeModeV2::kPessimistic;
  pContext.EmitLog(LogLevelV2::kWarning,
                   "Decode switched to pessimistic mode: " + pReason);
}

bool TryAcceptMissingPreviewManifest(DecodeStageContextV2& pContext,
                                     const SectionHeaderV2& pSectionHeader) {
  DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  DecodeManifestStateV2& aManifest = pContext.State().mManifest;
  if (aBootstrap.mExpectedPreviewManifestBlockCount == 0u ||
      aManifest.mPreviewManifestBlocksProcessed != 0u ||
      pSectionHeader.mSectionType !=
          static_cast<std::uint8_t>(SectionTypeV2::kArchiveData)) {
    return false;
  }

  aBootstrap.mExpectedPreviewManifestBlockCount = 0u;
  pContext.EmitLog(LogLevelV2::kWarning,
                   "[Decode][Manifest Discovery] Header advertised preview "
                   "manifest blocks, but archive data started immediately. "
                   "Treating preview manifest count as zero.");
  return true;
}

bool TryReadValidatedSectionHeader(const unsigned char* pBlockBytes,
                                   SectionHeaderV2& pOutHeader) {
  if (!ReadSectionHeader(pBlockBytes, kSectionHeaderBytesV2, pOutHeader, nullptr)) {
    return false;
  }
  return ValidateSectionCheckSum(
      pOutHeader,
      pBlockBytes + kSectionHeaderBytesV2,
      kSectionPayloadBytesV2);
}

bool ShouldTryPlaintextPreviewManifestBlock(const DecodeStageContextV2& pContext) {
  const DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  const DecodeManifestStateV2& aManifest = pContext.State().mManifest;
  return aBootstrap.mFirstHeader.mIsEncrypted != 0u &&
         aManifest.mEmptyFolderBlocksProcessed >=
             aBootstrap.mExpectedEmptyFolderBlockCount &&
         aManifest.mPreviewManifestBlocksProcessed <
             aBootstrap.mExpectedPreviewManifestBlockCount;
}

std::uint8_t ExpectedSectionType(const DecodeStageContextV2& pContext) {
  const DecodeManifestStateV2& aManifest = pContext.State().mManifest;
  const DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  if (aManifest.mEmptyFolderBlocksProcessed <
      aBootstrap.mExpectedEmptyFolderBlockCount) {
    return static_cast<std::uint8_t>(SectionTypeV2::kEmptyFolderManifest);
  }
  if (aManifest.mPreviewManifestBlocksProcessed <
      aBootstrap.mExpectedPreviewManifestBlockCount) {
    return static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest);
  }
  if (aManifest.mArchiveDataBlocksProcessed <
      aBootstrap.mExpectedArchiveDataBlockCount) {
    return static_cast<std::uint8_t>(SectionTypeV2::kArchiveData);
  }
  if (aManifest.mRepairBlocksProcessed <
      aBootstrap.mExpectedRepairBlockCount) {
    return static_cast<std::uint8_t>(SectionTypeV2::kRepairData);
  }
  return static_cast<std::uint8_t>(SectionTypeV2::kRepairData);
}

bool ReadBlock(const DiscoveredArchiveFileV2& pArchive,
               std::uint64_t pBlockIndex,
               FileSystemV2& pFileSystem,
               ByteBufferV2& pOutEncryptedBlock) {
  const std::size_t aOffset =
      static_cast<std::size_t>(kArchiveHeaderBytesV2 +
                               (pBlockIndex * kArchiveBlockBytesV2));
  std::unique_ptr<FileReadStreamV2> aRead =
      pFileSystem.OpenReadStream(pArchive.mPath);
  if (aRead == nullptr || !aRead->IsReady()) {
    return false;
  }
  return aRead->Read(aOffset, pOutEncryptedBlock.Data(), kArchiveBlockBytesV2);
}

bool HandleDamagedBlock(DecodeStageContextV2& pContext,
                        const std::string& pReason) {
  if (!ShouldContinuePastDamagedBlock(pContext)) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                         ProgressStageV2::kArchiveDecode,
                         pReason));
    return false;
  }

  SwitchToPessimistic(pContext, pReason);
  return true;
}

}  // namespace

bool DecodeArchiveDecodeV2::Run(DecodeStageContextV2& pContext) {
  ByteBufferV2 aEncryptedBlock(kArchiveBlockBytesV2);
  ByteBufferV2 aPlainBlock(kArchiveBlockBytesV2);
  if (aEncryptedBlock.Empty() || aPlainBlock.Empty()) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: block buffers could not be allocated.");
    return false;
  }

  DecodeLogicalRecordDecoderV2 aFolderDecoder(
      pContext.Request().mDestinationDirectory,
      pContext.FileSystem());
  DecodeLogicalRecordDecoderV2 aFileDecoder(
      pContext.Request().mDestinationDirectory,
      pContext.FileSystem());

  std::uint64_t aProcessedBlocks = 0u;
  for (const DiscoveredArchiveFileV2& aArchive : pContext.State().mDiscovery.mArchives) {
    for (std::uint64_t aBlockIndex = 0u;
         aBlockIndex < aArchive.mReadableBlockCount;
         ++aBlockIndex) {
      if (!ReadBlock(aArchive, aBlockIndex, pContext.FileSystem(), aEncryptedBlock)) {
        if (!HandleDamagedBlock(pContext, "a block could not be read from disk.")) {
          return false;
        }
        continue;
      }

      SectionHeaderV2 aSectionHeader;
      bool aHasValidatedSectionHeader = false;
      if (ShouldTryPlaintextPreviewManifestBlock(pContext)) {
        std::memcpy(aPlainBlock.Data(), aEncryptedBlock.Data(), kArchiveBlockBytesV2);
        if (TryReadValidatedSectionHeader(aPlainBlock.Data(), aSectionHeader) &&
            aSectionHeader.mSectionType ==
                static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest)) {
          aHasValidatedSectionHeader = true;
        }
      }

      if (!aHasValidatedSectionHeader &&
          pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u) {
        std::string aUnsealError;
        if (!pContext.State().mCipher.mCipher.Unseal(aEncryptedBlock.Data(),
                                                     aPlainBlock.Data(),
                                                     kArchiveBlockBytesV2,
                                                     &aUnsealError)) {
          if (!HandleDamagedBlock(pContext,
                                  aUnsealError.empty()
                                      ? "a block could not be unsealed."
                                      : "a block could not be unsealed: " +
                                            aUnsealError)) {
            return false;
          }
          continue;
        }
      } else {
        std::memcpy(aPlainBlock.Data(),
                    aEncryptedBlock.Data(),
                    kArchiveBlockBytesV2);
      }

      if (!aHasValidatedSectionHeader) {
        if (!TryReadValidatedSectionHeader(aPlainBlock.Data(), aSectionHeader)) {
          if (!HandleDamagedBlock(pContext,
                                  "a section header failed validation.")) {
            return false;
          }
          continue;
        }
      }

      if (pContext.State().mDiscovery.mMode == DecodeModeV2::kOptimistic) {
        const std::uint8_t aExpectedType = ExpectedSectionType(pContext);
        if (aSectionHeader.mSectionType != aExpectedType) {
          if (TryAcceptMissingPreviewManifest(pContext, aSectionHeader)) {
            // Recomputed expectations now match the on-disk section stream.
          } else
          if (!HandleDamagedBlock(
                  pContext,
                  "a section type disagreed with the optimistic family layout.")) {
            return false;
          }
        }
      }

      bool aTerminated = false;
      bool aStoppedAtPadding = false;
      bool aParseError = false;
      std::string aParseErrorMessage;
      std::uint64_t aDataBytesWritten = 0u;

      switch (static_cast<SectionTypeV2>(aSectionHeader.mSectionType)) {
        case SectionTypeV2::kEmptyFolderManifest:
          if (!aFolderDecoder.Consume(aPlainBlock.Data() + kSectionHeaderBytesV2,
                                      0u,
                                      kSectionPayloadBytesV2,
                                      true,
                                      aTerminated,
                                      aStoppedAtPadding,
                                      aParseError,
                                      aParseErrorMessage,
                                      aDataBytesWritten)) {
            pContext.EmitLog(LogLevelV2::kError,
                             "Archive decode failed: " + aParseErrorMessage);
            return false;
          }
          ++pContext.State().mManifest.mEmptyFolderBlocksProcessed;
          break;

        case SectionTypeV2::kPreviewManifest:
          ++pContext.State().mManifest.mPreviewManifestBlocksProcessed;
          break;

        case SectionTypeV2::kArchiveData: {
          const bool aTreatZeroLengthAsPadding =
              (aSectionHeader.mSectionFlags &
               kSectionFlagSparsePaddingHasNextRecordV2) != 0u ||
              pContext.State().mDiscovery.mMode == DecodeModeV2::kPessimistic ||
              (pContext.State().mManifest.mArchiveDataBlocksProcessed + 1u <
               pContext.State().mBootstrap.mExpectedArchiveDataBlockCount);
          if (!aFileDecoder.Consume(aPlainBlock.Data() + kSectionHeaderBytesV2,
                                    0u,
                                    kSectionPayloadBytesV2,
                                    aTreatZeroLengthAsPadding,
                                    aTerminated,
                                    aStoppedAtPadding,
                                    aParseError,
                                    aParseErrorMessage,
                                    aDataBytesWritten)) {
            pContext.EmitLog(LogLevelV2::kError,
                             "Archive decode failed: " + aParseErrorMessage);
            return false;
          }
          ++pContext.State().mManifest.mArchiveDataBlocksProcessed;
          if (aTerminated) {
            pContext.State().mOutput.mArchiveTerminated = true;
          }
          break;
        }

        case SectionTypeV2::kRepairData:
          ++pContext.State().mManifest.mRepairBlocksProcessed;
          break;
      }

      if (aParseError) {
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive decode parse error: " + aParseErrorMessage);
        return false;
      }

      ++aProcessedBlocks;
      pContext.EmitPhaseProgress(
          pContext.State().mDiscovery.mTotalReadableBlocks == 0u
              ? 1.0
              : static_cast<double>(aProcessedBlocks) /
                    static_cast<double>(pContext.State().mDiscovery.mTotalReadableBlocks),
          "Decoding archive blocks");
      if (pContext.IsCancelRequested()) {
        return false;
      }
    }
  }

  std::string aFinalizeError;
  if (!aFolderDecoder.Finalize(aFinalizeError)) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: " + aFinalizeError);
    return false;
  }
  if (!aFileDecoder.Finalize(aFinalizeError)) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: " + aFinalizeError);
    return false;
  }

  pContext.State().mOutput.mFilesWritten = aFileDecoder.FilesWritten();
  pContext.State().mOutput.mFoldersCreated = aFolderDecoder.FoldersCreated();
  pContext.State().mOutput.mBytesWritten = aFileDecoder.BytesWritten();

  pContext.EmitLog(
      LogLevelV2::kInfo,
      "Archive decode wrote " +
          std::to_string(pContext.State().mOutput.mFilesWritten) + " files and " +
          std::to_string(pContext.State().mOutput.mFoldersCreated) + " folders.");
  pContext.EmitPhaseProgress(1.0, "Archive decode complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
