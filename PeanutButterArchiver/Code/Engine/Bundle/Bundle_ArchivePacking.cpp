#include "Bundle_ArchivePacking.hpp"

#include <array>
#include <cstring>

#include "../MemoryLayout/FormatUtilities.hpp"
#include "Bundle_LogicalRecordEncoder.hpp"

namespace peanutbutter {
namespace {

using namespace memory_layout;

bool BuildArchiveHeader(const BundleStageContextV2& pContext,
                        std::uint64_t pArchiveIndex,
                        std::uint8_t pDirtyState,
                        ArchiveHeaderV2& pOutHeader) {
  pOutHeader = ArchiveHeaderV2{};
  pOutHeader.mDirtyState = pDirtyState;
  pOutHeader.mIsEncrypted = pContext.Request().mEncryptionEnabled ? 1u : 0u;
  pOutHeader.mCipherProfile =
      static_cast<std::uint8_t>(pContext.Request().mEncryptionStrength);
  pOutHeader.mExpanderProfile =
      static_cast<std::uint8_t>(pContext.Request().mTableStrength);
  pOutHeader.mArchiveFamilyId = pContext.State().mMemoryPlan.mArchiveFamilyId;

  return TrySetPackedUint48(pOutHeader.mArchiveIndex,
                            pArchiveIndex,
                            nullptr,
                            "ArchiveIndex") &&
         TrySetPackedUint48(pOutHeader.mArchiveCount,
                            pContext.State().mMemoryPlan.mArchiveCount,
                            nullptr,
                            "ArchiveCount") &&
         TrySetPackedUint48(pOutHeader.mArchiveDataBlockCount,
                            pContext.State().mMemoryPlan.mArchiveDataBlockCount,
                            nullptr,
                            "ArchiveDataBlockCount") &&
         TrySetPackedUint48(pOutHeader.mEmptyFolderBlockCount,
                            pContext.State().mMemoryPlan.mEmptyFolderBlockCount,
                            nullptr,
                            "EmptyFolderBlockCount") &&
         TrySetPackedUint48(pOutHeader.mPreviewManifestBlockCount,
                            pContext.State().mMemoryPlan.mPreviewManifestBlockCount,
                            nullptr,
                            "PreviewManifestBlockCount") &&
         TrySetPackedUint48(pOutHeader.mRepairSectorBlockCount,
                            pContext.State().mMemoryPlan.mRepairSectorBlockCount,
                            nullptr,
                            "RepairSectorBlockCount");
}

bool WriteArchiveHeaderPrefix(const BundleStageContextV2& pContext,
                              const PlannedArchiveFileV2& pArchive,
                              FileWriteStreamV2& pWriteStream) {
  std::array<unsigned char, kArchiveHeaderBytesV2> aHeaderBytes{};
  ArchiveHeaderV2 aHeader;
  if (!BuildArchiveHeader(
          pContext,
          pArchive.mArchiveIndex,
          static_cast<std::uint8_t>(ArchiveDirtyStateV2::kInvalid),
          aHeader)) {
    return false;
  }
  if (!WriteArchiveHeader(aHeader, aHeaderBytes.data(), aHeaderBytes.size(), nullptr)) {
    return false;
  }
  return pWriteStream.Write(aHeaderBytes.data(), aHeaderBytes.size());
}

bool FillSectionPayload(BundleStageContextV2& pContext,
                        BundleLogicalRecordEncoderV2& pEncoder,
                        bool pSafeMode,
                        unsigned char* pPayloadBytes,
                        std::size_t& pOutPayloadBytes,
                        std::uint64_t& pOutFileBytes,
                        bool& pOutPausedAtBoundary,
                        std::string& pOutFailureMessage) {
  std::uint64_t aLogicalBytes = 0u;
  pOutPausedAtBoundary = false;
  return pEncoder.Fill(pPayloadBytes,
                       kSectionPayloadBytesV2,
                       pSafeMode,
                       pOutPayloadBytes,
                       aLogicalBytes,
                       pOutFileBytes,
                       pOutPausedAtBoundary,
                       pOutFailureMessage);
}

bool WriteSectionBlock(BundleStageContextV2& pContext,
                       const PlannedArchiveFileV2& pArchive,
                       std::uint64_t pFamilyBlockIndex,
                       std::uint32_t pLocalBlockIndex,
                       SectionTypeV2 pSectionType,
                       BundleLogicalRecordEncoderV2& pEncoder,
                       bool pSafeMode,
                       bool pEncryptBlock,
                       FileWriteStreamV2& pWriteStream,
                       std::uint64_t& pOutFileBytesWritten,
                       std::string& pOutFailureMessage) {
  ByteBufferV2 aPlainBlock(kArchiveBlockBytesV2);
  ByteBufferV2 aSealedBlock(kArchiveBlockBytesV2);
  if (aPlainBlock.Empty() ||
      (pEncryptBlock && aSealedBlock.Empty())) {
    pOutFailureMessage = "failed allocating block buffers.";
    return false;
  }

  std::memset(aPlainBlock.Data(), 0, kArchiveBlockBytesV2);
  unsigned char* aPayload = aPlainBlock.Data() + kSectionHeaderBytesV2;
  std::size_t aPayloadBytesWritten = 0u;
  bool aPausedAtBoundary = false;
  if (!FillSectionPayload(pContext,
                          pEncoder,
                          pSafeMode,
                          aPayload,
                          aPayloadBytesWritten,
                          pOutFileBytesWritten,
                          aPausedAtBoundary,
                          pOutFailureMessage)) {
    return false;
  }

  SectionHeaderV2 aSectionHeader{};
  aSectionHeader.mSectionType = static_cast<std::uint8_t>(pSectionType);
  if (pSectionType == SectionTypeV2::kArchiveData &&
      aPausedAtBoundary &&
      pEncoder.HasRemainingRecords()) {
    aSectionHeader.mSectionFlags |= kSectionFlagSparsePaddingHasNextRecordV2;
  }
  aSectionHeader.mRepairRecord =
      MakeIgnoredRepairRecord(pContext.State().mMemoryPlan.mArchiveFamilyId,
                              pArchive.mArchiveIndex,
                              static_cast<std::uint64_t>(pLocalBlockIndex));
  aSectionHeader.mCheckSum =
      ComputeSectionCheckSum(aPayload,
                             kSectionPayloadBytesV2,
                             aSectionHeader.mSkipRecord,
                             aSectionHeader.mSectionType,
                             aSectionHeader.mSectionFlags,
                             aSectionHeader.mRepairRecord);

  if (!WriteSectionHeader(aSectionHeader,
                          aPlainBlock.Data(),
                          kSectionHeaderBytesV2,
                          nullptr)) {
    pOutFailureMessage = "failed writing section header.";
    return false;
  }

  const unsigned char* aSourceBytes = aPlainBlock.Data();
  if (pEncryptBlock) {
    if (!pContext.State().mCipher.mAssembled) {
      pOutFailureMessage = "bundle archive packing expected an assembled cipher.";
      return false;
    }
    std::string aSealError;
    if (!pContext.State().mCipher.mCipher.Seal(aPlainBlock.Data(),
                                               aSealedBlock.Data(),
                                               kArchiveBlockBytesV2,
                                               &aSealError)) {
      pOutFailureMessage = "failed sealing section block: " + aSealError;
      return false;
    }
    aSourceBytes = aSealedBlock.Data();
  }

  if (!pWriteStream.Write(aSourceBytes, kArchiveBlockBytesV2)) {
    pOutFailureMessage =
        "failed writing archive block: " + pWriteStream.LastErrorMessage();
    return false;
  }

  (void)pFamilyBlockIndex;
  return true;
}

bool WritePreviewManifestBlock(BundleStageContextV2& pContext,
                               const PlannedArchiveFileV2& pArchive,
                               std::uint64_t pFamilyBlockIndex,
                               std::uint32_t pLocalBlockIndex,
                               const std::string& pPayload,
                               std::size_t& pInOutPayloadOffset,
                               FileWriteStreamV2& pWriteStream,
                               std::string& pOutFailureMessage) {
  ByteBufferV2 aPlainBlock(kArchiveBlockBytesV2);
  if (aPlainBlock.Empty()) {
    pOutFailureMessage = "failed allocating preview block buffer.";
    return false;
  }

  std::memset(aPlainBlock.Data(), 0, kArchiveBlockBytesV2);
  unsigned char* aPayloadBytes = aPlainBlock.Data() + kSectionHeaderBytesV2;
  const std::size_t aRemainingBytes =
      pInOutPayloadOffset < pPayload.size()
          ? (pPayload.size() - pInOutPayloadOffset)
          : 0u;
  const std::size_t aChunkBytes =
      std::min<std::size_t>(aRemainingBytes, kSectionPayloadBytesV2);
  if (aChunkBytes > 0u) {
    std::memcpy(aPayloadBytes,
                pPayload.data() + pInOutPayloadOffset,
                aChunkBytes);
    pInOutPayloadOffset += aChunkBytes;
  }

  SectionHeaderV2 aSectionHeader{};
  aSectionHeader.mSectionType =
      static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest);
  aSectionHeader.mRepairRecord =
      MakeIgnoredRepairRecord(pContext.State().mMemoryPlan.mArchiveFamilyId,
                              pArchive.mArchiveIndex,
                              static_cast<std::uint64_t>(pLocalBlockIndex));
  aSectionHeader.mCheckSum =
      ComputeSectionCheckSum(aPayloadBytes,
                             kSectionPayloadBytesV2,
                             aSectionHeader.mSkipRecord,
                             aSectionHeader.mSectionType,
                             aSectionHeader.mSectionFlags,
                             aSectionHeader.mRepairRecord);

  if (!WriteSectionHeader(aSectionHeader,
                          aPlainBlock.Data(),
                          kSectionHeaderBytesV2,
                          nullptr)) {
    pOutFailureMessage = "failed writing preview section header.";
    return false;
  }

  if (!pWriteStream.Write(aPlainBlock.Data(), kArchiveBlockBytesV2)) {
    pOutFailureMessage =
        "failed writing preview manifest block: " +
        pWriteStream.LastErrorMessage();
    return false;
  }

  (void)pFamilyBlockIndex;
  return true;
}

}  // namespace

bool BundleArchivePackingV2::Run(BundleStageContextV2& pContext) {
  BundlePackingStateV2& aPacking = pContext.State().mPacking;
  const BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;
  aPacking.mArchivePaths.clear();
  aPacking.mArchivePackedBlockCount = 0u;

  BundleLogicalRecordEncoderV2 aFolderEncoder(
      pContext.State().mDiscovery.mEmptyFolderRecords,
      pContext.FileSystem());
  BundleLogicalRecordEncoderV2 aFileEncoder(
      pContext.State().mDiscovery.mFileRecords,
      pContext.FileSystem());
  const std::string& aPreviewManifestPayload =
      pContext.State().mManifest.mPreviewManifestPayload;
  std::size_t aPreviewManifestOffset = 0u;

  std::uint64_t aGlobalBlockIndex = 0u;
  std::uint64_t aFileBytesPacked = 0u;
  for (const PlannedArchiveFileV2& aArchive : aMemoryPlan.mArchives) {
    std::unique_ptr<FileWriteStreamV2> aWrite =
        pContext.FileSystem().OpenWriteStream(aArchive.mPath);
    if (aWrite == nullptr || !aWrite->IsReady()) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive packing failed: could not open destination archive file.");
      return false;
    }
    if (!WriteArchiveHeaderPrefix(pContext, aArchive, *aWrite)) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive packing failed while writing archive header prefix.");
      return false;
    }

    for (std::uint32_t aLocalBlockIndex = 0u;
         aLocalBlockIndex < aArchive.mBlockCount;
         ++aLocalBlockIndex, ++aGlobalBlockIndex) {
      const bool aIsFolderBlock =
          aGlobalBlockIndex < aMemoryPlan.mEmptyFolderBlockCount;
      std::string aFailureMessage;
      std::uint64_t aBlockFileBytes = 0u;
      const bool aIsPreviewBlock =
          !aIsFolderBlock &&
          aGlobalBlockIndex < (aMemoryPlan.mEmptyFolderBlockCount +
                               aMemoryPlan.mPreviewManifestBlockCount);
      if (aIsPreviewBlock) {
        if (!WritePreviewManifestBlock(pContext,
                                       aArchive,
                                       aGlobalBlockIndex,
                                       aLocalBlockIndex,
                                       aPreviewManifestPayload,
                                       aPreviewManifestOffset,
                                       *aWrite,
                                       aFailureMessage)) {
          pContext.EmitLog(LogLevelV2::kError,
                           "Archive packing failed: " + aFailureMessage);
          return false;
        }
      } else {
        const SectionTypeV2 aSectionType = aIsFolderBlock
                                               ? SectionTypeV2::kEmptyFolderManifest
                                               : SectionTypeV2::kArchiveData;
        BundleLogicalRecordEncoderV2& aEncoder = aIsFolderBlock
                                                     ? aFolderEncoder
                                                     : aFileEncoder;
        const bool aSafeMode =
            !aIsFolderBlock && pContext.Request().mSafeModeEnabled;
        if (!WriteSectionBlock(pContext,
                               aArchive,
                               aGlobalBlockIndex,
                               aLocalBlockIndex,
                               aSectionType,
                               aEncoder,
                               aSafeMode,
                               pContext.Request().mEncryptionEnabled,
                               *aWrite,
                               aBlockFileBytes,
                               aFailureMessage)) {
          pContext.EmitLog(LogLevelV2::kError,
                           "Archive packing failed: " + aFailureMessage);
          return false;
        }
      }
      if (!aFailureMessage.empty()) {
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive packing failed: " + aFailureMessage);
        return false;
      }

      aFileBytesPacked += aBlockFileBytes;
      ++aPacking.mArchivePackedBlockCount;
      pContext.EmitPhaseProgress(
          aMemoryPlan.mTotalFamilyBlockCount == 0u
              ? 1.0
              : static_cast<double>(aPacking.mArchivePackedBlockCount) /
                    static_cast<double>(aMemoryPlan.mTotalFamilyBlockCount),
          "Packing archive blocks");
      if (pContext.IsCancelRequested()) {
        return false;
      }
    }

    if (!aWrite->Close()) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive packing failed while closing destination archive.");
      return false;
    }
    aPacking.mArchivePaths.push_back(aArchive.mPath);
  }

  pContext.EmitLog(
      LogLevelV2::kInfo,
      "Archive packing wrote " +
          std::to_string(aPacking.mArchivePackedBlockCount) +
          " blocks across " + std::to_string(aPacking.mArchivePaths.size()) +
          " archives. Safe mode " +
          std::string(pContext.Request().mSafeModeEnabled ? "aligns file starts to block boundaries." : "uses tight file packing."));
  pContext.EmitPhaseProgress(1.0, "Archive packing complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
