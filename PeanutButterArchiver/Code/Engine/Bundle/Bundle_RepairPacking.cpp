#include "Bundle_RepairPacking.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <memory>
#include <random>
#include <vector>

#include "../../Knobs.hpp"
#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "Bundle_LogicalRecordEncoder.hpp"

namespace peanutbutter {
namespace {

using namespace memory_layout;

constexpr std::size_t kInvalidCopyIndex = std::numeric_limits<std::size_t>::max();

template <typename UInt>
UInt RandomInclusive(UInt pMin, UInt pMax) {
  if (pMin >= pMax) {
    return pMin;
  }
  static thread_local std::mt19937_64 sGenerator([]() {
    std::random_device aDevice;
    std::seed_seq aSeed{
        aDevice(), aDevice(), aDevice(), aDevice(), aDevice(), aDevice()};
    return std::mt19937_64(aSeed);
  }());
  std::uniform_int_distribution<std::uint64_t> aDistribution(
      static_cast<std::uint64_t>(pMin), static_cast<std::uint64_t>(pMax));
  return static_cast<UInt>(aDistribution(sGenerator));
}

constexpr std::uint32_t kSkipArchiveIndexMaxV2 = 0x00FFFFFFu;
constexpr std::uint32_t kSkipByteIndexMaxV2 = 0x00FFFFFFu;

std::uint32_t ComputeMaxLegalSkipArchiveIndex(const BundleStageContextV2& pContext) {
  const std::uint64_t aArchiveCount = pContext.State().mMemoryPlan.mArchiveCount;
  if (aArchiveCount == 0u) {
    return 0u;
  }
  const std::uint64_t aMaxLegal = aArchiveCount - 1u;
  const std::uint64_t aCeiling = static_cast<std::uint64_t>(kSkipArchiveIndexMaxV2);
  if (aMaxLegal >= aCeiling) {
    return kSkipArchiveIndexMaxV2;
  }
  return static_cast<std::uint32_t>(aMaxLegal);
}

std::uint16_t ComputeMaxLegalSkipBlockIndex(const BundleStageContextV2& pContext) {
  const std::uint64_t aBlocksPerArchive =
      static_cast<std::uint64_t>(pContext.Layout().mMaxBlocksPerArchive);
  if (aBlocksPerArchive == 0u) {
    return 0u;
  }
  const std::uint64_t aMaxLegal = aBlocksPerArchive - 1u;
  const std::uint64_t aCeiling = static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max());
  if (aMaxLegal >= aCeiling) {
    return std::numeric_limits<std::uint16_t>::max();
  }
  return static_cast<std::uint16_t>(aMaxLegal);
}

std::uint32_t ComputeMaxLegalSkipByteIndex(const BundleStageContextV2& pContext) {
  const std::uint64_t aPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (aPayloadBytes == 0u) {
    return 0u;
  }
  const std::uint64_t aMaxLegal = aPayloadBytes - 1u;
  const std::uint64_t aCeiling = static_cast<std::uint64_t>(kSkipByteIndexMaxV2);
  if (aMaxLegal >= aCeiling) {
    return kSkipByteIndexMaxV2;
  }
  return static_cast<std::uint32_t>(aMaxLegal);
}

SkipRecordV2 MakeInvalidSkipRecord(const BundleStageContextV2& pContext) {
  SkipRecordV2 aSkip{};
  const std::uint32_t aArchiveMaxLegal = ComputeMaxLegalSkipArchiveIndex(pContext);
  const std::uint32_t aArchiveLower = (aArchiveMaxLegal < kSkipArchiveIndexMaxV2)
                                          ? (aArchiveMaxLegal + 1u)
                                          : kSkipArchiveIndexMaxV2;
  const std::uint32_t aArchiveInvalid =
      RandomInclusive<std::uint32_t>(aArchiveLower, kSkipArchiveIndexMaxV2);
  (void)SetSkipRecordArchiveIndex(aSkip, aArchiveInvalid, nullptr);

  const std::uint16_t aBlockMaxLegal = ComputeMaxLegalSkipBlockIndex(pContext);
  const std::uint16_t aBlockLower = (aBlockMaxLegal < std::numeric_limits<std::uint16_t>::max())
                                        ? static_cast<std::uint16_t>(aBlockMaxLegal + 1u)
                                        : std::numeric_limits<std::uint16_t>::max();
  aSkip.mBlockIndex =
      RandomInclusive<std::uint16_t>(aBlockLower, std::numeric_limits<std::uint16_t>::max());

  const std::uint32_t aByteUpper =
      kSkipByteIndexMaxV2;
  const std::uint32_t aByteMaxLegal = ComputeMaxLegalSkipByteIndex(pContext);
  const std::uint32_t aByteLower = (aByteMaxLegal < aByteUpper) ? (aByteMaxLegal + 1u) : aByteUpper;
  (void)SetSkipRecordByteDistance(
      aSkip, RandomInclusive<std::uint32_t>(aByteLower, aByteUpper), nullptr);
  return aSkip;
}

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
  std::size_t aTotalCopies = 0u;
  std::size_t aMaxLayer = 0u;
  for (const std::vector<std::uint32_t>& aSourceLocalBlocks :
       pRepairCopySourceLocalBlocks) {
    aTotalCopies += aSourceLocalBlocks.size();
    aMaxLayer = std::max(aMaxLayer, aSourceLocalBlocks.size());
  }
  aCopies.reserve(aTotalCopies);
  if (aMaxLayer == 0u) {
    return aCopies;
  }

  std::vector<std::vector<RepairCopyRefV2>> aLayerBuckets(aMaxLayer);
  for (std::size_t aArchiveIndex = 0u;
       aArchiveIndex < pRepairCopySourceLocalBlocks.size();
       ++aArchiveIndex) {
    const std::vector<std::uint32_t>& aSourceLocalBlocks =
        pRepairCopySourceLocalBlocks[aArchiveIndex];
    for (std::size_t aLayer = 0u; aLayer < aSourceLocalBlocks.size(); ++aLayer) {
      aLayerBuckets[aLayer].push_back(
          {aArchiveIndex, aSourceLocalBlocks[aLayer]});
    }
  }
  for (const std::vector<RepairCopyRefV2>& aLayerCopies : aLayerBuckets) {
    aCopies.insert(aCopies.end(), aLayerCopies.begin(), aLayerCopies.end());
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
  (void)TrySetPackedUint48(
      pOutHeader.mBlockCountMain, aPlan.mBlockCountMain, nullptr, "BlockCountMain");
  (void)TrySetPackedUint48(pOutHeader.mBlockCountPreview,
                           aPlan.mBlockCountPreview,
                           nullptr,
                           "BlockCountPreview");
  (void)TrySetPackedUint48(pOutHeader.mBlockCountRepair,
                           aPlan.mRepairSectorBlockCount,
                           nullptr,
                           "BlockCountRepair");
  pOutHeader.mArchiveFamilyId = aPlan.mArchiveFamilyId;
}

bool TrySetRepairRecordTarget(std::uint64_t pArchiveIndex,
                              std::uint32_t pLocalBlockIndex,
                              SectionHeaderV2& pOutHeader,
                              std::string& pOutFailureMessage) {
  if (pArchiveIndex > std::numeric_limits<std::uint16_t>::max()) {
    pOutFailureMessage =
        "repair record archive index exceeds uint16_t range.";
    return false;
  }
  if (pLocalBlockIndex > std::numeric_limits<std::uint16_t>::max()) {
    pOutFailureMessage =
        "repair record block index exceeds uint16_t range.";
    return false;
  }
  pOutHeader.mRepairRecord.mArchiveIndex =
      static_cast<std::uint16_t>(pArchiveIndex);
  pOutHeader.mRepairRecord.mBlockIndex =
      static_cast<std::uint16_t>(pLocalBlockIndex);
  return true;
}

bool BuildSectionBlock(BundleStageContextV2& pContext,
                       const PlannedArchiveFileV2& pArchive,
                       std::uint64_t pFamilyBlockIndex,
                       std::uint32_t pLocalBlockIndex,
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
  aSectionHeader.mSkipRecord = MakeInvalidSkipRecord(pContext);
  aSectionHeader.mSectionType =
      static_cast<std::uint8_t>(SectionTypeV2::kRepairData);
  PopulateSectionBootstrapFields(pContext,
                                 pArchive,
                                 pLocalBlockIndex,
                                 static_cast<std::uint32_t>(aPayloadBytesWritten),
                                 aSectionHeader);
  if (!TrySetRepairRecordTarget(
          pArchive.mArchiveIndex,
          pLocalBlockIndex,
          aSectionHeader,
          pOutFailureMessage)) {
    return false;
  }
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
  aSectionHeader.mSkipRecord = MakeInvalidSkipRecord(pContext);
  aSectionHeader.mSectionType =
      static_cast<std::uint8_t>(SectionTypeV2::kRepairData);
  PopulateSectionBootstrapFields(pContext,
                                 pArchive,
                                 pLocalBlockIndex,
                                 static_cast<std::uint32_t>(aChunkBytes),
                                 aSectionHeader);
  if (!TrySetRepairRecordTarget(
          pArchive.mArchiveIndex,
          pLocalBlockIndex,
          aSectionHeader,
          pOutFailureMessage)) {
    return false;
  }
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

bool RetargetRebuiltRepairBlock(BundleStageContextV2& pContext,
                                const PlannedArchiveFileV2& pDestinationArchive,
                                std::uint32_t pDestinationLocalBlockIndex,
                                const PlannedArchiveFileV2& pSourceArchive,
                                std::uint32_t pSourceLocalBlockIndex,
                                bool pSourceWasPreviewBlock,
                                FixedBlockBufferV2& pInOutBlockBytes,
                                std::string& pOutFailureMessage) {
  pOutFailureMessage.clear();
  if (pInOutBlockBytes.Empty()) {
    pOutFailureMessage = "rebuild buffer was empty while retargeting repair block.";
    return false;
  }

  SectionHeaderV2 aHeader{};
  if (!ReadSectionHeader(
          pInOutBlockBytes.Data(), kSectionHeaderBytesV2, aHeader, nullptr)) {
    pOutFailureMessage = "failed reading rebuilt repair block header.";
    return false;
  }

  const std::uint32_t aPayloadBytesUsed = aHeader.mPayloadBytesUsed;
  aHeader.mSkipRecord = MakeInvalidSkipRecord(pContext);
  aHeader.mSectionType =
      static_cast<std::uint8_t>(SectionTypeV2::kRepairData);
  PopulateSectionBootstrapFields(pContext,
                                 pDestinationArchive,
                                 pDestinationLocalBlockIndex,
                                 aPayloadBytesUsed,
                                 aHeader);
  if (!TrySetRepairRecordTarget(
          pSourceArchive.mArchiveIndex,
          pSourceLocalBlockIndex,
          aHeader,
          pOutFailureMessage)) {
    return false;
  }

  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  unsigned char* aPayload = pInOutBlockBytes.Data() + kSectionHeaderBytesV2;
  aHeader.mCheckSum =
      ComputeSectionCheckSum(aPayload, aSectionPayloadBytes, aHeader);

  if (!WriteSectionHeader(
          aHeader, pInOutBlockBytes.Data(), kSectionHeaderBytesV2, nullptr)) {
    pOutFailureMessage = "failed writing retargeted repair block header.";
    return false;
  }

  if (pContext.Request().mEncryptionEnabled && !pSourceWasPreviewBlock) {
    if (!pContext.State().mCipher.mAssembled) {
      pOutFailureMessage = "repair retarget expected an assembled cipher.";
      return false;
    }
    if (pContext.State().mCipher.mWorkerBuffer.Size() <
        pContext.Layout().mArchiveBlockBytes) {
      pOutFailureMessage =
          "cipher worker buffer is too small while retargeting repair block.";
      return false;
    }

    std::string aSealError;
    if (!pContext.State().mCipher.mCipher.Seal(
            pInOutBlockBytes.Data(),
            pContext.State().mCipher.mWorkerBuffer.Data(),
            pInOutBlockBytes.Data(),
            pContext.Layout().mArchiveBlockBytes,
            &aSealError)) {
      pOutFailureMessage =
          "failed sealing retargeted repair block: " + aSealError;
      return false;
    }
  }

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
  std::sort(aPreviewRecords.begin(),
            aPreviewRecords.end(),
            [](const BundleRecordEntryV2& pLeft,
               const BundleRecordEntryV2& pRight) {
              return pLeft.mRelativePath < pRight.mRelativePath;
            });
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
                 bool& pOutIsPreviewBlock,
                 bool& pOutHasBlock,
                 std::string& pOutFailureMessage) {
    pOutFailureMessage.clear();
    pOutArchiveIndex = 0u;
    pOutLocalBlockIndex = 0u;
    pOutIsPreviewBlock = false;
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
          mGlobalBlockIndex < pMemoryPlan.mBlockCountPreview;
      pOutIsPreviewBlock = aIsPreviewBlock;
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
                               aEncoder,
                               false,
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
    bool aSourceWasPreviewBlock = false;
    std::size_t aCopyIndex = kInvalidCopyIndex;
    bool aFoundCopySource = false;
    while (!aFoundCopySource) {
      std::string aBuildError;
      bool aCurrentSourceWasPreviewBlock = false;
      bool aHasSourceBlock = false;
      if (!aCursor.mSourceRebuild.NextBlock(pContext,
                                            aMemoryPlan,
                                            aSourceArchiveIndex,
                                            aSourceLocalBlockIndex,
                                            aCurrentSourceWasPreviewBlock,
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
      aSourceWasPreviewBlock = aCurrentSourceWasPreviewBlock;
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

    std::string aRetargetError;
    if (!RetargetRebuiltRepairBlock(pContext,
                                    aDestinationArchive,
                                    aDestination.mLocalBlockIndex,
                                    aSourceArchive,
                                    aSourceLocalBlockIndex,
                                    aSourceWasPreviewBlock,
                                    aCursor.mSourceRebuild.mBlockBytes,
                                    aRetargetError)) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kBundle,
                           ProgressStageV2::kRepairPacking,
                           aRetargetError));
      aCursorPtr.reset();
      return false;
    }

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
