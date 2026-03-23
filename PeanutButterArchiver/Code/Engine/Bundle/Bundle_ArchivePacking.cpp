#include "Bundle_ArchivePacking.hpp"

#include <array>
#include <algorithm>
#include <cstring>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "Bundle_LogicalRecordEncoder.hpp"

namespace peanutbutter {
namespace {

using namespace memory_layout;

constexpr std::uint64_t kArchiveProgressArchiveLogIntervalV2 = 64u;
constexpr std::uint64_t kArchiveProgressFileLogIntervalV2 = 1000u;
constexpr std::uint64_t kArchiveProgressByteLogIntervalV2 =
    250u * 1024u * 1024u;

void PopulateSectionBootstrapFields(const BundleStageContextV2& pContext,
                                    const PlannedArchiveFileV2& pArchive,
                                    std::uint32_t pLocalBlockIndex,
                                    std::uint32_t pPayloadBytesUsed,
                                    SectionHeaderV2& pOutHeader) {
  const BundleMemoryPlanV2& aPlan = pContext.State().mMemoryPlan;
  pOutHeader.mPayloadBytesUsed = pPayloadBytesUsed;
  pOutHeader.mArchiveFileCount = static_cast<std::uint32_t>(aPlan.mArchiveCount);
  pOutHeader.mArchiveBlockCount = pArchive.mBlockCount;
  pOutHeader.mArchiveIndex = static_cast<std::uint32_t>(pArchive.mArchiveIndex);
  pOutHeader.mBlockIndex = pLocalBlockIndex;
  pOutHeader.mArchiveDataBlockCount = static_cast<std::uint32_t>(aPlan.mArchiveDataBlockCount);
  pOutHeader.mPreviewManifestBlockCount =
      static_cast<std::uint32_t>(aPlan.mPreviewManifestBlockCount);
  pOutHeader.mFolderManifestBlockCount =
      static_cast<std::uint32_t>(aPlan.mEmptyFolderBlockCount);
  pOutHeader.mRepairDataBlockCount = static_cast<std::uint32_t>(aPlan.mRepairSectorBlockCount);
  pOutHeader.mArchiveFamilyId = aPlan.mArchiveFamilyId;
}

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
                        std::size_t pPayloadCapacity,
                        unsigned char* pPayloadBytes,
                        std::size_t& pOutPayloadBytes,
                        std::uint64_t& pOutFileBytes,
                        bool& pOutPausedAtBoundary,
                        std::string& pOutFailureMessage) {
  std::uint64_t aLogicalBytes = 0u;
  pOutPausedAtBoundary = false;
  return pEncoder.Fill(pPayloadBytes,
                       pPayloadCapacity,
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
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  ByteBufferV2 aPlainBlock(aArchiveBlockBytes);
  ByteBufferV2 aSealedBlock(aArchiveBlockBytes);
  if (aPlainBlock.Empty() ||
      (pEncryptBlock && aSealedBlock.Empty())) {
    pOutFailureMessage = "failed allocating block buffers.";
    return false;
  }

  std::memset(aPlainBlock.Data(), 0, aArchiveBlockBytes);
  unsigned char* aPayload = aPlainBlock.Data() + kSectionHeaderBytesV2;
  std::size_t aPayloadBytesWritten = 0u;
  bool aPausedAtBoundary = false;
  if (!FillSectionPayload(pContext,
                          pEncoder,
                          pSafeMode,
                          aSectionPayloadBytes,
                          aPayload,
                          aPayloadBytesWritten,
                          pOutFileBytesWritten,
                          aPausedAtBoundary,
                          pOutFailureMessage)) {
    return false;
  }

  SectionHeaderV2 aSectionHeader{};
  aSectionHeader.mSectionType = static_cast<std::uint8_t>(pSectionType);
  (void)aPausedAtBoundary;
  PopulateSectionBootstrapFields(pContext,
                                 pArchive,
                                 pLocalBlockIndex,
                                 static_cast<std::uint32_t>(aPayloadBytesWritten),
                                 aSectionHeader);
  aSectionHeader.mRepairRecord =
      MakeIgnoredRepairRecord(pContext.State().mMemoryPlan.mArchiveFamilyId,
                              pArchive.mArchiveIndex,
                              static_cast<std::uint64_t>(pLocalBlockIndex));
  aSectionHeader.mCheckSum =
      ComputeSectionCheckSum(aPayload, aSectionPayloadBytes, aSectionHeader);

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
                                               aArchiveBlockBytes,
                                               &aSealError)) {
      pOutFailureMessage = "failed sealing section block: " + aSealError;
      return false;
    }
    aSourceBytes = aSealedBlock.Data();
  }

  if (!pWriteStream.Write(aSourceBytes, aArchiveBlockBytes)) {
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
                               BundleLogicalRecordEncoderV2& pEncoder,
                               FileWriteStreamV2& pWriteStream,
                               std::string& pOutFailureMessage) {
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  ByteBufferV2 aPlainBlock(aArchiveBlockBytes);
  if (aPlainBlock.Empty()) {
    pOutFailureMessage = "failed allocating preview block buffer.";
    return false;
  }

  std::memset(aPlainBlock.Data(), 0, aArchiveBlockBytes);
  unsigned char* aPayloadBytes = aPlainBlock.Data() + kSectionHeaderBytesV2;
  std::size_t aChunkBytes = 0u;
  std::uint64_t aLogicalBytes = 0u;
  std::uint64_t aFileBytesWritten = 0u;
  bool aPausedAtBoundary = false;
  if (!pEncoder.Fill(aPayloadBytes,
                     aSectionPayloadBytes,
                     false,
                     aChunkBytes,
                     aLogicalBytes,
                     aFileBytesWritten,
                     aPausedAtBoundary,
                     pOutFailureMessage)) {
    return false;
  }
  (void)aLogicalBytes;
  (void)aFileBytesWritten;
  (void)aPausedAtBoundary;

  SectionHeaderV2 aSectionHeader{};
  aSectionHeader.mSectionType =
      static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest);
  PopulateSectionBootstrapFields(pContext,
                                 pArchive,
                                 pLocalBlockIndex,
                                 static_cast<std::uint32_t>(aChunkBytes),
                                 aSectionHeader);
  aSectionHeader.mRepairRecord =
      MakeIgnoredRepairRecord(pContext.State().mMemoryPlan.mArchiveFamilyId,
                              pArchive.mArchiveIndex,
                              static_cast<std::uint64_t>(pLocalBlockIndex));
  aSectionHeader.mCheckSum =
      ComputeSectionCheckSum(aPayloadBytes, aSectionPayloadBytes, aSectionHeader);

  if (!WriteSectionHeader(aSectionHeader,
                          aPlainBlock.Data(),
                          kSectionHeaderBytesV2,
                          nullptr)) {
    pOutFailureMessage = "failed writing preview section header.";
    return false;
  }

  if (!pWriteStream.Write(aPlainBlock.Data(), aArchiveBlockBytes)) {
    pOutFailureMessage =
        "failed writing preview manifest block: " +
        pWriteStream.LastErrorMessage();
    return false;
  }

  (void)pFamilyBlockIndex;
  return true;
}

LoggingStatV2 BuildArchivePackingStat(const BundleStageContextV2& pContext,
                                      const BundleLogicalRecordEncoderV2& pFolderEncoder,
                                      const BundleLogicalRecordEncoderV2& pFileEncoder,
                                      std::uint64_t pArchivesCompleted,
                                      std::uint64_t pBytesPacked) {
  LoggingStatV2 aStat;
  aStat.mArchivesCompleted = pArchivesCompleted;
  aStat.mArchivesTotal = pContext.State().mMemoryPlan.mArchiveCount;
  aStat.mFilesCompleted = static_cast<std::uint64_t>(pFileEncoder.PackedItemCount());
  aStat.mFilesTotal = pContext.State().mDiscovery.mFileCount;
  aStat.mFoldersCompleted = static_cast<std::uint64_t>(pFolderEncoder.PackedItemCount());
  aStat.mFoldersTotal = pContext.State().mDiscovery.mEmptyFolderCount;
  aStat.mBytesCompleted = pBytesPacked;
  aStat.mBytesTotal = pContext.State().mDiscovery.mTotalSourceBytes;
  return aStat;
}

std::string BuildArchivePackingProgressLabel(const LoggingStatV2& pStat) {
  const std::string aSummary = BuildStatSummaryV2(pStat);
  if (aSummary.empty()) {
    return "Packing archive blocks";
  }
  return "Packing archive blocks: " + aSummary;
}

bool ShouldEmitArchivePackingSlice(const LoggingStatV2& pStat,
                                   std::uint64_t& pNextArchiveLog,
                                   std::uint64_t& pNextFileLog,
                                   std::uint64_t& pNextByteLog) {
  bool aShouldEmit = false;
  while (pStat.mArchivesCompleted >= pNextArchiveLog) {
    aShouldEmit = true;
    pNextArchiveLog += kArchiveProgressArchiveLogIntervalV2;
  }
  while (pStat.mFilesCompleted >= pNextFileLog) {
    aShouldEmit = true;
    pNextFileLog += kArchiveProgressFileLogIntervalV2;
  }
  while (pStat.mBytesCompleted >= pNextByteLog) {
    aShouldEmit = true;
    pNextByteLog += kArchiveProgressByteLogIntervalV2;
  }
  return aShouldEmit;
}

bool HandleArchivePackingCancel(BundleStageContextV2& pContext,
                                const std::string& pFileReferenceAtBlockStart,
                                const std::string& pFileReferenceAfterBlock) {
  if (!pContext.IsCancelRequested()) {
    return false;
  }

  BundleCancelStateV2& aCancel = pContext.State().mCancel;
  if (!aCancel.mObserved) {
    aCancel = BundleCancelStateV2{};
    aCancel.mObserved = true;
    aCancel.mCancelFileReference = pFileReferenceAtBlockStart;
    if (aCancel.mCancelFileReference.empty()) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Bundle][Archive Packing] Cancel requested at a block boundary; finalizing headers as canceled.");
    } else {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Bundle][Archive Packing] Cancel requested; finishing file '" +
              aCancel.mCancelFileReference + "' within " +
              std::to_string(pContext.Request().mCancelFinishBlocks) +
              " blocks before finalizing headers as canceled.");
    }
  }

  if (!aCancel.mCancelFileReference.empty()) {
    ++aCancel.mCancelBlocksWritten;
  }

  const bool aReachedFileBoundary =
      aCancel.mCancelFileReference.empty() ||
      pFileReferenceAfterBlock != aCancel.mCancelFileReference;
  const bool aBudgetExceeded =
      !aReachedFileBoundary &&
      aCancel.mCancelBlocksWritten >= pContext.Request().mCancelFinishBlocks;
  if (!aReachedFileBoundary && !aBudgetExceeded) {
    return false;
  }

  aCancel.mShouldFinalizeAfterCancel = true;
  aCancel.mBudgetExceeded = aBudgetExceeded;
  if (aBudgetExceeded) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        "[Bundle][Archive Packing] Cancel budget reached after " +
            std::to_string(aCancel.mCancelBlocksWritten) +
            " blocks while writing '" + aCancel.mCancelFileReference +
            "'; stopping at this block boundary and finalizing headers as canceled.");
  } else if (!aCancel.mCancelFileReference.empty()) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        "[Bundle][Archive Packing] Finished file '" +
            aCancel.mCancelFileReference + "' after " +
            std::to_string(aCancel.mCancelBlocksWritten) +
            " cancel-drain blocks; finalizing headers as canceled.");
  }
  return true;
}

}  // namespace

bool BundleArchivePackingV2::Run(BundleStageContextV2& pContext) {
  BundlePackingStateV2& aPacking = pContext.State().mPacking;
  const BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;
  const std::uint64_t aNonRepairBlockCount = aMemoryPlan.mNonRepairFamilyBlockCount;
  aPacking.mArchivePaths.clear();
  aPacking.mArchivePackedBlockCount = 0u;
  pContext.State().mCancel = BundleCancelStateV2{};

  std::vector<BundleRecordEntryV2> aPreviewRecords =
      pContext.State().mDiscovery.mEmptyFolderRecords;
  aPreviewRecords.insert(aPreviewRecords.end(),
                         pContext.State().mDiscovery.mFileRecords.begin(),
                         pContext.State().mDiscovery.mFileRecords.end());

  BundleLogicalRecordEncoderV2 aFolderEncoder(
      pContext.State().mDiscovery.mEmptyFolderRecords,
      pContext.FileSystem(),
      TypedRecordTypeV2::kManifestFile,
      TypedRecordTypeV2::kManifestFolder);
  BundleLogicalRecordEncoderV2 aFileEncoder(
      pContext.State().mDiscovery.mFileRecords,
      pContext.FileSystem(),
      TypedRecordTypeV2::kDataFile,
      TypedRecordTypeV2::kDataFolder);
  BundleLogicalRecordEncoderV2 aPreviewEncoder(
      aPreviewRecords,
      pContext.FileSystem(),
      TypedRecordTypeV2::kManifestFile,
      TypedRecordTypeV2::kManifestFolder);

  std::uint64_t aGlobalBlockIndex = 0u;
  std::uint64_t aFileBytesPacked = 0u;
  std::uint64_t aNextArchiveLog = kArchiveProgressArchiveLogIntervalV2;
  std::uint64_t aNextFileLog = kArchiveProgressFileLogIntervalV2;
  std::uint64_t aNextByteLog = kArchiveProgressByteLogIntervalV2;

  pContext.EmitLog(
      LogLevelV2::kInfo,
      "[Bundle][Archive Packing] START. " +
          BuildStatSummaryV2(BuildArchivePackingStat(
              pContext, aFolderEncoder, aFileEncoder, 0u, 0u)) +
          ".");
  bool aStopAfterCurrentArchive = false;
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
      if (aGlobalBlockIndex >= aNonRepairBlockCount) {
        break;
      }
      const bool aIsFolderBlock =
          aGlobalBlockIndex < aMemoryPlan.mEmptyFolderBlockCount;
      std::string aFailureMessage;
      std::uint64_t aBlockFileBytes = 0u;
      const std::string aBlockStartFileReference =
          aIsFolderBlock
              ? std::string()
              : aFileEncoder.CurrentFileReference();
      const bool aIsPreviewBlock =
          !aIsFolderBlock &&
          aGlobalBlockIndex < (aMemoryPlan.mEmptyFolderBlockCount +
                               aMemoryPlan.mPreviewManifestBlockCount);
      if (aIsPreviewBlock) {
        if (!WritePreviewManifestBlock(pContext,
                                       aArchive,
                                       aGlobalBlockIndex,
                                       aLocalBlockIndex,
                                       aPreviewEncoder,
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
      const LoggingStatV2 aStat = BuildArchivePackingStat(
          pContext,
          aFolderEncoder,
          aFileEncoder,
          static_cast<std::uint64_t>(aPacking.mArchivePaths.size()),
          aFileBytesPacked);
      pContext.EmitPhaseProgress(
          aNonRepairBlockCount == 0u
              ? 1.0
              : static_cast<double>(aPacking.mArchivePackedBlockCount) /
                    static_cast<double>(aNonRepairBlockCount),
          BuildArchivePackingProgressLabel(aStat));
      if (ShouldEmitArchivePackingSlice(
              aStat, aNextArchiveLog, aNextFileLog, aNextByteLog)) {
        pContext.EmitLog(
            LogLevelV2::kInfo,
            "[Bundle][Archive Packing] " + BuildStatSummaryV2(aStat) + ".");
      }
      const std::string aFileReferenceAfterBlock =
          (aIsFolderBlock || aIsPreviewBlock)
              ? std::string()
              : aFileEncoder.CurrentFileReference();
      if (HandleArchivePackingCancel(
              pContext, aBlockStartFileReference, aFileReferenceAfterBlock)) {
        aStopAfterCurrentArchive = true;
        break;
      }
    }

    if (!aWrite->Close()) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive packing failed while closing destination archive.");
      return false;
    }
    aPacking.mArchivePaths.push_back(aArchive.mPath);

    const LoggingStatV2 aArchiveClosedStat = BuildArchivePackingStat(
        pContext,
        aFolderEncoder,
        aFileEncoder,
        static_cast<std::uint64_t>(aPacking.mArchivePaths.size()),
        aFileBytesPacked);
    if (ShouldEmitArchivePackingSlice(aArchiveClosedStat,
                                      aNextArchiveLog,
                                      aNextFileLog,
                                      aNextByteLog)) {
      pContext.EmitLog(
          LogLevelV2::kInfo,
          "[Bundle][Archive Packing] " +
              BuildStatSummaryV2(aArchiveClosedStat) + ".");
    }
    if (aStopAfterCurrentArchive) {
      return true;
    }
  }

  if (pContext.IsCancelRequested()) {
    pContext.State().mCancel.mObserved = true;
    pContext.State().mCancel.mShouldFinalizeAfterCancel = true;
    pContext.EmitLog(
        LogLevelV2::kWarning,
        "[Bundle][Archive Packing] Cancel requested after archive packing completed; finalizing headers as canceled.");
    return true;
  }

  pContext.EmitLog(
      LogLevelV2::kInfo,
      "Archive packing wrote " +
          std::to_string(aPacking.mArchivePackedBlockCount) +
          " blocks across " + std::to_string(aPacking.mArchivePaths.size()) +
          " archives. Safe mode " +
          std::string(pContext.Request().mSafeModeEnabled ? "aligns file starts to block boundaries." : "uses tight file packing."));
  pContext.EmitPhaseProgress(1.0, "Archive packing complete");
  return true;
}

}  // namespace peanutbutter
