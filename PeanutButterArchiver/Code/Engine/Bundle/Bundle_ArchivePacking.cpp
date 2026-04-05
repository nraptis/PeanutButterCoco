#include "Bundle_ArchivePacking.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <random>

#include "../../Knobs.hpp"
#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "Bundle_LogicalRecordEncoder.hpp"

namespace peanutbutter {
namespace {

using namespace memory_layout;

constexpr std::uint64_t kArchiveProgressArchiveLogIntervalV2 =
    knobs::kBundleArchiveProgressArchiveLogIntervalV2;
constexpr std::uint64_t kArchiveProgressFileLogIntervalV2 =
    knobs::kBundleArchiveProgressFileLogIntervalV2;
constexpr std::uint64_t kArchiveProgressByteLogIntervalV2 =
    knobs::kBundleArchiveProgressByteLogIntervalV2;

struct PrecomputedSkipRecordEntryV2 {
  bool mHasValidSkip = false;
  SkipRecordV2 mSkipRecord{};
};

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

const char* SectionTypeLabel(SectionTypeV2 pSectionType) {
  switch (pSectionType) {
    case SectionTypeV2::kPreviewManifest:
      return "preview_manifest";
    case SectionTypeV2::kArchiveData:
      return "archive_data";
    case SectionTypeV2::kRepairData:
      return "repair_data";
  }
  return "unknown";
}

void EmitBundleArchiveEvent(BundleStageContextV2& pContext,
                            const PlannedArchiveFileV2& pArchive) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kBundleArchiveStarted;
  aEvent.mStage = ProgressStageV2::kArchivePacking;
  aEvent.mLabel =
      "Bundle started writing archive " + std::to_string(pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("block_count", static_cast<std::uint64_t>(pArchive.mBlockCount));
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitBundleArchiveFinishedEvent(BundleStageContextV2& pContext,
                                    const PlannedArchiveFileV2& pArchive,
                                    std::uint64_t pWrittenBlockCount) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kBundleArchiveFinished;
  aEvent.mStage = ProgressStageV2::kArchivePacking;
  aEvent.mLabel =
      "Bundle finished writing archive " + std::to_string(pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("block_count", static_cast<std::uint64_t>(pArchive.mBlockCount));
  aEvent.SetInfo("written_block_count", pWrittenBlockCount);
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitBundleArchiveHeaderWrittenEvent(BundleStageContextV2& pContext,
                                         const PlannedArchiveFileV2& pArchive,
                                         const ArchiveHeaderV2& pHeader) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kBundleArchiveHeaderWritten;
  aEvent.mStage = ProgressStageV2::kArchivePacking;
  aEvent.mLabel =
      "Bundle wrote provisional archive header for archive " +
      std::to_string(pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("archive_count",
                 PackedUint48ToUInt64(pHeader.mArchiveCount));
  aEvent.SetInfo("archive_family_id", pHeader.mArchiveFamilyId);
  aEvent.SetInfo("dirty_state", static_cast<std::uint64_t>(pHeader.mDirtyState));
  aEvent.SetInfo("is_encrypted", pHeader.mIsEncrypted != 0u);
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitBundleBlockEvent(BundleStageContextV2& pContext,
                          RuntimeEventKindV2 pKind,
                          const PlannedArchiveFileV2& pArchive,
                          std::uint64_t pFamilyBlockIndex,
                          std::uint32_t pLocalBlockIndex,
                          SectionTypeV2 pSectionType,
                          const std::string& pFileReference) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = pKind;
  aEvent.mStage = ProgressStageV2::kArchivePacking;
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("family_block_index", pFamilyBlockIndex);
  aEvent.SetInfo("block_index", static_cast<std::uint64_t>(pLocalBlockIndex));
  aEvent.SetInfo("section_type", SectionTypeLabel(pSectionType));
  if (!pFileReference.empty()) {
    aEvent.SetInfo("file_name", pFileReference);
    aEvent.SetInfo("relative_path", pFileReference);
  }

  aEvent.mLabel =
      std::string(pKind == RuntimeEventKindV2::kBundleBlockStarted
                      ? "Bundle started block "
                      : "Bundle finished block ") +
      std::to_string(pLocalBlockIndex) + " in archive " +
      std::to_string(pArchive.mArchiveIndex);
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitBundleBlockHeaderEvent(BundleStageContextV2& pContext,
                                const PlannedArchiveFileV2& pArchive,
                                std::uint64_t pFamilyBlockIndex,
                                std::uint32_t pLocalBlockIndex,
                                SectionTypeV2 pSectionType,
                                std::uint32_t pPayloadBytesUsed,
                                const std::string& pFileReference) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kBundleBlockHeaderWritten;
  aEvent.mStage = ProgressStageV2::kArchivePacking;
  aEvent.mLabel =
      "Bundle wrote block header " + std::to_string(pLocalBlockIndex) +
      " in archive " + std::to_string(pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("family_block_index", pFamilyBlockIndex);
  aEvent.SetInfo("block_index", static_cast<std::uint64_t>(pLocalBlockIndex));
  aEvent.SetInfo("section_type", SectionTypeLabel(pSectionType));
  aEvent.SetInfo("payload_bytes_used",
                 static_cast<std::uint64_t>(pPayloadBytesUsed));
  if (!pFileReference.empty()) {
    aEvent.SetInfo("file_name", pFileReference);
    aEvent.SetInfo("relative_path", pFileReference);
  }
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitBundleEncryptionEvent(BundleStageContextV2& pContext,
                               const PlannedArchiveFileV2& pArchive,
                               std::uint64_t pFamilyBlockIndex,
                               std::uint32_t pLocalBlockIndex) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kBundleEncryptionFinished;
  aEvent.mStage = ProgressStageV2::kArchivePacking;
  aEvent.mLabel =
      "Bundle sealed block " + std::to_string(pLocalBlockIndex) +
      " in archive " + std::to_string(pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("family_block_index", pFamilyBlockIndex);
  aEvent.SetInfo("block_index", static_cast<std::uint64_t>(pLocalBlockIndex));
  pContext.EmitRuntimeEvent(aEvent);
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

RepairRecordV2 MakeInvalidRepairRecord(const BundleStageContextV2& pContext) {
  RepairRecordV2 aRepair{};
  const std::uint16_t aCeiling = std::numeric_limits<std::uint16_t>::max();
  const std::uint64_t aArchiveCount = pContext.State().mMemoryPlan.mArchiveCount;
  const std::uint64_t aArchiveMaxLegal = aArchiveCount == 0u
                                             ? 0u
                                             : (aArchiveCount - 1u);
  const std::uint64_t aArchiveCeiling = static_cast<std::uint64_t>(aCeiling);
  if (aArchiveMaxLegal < aArchiveCeiling) {
    const std::uint16_t aArchiveLower = static_cast<std::uint16_t>(aArchiveMaxLegal + 1u);
    aRepair.mArchiveIndex = RandomInclusive<std::uint16_t>(aArchiveLower, aCeiling);
  } else {
    aRepair.mArchiveIndex = aCeiling;
  }

  const std::uint64_t aBlocksPerArchive =
      static_cast<std::uint64_t>(pContext.Layout().mMaxBlocksPerArchive);
  const std::uint64_t aBlockMaxLegal = aBlocksPerArchive == 0u
                                           ? 0u
                                           : (aBlocksPerArchive - 1u);
  const std::uint64_t aBlockCeiling = static_cast<std::uint64_t>(aCeiling);
  if (aBlockMaxLegal < aBlockCeiling) {
    const std::uint16_t aBlockLower = static_cast<std::uint16_t>(aBlockMaxLegal + 1u);
    aRepair.mBlockIndex = RandomInclusive<std::uint16_t>(aBlockLower, aCeiling);
  } else {
    aRepair.mBlockIndex = aCeiling;
  }

  return aRepair;
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
  SetArchiveHeaderCountMods(
      pOutHeader,
      pContext.State().mMemoryPlan.mFileCountMod256,
      pContext.State().mMemoryPlan.mFolderCountMod256);
  pOutHeader.mArchiveFamilyId = pContext.State().mMemoryPlan.mArchiveFamilyId;

  return TrySetPackedUint48(pOutHeader.mArchiveIndex,
                            pArchiveIndex,
                            nullptr,
                            "ArchiveIndex") &&
         TrySetPackedUint48(pOutHeader.mArchiveCount,
                            pContext.State().mMemoryPlan.mArchiveCount,
                            nullptr,
                            "ArchiveCount") &&
         TrySetPackedUint48(pOutHeader.mBlockCountMain,
                            pContext.State().mMemoryPlan.mBlockCountMain,
                            nullptr,
                            "BlockCountMain") &&
         TrySetPackedUint48(pOutHeader.mReservedCount0,
                            0u,
                            nullptr,
                            "ReservedCount0") &&
         TrySetPackedUint48(pOutHeader.mBlockCountPreview,
                            pContext.State().mMemoryPlan.mBlockCountPreview,
                            nullptr,
                            "BlockCountPreview") &&
         TrySetPackedUint48(pOutHeader.mBlockCountRepair,
                            pContext.State().mMemoryPlan.mRepairSectorBlockCount,
                            nullptr,
                            "BlockCountRepair");
}

bool WriteArchiveHeaderPrefix(BundleStageContextV2& pContext,
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
  if (!pWriteStream.Write(aHeaderBytes.data(), aHeaderBytes.size())) {
    return false;
  }
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleArchiveHeaderWritten)) {
    EmitBundleArchiveHeaderWrittenEvent(pContext, pArchive, aHeader);
  }
  return true;
}

bool FillSectionPayload(BundleStageContextV2& pContext,
                        BundleLogicalRecordEncoderV2& pEncoder,
                        std::size_t pPayloadCapacity,
                        bool pPauseAfterCurrentFileBoundary,
                        unsigned char* pPayloadBytes,
                        std::size_t& pOutPayloadBytes,
                        std::uint64_t& pOutFileBytes,
                        bool& pOutPausedAtBoundary,
                        std::string& pOutFailureMessage) {
  std::uint64_t aLogicalBytes = 0u;
  pOutPausedAtBoundary = false;
  return pEncoder.Fill(pPayloadBytes,
                       pPayloadCapacity,
                       pPauseAfterCurrentFileBoundary,
                       pOutPayloadBytes,
                       aLogicalBytes,
                       pOutFileBytes,
                       pOutPausedAtBoundary,
                       pOutFailureMessage,
                       true);
}

std::uint64_t ComputeDataRecordLogicalBytes(
    const BundleRecordEntryV2& pRecord) {
  const TypedRecordTypeV2 aType = pRecord.mIsReference
                                      ? TypedRecordTypeV2::kDataReference
                                      : (pRecord.mIsDirectory
                                             ? TypedRecordTypeV2::kDataFolder
                                             : TypedRecordTypeV2::kDataFile);
  std::uint64_t aBytes =
      2u + static_cast<std::uint64_t>(pRecord.mRelativePath.size()) + 1u;
  if (TypedRecordTypeIsReferenceV2(static_cast<std::uint8_t>(aType))) {
    aBytes += 1u;  // reference kind
    aBytes += 2u;  // reference target length
    aBytes += static_cast<std::uint64_t>(pRecord.mReferenceTargetRelativePath.size());
    return aBytes;
  }
  if (TypedRecordTypeHasFileSizeV2(static_cast<std::uint8_t>(aType))) {
    aBytes += 8u;
  }
  if (TypedRecordTypeHasContentBytesV2(static_cast<std::uint8_t>(aType))) {
    aBytes += pRecord.mContentLength;
  }
  return aBytes;
}

bool BuildPrecomputedDataSkipRecords(
    const BundleStageContextV2& pContext,
    const std::vector<BundleRecordEntryV2>& pDataRecords,
    std::vector<PrecomputedSkipRecordEntryV2>& pOutSkipRecords,
    std::string& pOutFailureMessage) {
  pOutFailureMessage.clear();
  pOutSkipRecords.clear();

  const BundleMemoryPlanV2& aPlan = pContext.State().mMemoryPlan;
  const std::uint64_t aPayloadBytesPerBlock =
      static_cast<std::uint64_t>(pContext.Layout().SectionPayloadBytes());
  if (aPayloadBytesPerBlock == 0u) {
    pOutFailureMessage =
        "section payload bytes must be at least 1 for precomputed skip records.";
    return false;
  }

  const std::uint64_t aDataBlockCount = aPlan.mBlockCountMain;
  if (aDataBlockCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    pOutFailureMessage = "archive-data block count exceeded addressable size.";
    return false;
  }
  pOutSkipRecords.resize(static_cast<std::size_t>(aDataBlockCount));
  for (PrecomputedSkipRecordEntryV2& aEntry : pOutSkipRecords) {
    aEntry.mHasValidSkip = false;
    aEntry.mSkipRecord = MakeInvalidSkipRecord(pContext);
  }
  if (aDataBlockCount == 0u) {
    return true;
  }

  std::vector<std::uint64_t> aRecordStartBytes;
  aRecordStartBytes.reserve(pDataRecords.size());
  std::uint64_t aRecordCursorBytes = 0u;
  for (const BundleRecordEntryV2& aRecord : pDataRecords) {
    aRecordStartBytes.push_back(aRecordCursorBytes);
    aRecordCursorBytes += ComputeDataRecordLogicalBytes(aRecord);
  }

  const std::uint64_t aPreviewBlockCount = aPlan.mBlockCountPreview;
  const std::uint64_t aNonRepairBlockCount = aPlan.mNonRepairFamilyBlockCount;
  if (aNonRepairBlockCount >
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    pOutFailureMessage = "non-repair family block count exceeded addressable size.";
    return false;
  }

  const std::size_t aNonRepairBlockCountSz =
      static_cast<std::size_t>(aNonRepairBlockCount);
  std::vector<std::uint32_t> aArchiveByFamily(
      aNonRepairBlockCountSz, std::numeric_limits<std::uint32_t>::max());
  std::vector<std::uint32_t> aLocalByFamily(
      aNonRepairBlockCountSz, std::numeric_limits<std::uint32_t>::max());
  for (const PlannedArchiveFileV2& aArchive : aPlan.mArchives) {
    if (aArchive.mFamilyBlockStart >= aNonRepairBlockCount) {
      break;
    }
    if (aArchive.mArchiveIndex >
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
      continue;
    }
    const std::uint64_t aMaxLocal =
        std::min<std::uint64_t>(static_cast<std::uint64_t>(aArchive.mBlockCount),
                                aNonRepairBlockCount - aArchive.mFamilyBlockStart);
    for (std::uint64_t aLocal = 0u; aLocal < aMaxLocal; ++aLocal) {
      const std::uint64_t aFamily = aArchive.mFamilyBlockStart + aLocal;
      if (aFamily >= aNonRepairBlockCount) {
        break;
      }
      const std::size_t aFamilySlot = static_cast<std::size_t>(aFamily);
      aArchiveByFamily[aFamilySlot] = static_cast<std::uint32_t>(aArchive.mArchiveIndex);
      aLocalByFamily[aFamilySlot] = static_cast<std::uint32_t>(aLocal);
    }
  }

  std::size_t aBoundaryIndex = 0u;
  for (std::uint64_t aDataBlockIndex = 0u; aDataBlockIndex < aDataBlockCount;
       ++aDataBlockIndex) {
    if (aDataBlockIndex == 0u) {
      continue;
    }
    const std::uint64_t aFamilyBlockIndex = aPreviewBlockCount + aDataBlockIndex;
    if (aFamilyBlockIndex == 0u) {
      continue;
    }

    const std::uint64_t aBlockStartBytes = aDataBlockIndex * aPayloadBytesPerBlock;
    while (aBoundaryIndex < aRecordStartBytes.size() &&
           aRecordStartBytes[aBoundaryIndex] < aBlockStartBytes) {
      ++aBoundaryIndex;
    }
    if (aBoundaryIndex >= aRecordStartBytes.size()) {
      continue;
    }

    const std::uint64_t aTargetRecordStartBytes = aRecordStartBytes[aBoundaryIndex];
    const std::uint64_t aTargetDataBlockIndex =
        aTargetRecordStartBytes / aPayloadBytesPerBlock;
    const std::uint32_t aTargetByteOffset = static_cast<std::uint32_t>(
        aTargetRecordStartBytes % aPayloadBytesPerBlock);
    const std::uint64_t aTargetFamilyBlockIndex =
        aPreviewBlockCount + aTargetDataBlockIndex;
    if (aTargetFamilyBlockIndex >= aNonRepairBlockCount) {
      continue;
    }

    const std::size_t aTargetFamilySlot =
        static_cast<std::size_t>(aTargetFamilyBlockIndex);
    const std::uint32_t aTargetArchiveIndex = aArchiveByFamily[aTargetFamilySlot];
    const std::uint32_t aTargetLocalBlockIndex = aLocalByFamily[aTargetFamilySlot];
    if (aTargetArchiveIndex == std::numeric_limits<std::uint32_t>::max() ||
        aTargetArchiveIndex > kSkipArchiveIndexMaxV2 ||
        aTargetLocalBlockIndex > std::numeric_limits<std::uint16_t>::max()) {
      continue;
    }

    SkipRecordV2 aSkipRecord{};
    if (!SetSkipRecordArchiveIndex(aSkipRecord, aTargetArchiveIndex, nullptr)) {
      continue;
    }
    aSkipRecord.mBlockIndex = static_cast<std::uint16_t>(aTargetLocalBlockIndex);
    if (!SetSkipRecordByteDistance(aSkipRecord, aTargetByteOffset, nullptr)) {
      continue;
    }

    PrecomputedSkipRecordEntryV2& aEntry =
        pOutSkipRecords[static_cast<std::size_t>(aDataBlockIndex)];
    aEntry.mHasValidSkip = true;
    aEntry.mSkipRecord = aSkipRecord;
  }
  return true;
}

bool WriteSectionBlock(BundleStageContextV2& pContext,
                       const PlannedArchiveFileV2& pArchive,
                       std::uint64_t pFamilyBlockIndex,
                       std::uint32_t pLocalBlockIndex,
                       SectionTypeV2 pSectionType,
                       const std::string& pFileReference,
                       BundleLogicalRecordEncoderV2& pEncoder,
                       const PrecomputedSkipRecordEntryV2* pPrecomputedSkipRecord,
                       bool pPauseAfterCurrentFileBoundary,
                       bool pEncryptBlock,
                       FixedBlockBufferV2& pPlainBlock,
                       FileWriteStreamV2& pWriteStream,
                       std::uint64_t& pOutFileBytesWritten,
                       bool& pOutPausedAtBoundary,
                       std::string& pOutFailureMessage) {
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (pPlainBlock.Empty()) {
    pOutFailureMessage = "failed allocating block buffers.";
    return false;
  }

  unsigned char* aPayload = pPlainBlock.Data() + kSectionHeaderBytesV2;
  std::size_t aPayloadBytesWritten = 0u;
  bool aPausedAtBoundary = false;
  if (!FillSectionPayload(pContext,
                          pEncoder,
                          aSectionPayloadBytes,
                          pPauseAfterCurrentFileBoundary,
                          aPayload,
                          aPayloadBytesWritten,
                          pOutFileBytesWritten,
                          aPausedAtBoundary,
                          pOutFailureMessage)) {
    return false;
  }

  SectionHeaderV2 aSectionHeader{};
  aSectionHeader.mSkipRecord = MakeInvalidSkipRecord(pContext);
  aSectionHeader.mSectionType = static_cast<std::uint8_t>(pSectionType);
  (void)aPausedAtBoundary;
  PopulateSectionBootstrapFields(pContext,
                                 pArchive,
                                 pLocalBlockIndex,
                                 static_cast<std::uint32_t>(aPayloadBytesWritten),
                                 aSectionHeader);
  if (pSectionType == SectionTypeV2::kArchiveData) {
    if (pPrecomputedSkipRecord == nullptr) {
      pOutFailureMessage =
          "precomputed skip record was unavailable for archive-data block.";
      return false;
    }
    if (pPrecomputedSkipRecord->mHasValidSkip) {
      aSectionHeader.mSkipRecord = pPrecomputedSkipRecord->mSkipRecord;
    }
  }
  aSectionHeader.mRepairRecord = MakeInvalidRepairRecord(pContext);
  aSectionHeader.mCheckSum =
      ComputeSectionCheckSum(aPayload, aSectionPayloadBytes, aSectionHeader);

  if (!WriteSectionHeader(aSectionHeader,
                          pPlainBlock.Data(),
                          kSectionHeaderBytesV2,
                          nullptr)) {
    pOutFailureMessage = "failed writing section header.";
    return false;
  }
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleBlockHeaderWritten)) {
    EmitBundleBlockHeaderEvent(pContext,
                               pArchive,
                               pFamilyBlockIndex,
                               pLocalBlockIndex,
                               pSectionType,
                               static_cast<std::uint32_t>(aPayloadBytesWritten),
                               pFileReference);
  }

  if (pEncryptBlock) {
    if (!pContext.State().mCipher.mAssembled) {
      pOutFailureMessage = "bundle archive packing expected an assembled cipher.";
      return false;
    }
    std::string aSealError;
    if (pContext.State().mCipher.mWorkerBuffer.Size() < aArchiveBlockBytes) {
      pOutFailureMessage = "cipher worker buffer is too small for archive block encryption.";
      return false;
    }
    if (!pContext.State().mCipher.mCipher.Seal(pPlainBlock.Data(),
                                               pContext.State().mCipher.mWorkerBuffer.Data(),
                                               pPlainBlock.Data(),
                                               aArchiveBlockBytes,
                                               &aSealError)) {
      pOutFailureMessage = "failed sealing section block: " + aSealError;
      return false;
    }
    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleEncryptionFinished)) {
      EmitBundleEncryptionEvent(
          pContext, pArchive, pFamilyBlockIndex, pLocalBlockIndex);
    }
  }

  if (!pWriteStream.Write(pPlainBlock.Data(), aArchiveBlockBytes)) {
    pOutFailureMessage =
        "failed writing archive block: " + pWriteStream.LastErrorMessage();
    return false;
  }

  (void)pFamilyBlockIndex;
  pOutPausedAtBoundary = aPausedAtBoundary;
  return true;
}

bool WritePreviewManifestBlock(BundleStageContextV2& pContext,
                               const PlannedArchiveFileV2& pArchive,
                               std::uint64_t pFamilyBlockIndex,
                               std::uint32_t pLocalBlockIndex,
                               const std::string& pFileReference,
                               BundleLogicalRecordEncoderV2& pEncoder,
                               FixedBlockBufferV2& pPlainBlock,
                               FileWriteStreamV2& pWriteStream,
                               std::string& pOutFailureMessage) {
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (pPlainBlock.Empty()) {
    pOutFailureMessage = "failed allocating preview block buffer.";
    return false;
  }

  unsigned char* aPayloadBytes = pPlainBlock.Data() + kSectionHeaderBytesV2;
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
      static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest);
  PopulateSectionBootstrapFields(pContext,
                                 pArchive,
                                 pLocalBlockIndex,
                                 static_cast<std::uint32_t>(aChunkBytes),
                                 aSectionHeader);
  aSectionHeader.mRepairRecord = MakeInvalidRepairRecord(pContext);
  aSectionHeader.mCheckSum =
      ComputeSectionCheckSum(aPayloadBytes, aSectionPayloadBytes, aSectionHeader);

  if (!WriteSectionHeader(aSectionHeader,
                          pPlainBlock.Data(),
                          kSectionHeaderBytesV2,
                          nullptr)) {
    pOutFailureMessage = "failed writing preview section header.";
    return false;
  }
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleBlockHeaderWritten)) {
    EmitBundleBlockHeaderEvent(pContext,
                               pArchive,
                               pFamilyBlockIndex,
                               pLocalBlockIndex,
                               SectionTypeV2::kPreviewManifest,
                               static_cast<std::uint32_t>(aChunkBytes),
                               pFileReference);
  }

  if (!pWriteStream.Write(pPlainBlock.Data(), aArchiveBlockBytes)) {
    pOutFailureMessage =
        "failed writing preview manifest block: " +
        pWriteStream.LastErrorMessage();
    return false;
  }

  (void)pFamilyBlockIndex;
  return true;
}

LoggingStatV2 BuildArchivePackingStat(const BundleStageContextV2& pContext,
                                      const BundleLogicalRecordEncoderV2& pDataEncoder,
                                      std::uint64_t pArchivesCompleted,
                                      std::uint64_t pBytesPacked) {
  LoggingStatV2 aStat;
  aStat.mArchivesCompleted = pArchivesCompleted;
  aStat.mArchivesTotal = pContext.State().mMemoryPlan.mArchiveCount;
  aStat.mFilesCompleted = static_cast<std::uint64_t>(pDataEncoder.PackedFileCount());
  aStat.mFilesTotal = pContext.State().mDiscovery.mFileCount;
  aStat.mFoldersCompleted = static_cast<std::uint64_t>(pDataEncoder.PackedFolderCount());
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
                                const std::string& pFileReferenceAtBlockStart) {
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
          "[Bundle][Archive Packing] Cancel requested while writing '" +
              aCancel.mCancelFileReference +
              "'; stopping once this file finishes and finalizing headers as canceled.");
    }
  }

  aCancel.mShouldFinalizeAfterCancel = true;
  return true;
}

}  // namespace

class BundleArchivePackingCursorV2 {
 public:
  static std::vector<BundleRecordEntryV2> BuildDataRecords(
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

  static std::vector<BundleRecordEntryV2> BuildPreviewRecords(
      const BundleStageContextV2& pContext) {
    std::vector<BundleRecordEntryV2> aPreviewRecords =
        pContext.State().mDiscovery.mEmptyFolderRecords;
    aPreviewRecords.insert(aPreviewRecords.end(),
                           pContext.State().mDiscovery.mFileRecords.begin(),
                           pContext.State().mDiscovery.mFileRecords.end());
    return aPreviewRecords;
  }

  explicit BundleArchivePackingCursorV2(BundleStageContextV2& pContext)
      : mDataRecords(BuildDataRecords(pContext)),
        mPreviewRecords(BuildPreviewRecords(pContext)),
        mDataEncoder(
            mDataRecords,
            pContext.FileSystem(),
            TypedRecordTypeV2::kDataFile,
            TypedRecordTypeV2::kDataFolder,
            TypedRecordTypeV2::kDataReference,
            ProgressStageV2::kArchivePacking,
            RuntimeEventKindV2::kBundleFileStarted,
            RuntimeEventKindV2::kBundleFileFinished,
            false,
            [&pContext](const RuntimeEventV2& pEvent) {
              return pContext.EmitRuntimeEvent(pEvent);
            }),
        mPreviewEncoder(
            mPreviewRecords,
            pContext.FileSystem(),
            TypedRecordTypeV2::kManifestFile,
            TypedRecordTypeV2::kManifestFolder,
            TypedRecordTypeV2::kDataReference,
            ProgressStageV2::kArchivePacking,
            RuntimeEventKindV2::kBundleManifestItemStarted,
            RuntimeEventKindV2::kBundleManifestItemFinished,
            true,
            [&pContext](const RuntimeEventV2& pEvent) {
              return pContext.EmitRuntimeEvent(pEvent);
            }),
        mPlainBlock(pContext.Layout().mArchiveBlockBytes) {
    mNextArchiveLog = kArchiveProgressArchiveLogIntervalV2;
    mNextFileLog = kArchiveProgressFileLogIntervalV2;
    mNextByteLog = kArchiveProgressByteLogIntervalV2;
    mDataSkipRecordsReady = BuildPrecomputedDataSkipRecords(
        pContext, mDataRecords, mDataSkipRecords, mDataSkipPlanError);
  }

  bool RebuildDataSkipPlan(BundleStageContextV2& pContext,
                           std::string& pOutFailureMessage) {
    mDataSkipPlanError.clear();
    mDataSkipRecordsReady = BuildPrecomputedDataSkipRecords(
        pContext, mDataRecords, mDataSkipRecords, mDataSkipPlanError);
    if (!mDataSkipRecordsReady) {
      pOutFailureMessage = mDataSkipPlanError.empty()
                               ? "failed rebuilding precomputed skip records."
                               : mDataSkipPlanError;
      return false;
    }
    pOutFailureMessage.clear();
    return true;
  }

  bool EnsureDataSkipPlan(BundleStageContextV2& pContext,
                          std::string& pOutFailureMessage) {
    const std::uint64_t aExpectedDataBlocks =
        pContext.State().mMemoryPlan.mBlockCountMain;
    const bool aSizeMismatch =
        aExpectedDataBlocks !=
        static_cast<std::uint64_t>(mDataSkipRecords.size());
    if (!mDataSkipRecordsReady || aSizeMismatch) {
      return RebuildDataSkipPlan(pContext, pOutFailureMessage);
    }
    pOutFailureMessage.clear();
    return true;
  }

  std::uint64_t ExpectedSourceBlocksForArchive(const BundleMemoryPlanV2& pPlan,
                                               std::size_t pArchiveIndex) const {
    if (pArchiveIndex < pPlan.mSourceArchiveBlockCounts.size()) {
      return static_cast<std::uint64_t>(
          pPlan.mSourceArchiveBlockCounts[pArchiveIndex]);
    }
    if (pArchiveIndex >= pPlan.mArchives.size()) {
      return 0u;
    }
    const PlannedArchiveFileV2& aArchive = pPlan.mArchives[pArchiveIndex];
    const std::uint64_t aStart = aArchive.mFamilyBlockStart;
    const std::uint64_t aEnd = std::min<std::uint64_t>(
        aArchive.mFamilyBlockStart + static_cast<std::uint64_t>(aArchive.mBlockCount),
        pPlan.mNonRepairFamilyBlockCount);
    return aEnd > aStart ? (aEnd - aStart) : 0u;
  }

  std::uint64_t ExpectedArchiveBytesForPackedArchive(
      const BundleStageContextV2& pContext,
      std::size_t pArchiveIndex) const {
    const BundleMemoryPlanV2& aPlan = pContext.State().mMemoryPlan;
    const std::uint64_t aSourceBlocks = ExpectedSourceBlocksForArchive(
        aPlan, pArchiveIndex);
    return static_cast<std::uint64_t>(kArchiveHeaderBytesV2) +
           (aSourceBlocks *
            static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes));
  }

  bool TryGetArchiveFileLength(BundleStageContextV2& pContext,
                               const std::string& pPath,
                               std::uint64_t& pOutLength) const {
    pOutLength = 0u;
    std::unique_ptr<FileReadStreamV2> aRead =
        pContext.FileSystem().OpenReadStream(pPath);
    if (aRead == nullptr || !aRead->IsReady()) {
      return false;
    }
    pOutLength = static_cast<std::uint64_t>(aRead->GetLength());
    return true;
  }

  bool ResetEncodersToGlobalBlockIndex(BundleStageContextV2& pContext,
                                       std::uint64_t pGlobalBlockIndex,
                                       std::string& pOutFailureMessage) {
    pOutFailureMessage.clear();
    const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
    if (aSectionPayloadBytes == 0u) {
      pOutFailureMessage = "section payload bytes must be at least 1 when replaying cursor state.";
      return false;
    }

    ByteBufferV2 aScratch(aSectionPayloadBytes);
    if (aScratch.Empty()) {
      pOutFailureMessage = "failed allocating replay buffer for archive packing reconciliation.";
      return false;
    }

    mDataEncoder.Reset();
    mPreviewEncoder.Reset();
    mFileBytesPacked = 0u;

    const std::uint64_t aPreviewBlockCount =
        pContext.State().mMemoryPlan.mBlockCountPreview;
    const std::uint64_t aNonRepairBlockCount =
        pContext.State().mMemoryPlan.mNonRepairFamilyBlockCount;
    const std::uint64_t aReplayBlocks =
        std::min<std::uint64_t>(pGlobalBlockIndex, aNonRepairBlockCount);
    for (std::uint64_t aGlobalBlock = 0u; aGlobalBlock < aReplayBlocks; ++aGlobalBlock) {
      BundleLogicalRecordEncoderV2& aEncoder =
          aGlobalBlock < aPreviewBlockCount ? mPreviewEncoder : mDataEncoder;
      std::size_t aPayloadBytesWritten = 0u;
      std::uint64_t aLogicalBytesWritten = 0u;
      std::uint64_t aFileBytesWritten = 0u;
      bool aPausedAtBoundary = false;
      if (!aEncoder.Fill(aScratch.Data(),
                         aSectionPayloadBytes,
                         false,
                         aPayloadBytesWritten,
                         aLogicalBytesWritten,
                         aFileBytesWritten,
                         aPausedAtBoundary,
                         pOutFailureMessage,
                         true)) {
        if (pOutFailureMessage.empty()) {
          pOutFailureMessage =
              "failed replaying record encoders while rebuilding archive packing cursor.";
        }
        return false;
      }
      (void)aPayloadBytesWritten;
      (void)aLogicalBytesWritten;
      (void)aPausedAtBoundary;
      mFileBytesPacked += aFileBytesWritten;
    }
    return true;
  }

  bool ReconcilePackedAndPendingBoxes(BundleStageContextV2& pContext,
                                      BundlePackingStateV2& pPacking,
                                      const std::string& pReason,
                                      std::string& pOutFailureMessage) {
    pOutFailureMessage.clear();
    const BundleMemoryPlanV2& aPlan = pContext.State().mMemoryPlan;
    const std::size_t aArchiveCount = aPlan.mArchives.size();

    std::size_t aPackedCount = std::min<std::size_t>(pPacking.mArchivePaths.size(), aArchiveCount);
    for (std::size_t aArchiveIndex = 0u; aArchiveIndex < aPackedCount; ++aArchiveIndex) {
      const PlannedArchiveFileV2& aArchive = aPlan.mArchives[aArchiveIndex];
      const std::string& aObservedPath = pPacking.mArchivePaths[aArchiveIndex];
      if (aObservedPath != aArchive.mPath) {
        aPackedCount = aArchiveIndex;
        break;
      }
      std::uint64_t aObservedBytes = 0u;
      if (!TryGetArchiveFileLength(pContext, aArchive.mPath, aObservedBytes)) {
        aPackedCount = aArchiveIndex;
        break;
      }
      const std::uint64_t aExpectedBytes =
          ExpectedArchiveBytesForPackedArchive(pContext, aArchiveIndex);
      if (aObservedBytes != aExpectedBytes) {
        aPackedCount = aArchiveIndex;
        break;
      }
    }

    const std::uint64_t aExpectedGlobalAtPackedBoundaryRaw =
        aPackedCount >= aArchiveCount
            ? aPlan.mNonRepairFamilyBlockCount
            : aPlan.mArchives[aPackedCount].mFamilyBlockStart;
    const std::uint64_t aExpectedGlobalAtPackedBoundary =
        std::min<std::uint64_t>(aExpectedGlobalAtPackedBoundaryRaw,
                                aPlan.mNonRepairFamilyBlockCount);
    const std::size_t aPendingCount =
        aArchiveCount > aPackedCount ? (aArchiveCount - aPackedCount) : 0u;

    const bool aNeedsCursorReset =
        (mArchiveIndex != aPackedCount) ||
        (mLocalBlockIndex != 0u) ||
        (mArchiveBlocksWritten != 0u) ||
        (mGlobalBlockIndex != aExpectedGlobalAtPackedBoundary) ||
        (mWrite != nullptr) ||
        (pPacking.mArchivePaths.size() != aPackedCount) ||
        (pPacking.mArchivePackedBlockCount != aExpectedGlobalAtPackedBoundary);
    const bool aNeedsSkipRebuild =
        !mDataSkipRecordsReady ||
        (mDataSkipRecords.size() !=
         static_cast<std::size_t>(aPlan.mBlockCountMain));

    if (!aNeedsCursorReset && !aNeedsSkipRebuild) {
      pOutFailureMessage =
          "archive packing reconciliation found no state delta to apply.";
      return false;
    }

    if (mWrite != nullptr) {
      if (!mWrite->Close()) {
        pOutFailureMessage = "failed closing active archive during reconciliation.";
        return false;
      }
      mWrite.reset();
    }

    pPacking.mArchivePaths.resize(aPackedCount);
    pPacking.mArchivePackedBlockCount = aExpectedGlobalAtPackedBoundary;
    mArchiveIndex = aPackedCount;
    mLocalBlockIndex = 0u;
    mArchiveBlocksWritten = 0u;
    mGlobalBlockIndex = aExpectedGlobalAtPackedBoundary;
    mCanStopAtSafeBoundary = true;
    mSafeBoundaryFileReference.clear();

    if (!ResetEncodersToGlobalBlockIndex(
            pContext, aExpectedGlobalAtPackedBoundary, pOutFailureMessage)) {
      return false;
    }
    if (!RebuildDataSkipPlan(pContext, pOutFailureMessage)) {
      return false;
    }

    pContext.EmitLog(
        LogLevelV2::kWarning,
        "[Bundle][Archive Packing] Reconciled archive tables after '" + pReason +
            "': packed=" + std::to_string(aPackedCount) +
            ", still_to_pack=" + std::to_string(aPendingCount) + ".");
    return true;
  }

  std::vector<BundleRecordEntryV2> mDataRecords;
  std::vector<BundleRecordEntryV2> mPreviewRecords;
  std::vector<PrecomputedSkipRecordEntryV2> mDataSkipRecords;
  BundleLogicalRecordEncoderV2 mDataEncoder;
  BundleLogicalRecordEncoderV2 mPreviewEncoder;
  FixedBlockBufferV2 mPlainBlock;
  std::unique_ptr<FileWriteStreamV2> mWrite;
  std::size_t mArchiveIndex = 0u;
  std::uint32_t mLocalBlockIndex = 0u;
  std::uint64_t mArchiveBlocksWritten = 0u;
  std::uint64_t mGlobalBlockIndex = 0u;
  std::uint64_t mFileBytesPacked = 0u;
  std::uint64_t mNextArchiveLog = 0u;
  std::uint64_t mNextFileLog = 0u;
  std::uint64_t mNextByteLog = 0u;
  bool mCanStopAtSafeBoundary = false;
  std::string mSafeBoundaryFileReference;
  bool mDataSkipRecordsReady = false;
  std::string mDataSkipPlanError;
};

namespace {

bool CloseArchiveForCursor(BundleStageContextV2& pContext,
                           BundleArchivePackingCursorV2& pCursor,
                           const PlannedArchiveFileV2& pArchive,
                           BundlePackingStateV2& pPacking) {
  if (pCursor.mWrite == nullptr) {
    return false;
  }

  if (!pCursor.mWrite->Close()) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive packing failed while closing destination archive.");
    return false;
  }
  pCursor.mWrite.reset();
  pPacking.mArchivePaths.push_back(pArchive.mPath);
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleArchiveFinished)) {
    EmitBundleArchiveFinishedEvent(pContext, pArchive, pCursor.mArchiveBlocksWritten);
  }

  const LoggingStatV2 aArchiveClosedStat = BuildArchivePackingStat(
      pContext,
      pCursor.mDataEncoder,
      static_cast<std::uint64_t>(pPacking.mArchivePaths.size()),
      pCursor.mFileBytesPacked);
  if (ShouldEmitArchivePackingSlice(aArchiveClosedStat,
                                    pCursor.mNextArchiveLog,
                                    pCursor.mNextFileLog,
                                    pCursor.mNextByteLog)) {
    pContext.EmitLog(
        LogLevelV2::kInfo,
        "[Bundle][Archive Packing] " +
            BuildStatSummaryV2(aArchiveClosedStat) + ".");
  }

  ++pCursor.mArchiveIndex;
  pCursor.mLocalBlockIndex = 0u;
  pCursor.mArchiveBlocksWritten = 0u;
  pCursor.mCanStopAtSafeBoundary = true;
  pCursor.mSafeBoundaryFileReference.clear();
  return true;
}

bool MaterializeRemainingArchives(BundleStageContextV2& pContext,
                                  BundleArchivePackingCursorV2& pCursor,
                                  BundlePackingStateV2& pPacking) {
  const std::vector<PlannedArchiveFileV2>& aArchives =
      pContext.State().mMemoryPlan.mArchives;
  while (pCursor.mArchiveIndex < aArchives.size()) {
    const PlannedArchiveFileV2& aArchive = aArchives[pCursor.mArchiveIndex];
    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleArchiveStarted)) {
      EmitBundleArchiveEvent(pContext, aArchive);
    }

    std::unique_ptr<FileWriteStreamV2> aWrite =
        pContext.FileSystem().OpenWriteStream(aArchive.mPath);
    if (aWrite == nullptr || !aWrite->IsReady()) {
      const std::string aArchiveParent =
          pContext.FileSystem().ParentPath(aArchive.mPath);
      if (!aArchiveParent.empty()) {
        (void)pContext.FileSystem().EnsureDirectory(aArchiveParent);
        aWrite = pContext.FileSystem().OpenWriteStream(aArchive.mPath);
      }
    }
    if (aWrite == nullptr || !aWrite->IsReady()) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive packing failed: could not materialize remaining archive file.");
      return false;
    }
    if (!WriteArchiveHeaderPrefix(pContext, aArchive, *aWrite)) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive packing failed while writing archive header prefix.");
      return false;
    }
    if (!aWrite->Close()) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive packing failed while closing destination archive.");
      return false;
    }

    pPacking.mArchivePaths.push_back(aArchive.mPath);
    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleArchiveFinished)) {
      EmitBundleArchiveFinishedEvent(pContext, aArchive, 0u);
    }

    ++pCursor.mArchiveIndex;
    pCursor.mLocalBlockIndex = 0u;
    pCursor.mArchiveBlocksWritten = 0u;
  }

  return true;
}

bool ValidateAllRecordsWerePacked(BundleStageContextV2& pContext,
                                  const BundleArchivePackingCursorV2& pCursor) {
  if (pContext.IsCancelRequested()) {
    return true;
  }

  const bool aDataDone = pCursor.mDataEncoder.IsDone();
  const bool aPreviewDone =
      !pContext.Request().mIncludePreviewManifest ||
      pCursor.mPreviewEncoder.IsDone();
  if (aDataDone && aPreviewDone) {
    return true;
  }

  std::string aMessage =
      "Archive packing failed: record stream was not fully encoded before archive block budget was exhausted.";
  aMessage += " packed_files=" +
              std::to_string(pCursor.mDataEncoder.PackedFileCount()) +
              "/" + std::to_string(pContext.State().mDiscovery.mFileCount);
  aMessage += ", packed_folders=" +
              std::to_string(pCursor.mDataEncoder.PackedFolderCount()) +
              "/" + std::to_string(pContext.State().mDiscovery.mEmptyFolderCount);
  aMessage += ", packed_data_records=" +
              std::to_string(pCursor.mDataEncoder.PackedItemCount()) +
              "/" + std::to_string(pCursor.mDataRecords.size());
  if (pContext.Request().mIncludePreviewManifest) {
    aMessage += ", packed_preview_records=" +
                std::to_string(pCursor.mPreviewEncoder.PackedItemCount()) +
                "/" + std::to_string(pCursor.mPreviewRecords.size());
  }
  pContext.EmitLog(LogLevelV2::kError, aMessage);
  return false;
}

bool FinalizeArchivePacking(BundleStageContextV2& pContext,
                            std::shared_ptr<BundleArchivePackingCursorV2>& pCursorPtr) {
  BundleArchivePackingCursorV2& aCursor = *pCursorPtr;
  BundlePackingStateV2& aPacking = pContext.State().mPacking;
  if (!pContext.IsCancelRequested() &&
      aCursor.mArchiveIndex < pContext.State().mMemoryPlan.mArchives.size()) {
    if (!MaterializeRemainingArchives(pContext, aCursor, aPacking)) {
      pCursorPtr.reset();
      return false;
    }
  }

  if (pContext.IsCancelRequested()) {
    pContext.State().mCancel.mObserved = true;
    pContext.State().mCancel.mShouldFinalizeAfterCancel = true;
    pContext.EmitLog(
        LogLevelV2::kWarning,
        "[Bundle][Archive Packing] Cancel requested after archive packing completed; finalizing headers as canceled.");
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseCompletedV2(LogActionV2::kBundle,
                                         ProgressStageV2::kArchivePacking));
    pCursorPtr.reset();
    return true;
  }

  if (!ValidateAllRecordsWerePacked(pContext, aCursor)) {
    pCursorPtr.reset();
    return false;
  }

  const LoggingStatV2 aFinalStat = BuildArchivePackingStat(
      pContext,
      aCursor.mDataEncoder,
      static_cast<std::uint64_t>(pContext.State().mPacking.mArchivePaths.size()),
      aCursor.mFileBytesPacked);
  pContext.EmitLog(
      LogLevelV2::kInfo,
      "[Bundle][Archive Packing] END. " +
          BuildStatSummaryV2(aFinalStat) + ".");
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kArchivePacking));
  pContext.EmitPhaseProgress(1.0, "Archive packing complete");
  pCursorPtr.reset();
  return true;
}

}  // namespace

bool BundleArchivePackingV2::Run(BundleStageContextV2& pContext) {
  BundlePackingStateV2& aPacking = pContext.State().mPacking;
  const BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;
  const std::uint64_t aNonRepairBlockCount = aMemoryPlan.mNonRepairFamilyBlockCount;
  std::shared_ptr<BundleArchivePackingCursorV2>& aCursorPtr =
      pContext.State().mCursor.mArchivePacking;

  if (!aCursorPtr) {
    aPacking.mArchivePaths.clear();
    aPacking.mArchivePackedBlockCount = 0u;
    pContext.State().mCancel = BundleCancelStateV2{};
    aCursorPtr = std::make_shared<BundleArchivePackingCursorV2>(pContext);
    if (aCursorPtr->mPlainBlock.Empty()) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive packing failed: block buffers could not be allocated.");
      aCursorPtr.reset();
      return false;
    }
    std::string aSkipPlanError;
    if (!aCursorPtr->EnsureDataSkipPlan(pContext, aSkipPlanError)) {
      std::string aReconcileError;
      if (!aCursorPtr->ReconcilePackedAndPendingBoxes(
              pContext,
              aPacking,
              "initial skip-table build",
              aReconcileError)) {
        const std::string aError = aSkipPlanError.empty() ? aReconcileError
                                                          : aSkipPlanError;
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive packing failed: " + aError);
        aCursorPtr.reset();
        return false;
      }
    }
    pContext.EmitLog(
        LogLevelV2::kInfo,
        "[Bundle][Archive Packing] START. " +
            BuildStatSummaryV2(BuildArchivePackingStat(
                pContext, aCursorPtr->mDataEncoder, 0u, 0u)) +
            ".");
  }

  BundleArchivePackingCursorV2& aCursor = *aCursorPtr;
  std::string aSkipPlanError;
  if (!aCursor.EnsureDataSkipPlan(pContext, aSkipPlanError)) {
    std::string aReconcileError;
    if (aCursor.ReconcilePackedAndPendingBoxes(pContext,
                                               aPacking,
                                               "skip-table validation",
                                               aReconcileError)) {
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }
    pContext.EmitLog(
        LogLevelV2::kError,
        "Archive packing failed: " +
            (aSkipPlanError.empty() ? aReconcileError : aSkipPlanError));
    aCursorPtr.reset();
    return false;
  }
  std::size_t aRemainingBlockBudget =
      std::max<std::size_t>(
          1u,
          static_cast<std::size_t>(knobs::kBatchSizeBundleV2));
  while (aCursor.mArchiveIndex < aMemoryPlan.mArchives.size()) {
    if (aRemainingBlockBudget == 0u) {
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    const PlannedArchiveFileV2& aArchive = aMemoryPlan.mArchives[aCursor.mArchiveIndex];

    if (pContext.IsCancelRequested() && aCursor.mCanStopAtSafeBoundary) {
      BundleCancelStateV2& aCancel = pContext.State().mCancel;
      if (!aCancel.mObserved) {
        aCancel = BundleCancelStateV2{};
        aCancel.mObserved = true;
        aCancel.mShouldFinalizeAfterCancel = true;
        aCancel.mCancelFileReference = aCursor.mSafeBoundaryFileReference;
        if (aCancel.mCancelFileReference.empty()) {
          pContext.EmitLog(
              LogLevelV2::kWarning,
              "[Bundle][Archive Packing] Cancel requested at a checkpoint boundary; finalizing headers as canceled.");
        } else {
          pContext.EmitLog(
              LogLevelV2::kWarning,
              "[Bundle][Archive Packing] Cancel requested after finishing file '" +
                  aCancel.mCancelFileReference +
                  "'; finalizing headers as canceled before starting another file.");
        }
      }

      if (aCursor.mWrite != nullptr &&
          !CloseArchiveForCursor(pContext, aCursor, aArchive, aPacking)) {
        aCursorPtr.reset();
        return false;
      }

      aCursorPtr.reset();
      return true;
    }

    if (aCursor.mWrite == nullptr) {
      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleArchiveStarted)) {
        EmitBundleArchiveEvent(pContext, aArchive);
      }
      aCursor.mWrite = pContext.FileSystem().OpenWriteStream(aArchive.mPath);
      if (aCursor.mWrite == nullptr || !aCursor.mWrite->IsReady()) {
        const std::string aArchiveParent =
            pContext.FileSystem().ParentPath(aArchive.mPath);
        if (!aArchiveParent.empty()) {
          (void)pContext.FileSystem().EnsureDirectory(aArchiveParent);
          aCursor.mWrite = pContext.FileSystem().OpenWriteStream(aArchive.mPath);
        }
      }
      if (aCursor.mWrite == nullptr || !aCursor.mWrite->IsReady()) {
        std::string aReconcileError;
        if (aCursor.ReconcilePackedAndPendingBoxes(
                pContext,
                aPacking,
                "destination archive open failure",
                aReconcileError)) {
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive packing failed: could not open destination archive file.");
        aCursorPtr.reset();
        return false;
      }
      if (!WriteArchiveHeaderPrefix(pContext, aArchive, *aCursor.mWrite)) {
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive packing failed while writing archive header prefix.");
        aCursorPtr.reset();
        return false;
      }
      aCursor.mArchiveBlocksWritten = 0u;
    }

    if (aCursor.mLocalBlockIndex >= aArchive.mBlockCount ||
        aCursor.mGlobalBlockIndex >= aNonRepairBlockCount) {
      if (!CloseArchiveForCursor(pContext, aCursor, aArchive, aPacking)) {
        aCursorPtr.reset();
        return false;
      }
      break;
    }

    static const std::string kEmptyFileReference;
    const std::string& aCurrentFileReference = aCursor.mDataEncoder.CurrentFileReference();
    const std::string& aCurrentPreviewReference =
        aCursor.mPreviewEncoder.CurrentFileReference();
    std::string aFailureMessage;
    std::uint64_t aBlockFileBytes = 0u;
    const bool aIsPreviewBlock =
        aCursor.mGlobalBlockIndex < aMemoryPlan.mBlockCountPreview;
    const SectionTypeV2 aSectionType = aIsPreviewBlock
                                           ? SectionTypeV2::kPreviewManifest
                                           : SectionTypeV2::kArchiveData;
    const std::string& aObservedFileReference = aIsPreviewBlock
                                                    ? kEmptyFileReference
                                                    : aCurrentFileReference;
    const std::string& aPreviewFileReference =
        aIsPreviewBlock ? aCurrentPreviewReference : kEmptyFileReference;
    const std::string& aBlockStartFileReference = aObservedFileReference;
    aCursor.mCanStopAtSafeBoundary = false;
    aCursor.mSafeBoundaryFileReference.clear();
    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleBlockStarted)) {
      EmitBundleBlockEvent(pContext,
                           RuntimeEventKindV2::kBundleBlockStarted,
                           aArchive,
                           aCursor.mGlobalBlockIndex,
                           aCursor.mLocalBlockIndex,
                           aSectionType,
                           aObservedFileReference);
    }

    if (aIsPreviewBlock) {
      if (!WritePreviewManifestBlock(pContext,
                                     aArchive,
                                     aCursor.mGlobalBlockIndex,
                                     aCursor.mLocalBlockIndex,
                                     aPreviewFileReference,
                                     aCursor.mPreviewEncoder,
                                     aCursor.mPlainBlock,
                                     *aCursor.mWrite,
                                     aFailureMessage)) {
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive packing failed: " + aFailureMessage);
        aCursorPtr.reset();
        return false;
      }
    } else {
      BundleLogicalRecordEncoderV2& aEncoder = aCursor.mDataEncoder;
      if (!aCursor.EnsureDataSkipPlan(pContext, aSkipPlanError)) {
        std::string aReconcileError;
        if (aCursor.ReconcilePackedAndPendingBoxes(
                pContext,
                aPacking,
                "skip-table access failure",
                aReconcileError)) {
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
        pContext.EmitLog(
            LogLevelV2::kError,
            "Archive packing failed: " +
                (aSkipPlanError.empty() ? aReconcileError : aSkipPlanError));
        aCursorPtr.reset();
        return false;
      }
      const std::uint64_t aDataBlockIndex =
          aCursor.mGlobalBlockIndex - aMemoryPlan.mBlockCountPreview;
      if (aDataBlockIndex >=
          static_cast<std::uint64_t>(aCursor.mDataSkipRecords.size())) {
        std::string aReconcileError;
        if (aCursor.ReconcilePackedAndPendingBoxes(
                pContext,
                aPacking,
                "precomputed skip index moved out of range",
                aReconcileError)) {
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive packing failed: precomputed skip index moved out of range.");
        aCursorPtr.reset();
        return false;
      }
      const PrecomputedSkipRecordEntryV2* aPrecomputedSkipRecord =
          &aCursor.mDataSkipRecords[static_cast<std::size_t>(aDataBlockIndex)];
      const bool aPauseAfterCurrentFileBoundary =
          pContext.IsCancelRequested() &&
          !aCurrentFileReference.empty() &&
          !aIsPreviewBlock;
      bool aPausedAtBoundary = false;
      if (!WriteSectionBlock(pContext,
                             aArchive,
                             aCursor.mGlobalBlockIndex,
                             aCursor.mLocalBlockIndex,
                             aSectionType,
                             aObservedFileReference,
                             aEncoder,
                             aPrecomputedSkipRecord,
                             aPauseAfterCurrentFileBoundary,
                             pContext.Request().mEncryptionEnabled,
                             aCursor.mPlainBlock,
                             *aCursor.mWrite,
                             aBlockFileBytes,
                             aPausedAtBoundary,
                             aFailureMessage)) {
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive packing failed: " + aFailureMessage);
        aCursorPtr.reset();
        return false;
      }
      aCursor.mCanStopAtSafeBoundary = aPausedAtBoundary;
      aCursor.mSafeBoundaryFileReference =
          aPausedAtBoundary ? aBlockStartFileReference : std::string();
    }
    if (!aFailureMessage.empty()) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive packing failed: " + aFailureMessage);
      aCursorPtr.reset();
      return false;
    }

    aCursor.mFileBytesPacked += aBlockFileBytes;
    ++aCursor.mArchiveBlocksWritten;
    ++aPacking.mArchivePackedBlockCount;
    const LoggingStatV2 aStat = BuildArchivePackingStat(
        pContext,
        aCursor.mDataEncoder,
        static_cast<std::uint64_t>(aPacking.mArchivePaths.size()),
        aCursor.mFileBytesPacked);
    pContext.EmitPhaseProgress(
        aNonRepairBlockCount == 0u
            ? 1.0
            : static_cast<double>(aPacking.mArchivePackedBlockCount) /
                  static_cast<double>(aNonRepairBlockCount),
        BuildArchivePackingProgressLabel(aStat));
    if (ShouldEmitArchivePackingSlice(
            aStat, aCursor.mNextArchiveLog, aCursor.mNextFileLog, aCursor.mNextByteLog)) {
      pContext.EmitLog(
          LogLevelV2::kInfo,
          "[Bundle][Archive Packing] " + BuildStatSummaryV2(aStat) + ".");
    }
    const std::string& aFileReferenceAfterBlock =
        aIsPreviewBlock
            ? kEmptyFileReference
            : aCursor.mDataEncoder.CurrentFileReference();
    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleBlockFinished)) {
      EmitBundleBlockEvent(pContext,
                           RuntimeEventKindV2::kBundleBlockFinished,
                           aArchive,
                           aCursor.mGlobalBlockIndex,
                           aCursor.mLocalBlockIndex,
                           aSectionType,
                           aFileReferenceAfterBlock);
    }
    if (HandleArchivePackingCancel(pContext, aBlockStartFileReference) &&
        aBlockStartFileReference.empty() &&
        aFileReferenceAfterBlock.empty()) {
      aCursor.mCanStopAtSafeBoundary = true;
      aCursor.mSafeBoundaryFileReference.clear();
    }

    ++aCursor.mLocalBlockIndex;
    ++aCursor.mGlobalBlockIndex;

    if (aCursor.mLocalBlockIndex >= aArchive.mBlockCount ||
        aCursor.mGlobalBlockIndex >= aNonRepairBlockCount) {
      if (!CloseArchiveForCursor(pContext, aCursor, aArchive, aPacking)) {
        aCursorPtr.reset();
        return false;
      }
    }

    if (aRemainingBlockBudget > 0u) {
      --aRemainingBlockBudget;
    }
    if (aCursor.mArchiveIndex < aMemoryPlan.mArchives.size() &&
        aRemainingBlockBudget == 0u) {
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }
  }

  return FinalizeArchivePacking(pContext, aCursorPtr);
}

}  // namespace peanutbutter
