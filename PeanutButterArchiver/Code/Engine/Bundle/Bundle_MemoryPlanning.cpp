#include "Bundle_MemoryPlanning.hpp"

#include <algorithm>
#include <optional>

#include "../../Common/LogCatalog.hpp"
#include "../FileAccess/ConflictNamePolicy.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {
namespace {

std::uint64_t CeilingDivide(std::uint64_t pValue,
                            std::uint64_t pDivisor) {
  return pDivisor == 0u ? 0u : ((pValue + pDivisor - 1u) / pDivisor);
}

std::uint64_t GetExpectedRepairBlockCount(
    std::uint64_t pEligibleSourceBlockCount,
    RepairCoveragePresetV2 pCoverage) {
  if (pEligibleSourceBlockCount == 0u) {
    return 0u;
  }
  const std::uint64_t aCoverage =
      static_cast<std::uint64_t>(RepairCoveragePercentV2(pCoverage));
  return ((pEligibleSourceBlockCount * aCoverage) + 99u) / 100u;
}

std::vector<std::uint32_t> BuildArchiveBlockCounts(std::uint64_t pTotalBlockCount,
                                                   std::uint32_t pBlocksPerArchive) {
  std::vector<std::uint32_t> aCounts;
  std::uint64_t aBlocksRemaining = pTotalBlockCount;
  const std::uint64_t aPerArchive = std::max<std::uint32_t>(1u, pBlocksPerArchive);
  while (aBlocksRemaining > 0u) {
    const std::uint32_t aBlockCount = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(aBlocksRemaining, aPerArchive));
    aCounts.push_back(aBlockCount);
    aBlocksRemaining -= static_cast<std::uint64_t>(aBlockCount);
  }
  return aCounts;
}

std::uint64_t HashString(std::uint64_t pState, const std::string& pValue) {
  static constexpr std::uint64_t kPrime = 1099511628211ULL;
  for (unsigned char aByte : pValue) {
    pState ^= static_cast<std::uint64_t>(aByte);
    pState *= kPrime;
  }
  return pState;
}

std::uint64_t HashU64(std::uint64_t pState, std::uint64_t pValue) {
  for (int aByte = 0; aByte < 8; ++aByte) {
    pState ^= static_cast<std::uint64_t>((pValue >> (aByte * 8)) & 0xFFu);
    pState *= 1099511628211ULL;
  }
  return pState;
}

std::uint64_t GetArchiveFamilyID(
    const std::string& pSourceStem,
    const std::string& pFilePrefix,
    std::uint64_t pBlockCount,
    bool pEncryptionEnabled,
    bool pIncludePreviewManifest,
    std::uint8_t pFileCountMod256,
    std::uint8_t pFolderCountMod256,
    const std::vector<BundleRecordEntryV2>& pFileRecords,
    const std::vector<BundleRecordEntryV2>& pFolderRecords) {
  std::uint64_t aHash = 1469598103934665603ULL;
  aHash = HashString(aHash, pSourceStem);
  aHash = HashString(aHash, pFilePrefix);
  aHash = HashU64(aHash, pBlockCount);
  aHash = HashU64(aHash, pEncryptionEnabled ? 1u : 0u);
  aHash = HashU64(aHash, pIncludePreviewManifest ? 1u : 0u);
  aHash = HashU64(aHash, static_cast<std::uint64_t>(pFileCountMod256));
  aHash = HashU64(aHash, static_cast<std::uint64_t>(pFolderCountMod256));
  for (const BundleRecordEntryV2& aFileRecord : pFileRecords) {
    aHash = HashString(aHash, aFileRecord.mRelativePath);
    aHash = HashU64(aHash, aFileRecord.mContentLength);
    aHash = HashU64(aHash, aFileRecord.mIsReference ? 1u : 0u);
    if (aFileRecord.mIsReference) {
      aHash = HashU64(aHash, static_cast<std::uint64_t>(aFileRecord.mReferenceKind));
      aHash = HashString(aHash, aFileRecord.mReferenceTargetRelativePath);
    }
  }
  for (const BundleRecordEntryV2& aFolderRecord : pFolderRecords) {
    aHash = HashString(aHash, aFolderRecord.mRelativePath);
  }
  return aHash == 0u ? 1u : aHash;
}

std::uint64_t RecordLogicalBytes(
    const BundleRecordEntryV2& pRecord,
    memory_layout::TypedRecordTypeV2 pFileType,
    memory_layout::TypedRecordTypeV2 pFolderType,
    bool pIncludePreviewPlaceholderByte,
    std::optional<memory_layout::TypedRecordTypeV2> pReferenceType = std::nullopt) {
  const std::uint8_t aType = static_cast<std::uint8_t>(
      pRecord.mIsReference && pReferenceType.has_value()
          ? *pReferenceType
          : (pRecord.mIsDirectory ? pFolderType : pFileType));
  std::uint64_t aBytes =
      2u + static_cast<std::uint64_t>(pRecord.mRelativePath.size()) + 1u;

  if (pRecord.mIsReference && pReferenceType.has_value()) {
    aBytes += 1u;  // reference kind
    aBytes += 2u + static_cast<std::uint64_t>(pRecord.mReferenceTargetRelativePath.size());
    return aBytes;
  }

  if (pIncludePreviewPlaceholderByte) {
    aBytes += static_cast<std::uint64_t>(
        memory_layout::specs_verified::kPreviewRecordPlaceholderBytesV2);
  }
  if (memory_layout::TypedRecordTypeHasFileSizeV2(aType)) {
    aBytes += 8u;
  }
  if (memory_layout::TypedRecordTypeHasContentBytesV2(aType)) {
    aBytes += pRecord.mContentLength;
  }
  return aBytes;
}

}  // namespace

bool BundleMemoryPlanningV2::Run(BundleStageContextV2& pContext) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionV2::kBundle,
                                     ProgressStageV2::kMemoryPlanning));
  const BundleDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;
  BundleManifestStateV2& aManifest = pContext.State().mManifest;
  const std::uint64_t aSectionPayloadBytes =
      static_cast<std::uint64_t>(pContext.Layout().SectionPayloadBytes());
  aMemoryPlan = BundleMemoryPlanV2{};
  aManifest.mPreviewManifestPayload.clear();
  aManifest.mPreviewManifestBytes = 0u;
  aManifest.mBlockCountPreview = 0u;

  for (const BundleRecordEntryV2& aFileRecord : aDiscovery.mFileRecords) {
    aMemoryPlan.mArchiveDataLogicalBytes += RecordLogicalBytes(
        aFileRecord,
        memory_layout::TypedRecordTypeV2::kDataFile,
        memory_layout::TypedRecordTypeV2::kDataFolder,
        false,
        memory_layout::TypedRecordTypeV2::kDataReference);
  }

  for (const BundleRecordEntryV2& aFolderRecord : aDiscovery.mEmptyFolderRecords) {
    aMemoryPlan.mArchiveDataLogicalBytes += RecordLogicalBytes(
        aFolderRecord,
        memory_layout::TypedRecordTypeV2::kDataFile,
        memory_layout::TypedRecordTypeV2::kDataFolder,
        false);
  }

  aMemoryPlan.mBlockCountMain = CeilingDivide(
      aMemoryPlan.mArchiveDataLogicalBytes, aSectionPayloadBytes);
  if (pContext.Request().mIncludePreviewManifest) {
    for (const BundleRecordEntryV2& aFolderRecord : aDiscovery.mEmptyFolderRecords) {
      aManifest.mPreviewManifestBytes += RecordLogicalBytes(
          aFolderRecord,
          memory_layout::TypedRecordTypeV2::kManifestFile,
          memory_layout::TypedRecordTypeV2::kManifestFolder,
          true);
    }
    for (const BundleRecordEntryV2& aFileRecord : aDiscovery.mFileRecords) {
      aManifest.mPreviewManifestBytes += RecordLogicalBytes(
          aFileRecord,
          memory_layout::TypedRecordTypeV2::kManifestFile,
          memory_layout::TypedRecordTypeV2::kManifestFolder,
          true);
    }
    aManifest.mBlockCountPreview = CeilingDivide(
        aManifest.mPreviewManifestBytes, aSectionPayloadBytes);
  }
  aMemoryPlan.mBlockCountPreview = aManifest.mBlockCountPreview;
  aMemoryPlan.mNonRepairFamilyBlockCount =
      aMemoryPlan.mBlockCountPreview +
      aMemoryPlan.mBlockCountMain;

  const std::uint32_t aBlocksPerArchive =
      std::max<std::uint32_t>(1u, pContext.Request().mBlockCount);
  aMemoryPlan.mSourceArchiveBlockCounts = BuildArchiveBlockCounts(
      aMemoryPlan.mNonRepairFamilyBlockCount, aBlocksPerArchive);

  aMemoryPlan.mRepairCopyBlockCounts.clear();
  aMemoryPlan.mRepairCopySourceLocalBlocks.clear();
  aMemoryPlan.mRepairSectorBlockCount = 0u;
  if (pContext.Request().mRepairEnabled) {
    const std::uint64_t aPreviewEnd = aMemoryPlan.mBlockCountPreview;
    std::uint64_t aSourceFamilyBlockCursor = 0u;
    std::vector<std::vector<std::uint32_t>> aSelectedSourceLocalBlocksByArchive;
    aSelectedSourceLocalBlocksByArchive.reserve(
        aMemoryPlan.mSourceArchiveBlockCounts.size());
    std::uint64_t aTotalEligibleSourceBlockCount = 0u;
    for (std::uint32_t aSourceBlockCount : aMemoryPlan.mSourceArchiveBlockCounts) {
      for (std::uint32_t aLocalBlockIndex = 0u;
           aLocalBlockIndex < aSourceBlockCount;
           ++aLocalBlockIndex) {
        const std::uint64_t aFamilyBlockIndex =
            aSourceFamilyBlockCursor + static_cast<std::uint64_t>(aLocalBlockIndex);
        if (aFamilyBlockIndex >= aPreviewEnd) {
          ++aTotalEligibleSourceBlockCount;
        }
      }
      aSelectedSourceLocalBlocksByArchive.push_back({});
      aSourceFamilyBlockCursor += static_cast<std::uint64_t>(aSourceBlockCount);
    }

    const std::uint64_t aTargetRepairBlockCount = GetExpectedRepairBlockCount(
        aTotalEligibleSourceBlockCount, pContext.Request().mRepairCoverage);
    std::uint64_t aRemainingRepairSelections = aTargetRepairBlockCount;
    aSourceFamilyBlockCursor = 0u;
    for (std::size_t aArchiveIndex = 0u;
         aArchiveIndex < aMemoryPlan.mSourceArchiveBlockCounts.size();
         ++aArchiveIndex) {
      const std::uint32_t aSourceBlockCount =
          aMemoryPlan.mSourceArchiveBlockCounts[aArchiveIndex];
      std::vector<std::uint32_t>& aSelected =
          aSelectedSourceLocalBlocksByArchive[aArchiveIndex];
      for (std::uint32_t aLocalBlockIndex = 0u;
           aLocalBlockIndex < aSourceBlockCount &&
           aRemainingRepairSelections > 0u;
           ++aLocalBlockIndex) {
        const std::uint64_t aFamilyBlockIndex =
            aSourceFamilyBlockCursor + static_cast<std::uint64_t>(aLocalBlockIndex);
        if (aFamilyBlockIndex < aPreviewEnd) {
          continue;
        }
        aSelected.push_back(aLocalBlockIndex);
        --aRemainingRepairSelections;
      }
      aSourceFamilyBlockCursor += static_cast<std::uint64_t>(aSourceBlockCount);
      if (aRemainingRepairSelections == 0u) {
        break;
      }
    }

    aMemoryPlan.mRepairCopySourceLocalBlocks.reserve(
        aSelectedSourceLocalBlocksByArchive.size());
    aMemoryPlan.mRepairCopyBlockCounts.reserve(
        aSelectedSourceLocalBlocksByArchive.size());
    for (std::vector<std::uint32_t>& aSelected :
         aSelectedSourceLocalBlocksByArchive) {
      const std::uint32_t aCount =
          static_cast<std::uint32_t>(aSelected.size());
      aMemoryPlan.mRepairCopyBlockCounts.push_back(aCount);
      aMemoryPlan.mRepairCopySourceLocalBlocks.push_back(std::move(aSelected));
      aMemoryPlan.mRepairSectorBlockCount += static_cast<std::uint64_t>(aCount);
    }
  }

  aMemoryPlan.mTotalFamilyBlockCount =
      aMemoryPlan.mNonRepairFamilyBlockCount +
      aMemoryPlan.mRepairSectorBlockCount;
  aMemoryPlan.mArchiveCount =
      CeilingDivide(aMemoryPlan.mTotalFamilyBlockCount,
                    static_cast<std::uint64_t>(aBlocksPerArchive));
  if (aMemoryPlan.mArchiveCount > pContext.Layout().mMaxArchiveCount) {
    pContext.EmitLog(
        LogLevelV2::kError,
        "Memory planning failed: archive count exceeds configured maximum of " +
            std::to_string(pContext.Layout().mMaxArchiveCount) + ".");
    return false;
  }

  aMemoryPlan.mFileCountMod256 =
      static_cast<std::uint8_t>(aDiscovery.mFileCount % 256u);
  aMemoryPlan.mFolderCountMod256 =
      static_cast<std::uint8_t>(aDiscovery.mEmptyFolderCount % 256u);
  aMemoryPlan.mArchiveFamilyId = GetArchiveFamilyID(
      aDiscovery.mSourceStem,
      pContext.Request().mFilePrefix,
      pContext.Request().mBlockCount,
      pContext.Request().mEncryptionEnabled,
      pContext.Request().mIncludePreviewManifest,
      aMemoryPlan.mFileCountMod256,
      aMemoryPlan.mFolderCountMod256,
      aDiscovery.mFileRecords,
      aDiscovery.mEmptyFolderRecords);

  std::uint64_t aFamilyBlockCursor = 0u;
  std::vector<std::string> aReservedArchivePaths;
  aReservedArchivePaths.reserve(static_cast<std::size_t>(aMemoryPlan.mArchiveCount));
  for (std::uint64_t aArchiveIndex = 0u;
       aArchiveIndex < aMemoryPlan.mArchiveCount;
       ++aArchiveIndex) {
    PlannedArchiveFileV2 aArchive;
    aArchive.mArchiveIndex = aArchiveIndex;
    aArchive.mFamilyBlockStart = aFamilyBlockCursor;
    const std::uint64_t aBlocksRemaining =
        aMemoryPlan.mTotalFamilyBlockCount - aFamilyBlockCursor;
    aArchive.mBlockCount = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(aBlocksRemaining,
                                static_cast<std::uint64_t>(aBlocksPerArchive)));
    const std::string aFileName = memory_layout::MakeArchiveFileNameV2(
        pContext.Request().mFilePrefix,
        static_cast<std::size_t>(aArchiveIndex),
        static_cast<std::size_t>(aMemoryPlan.mArchiveCount));
    if (!ResolveNoOverwritePathV2(pContext.FileSystem(),
                                  pContext.Request().mDestinationDirectory,
                                  aFileName,
                                  aArchive.mPath,
                                  &aReservedArchivePaths)) {
      pContext.EmitLog(
          LogLevelV2::kError,
          "Memory planning failed: could not resolve a non-overwrite path for archive " +
              std::to_string(aArchiveIndex) + ".");
      return false;
    }
    aMemoryPlan.mArchives.push_back(std::move(aArchive));
    aReservedArchivePaths.push_back(aMemoryPlan.mArchives.back().mPath);
    aFamilyBlockCursor += aMemoryPlan.mArchives.back().mBlockCount;
  }

  pContext.EmitLog(LogLevelV2::kInfo,
                   LogBundleMemoryPlanSummaryV2(aMemoryPlan.mArchiveCount,
                                                aMemoryPlan.mTotalFamilyBlockCount));
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kMemoryPlanning));
  pContext.EmitPhaseProgress(1.0, "Memory plan complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
