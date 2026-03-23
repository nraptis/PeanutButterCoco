#include "Decode_ArchiveDecode.hpp"

#include <array>
#include <cstring>
#include <vector>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "Decode_LogicalRecordDecoder.hpp"

namespace peanutbutter {
namespace {

using namespace memory_layout;

constexpr std::uint64_t kDecodeProgressArchiveLogIntervalV2 = 64u;
constexpr std::uint64_t kDecodeProgressFileLogIntervalV2 = 1000u;
constexpr std::uint64_t kDecodeProgressFolderLogIntervalV2 = 256u;
constexpr std::uint64_t kDecodeProgressByteLogIntervalV2 =
    250u * 1024u * 1024u;

std::string DecodeStagePrefix(DecodeIntentV2 pIntent,
                              ProgressStageV2 pStage) {
  return "[" + LogActionLabelV2(LogActionFromDecodeIntentV2(pIntent)) + "][" +
         ProgressStageLabelV2(pStage) + "]";
}

LoggingStatV2 BuildArchiveDecodeStat(const DecodeStageContextV2& pContext,
                                     const DecodeLogicalRecordDecoderV2& pFolderDecoder,
                                     const DecodeLogicalRecordDecoderV2& pFileDecoder,
                                     std::uint64_t pArchivesCompleted) {
  LoggingStatV2 aStat;
  aStat.mArchivesCompleted = pArchivesCompleted;
  aStat.mArchivesTotal = static_cast<std::uint64_t>(
      pContext.State().mDiscovery.mArchives.size());
  aStat.mFilesCompleted = pFileDecoder.FilesWritten();
  aStat.mFoldersCompleted = pFolderDecoder.FoldersCreated();
  aStat.mBytesCompleted = pFileDecoder.BytesWritten();
  return aStat;
}

std::string BuildArchiveDecodeSummary(const LoggingStatV2& pStat) {
  std::vector<std::string> aParts;
  if (pStat.mArchivesTotal > 0u) {
    aParts.push_back(std::to_string(pStat.mArchivesCompleted) + " of " +
                     std::to_string(pStat.mArchivesTotal) + " archives");
  }
  aParts.push_back(std::to_string(pStat.mFilesCompleted) + " files");
  aParts.push_back(std::to_string(pStat.mFoldersCompleted) + " folders");
  if (pStat.mBytesCompleted > 0u) {
    aParts.push_back(FormatHumanBytesV2(pStat.mBytesCompleted));
  } else {
    aParts.push_back("0B");
  }

  std::string aSummary;
  for (std::size_t aIndex = 0u; aIndex < aParts.size(); ++aIndex) {
    if (aIndex != 0u) {
      aSummary += ", ";
    }
    aSummary += aParts[aIndex];
  }
  return aSummary;
}

std::string BuildArchiveDecodeProgressLabel(const LoggingStatV2& pStat) {
  return "Decoding archive blocks: " + BuildArchiveDecodeSummary(pStat);
}

bool ShouldEmitArchiveDecodeSlice(const LoggingStatV2& pStat,
                                  std::uint64_t& pNextArchiveLog,
                                  std::uint64_t& pNextFileLog,
                                  std::uint64_t& pNextFolderLog,
                                  std::uint64_t& pNextByteLog) {
  bool aShouldEmit = false;
  while (pStat.mArchivesCompleted >= pNextArchiveLog) {
    aShouldEmit = true;
    pNextArchiveLog += kDecodeProgressArchiveLogIntervalV2;
  }
  while (pStat.mFilesCompleted >= pNextFileLog) {
    aShouldEmit = true;
    pNextFileLog += kDecodeProgressFileLogIntervalV2;
  }
  while (pStat.mFoldersCompleted >= pNextFolderLog) {
    aShouldEmit = true;
    pNextFolderLog += kDecodeProgressFolderLogIntervalV2;
  }
  while (pStat.mBytesCompleted >= pNextByteLog) {
    aShouldEmit = true;
    pNextByteLog += kDecodeProgressByteLogIntervalV2;
  }
  return aShouldEmit;
}

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
                   LogPessimisticSwitchV2(
                       LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                       pReason));
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
                   DecodeStagePrefix(pContext.Request().mIntent,
                                     ProgressStageV2::kManifestDiscovery) +
                       " Header advertised preview manifest blocks, but archive "
                       "data started immediately. Treating preview manifest "
                       "count as zero.");
  return true;
}

bool TryReadValidatedSectionHeader(const unsigned char* pBlockBytes,
                                   std::size_t pPayloadBytes,
                                   SectionHeaderV2& pOutHeader) {
  if (!ReadSectionHeader(pBlockBytes, kSectionHeaderBytesV2, pOutHeader, nullptr)) {
    return false;
  }
  return ValidateSectionCheckSum(
      pOutHeader,
      pBlockBytes + kSectionHeaderBytesV2,
      pPayloadBytes);
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
               std::size_t pArchiveBlockBytes,
               ByteBufferV2& pOutEncryptedBlock) {
  if (!pArchive.mIsPresent || pArchive.mPath.empty()) {
    return false;
  }
  const std::size_t aOffset =
      static_cast<std::size_t>(kArchiveHeaderBytesV2 +
                               (pBlockIndex * pArchiveBlockBytes));
  std::unique_ptr<FileReadStreamV2> aRead =
      pFileSystem.OpenReadStream(pArchive.mPath);
  if (aRead == nullptr || !aRead->IsReady()) {
    return false;
  }
  return aRead->Read(aOffset, pOutEncryptedBlock.Data(), pArchiveBlockBytes);
}

bool IsExpectedContinuationBlock(const std::vector<DiscoveredArchiveFileV2>& pArchives,
                                 std::size_t pPreviousArchiveSlot,
                                 std::uint64_t pPreviousBlockIndex,
                                 std::size_t pCurrentArchiveSlot,
                                 std::uint64_t pCurrentBlockIndex) {
  if (pPreviousArchiveSlot >= pArchives.size() || pCurrentArchiveSlot >= pArchives.size()) {
    return false;
  }
  if (pCurrentArchiveSlot == pPreviousArchiveSlot) {
    return pCurrentBlockIndex == (pPreviousBlockIndex + 1u);
  }
  if (pCurrentArchiveSlot != (pPreviousArchiveSlot + 1u) || pCurrentBlockIndex != 0u) {
    return false;
  }

  const DiscoveredArchiveFileV2& aPreviousArchive = pArchives[pPreviousArchiveSlot];
  const std::uint64_t aPreviousArchiveBlockCount =
      aPreviousArchive.mArchiveBlockCount != 0u ? aPreviousArchive.mArchiveBlockCount
                                                : aPreviousArchive.mReadableBlockCount;
  return aPreviousArchiveBlockCount != 0u &&
         (pPreviousBlockIndex + 1u) >= aPreviousArchiveBlockCount;
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

bool HandleArchiveDecodeCancel(DecodeStageContextV2& pContext,
                               DecodeLogicalRecordDecoderV2& pFileDecoder,
                               const std::string& pFileReferenceAtBlockStart,
                               const std::string& pFileReferenceAfterBlock) {
  if (!pContext.IsCancelRequested()) {
    return false;
  }

  DecodeCancelStateV2& aCancel = pContext.State().mCancel;
  if (!aCancel.mObserved) {
    aCancel = DecodeCancelStateV2{};
    aCancel.mObserved = true;
    aCancel.mCancelFileReference = pFileReferenceAtBlockStart;
    if (aCancel.mCancelFileReference.empty()) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel requested at a block boundary.");
    } else {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel requested; finishing file '" +
              aCancel.mCancelFileReference + "' within " +
              std::to_string(pContext.Request().mCancelFinishBlocks) +
              " blocks.");
    }
  }

  if (!aCancel.mCancelFileReference.empty()) {
    ++aCancel.mCancelBlocksRead;
  }

  const bool aReachedFileBoundary =
      aCancel.mCancelFileReference.empty() ||
      pFileReferenceAfterBlock != aCancel.mCancelFileReference;
  const bool aBudgetExceeded =
      !aReachedFileBoundary &&
      aCancel.mCancelBlocksRead >= pContext.Request().mCancelFinishBlocks;
  if (!aReachedFileBoundary && !aBudgetExceeded) {
    return false;
  }

  if (!pFileReferenceAfterBlock.empty() &&
      pFileReferenceAfterBlock != aCancel.mCancelFileReference) {
    if (!pFileDecoder.AbortCurrentFile()) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel could not remove the trailing partial file '" +
              pFileReferenceAfterBlock + "'.");
    }
  } else if (aBudgetExceeded && !pFileReferenceAfterBlock.empty()) {
    if (!pFileDecoder.AbortCurrentFile()) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel could not remove the partial file '" +
              pFileReferenceAfterBlock + "'.");
    }
  }

  aCancel.mShouldFinalizeAfterCancel = true;
  aCancel.mBudgetExceeded = aBudgetExceeded;
  if (aBudgetExceeded) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Cancel budget reached after " +
            std::to_string(aCancel.mCancelBlocksRead) +
            " blocks while reading '" + aCancel.mCancelFileReference +
            "'; stopping at this block boundary.");
  } else if (!aCancel.mCancelFileReference.empty()) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Finished file '" + aCancel.mCancelFileReference + "' after " +
            std::to_string(aCancel.mCancelBlocksRead) +
            " cancel-drain blocks.");
  }
  return true;
}

}  // namespace

bool DecodeArchiveDecodeV2::Run(DecodeStageContextV2& pContext) {
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  ByteBufferV2 aEncryptedBlock(aArchiveBlockBytes);
  ByteBufferV2 aPlainBlock(aArchiveBlockBytes);
  if (aEncryptedBlock.Empty() || aPlainBlock.Empty()) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: block buffers could not be allocated.");
    return false;
  }

  DecodeLogicalRecordDecoderV2 aFolderDecoder(
      pContext.Request().mDestinationDirectory,
      pContext.FileSystem(),
      pContext.Layout(),
      DecodeLogicalZoneV2::kFolderManifest);
  DecodeLogicalRecordDecoderV2 aFileDecoder(
      pContext.Request().mDestinationDirectory,
      pContext.FileSystem(),
      pContext.Layout(),
      DecodeLogicalZoneV2::kData);
  pContext.State().mCancel = DecodeCancelStateV2{};

  std::uint64_t aProcessedBlocks = 0u;
  std::uint64_t aArchivesCompleted = 0u;
  std::uint64_t aNextArchiveLog = kDecodeProgressArchiveLogIntervalV2;
  std::uint64_t aNextFileLog = kDecodeProgressFileLogIntervalV2;
  std::uint64_t aNextFolderLog = kDecodeProgressFolderLogIntervalV2;
  std::uint64_t aNextByteLog = kDecodeProgressByteLogIntervalV2;
  bool aHasOpenFileContinuation = false;
  std::size_t aContinuationArchiveSlot = 0u;
  std::uint64_t aContinuationBlockIndex = 0u;
  pContext.EmitLog(
      LogLevelV2::kInfo,
      DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
          " START. " +
          BuildArchiveDecodeSummary(
              BuildArchiveDecodeStat(pContext, aFolderDecoder, aFileDecoder, 0u)) +
          ".");
  for (std::size_t aArchiveSlot = 0u;
       aArchiveSlot < pContext.State().mDiscovery.mArchives.size();
       ++aArchiveSlot) {
    const DiscoveredArchiveFileV2& aArchive =
        pContext.State().mDiscovery.mArchives[aArchiveSlot];
    if (!aArchive.mIsPresent) {
      ++aArchivesCompleted;
      continue;
    }
    for (std::uint64_t aBlockIndex = 0u;
         aBlockIndex < aArchive.mReadableBlockCount;
         ++aBlockIndex) {
      if (!ReadBlock(aArchive,
                     aBlockIndex,
                     pContext.FileSystem(),
                     aArchiveBlockBytes,
                     aEncryptedBlock)) {
        if (!HandleDamagedBlock(pContext, "a block could not be read from disk.")) {
          return false;
        }
        continue;
      }

      SectionHeaderV2 aSectionHeader;
      bool aHasValidatedSectionHeader = false;
      if (ShouldTryPlaintextPreviewManifestBlock(pContext)) {
        std::memcpy(aPlainBlock.Data(), aEncryptedBlock.Data(), aArchiveBlockBytes);
        if (TryReadValidatedSectionHeader(aPlainBlock.Data(),
                                          aSectionPayloadBytes,
                                          aSectionHeader) &&
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
                                                     aArchiveBlockBytes,
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
                    aArchiveBlockBytes);
      }

      if (!aHasValidatedSectionHeader) {
        if (!TryReadValidatedSectionHeader(aPlainBlock.Data(),
                                           aSectionPayloadBytes,
                                           aSectionHeader)) {
          if (!HandleDamagedBlock(pContext,
                                  "a section header failed validation.")) {
            return false;
          }
          continue;
        }
      }

      const std::uint8_t aExpectedType = ExpectedSectionType(pContext);
      const bool aPhysicalRepairZone =
          aExpectedType == static_cast<std::uint8_t>(SectionTypeV2::kRepairData);

      if (pContext.State().mDiscovery.mMode == DecodeModeV2::kOptimistic) {
        if (!aPhysicalRepairZone && aSectionHeader.mSectionType != aExpectedType) {
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

      if (aHasOpenFileContinuation) {
        const bool aArchiveDataSection =
            static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
            SectionTypeV2::kArchiveData;
        const bool aExpectedContinuation =
            aArchiveDataSection &&
            IsExpectedContinuationBlock(pContext.State().mDiscovery.mArchives,
                                        aContinuationArchiveSlot,
                                        aContinuationBlockIndex,
                                        aArchiveSlot,
                                        aBlockIndex);
        if (!aExpectedContinuation) {
          (void)aFileDecoder.AbortCurrentFile();
          aHasOpenFileContinuation = false;
          if (!HandleDamagedBlock(
                  pContext,
                  "a continued file did not resume at the expected next block.")) {
            return false;
          }
          continue;
        }
      }

      bool aTerminated = false;
      bool aStoppedAtPadding = false;
      bool aParseError = false;
      std::string aParseErrorMessage;
      std::uint64_t aDataBytesWritten = 0u;
      if (aPhysicalRepairZone) {
        ++pContext.State().mManifest.mRepairBlocksProcessed;
        ++aProcessedBlocks;
        const LoggingStatV2 aStat = BuildArchiveDecodeStat(
            pContext,
            aFolderDecoder,
            aFileDecoder,
            aArchivesCompleted);
        pContext.EmitPhaseProgress(
            pContext.State().mDiscovery.mTotalReadableBlocks == 0u
                ? 1.0
                : static_cast<double>(aProcessedBlocks) /
                      static_cast<double>(pContext.State().mDiscovery.mTotalReadableBlocks),
            BuildArchiveDecodeProgressLabel(aStat));
        continue;
      }
      const std::string aBlockStartFileReference =
          static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
                  SectionTypeV2::kArchiveData
              ? aFileDecoder.CurrentFileReference()
              : std::string();
      const std::size_t aBlockPayloadEnd = std::min<std::size_t>(
          aSectionPayloadBytes, static_cast<std::size_t>(aSectionHeader.mPayloadBytesUsed));

      switch (static_cast<SectionTypeV2>(aSectionHeader.mSectionType)) {
        case SectionTypeV2::kEmptyFolderManifest:
          if (!aFolderDecoder.Consume(aPlainBlock.Data() + kSectionHeaderBytesV2,
                                      0u,
                                      aBlockPayloadEnd,
                                      false,
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
          if (!aFileDecoder.Consume(aPlainBlock.Data() + kSectionHeaderBytesV2,
                                    0u,
                                    aBlockPayloadEnd,
                                    false,
                                    aTerminated,
                                    aStoppedAtPadding,
                                    aParseError,
                                    aParseErrorMessage,
                                    aDataBytesWritten)) {
            if (aFileDecoder.IsInsideFile()) {
              (void)aFileDecoder.AbortCurrentFile();
            }
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
        if (static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
                SectionTypeV2::kArchiveData &&
            aFileDecoder.IsInsideFile()) {
          (void)aFileDecoder.AbortCurrentFile();
        }
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive decode parse error: " + aParseErrorMessage);
        return false;
      }

      ++aProcessedBlocks;
      const LoggingStatV2 aStat = BuildArchiveDecodeStat(
          pContext, aFolderDecoder, aFileDecoder, aArchivesCompleted);
      pContext.EmitPhaseProgress(
          pContext.State().mDiscovery.mTotalReadableBlocks == 0u
              ? 1.0
              : static_cast<double>(aProcessedBlocks) /
                    static_cast<double>(pContext.State().mDiscovery.mTotalReadableBlocks),
          BuildArchiveDecodeProgressLabel(aStat));
      if (ShouldEmitArchiveDecodeSlice(
              aStat,
              aNextArchiveLog,
              aNextFileLog,
              aNextFolderLog,
              aNextByteLog)) {
        pContext.EmitLog(
            LogLevelV2::kInfo,
            DecodeStagePrefix(pContext.Request().mIntent,
                              ProgressStageV2::kArchiveDecode) +
                " " + BuildArchiveDecodeSummary(aStat) + ".");
      }
      const std::string aFileReferenceAfterBlock =
          static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
                  SectionTypeV2::kArchiveData
              ? aFileDecoder.CurrentFileReference()
              : std::string();
      if (HandleArchiveDecodeCancel(
              pContext,
              aFileDecoder,
              aBlockStartFileReference,
              aFileReferenceAfterBlock)) {
        pContext.State().mOutput.mFilesWritten = aFileDecoder.FilesWritten();
        pContext.State().mOutput.mFoldersCreated = aFolderDecoder.FoldersCreated();
        pContext.State().mOutput.mBytesWritten = aFileDecoder.BytesWritten();
        return true;
      }

      if (aFileDecoder.IsInsideFile()) {
        aHasOpenFileContinuation = true;
        aContinuationArchiveSlot = aArchiveSlot;
        aContinuationBlockIndex = aBlockIndex;
      } else {
        aHasOpenFileContinuation = false;
      }
    }

    ++aArchivesCompleted;
    const LoggingStatV2 aArchiveClosedStat = BuildArchiveDecodeStat(
        pContext, aFolderDecoder, aFileDecoder, aArchivesCompleted);
    if (ShouldEmitArchiveDecodeSlice(aArchiveClosedStat,
                                     aNextArchiveLog,
                                     aNextFileLog,
                                     aNextFolderLog,
                                     aNextByteLog)) {
      pContext.EmitLog(
          LogLevelV2::kInfo,
          DecodeStagePrefix(pContext.Request().mIntent,
                            ProgressStageV2::kArchiveDecode) +
              " " + BuildArchiveDecodeSummary(aArchiveClosedStat) + ".");
    }
  }

  if (pContext.IsCancelRequested()) {
    pContext.State().mCancel.mObserved = true;
    pContext.State().mCancel.mShouldFinalizeAfterCancel = true;
    pContext.State().mOutput.mFilesWritten = aFileDecoder.FilesWritten();
    pContext.State().mOutput.mFoldersCreated = aFolderDecoder.FoldersCreated();
    pContext.State().mOutput.mBytesWritten = aFileDecoder.BytesWritten();
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Cancel requested after archive decode completed.");
    return true;
  }

  std::string aFinalizeError;
  if (!aFolderDecoder.Finalize(aFinalizeError)) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: " + aFinalizeError);
    return false;
  }
  if (!aFileDecoder.Finalize(aFinalizeError)) {
    if (aFileDecoder.IsInsideFile()) {
      (void)aFileDecoder.AbortCurrentFile();
    }
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: " + aFinalizeError);
    return false;
  }

  pContext.State().mOutput.mFilesWritten = aFileDecoder.FilesWritten();
  pContext.State().mOutput.mFoldersCreated = aFolderDecoder.FoldersCreated();
  pContext.State().mOutput.mBytesWritten = aFileDecoder.BytesWritten();

  pContext.EmitLog(
      LogLevelV2::kInfo,
      DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
          " Wrote " +
          std::to_string(pContext.State().mOutput.mFilesWritten) + " files and " +
          std::to_string(pContext.State().mOutput.mFoldersCreated) + " folders.");
  pContext.EmitPhaseProgress(1.0, "Archive decode complete");
  return true;
}

}  // namespace peanutbutter
