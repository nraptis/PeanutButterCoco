#include "Bundle_RepairPacking.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <memory>
#include <vector>

#include "../../Knobs.hpp"
#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "Bundle_LogicalRecordEncoder.hpp"

namespace peanutbutter {
namespace {

using namespace memory_layout;

constexpr std::size_t kInvalidCopyIndex = std::numeric_limits<std::size_t>::max();

struct RepairCopyRefV2 {
  std::size_t mArchiveIndex = 0u;
  std::uint32_t mLocalBlockIndex = 0u;
};

struct RepairDestinationRefV2 {
  std::size_t mArchiveIndex = 0u;
  std::uint32_t mLocalBlockIndex = 0u;
};

std::vector<RepairCopyRefV2> BuildRepairCopyOrder(
    const std::vector<std::vector<std::uint32_t>>& pRepairCopySourceLocalBlocks) {
  std::vector<RepairCopyRefV2> aCopies;
  std::uint32_t aMaxLayer = 0u;
  for (const std::vector<std::uint32_t>& aSourceLocalBlocks :
       pRepairCopySourceLocalBlocks) {
    aMaxLayer = std::max(
        aMaxLayer, static_cast<std::uint32_t>(aSourceLocalBlocks.size()));
  }

  for (std::uint32_t aLayer = 0u; aLayer < aMaxLayer; ++aLayer) {
    for (std::size_t aArchiveIndex = 0u;
         aArchiveIndex < pRepairCopySourceLocalBlocks.size();
         ++aArchiveIndex) {
      const std::vector<std::uint32_t>& aSourceLocalBlocks =
          pRepairCopySourceLocalBlocks[aArchiveIndex];
      if (aLayer >= aSourceLocalBlocks.size()) {
        continue;
      }
      aCopies.push_back(
          {aArchiveIndex, aSourceLocalBlocks[static_cast<std::size_t>(aLayer)]});
    }
  }

  return aCopies;
}

std::vector<RepairDestinationRefV2> BuildRepairDestinationOrder(
    const BundleMemoryPlanV2& pMemoryPlan) {
  std::vector<RepairDestinationRefV2> aDestinations;
  aDestinations.reserve(static_cast<std::size_t>(pMemoryPlan.mRepairSectorBlockCount));

  for (std::size_t aArchiveIndex = 0u;
       aArchiveIndex < pMemoryPlan.mArchives.size();
       ++aArchiveIndex) {
    const PlannedArchiveFileV2& aArchive = pMemoryPlan.mArchives[aArchiveIndex];
    const std::uint32_t aSourceBlocksInArchive =
        aArchiveIndex < pMemoryPlan.mSourceArchiveBlockCounts.size()
            ? pMemoryPlan.mSourceArchiveBlockCounts[aArchiveIndex]
            : 0u;
    const std::uint32_t aRepairBlocksInArchive =
        aArchive.mBlockCount > aSourceBlocksInArchive
            ? (aArchive.mBlockCount - aSourceBlocksInArchive)
            : 0u;
    for (std::uint32_t aRepairIndex = 0u;
         aRepairIndex < aRepairBlocksInArchive;
         ++aRepairIndex) {
      aDestinations.push_back(
          {aArchiveIndex,
           static_cast<std::uint32_t>(aSourceBlocksInArchive + aRepairIndex)});
    }
  }

  return aDestinations;
}

bool BuildCopyIndexBySource(const BundleMemoryPlanV2& pMemoryPlan,
                            const std::vector<RepairCopyRefV2>& pCopies,
                            std::vector<std::vector<std::size_t>>& pOutCopyIndexBySource,
                            std::string& pOutFailureMessage) {
  pOutFailureMessage.clear();
  pOutCopyIndexBySource.clear();
  pOutCopyIndexBySource.assign(pMemoryPlan.mSourceArchiveBlockCounts.size(),
                               std::vector<std::size_t>());
  for (std::size_t aArchiveIndex = 0u;
       aArchiveIndex < pMemoryPlan.mSourceArchiveBlockCounts.size();
       ++aArchiveIndex) {
    pOutCopyIndexBySource[aArchiveIndex].assign(
        pMemoryPlan.mSourceArchiveBlockCounts[aArchiveIndex],
        kInvalidCopyIndex);
  }

  for (std::size_t aCopyIndex = 0u; aCopyIndex < pCopies.size(); ++aCopyIndex) {
    const RepairCopyRefV2& aCopy = pCopies[aCopyIndex];
    if (aCopy.mArchiveIndex >= pOutCopyIndexBySource.size()) {
      pOutFailureMessage = "repair copy referenced an invalid source archive";
      return false;
    }
    if (aCopy.mLocalBlockIndex >= pOutCopyIndexBySource[aCopy.mArchiveIndex].size()) {
      pOutFailureMessage = "repair copy referenced an invalid source block";
      return false;
    }
    std::size_t& aSlot =
        pOutCopyIndexBySource[aCopy.mArchiveIndex][aCopy.mLocalBlockIndex];
    if (aSlot != kInvalidCopyIndex) {
      pOutFailureMessage = "duplicate repair copy source mapping detected";
      return false;
    }
    aSlot = aCopyIndex;
  }

  return true;
}

bool TryLocateArchiveLocalForFamilyBlock(
    const BundleMemoryPlanV2& pPlan,
    std::uint64_t pFamilyBlockIndex,
    std::uint32_t& pOutArchiveIndex,
    std::uint32_t& pOutLocalBlockIndex) {
  for (const PlannedArchiveFileV2& aArchive : pPlan.mArchives) {
    if (pFamilyBlockIndex < aArchive.mFamilyBlockStart ||
        pFamilyBlockIndex >=
            (aArchive.mFamilyBlockStart +
             static_cast<std::uint64_t>(aArchive.mBlockCount))) {
      continue;
    }
    const std::uint64_t aLocal64 = pFamilyBlockIndex - aArchive.mFamilyBlockStart;
    if (aArchive.mArchiveIndex > std::numeric_limits<std::uint32_t>::max() ||
        aLocal64 > std::numeric_limits<std::uint32_t>::max()) {
      return false;
    }
    pOutArchiveIndex = static_cast<std::uint32_t>(aArchive.mArchiveIndex);
    pOutLocalBlockIndex = static_cast<std::uint32_t>(aLocal64);
    return true;
  }
  return false;
}

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
  pOutHeader.mFolderManifestBlockCount = 0u;
  pOutHeader.mRepairDataBlockCount = static_cast<std::uint32_t>(aPlan.mRepairSectorBlockCount);
  pOutHeader.mArchiveFamilyId = aPlan.mArchiveFamilyId;
}

bool TryPopulateSkipRecordForArchiveData(
    const BundleStageContextV2& pContext,
    const PlannedArchiveFileV2& pArchive,
    std::uint64_t pFamilyBlockIndex,
    std::size_t pPayloadBytesWritten,
    BundleLogicalRecordEncoderV2& pEncoder,
    SectionHeaderV2& pOutHeader,
    std::string& pOutFailureMessage) {
  pOutHeader.mSkipRecord = SkipRecordV2{};

  std::uint64_t aDistanceToNextRecordStart = 0u;
  if (!pEncoder.TryGetLastFillFirstRecordBoundaryDistance(
          aDistanceToNextRecordStart)) {
    std::uint64_t aDistanceFromCursorToNextRecordStart = 0u;
    if (!pEncoder.TryMeasureDistanceToNextRecordStart(
            aDistanceFromCursorToNextRecordStart)) {
      return true;
    }
    aDistanceToNextRecordStart =
        static_cast<std::uint64_t>(pPayloadBytesWritten) +
        aDistanceFromCursorToNextRecordStart;
  }

  if (aDistanceToNextRecordStart == 0u) {
    return true;
  }

  const std::size_t aPayloadBytesPerBlock = pContext.Layout().SectionPayloadBytes();
  if (aPayloadBytesPerBlock == 0u) {
    pOutFailureMessage = "section payload bytes must be at least 1 for skip-record planning.";
    return false;
  }

  const std::uint64_t aDeltaBlocks =
      aDistanceToNextRecordStart / static_cast<std::uint64_t>(aPayloadBytesPerBlock);
  const std::uint32_t aTargetByteOffset = static_cast<std::uint32_t>(
      aDistanceToNextRecordStart % static_cast<std::uint64_t>(aPayloadBytesPerBlock));
  const std::uint64_t aTargetFamilyBlockIndex = pFamilyBlockIndex + aDeltaBlocks;
  if (aTargetFamilyBlockIndex >= pContext.State().mMemoryPlan.mNonRepairFamilyBlockCount) {
    return true;
  }

  std::uint32_t aTargetArchiveIndex = 0u;
  std::uint32_t aTargetLocalBlockIndex = 0u;
  if (!TryLocateArchiveLocalForFamilyBlock(pContext.State().mMemoryPlan,
                                           aTargetFamilyBlockIndex,
                                           aTargetArchiveIndex,
                                           aTargetLocalBlockIndex)) {
    pOutFailureMessage =
        "failed mapping skip-record target to archive/local block coordinates.";
    return false;
  }

  if (aTargetArchiveIndex < pArchive.mArchiveIndex) {
    pOutFailureMessage =
        "skip-record target archive mapped behind current archive.";
    return false;
  }
  const std::uint64_t aArchiveDistance64 =
      static_cast<std::uint64_t>(aTargetArchiveIndex) - pArchive.mArchiveIndex;
  if (aArchiveDistance64 > std::numeric_limits<std::uint16_t>::max()) {
    pOutFailureMessage = "skip-record archive distance exceeded 16-bit field.";
    return false;
  }
  if (aTargetLocalBlockIndex > std::numeric_limits<std::uint16_t>::max()) {
    pOutFailureMessage = "skip-record block distance exceeded 16-bit field.";
    return false;
  }

  pOutHeader.mSkipRecord.mArchiveDistance =
      static_cast<std::uint16_t>(aArchiveDistance64);
  pOutHeader.mSkipRecord.mBlockDistance =
      static_cast<std::uint16_t>(aTargetLocalBlockIndex);
  if (!SetSkipRecordByteDistance(
          pOutHeader.mSkipRecord, aTargetByteOffset, nullptr)) {
    pOutFailureMessage = "skip-record byte distance exceeded 24-bit field.";
    return false;
  }
  return true;
}

bool BuildSectionBlock(BundleStageContextV2& pContext,
                       const PlannedArchiveFileV2& pArchive,
                       std::uint64_t pFamilyBlockIndex,
                       std::uint32_t pLocalBlockIndex,
                       SectionTypeV2 pSectionType,
                       BundleLogicalRecordEncoderV2& pEncoder,
                       bool pEncryptBlock,
                       FixedBlockBufferV2& pOutBlockBytes,
                       std::string& pOutFailureMessage) {
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (pOutBlockBytes.Empty()) {
    pOutFailureMessage = "failed allocating section block buffer.";
    return false;
  }

  unsigned char* aPayload = pOutBlockBytes.Data() + kSectionHeaderBytesV2;
  std::size_t aPayloadBytesWritten = 0u;
  std::uint64_t aLogicalBytes = 0u;
  std::uint64_t aFileBytesWritten = 0u;
  bool aPausedAtBoundary = false;
  if (!pEncoder.Fill(aPayload,
                     aSectionPayloadBytes,
                     false,
                     aPayloadBytesWritten,
                     aLogicalBytes,
                     aFileBytesWritten,
                     aPausedAtBoundary,
                     pOutFailureMessage,
                     true)) {
    return false;
  }
  (void)aLogicalBytes;
  (void)aFileBytesWritten;
  (void)aPausedAtBoundary;

  SectionHeaderV2 aSectionHeader{};
  aSectionHeader.mSectionType = static_cast<std::uint8_t>(pSectionType);
  PopulateSectionBootstrapFields(pContext,
                                 pArchive,
                                 pLocalBlockIndex,
                                 static_cast<std::uint32_t>(aPayloadBytesWritten),
                                 aSectionHeader);
  if (pSectionType == SectionTypeV2::kArchiveData &&
      !TryPopulateSkipRecordForArchiveData(
          pContext,
          pArchive,
          pFamilyBlockIndex,
          aPayloadBytesWritten,
          pEncoder,
          aSectionHeader,
          pOutFailureMessage)) {
    return false;
  }
  aSectionHeader.mRepairRecord.mRepairPointerArchive =
      static_cast<std::uint32_t>(pArchive.mArchiveIndex);
  aSectionHeader.mRepairRecord.mRepairPointerBlock = pLocalBlockIndex;
  aSectionHeader.mCheckSum =
      ComputeSectionCheckSum(aPayload, aSectionPayloadBytes, aSectionHeader);

  if (!WriteSectionHeader(aSectionHeader,
                          pOutBlockBytes.Data(),
                          kSectionHeaderBytesV2,
                          nullptr)) {
    pOutFailureMessage = "failed writing section header.";
    return false;
  }

  if (pEncryptBlock) {
    if (!pContext.State().mCipher.mAssembled) {
      pOutFailureMessage = "repair rebuild expected an assembled cipher.";
      return false;
    }
    if (pContext.State().mCipher.mWorkerBuffer.Size() < aArchiveBlockBytes) {
      pOutFailureMessage = "cipher worker buffer is too small for repair rebuild.";
      return false;
    }
    std::string aSealError;
    if (!pContext.State().mCipher.mCipher.Seal(
            pOutBlockBytes.Data(),
            pContext.State().mCipher.mWorkerBuffer.Data(),
            pOutBlockBytes.Data(),
            aArchiveBlockBytes,
            &aSealError)) {
      pOutFailureMessage = "failed sealing rebuilt section block: " + aSealError;
      return false;
    }
  }

  return true;
}

bool BuildPreviewManifestBlock(BundleStageContextV2& pContext,
                               const PlannedArchiveFileV2& pArchive,
                               std::uint64_t pFamilyBlockIndex,
                               std::uint32_t pLocalBlockIndex,
                               BundleLogicalRecordEncoderV2& pEncoder,
                               FixedBlockBufferV2& pOutBlockBytes,
                               std::string& pOutFailureMessage) {
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (pOutBlockBytes.Empty()) {
    pOutFailureMessage = "failed allocating preview block buffer.";
    return false;
  }

  unsigned char* aPayloadBytes = pOutBlockBytes.Data() + kSectionHeaderBytesV2;
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
                     pOutFailureMessage,
                     true)) {
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
  aSectionHeader.mRepairRecord.mRepairPointerArchive =
      static_cast<std::uint32_t>(pArchive.mArchiveIndex);
  aSectionHeader.mRepairRecord.mRepairPointerBlock = pLocalBlockIndex;
  aSectionHeader.mCheckSum =
      ComputeSectionCheckSum(aPayloadBytes, aSectionPayloadBytes, aSectionHeader);

  if (!WriteSectionHeader(aSectionHeader,
                          pOutBlockBytes.Data(),
                          kSectionHeaderBytesV2,
                          nullptr)) {
    pOutFailureMessage = "failed writing preview section header.";
    return false;
  }

  (void)pFamilyBlockIndex;
  return true;
}

std::string BuildRepairPackingProgressLabel(std::uint64_t pPacked,
                                            std::uint64_t pTotal) {
  return "Packing repair copies: " + std::to_string(pPacked) + "/" +
         std::to_string(pTotal) + " blocks";
}

void EmitBundleRepairBlockEvent(BundleStageContextV2& pContext,
                                RuntimeEventKindV2 pKind,
                                const PlannedArchiveFileV2& pDestinationArchive,
                                std::uint32_t pDestinationLocalBlockIndex,
                                const PlannedArchiveFileV2& pSourceArchive,
                                std::uint32_t pSourceLocalBlockIndex) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = pKind;
  aEvent.mStage = ProgressStageV2::kRepairPacking;
  aEvent.SetInfo("destination_archive_index", pDestinationArchive.mArchiveIndex);
  aEvent.SetInfo("destination_archive_path", pDestinationArchive.mPath);
  aEvent.SetInfo("destination_block_index",
                 static_cast<std::uint64_t>(pDestinationLocalBlockIndex));
  aEvent.SetInfo("source_archive_index", pSourceArchive.mArchiveIndex);
  aEvent.SetInfo("source_archive_path", pSourceArchive.mPath);
  aEvent.SetInfo("source_block_index",
                 static_cast<std::uint64_t>(pSourceLocalBlockIndex));
  aEvent.mLabel =
      std::string(pKind == RuntimeEventKindV2::kBundleRepairBlockStarted
                      ? "Bundle started repair block "
                      : "Bundle finished repair block ") +
      std::to_string(pDestinationLocalBlockIndex) + " in archive " +
      std::to_string(pDestinationArchive.mArchiveIndex);
  pContext.EmitRuntimeEvent(aEvent);
}

std::vector<BundleRecordEntryV2> BuildPreviewRecords(
    const BundleStageContextV2& pContext) {
  std::vector<BundleRecordEntryV2> aPreviewRecords =
      pContext.State().mDiscovery.mEmptyFolderRecords;
  aPreviewRecords.insert(aPreviewRecords.end(),
                         pContext.State().mDiscovery.mFileRecords.begin(),
                         pContext.State().mDiscovery.mFileRecords.end());
  return aPreviewRecords;
}

std::vector<BundleRecordEntryV2> BuildDataRecords(
    const BundleStageContextV2& pContext) {
  std::vector<BundleRecordEntryV2> aDataRecords =
      pContext.State().mDiscovery.mEmptyFolderRecords;
  aDataRecords.insert(aDataRecords.end(),
                      pContext.State().mDiscovery.mFileRecords.begin(),
                      pContext.State().mDiscovery.mFileRecords.end());
  std::sort(aDataRecords.begin(),
            aDataRecords.end(),
            [](const BundleRecordEntryV2& pLeft,
               const BundleRecordEntryV2& pRight) {
              return pLeft.mRelativePath < pRight.mRelativePath;
            });
  return aDataRecords;
}

void NormalizeArchiveWriteTimesAscending(BundleStageContextV2& pContext) {
  const auto aBaseTime = std::filesystem::file_time_type::clock::now();
  const std::vector<PlannedArchiveFileV2>& aArchives =
      pContext.State().mMemoryPlan.mArchives;
  for (std::size_t aArchiveIndex = 0u; aArchiveIndex < aArchives.size(); ++aArchiveIndex) {
    const auto aAdjustedTime =
        aBaseTime + std::chrono::seconds(static_cast<long long>(aArchiveIndex));
    std::error_code aError;
    std::filesystem::last_write_time(
        std::filesystem::path(aArchives[aArchiveIndex].mPath),
        aAdjustedTime,
        aError);
    if (aError) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Bundle][Repair Packing] Could not normalize archive write times to ascending order.");
      return;
    }
  }
}

}  // namespace

class BundleRepairSourceRebuildCursorV2 {
 public:
  explicit BundleRepairSourceRebuildCursorV2(BundleStageContextV2& pContext)
      : mDataRecords(BuildDataRecords(pContext)),
        mPreviewRecords(BuildPreviewRecords(pContext)),
        mDataEncoder(mDataRecords,
                     pContext.FileSystem(),
                     TypedRecordTypeV2::kDataFile,
                     TypedRecordTypeV2::kDataFolder),
        mPreviewEncoder(mPreviewRecords,
                        pContext.FileSystem(),
                        TypedRecordTypeV2::kManifestFile,
                        TypedRecordTypeV2::kManifestFolder,
                        TypedRecordTypeV2::kDataReference,
                        ProgressStageV2::kArchivePacking,
                        RuntimeEventKindV2::kBundleManifestItemStarted,
                        RuntimeEventKindV2::kBundleManifestItemFinished,
                        true),
        mBlockBytes(pContext.Layout().mArchiveBlockBytes) {}

  bool NextBlock(BundleStageContextV2& pContext,
                 const BundleMemoryPlanV2& pMemoryPlan,
                 std::size_t& pOutArchiveIndex,
                 std::uint32_t& pOutLocalBlockIndex,
                 bool& pOutHasBlock,
                 std::string& pOutFailureMessage) {
    pOutFailureMessage.clear();
    pOutArchiveIndex = 0u;
    pOutLocalBlockIndex = 0u;
    pOutHasBlock = false;

    while (mArchiveIndex < pMemoryPlan.mArchives.size()) {
      const PlannedArchiveFileV2& aArchive = pMemoryPlan.mArchives[mArchiveIndex];

      if (mLocalBlockIndex >= aArchive.mBlockCount ||
          mGlobalBlockIndex >= pMemoryPlan.mNonRepairFamilyBlockCount) {
        ++mArchiveIndex;
        mLocalBlockIndex = 0u;
        continue;
      }

      const bool aIsPreviewBlock =
          mGlobalBlockIndex < pMemoryPlan.mPreviewManifestBlockCount;
      const SectionTypeV2 aSectionType = aIsPreviewBlock
                                             ? SectionTypeV2::kPreviewManifest
                                             : SectionTypeV2::kArchiveData;

      if (aIsPreviewBlock) {
        if (!BuildPreviewManifestBlock(pContext,
                                       aArchive,
                                       mGlobalBlockIndex,
                                       mLocalBlockIndex,
                                       mPreviewEncoder,
                                       mBlockBytes,
                                       pOutFailureMessage)) {
          return false;
        }
      } else {
        BundleLogicalRecordEncoderV2& aEncoder = mDataEncoder;
        if (!BuildSectionBlock(pContext,
                               aArchive,
                               mGlobalBlockIndex,
                               mLocalBlockIndex,
                               aSectionType,
                               aEncoder,
                               pContext.Request().mEncryptionEnabled,
                               mBlockBytes,
                               pOutFailureMessage)) {
          return false;
        }
      }

      pOutArchiveIndex = mArchiveIndex;
      pOutLocalBlockIndex = mLocalBlockIndex;
      pOutHasBlock = true;
      ++mLocalBlockIndex;
      ++mGlobalBlockIndex;
      return true;
    }

    return true;
  }

 FixedBlockBufferV2 mBlockBytes;

 private:
  std::vector<BundleRecordEntryV2> mDataRecords;
  std::vector<BundleRecordEntryV2> mPreviewRecords;
  BundleLogicalRecordEncoderV2 mDataEncoder;
  BundleLogicalRecordEncoderV2 mPreviewEncoder;
  std::size_t mArchiveIndex = 0u;
  std::uint32_t mLocalBlockIndex = 0u;
  std::uint64_t mGlobalBlockIndex = 0u;
};

class BundleRepairPackingCursorV2 {
 public:
  BundleRepairPackingCursorV2(BundleStageContextV2& pContext,
                              const BundleMemoryPlanV2& pMemoryPlan)
      : mRepairCopies(
            BuildRepairCopyOrder(pMemoryPlan.mRepairCopySourceLocalBlocks)),
        mRepairDestinations(BuildRepairDestinationOrder(pMemoryPlan)),
        mZeroBlock(pContext.Layout().mArchiveBlockBytes),
        mSourceRebuild(pContext) {}

  std::vector<RepairCopyRefV2> mRepairCopies;
  std::vector<RepairDestinationRefV2> mRepairDestinations;
  std::vector<std::vector<std::size_t>> mCopyIndexBySource;
  FixedBlockBufferV2 mZeroBlock;
  BundleRepairSourceRebuildCursorV2 mSourceRebuild;
  std::uint64_t mNextBlockLog = 64u;
};

bool BundleRepairPackingV2::Run(BundleStageContextV2& pContext) {
  BundlePackingStateV2& aPacking = pContext.State().mPacking;
  const BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;
  std::shared_ptr<BundleRepairPackingCursorV2>& aCursorPtr =
      pContext.State().mCursor.mRepairPacking;

  if (!aCursorPtr) {
    aPacking.mRepairPackedBlockCount = 0u;

    if (!pContext.Request().mRepairEnabled ||
        aMemoryPlan.mRepairSectorBlockCount == 0u) {
      pContext.EmitLog(LogLevelV2::kInfo,
                       LogPhaseSkippedV2(LogActionV2::kBundle,
                                         ProgressStageV2::kRepairPacking,
                                         "repair copies are disabled"));
      pContext.EmitPhaseProgress(1.0, "Repair packing complete");
      return !pContext.IsCancelRequested();
    }

    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseStartedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kRepairPacking));
    pContext.EmitLog(
        LogLevelV2::kInfo,
        "[Bundle][Repair Packing] Packed 0 / " +
            std::to_string(aMemoryPlan.mRepairSectorBlockCount) +
            " repair blocks.");
    if (aPacking.mArchivePaths.size() != aMemoryPlan.mArchives.size()) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           "archive packing did not leave a complete archive path list"));
      return false;
    }

    aCursorPtr = std::make_shared<BundleRepairPackingCursorV2>(
        pContext,
        aMemoryPlan);
    if (aCursorPtr->mRepairCopies.size() != aMemoryPlan.mRepairSectorBlockCount) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           "repair copy planning disagreed with repair-sector block count"));
      aCursorPtr.reset();
      return false;
    }
    if (aCursorPtr->mRepairDestinations.size() !=
        aMemoryPlan.mRepairSectorBlockCount) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           "repair destination planning disagreed with repair-sector block count"));
      aCursorPtr.reset();
      return false;
    }
    if (aCursorPtr->mZeroBlock.Empty() || aCursorPtr->mSourceRebuild.mBlockBytes.Empty()) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           "failed allocating repair packing buffers"));
      aCursorPtr.reset();
      return false;
    }

    std::string aMapFailure;
    if (!BuildCopyIndexBySource(aMemoryPlan,
                                aCursorPtr->mRepairCopies,
                                aCursorPtr->mCopyIndexBySource,
                                aMapFailure)) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           aMapFailure));
      aCursorPtr.reset();
      return false;
    }

    for (std::size_t aArchiveIndex = 0u;
         aArchiveIndex < aMemoryPlan.mArchives.size();
         ++aArchiveIndex) {
      const PlannedArchiveFileV2& aArchive = aMemoryPlan.mArchives[aArchiveIndex];
      const std::uint32_t aSourceBlocksInArchive =
          aArchiveIndex < aMemoryPlan.mSourceArchiveBlockCounts.size()
              ? aMemoryPlan.mSourceArchiveBlockCounts[aArchiveIndex]
              : 0u;
      const std::uint32_t aRepairBlocksInArchive =
          aArchive.mBlockCount > aSourceBlocksInArchive
              ? (aArchive.mBlockCount - aSourceBlocksInArchive)
              : 0u;
      for (std::uint32_t aRepairIndex = 0u;
           aRepairIndex < aRepairBlocksInArchive;
           ++aRepairIndex) {
        if (!pContext.FileSystem().AppendFile(aArchive.mPath,
                                              aCursorPtr->mZeroBlock.Data(),
                                              pContext.Layout().mArchiveBlockBytes)) {
          pContext.EmitLog(
              LogLevelV2::kError,
              LogPhaseFailedV2(LogActionV2::kBundle,
                               ProgressStageV2::kRepairPacking,
                               "failed padding repair block space"));
          aCursorPtr.reset();
          return false;
        }
      }
    }
  }

  BundleRepairPackingCursorV2& aCursor = *aCursorPtr;
  std::size_t aRemainingBatchBudget = std::max<std::size_t>(
      1u, static_cast<std::size_t>(knobs::kBatchSizeBundleRepairV2));
  while (aPacking.mRepairPackedBlockCount < aMemoryPlan.mRepairSectorBlockCount) {
    if (aRemainingBatchBudget == 0u) {
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    std::size_t aSourceArchiveIndex = 0u;
    std::uint32_t aSourceLocalBlockIndex = 0u;
    std::size_t aCopyIndex = kInvalidCopyIndex;
    bool aFoundCopySource = false;
    while (!aFoundCopySource) {
      std::string aBuildError;
      bool aHasSourceBlock = false;
      if (!aCursor.mSourceRebuild.NextBlock(pContext,
                                            aMemoryPlan,
                                            aSourceArchiveIndex,
                                            aSourceLocalBlockIndex,
                                            aHasSourceBlock,
                                            aBuildError)) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(LogActionV2::kBundle,
                             ProgressStageV2::kRepairPacking,
                             aBuildError));
        aCursorPtr.reset();
        return false;
      }
      if (!aHasSourceBlock) {
        break;
      }
      if (aSourceArchiveIndex >= aCursor.mCopyIndexBySource.size()) {
        continue;
      }
      if (aSourceLocalBlockIndex >=
          aCursor.mCopyIndexBySource[aSourceArchiveIndex].size()) {
        continue;
      }
      aCopyIndex =
          aCursor.mCopyIndexBySource[aSourceArchiveIndex][aSourceLocalBlockIndex];
      if (aCopyIndex == kInvalidCopyIndex) {
        continue;
      }
      aFoundCopySource = true;
    }

    if (!aFoundCopySource) {
      break;
    }
    if (aCopyIndex >= aCursor.mRepairDestinations.size()) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           "repair copy index exceeded destination placement list"));
      aCursorPtr.reset();
      return false;
    }

    const RepairDestinationRefV2& aDestination =
        aCursor.mRepairDestinations[aCopyIndex];
    if (aDestination.mArchiveIndex >= aMemoryPlan.mArchives.size()) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           "repair destination referenced an invalid archive"));
      aCursorPtr.reset();
      return false;
    }

    const PlannedArchiveFileV2& aDestinationArchive =
        aMemoryPlan.mArchives[aDestination.mArchiveIndex];
    const PlannedArchiveFileV2& aSourceArchive =
        aMemoryPlan.mArchives[aSourceArchiveIndex];

    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleRepairBlockStarted)) {
      EmitBundleRepairBlockEvent(pContext,
                                 RuntimeEventKindV2::kBundleRepairBlockStarted,
                                 aDestinationArchive,
                                 aDestination.mLocalBlockIndex,
                                 aSourceArchive,
                                 aSourceLocalBlockIndex);
    }

    const std::uint64_t aOffset64 =
        static_cast<std::uint64_t>(kArchiveHeaderBytesV2) +
        (static_cast<std::uint64_t>(aDestination.mLocalBlockIndex) *
         static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes));
    if (aOffset64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           "repair destination offset exceeded platform limits"));
      aCursorPtr.reset();
      return false;
    }

    if (!pContext.FileSystem().OverwriteFileRegion(
            aDestinationArchive.mPath,
            static_cast<std::size_t>(aOffset64),
            aCursor.mSourceRebuild.mBlockBytes.Data(),
            pContext.Layout().mArchiveBlockBytes)) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           "failed writing rebuilt repair block"));
      aCursorPtr.reset();
      return false;
    }

    ++aPacking.mRepairPackedBlockCount;
    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleRepairBlockFinished)) {
      EmitBundleRepairBlockEvent(pContext,
                                 RuntimeEventKindV2::kBundleRepairBlockFinished,
                                 aDestinationArchive,
                                 aDestination.mLocalBlockIndex,
                                 aSourceArchive,
                                 aSourceLocalBlockIndex);
    }
    pContext.EmitPhaseProgress(
        static_cast<double>(aPacking.mRepairPackedBlockCount) /
            static_cast<double>(aMemoryPlan.mRepairSectorBlockCount),
        BuildRepairPackingProgressLabel(aPacking.mRepairPackedBlockCount,
                                        aMemoryPlan.mRepairSectorBlockCount));
    while (aPacking.mRepairPackedBlockCount >= aCursor.mNextBlockLog) {
      pContext.EmitLog(
          LogLevelV2::kInfo,
          "[Bundle][Repair Packing] Packed " +
              std::to_string(aPacking.mRepairPackedBlockCount) + " / " +
              std::to_string(aMemoryPlan.mRepairSectorBlockCount) +
              " repair blocks.");
      aCursor.mNextBlockLog += 64u;
    }
    if (aRemainingBatchBudget > 0u) {
      --aRemainingBatchBudget;
    }
  }

  if (aPacking.mRepairPackedBlockCount != aMemoryPlan.mRepairSectorBlockCount) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionV2::kBundle,
                         ProgressStageV2::kRepairPacking,
                         "repair copy generation left unplaced repair blocks"));
    aCursorPtr.reset();
    return false;
  }

  NormalizeArchiveWriteTimesAscending(pContext);

  aCursorPtr.reset();
  pContext.EmitLog(
      LogLevelV2::kInfo,
      "[Bundle][Repair Packing] Packed " +
          std::to_string(aPacking.mRepairPackedBlockCount) +
          " exact repair copies (" +
          std::to_string(
              static_cast<std::uint64_t>(
                  RepairCoveragePercentV2(pContext.Request().mRepairCoverage))) +
          "% front-of-archive coverage).");
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kRepairPacking));
  pContext.EmitPhaseProgress(1.0, "Repair packing complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
