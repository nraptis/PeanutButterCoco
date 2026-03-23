#include "Bundle_MemoryPlanning.hpp"

#include <algorithm>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {
namespace {

std::uint64_t CeilingDivide(std::uint64_t pValue,
                            std::uint64_t pDivisor) {
  return pDivisor == 0u ? 0u : ((pValue + pDivisor - 1u) / pDivisor);
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

std::uint64_t RecordLogicalBytes(
    const BundleRecordEntryV2& pRecord,
    memory_layout::TypedRecordTypeV2 pFileType,
    memory_layout::TypedRecordTypeV2 pFolderType) {
  const std::uint8_t aType = static_cast<std::uint8_t>(
      pRecord.mIsDirectory ? pFolderType : pFileType);
  std::uint64_t aBytes =
      2u + static_cast<std::uint64_t>(pRecord.mRelativePath.size()) + 1u;
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
  const BundleDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;
  BundleManifestStateV2& aManifest = pContext.State().mManifest;
  const std::uint64_t aSectionPayloadBytes =
      static_cast<std::uint64_t>(pContext.Layout().SectionPayloadBytes());
  aMemoryPlan = BundleMemoryPlanV2{};
  aManifest.mPreviewManifestPayload.clear();
  aManifest.mPreviewManifestBytes = 0u;
  aManifest.mPreviewManifestBlockCount = 0u;

  for (const BundleRecordEntryV2& aFileRecord : aDiscovery.mFileRecords) {
    aMemoryPlan.mArchiveDataLogicalBytes += RecordLogicalBytes(
        aFileRecord,
        memory_layout::TypedRecordTypeV2::kDataFile,
        memory_layout::TypedRecordTypeV2::kDataFolder);
  }

  for (const BundleRecordEntryV2& aFolderRecord : aDiscovery.mEmptyFolderRecords) {
    aMemoryPlan.mEmptyFolderLogicalBytes += RecordLogicalBytes(
        aFolderRecord,
        memory_layout::TypedRecordTypeV2::kManifestFile,
        memory_layout::TypedRecordTypeV2::kManifestFolder);
  }

  if (pContext.Request().mSafeModeEnabled) {
    for (const BundleRecordEntryV2& aFileRecord : aDiscovery.mFileRecords) {
      aMemoryPlan.mArchiveDataBlockCount += CeilingDivide(
          RecordLogicalBytes(aFileRecord,
                             memory_layout::TypedRecordTypeV2::kDataFile,
                             memory_layout::TypedRecordTypeV2::kDataFolder),
          aSectionPayloadBytes);
    }
  } else {
    aMemoryPlan.mArchiveDataBlockCount = CeilingDivide(
        aMemoryPlan.mArchiveDataLogicalBytes, aSectionPayloadBytes);
  }
  aMemoryPlan.mEmptyFolderBlockCount = CeilingDivide(
      aMemoryPlan.mEmptyFolderLogicalBytes, aSectionPayloadBytes);
  if (pContext.Request().mIncludePreviewManifest) {
    for (const BundleRecordEntryV2& aFolderRecord : aDiscovery.mEmptyFolderRecords) {
      aManifest.mPreviewManifestBytes += RecordLogicalBytes(
          aFolderRecord,
          memory_layout::TypedRecordTypeV2::kManifestFile,
          memory_layout::TypedRecordTypeV2::kManifestFolder);
    }
    for (const BundleRecordEntryV2& aFileRecord : aDiscovery.mFileRecords) {
      aManifest.mPreviewManifestBytes += RecordLogicalBytes(
          aFileRecord,
          memory_layout::TypedRecordTypeV2::kManifestFile,
          memory_layout::TypedRecordTypeV2::kManifestFolder);
    }
    aManifest.mPreviewManifestBlockCount = CeilingDivide(
        aManifest.mPreviewManifestBytes, aSectionPayloadBytes);
  }
  aMemoryPlan.mPreviewManifestBlockCount = aManifest.mPreviewManifestBlockCount;
  aMemoryPlan.mNonRepairFamilyBlockCount =
      aMemoryPlan.mEmptyFolderBlockCount +
      aMemoryPlan.mPreviewManifestBlockCount +
      aMemoryPlan.mArchiveDataBlockCount;

  const std::uint32_t aBlocksPerArchive =
      std::max<std::uint32_t>(1u, pContext.Request().mBlockCount);
  aMemoryPlan.mSourceArchiveBlockCounts = BuildArchiveBlockCounts(
      aMemoryPlan.mNonRepairFamilyBlockCount, aBlocksPerArchive);

  aMemoryPlan.mRepairCopyBlockCounts.clear();
  aMemoryPlan.mRepairSectorBlockCount = 0u;
  if (pContext.Request().mRepairEnabled) {
    for (std::uint32_t aSourceBlockCount : aMemoryPlan.mSourceArchiveBlockCounts) {
      const std::uint64_t aRepairBlocks = CeilingDivide(
          static_cast<std::uint64_t>(aSourceBlockCount) *
              static_cast<std::uint64_t>(pContext.Request().mRepairPercent),
          100u);
      aMemoryPlan.mRepairCopyBlockCounts.push_back(
          static_cast<std::uint32_t>(aRepairBlocks));
      aMemoryPlan.mRepairSectorBlockCount += aRepairBlocks;
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

  std::uint64_t aHash = 1469598103934665603ULL;
  aHash = HashString(aHash, aDiscovery.mSourceStem);
  aHash = HashString(aHash, pContext.Request().mFilePrefix);
  aHash = HashU64(aHash, pContext.Request().mBlockCount);
  aHash = HashU64(aHash, pContext.Request().mEncryptionEnabled ? 1u : 0u);
  aHash = HashU64(aHash, pContext.Request().mIncludePreviewManifest ? 1u : 0u);
  aHash = HashU64(aHash, pContext.Request().mSafeModeEnabled ? 1u : 0u);
  for (const BundleRecordEntryV2& aFileRecord : aDiscovery.mFileRecords) {
    aHash = HashString(aHash, aFileRecord.mRelativePath);
    aHash = HashU64(aHash, aFileRecord.mContentLength);
  }
  for (const BundleRecordEntryV2& aFolderRecord : aDiscovery.mEmptyFolderRecords) {
    aHash = HashString(aHash, aFolderRecord.mRelativePath);
  }
  aMemoryPlan.mArchiveFamilyId = aHash == 0u ? 1u : aHash;

  std::uint64_t aFamilyBlockCursor = 0u;
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
        aDiscovery.mSourceStem,
        static_cast<std::size_t>(aArchiveIndex),
        static_cast<std::size_t>(aMemoryPlan.mArchiveCount));
    aArchive.mPath = pContext.FileSystem().JoinPath(
        pContext.Request().mDestinationDirectory,
        aFileName);
    aMemoryPlan.mArchives.push_back(std::move(aArchive));
    aFamilyBlockCursor += aMemoryPlan.mArchives.back().mBlockCount;
  }

  pContext.EmitLog(LogLevelV2::kInfo,
                   LogBundleMemoryPlanSummaryV2(aMemoryPlan.mArchiveCount,
                                                aMemoryPlan.mTotalFamilyBlockCount));
  pContext.EmitPhaseProgress(1.0, "Memory plan complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
