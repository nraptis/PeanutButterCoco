#include "Bundle_MemoryPlanning.hpp"

#include <algorithm>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {
namespace {

std::uint64_t RecordLogicalBytes(const BundleRecordEntryV2& pRecord) {
  return 2u + static_cast<std::uint64_t>(pRecord.mRelativePath.size()) + 8u +
         (pRecord.mIsDirectory ? 0u : pRecord.mContentLength);
}

std::uint64_t CeilingDivide(std::uint64_t pValue,
                            std::uint64_t pDivisor) {
  return pDivisor == 0u ? 0u : ((pValue + pDivisor - 1u) / pDivisor);
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

std::string BuildPreviewManifestPayload(const BundleDiscoveryStateV2& pDiscovery) {
  std::string aPayload;
  aPayload.reserve(128u +
                   (pDiscovery.mFileRecords.size() * 48u) +
                   (pDiscovery.mEmptyFolderRecords.size() * 16u));
  aPayload += "PBTR PUBLIC PREVIEW V2\n";
  aPayload += "SOURCE\t";
  aPayload += pDiscovery.mSourceStem;
  aPayload += "\nFILES\t";
  aPayload += std::to_string(pDiscovery.mFileRecords.size());
  aPayload += "\nEMPTY_FOLDERS\t";
  aPayload += std::to_string(pDiscovery.mEmptyFolderRecords.size());
  aPayload += "\n";

  for (const BundleRecordEntryV2& aFolderRecord : pDiscovery.mEmptyFolderRecords) {
    aPayload += "DIR\t";
    aPayload += aFolderRecord.mRelativePath;
    aPayload += "\n";
  }
  for (const BundleRecordEntryV2& aFileRecord : pDiscovery.mFileRecords) {
    aPayload += "FILE\t";
    aPayload += std::to_string(aFileRecord.mContentLength);
    aPayload += "\t";
    aPayload += aFileRecord.mRelativePath;
    aPayload += "\n";
  }
  return aPayload;
}

}  // namespace

bool BundleMemoryPlanningV2::Run(BundleStageContextV2& pContext) {
  const BundleDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;
  BundleManifestStateV2& aManifest = pContext.State().mManifest;
  aMemoryPlan = BundleMemoryPlanV2{};
  aManifest.mPreviewManifestPayload.clear();
  aManifest.mPreviewManifestBytes = 0u;
  aManifest.mPreviewManifestBlockCount = 0u;

  for (const BundleRecordEntryV2& aFileRecord : aDiscovery.mFileRecords) {
    aMemoryPlan.mArchiveDataLogicalBytes += RecordLogicalBytes(aFileRecord);
  }

  for (const BundleRecordEntryV2& aFolderRecord : aDiscovery.mEmptyFolderRecords) {
    aMemoryPlan.mEmptyFolderLogicalBytes += RecordLogicalBytes(aFolderRecord);
  }
  if (!aDiscovery.mEmptyFolderRecords.empty()) {
    aMemoryPlan.mEmptyFolderLogicalBytes += 2u;
  }

  if (pContext.Request().mSafeModeEnabled) {
    if (aDiscovery.mFileRecords.empty()) {
      aMemoryPlan.mArchiveDataBlockCount = 1u;
    } else {
      for (const BundleRecordEntryV2& aFileRecord : aDiscovery.mFileRecords) {
        aMemoryPlan.mArchiveDataBlockCount += CeilingDivide(
            RecordLogicalBytes(aFileRecord),
            static_cast<std::uint64_t>(memory_layout::kSectionPayloadBytesV2));
      }
    }
  } else {
    aMemoryPlan.mArchiveDataLogicalBytes += 2u;
    aMemoryPlan.mArchiveDataBlockCount = CeilingDivide(
        aMemoryPlan.mArchiveDataLogicalBytes,
        static_cast<std::uint64_t>(memory_layout::kSectionPayloadBytesV2));
  }
  aMemoryPlan.mEmptyFolderBlockCount = CeilingDivide(
      aMemoryPlan.mEmptyFolderLogicalBytes,
      static_cast<std::uint64_t>(memory_layout::kSectionPayloadBytesV2));
  if (pContext.Request().mIncludePreviewManifest) {
    aManifest.mPreviewManifestPayload = BuildPreviewManifestPayload(aDiscovery);
    aManifest.mPreviewManifestBytes =
        static_cast<std::uint64_t>(aManifest.mPreviewManifestPayload.size());
    aManifest.mPreviewManifestBlockCount = CeilingDivide(
        aManifest.mPreviewManifestBytes,
        static_cast<std::uint64_t>(memory_layout::kSectionPayloadBytesV2));
  }
  aMemoryPlan.mPreviewManifestBlockCount = aManifest.mPreviewManifestBlockCount;
  aMemoryPlan.mRepairSectorBlockCount = 0u;
  aMemoryPlan.mTotalFamilyBlockCount =
      aMemoryPlan.mEmptyFolderBlockCount +
      aMemoryPlan.mPreviewManifestBlockCount +
      aMemoryPlan.mArchiveDataBlockCount +
      aMemoryPlan.mRepairSectorBlockCount;

  const std::uint32_t aBlocksPerArchive =
      std::max<std::uint32_t>(1u, pContext.Request().mBlockCount);
  aMemoryPlan.mArchiveCount =
      CeilingDivide(aMemoryPlan.mTotalFamilyBlockCount,
                    static_cast<std::uint64_t>(aBlocksPerArchive));

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
