#include "Decode_ManifestDiscovery.hpp"

#include <array>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>

#include "../../Common/LogCatalog.hpp"
#include "../FileAccess/ConflictNamePolicy.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "../MemoryLayout/Primatives.hpp"

namespace peanutbutter {

class DecodeInspectionCursorV2 {
 public:
  std::size_t mArchiveSlot = 0u;
  std::uint64_t mBlockIndex = 0u;
  std::uint64_t mScannedBlocks = 0u;
  std::uint64_t mTotalBlocksToInspect = 1u;
};

class DecodeRepairApplyCursorV2 {
 public:
  struct ArchivePlanV2 {
    std::size_t mArchiveSlot = std::numeric_limits<std::size_t>::max();
    std::uint64_t mArchiveIndex = 0u;
    bool mHasSourceFile = false;
    bool mNeedsSyntheticHeader = false;
    std::string mSourcePath;
    std::string mOutputName;
    std::string mOutputPath;
    std::uint64_t mSourceFileBytes = 0u;
    std::uint64_t mExpectedBlocks = 0u;
    std::uint64_t mExpectedFileBytes = 0u;
    std::uint64_t mRepairableBytes = 0u;
    std::uint64_t mFullReadableBlocks = 0u;
    std::uint64_t mNonRepairBlocks = 0u;
    std::uint64_t mExpectedRepairBlocks = 0u;
    std::uint64_t mReadableRepairBlocks = 0u;
    std::uint64_t mRepairableBlocks = 0u;
  };

  enum class StageV2 {
    kPlanArchives = 0,
    kBeginArchive = 1,
    kCopySource = 2,
    kCloseCopiedFile = 3,
    kWriteHeader = 4,
    kZeroFill = 5,
    kFinishArchive = 6,
    kApplyRepairBlocks = 7,
  };

  StageV2 mStage = StageV2::kPlanArchives;
  std::string mArchiveNamePrefix;
  std::string mArchiveNameSuffix;
  std::size_t mArchiveNameDigits = 1u;
  std::vector<ArchivePlanV2> mPlans;
  std::uint64_t mExpectedArchiveCount = 0u;
  std::uint64_t mNominalBlocksPerArchive = 0u;
  std::uint64_t mTotalFamilyBlocks = 0u;
  std::uint64_t mTotalNonRepairBlocks = 0u;
  std::uint64_t mPlanArchiveIndex = 0u;
  std::size_t mPlanningArchiveSlot = std::numeric_limits<std::size_t>::max();
  std::uint64_t mPlanningBlockIndex = 0u;
  std::size_t mCurrentPlanIndex = 0u;
  std::size_t mRepairSourcePlanIndex = 0u;
  std::uint64_t mRepairSourceBlockIndex = 0u;
  std::vector<std::string> mReservedOutputPaths;
  FixedBlockBufferV2 mCopyBuffer;
  FixedBlockBufferV2 mZeroBuffer;
  FixedBlockBufferV2 mDecodeBuffer;
  std::unique_ptr<FileReadStreamV2> mRead;
  std::unique_ptr<FileWriteStreamV2> mWrite;
  std::uint64_t mBytesCopied = 0u;
  std::uint64_t mZeroBytesRemaining = 0u;
  std::uint64_t mSizeBeforeResize = 0u;
  bool mResizeEventPending = false;
  bool mStopAfterCurrentArchive = false;
  bool mCancelLoggedForCurrentArchive = false;
  memory_layout::ArchiveHeaderV2 mSyntheticHeader{};
};

namespace {

using namespace memory_layout;

bool TryReadValidatedInspectionHeader(DecodeStageContextV2& pContext,
                                      const DiscoveredArchiveFileV2& pArchive,
                                      std::uint64_t pBlockIndex,
                                      SectionHeaderV2& pOutHeader) {
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (!pArchive.mIsPresent || pBlockIndex >= pArchive.mReadableBlockCount) {
    return false;
  }

  ByteBufferV2 aBlockBytes(aArchiveBlockBytes);
  if (aBlockBytes.Empty()) {
    return false;
  }

  const std::size_t aOffset = static_cast<std::size_t>(
      kArchiveHeaderBytesV2 + (pBlockIndex * static_cast<std::uint64_t>(aArchiveBlockBytes)));
  std::unique_ptr<FileReadStreamV2> aRead =
      pContext.FileSystem().OpenReadStream(pArchive.mPath);
  if (aRead == nullptr || !aRead->IsReady() ||
      !aRead->Read(aOffset, aBlockBytes.Data(), aArchiveBlockBytes)) {
    return false;
  }

  if (pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u) {
    if (ReadSectionHeader(aBlockBytes.Data(), kSectionHeaderBytesV2, pOutHeader, nullptr) &&
        ValidateSectionCheckSum(
            pOutHeader, aBlockBytes.Data() + kSectionHeaderBytesV2, aSectionPayloadBytes) &&
        pOutHeader.mSectionType ==
            static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest)) {
      return true;
    }

    std::string aUnsealError;
    if (pContext.State().mCipher.mWorkerBuffer.Size() < aArchiveBlockBytes ||
        !pContext.State().mCipher.mCipher.Unseal(
            aBlockBytes.Data(),
            pContext.State().mCipher.mWorkerBuffer.Data(),
            aBlockBytes.Data(),
            aArchiveBlockBytes,
            &aUnsealError)) {
      return false;
    }
  }

  return ReadSectionHeader(aBlockBytes.Data(), kSectionHeaderBytesV2, pOutHeader, nullptr) &&
         ValidateSectionCheckSum(
             pOutHeader, aBlockBytes.Data() + kSectionHeaderBytesV2, aSectionPayloadBytes);
}

void RefineArchiveWindowFromInspection(DecodeStageContextV2& pContext,
                                       const SectionHeaderV2& pSectionHeader) {
  DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  DecodeDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;

  if (pSectionHeader.mArchiveFileCount != 0u) {
    aBootstrap.mExpectedArchiveCount = pSectionHeader.mArchiveFileCount;
  }
  aBootstrap.mExpectedEmptyFolderBlockCount = pSectionHeader.mFolderManifestBlockCount;
  aBootstrap.mExpectedPreviewManifestBlockCount = pSectionHeader.mPreviewManifestBlockCount;
  aBootstrap.mExpectedArchiveDataBlockCount = pSectionHeader.mArchiveDataBlockCount;
  aBootstrap.mExpectedRepairBlockCount = pSectionHeader.mRepairDataBlockCount;

  std::uint64_t aObservedArchiveCount = 0u;
  for (const DiscoveredArchiveFileV2& aArchive : aDiscovery.mArchives) {
    if (!aArchive.mIsPresent) {
      continue;
    }
    aObservedArchiveCount = std::max<std::uint64_t>(
        aObservedArchiveCount, aArchive.mArchiveIndex + 1u);
  }
  if (aObservedArchiveCount == 0u) {
    aObservedArchiveCount =
        static_cast<std::uint64_t>(aDiscovery.mArchives.size());
  }
  if (aObservedArchiveCount == 0u) {
    return;
  }
  aBootstrap.mExpectedArchiveCount = aObservedArchiveCount;

  std::vector<DiscoveredArchiveFileV2> aRefined(
      static_cast<std::size_t>(aObservedArchiveCount));
  for (std::uint64_t aIndex = 0u; aIndex < aObservedArchiveCount; ++aIndex) {
    aRefined[static_cast<std::size_t>(aIndex)].mArchiveIndex = aIndex;
    aRefined[static_cast<std::size_t>(aIndex)].mArchiveBlockCount =
        pSectionHeader.mArchiveBlockCount;
    aRefined[static_cast<std::size_t>(aIndex)].mIsPresent = false;
  }

  for (const DiscoveredArchiveFileV2& aArchive : aDiscovery.mArchives) {
    if (aArchive.mArchiveIndex >= aObservedArchiveCount) {
      continue;
    }
    DiscoveredArchiveFileV2& aSlot =
        aRefined[static_cast<std::size_t>(aArchive.mArchiveIndex)];
    if (!aSlot.mIsPresent ||
        (!aSlot.mHasReadableHeader && aArchive.mHasReadableHeader) ||
        (aSlot.mPath.empty() && !aArchive.mPath.empty())) {
      aSlot = aArchive;
      aSlot.mIsPresent = true;
    }
  }

  aDiscovery.mArchives = std::move(aRefined);
  aDiscovery.mTotalReadableBlocks = 0u;
  for (const DiscoveredArchiveFileV2& aArchive : aDiscovery.mArchives) {
    if (aArchive.mIsPresent) {
      aDiscovery.mTotalReadableBlocks += aArchive.mReadableBlockCount;
    }
  }
}

std::uint64_t CeilingDivideU64(std::uint64_t pValue,
                               std::uint64_t pDivisor) {
  return pDivisor == 0u ? 0u : ((pValue + pDivisor - 1u) / pDivisor);
}

std::uint64_t MultiplyClampedU64(std::uint64_t pLeft,
                                 std::uint64_t pRight) {
  if (pLeft == 0u || pRight == 0u) {
    return 0u;
  }
  if (pLeft > (std::numeric_limits<std::uint64_t>::max() / pRight)) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return pLeft * pRight;
}

struct ArchiveNameTemplateV2 {
  std::string mPrefix;
  std::string mSuffix;
  std::size_t mDigits = 1u;
};

bool BuildArchiveNameTemplate(const DecodeStageContextV2& pContext,
                              ArchiveNameTemplateV2& pOutTemplate) {
  pOutTemplate = ArchiveNameTemplateV2{};

  std::uint32_t aIgnoredIndex = 0u;
  if (!ParseArchiveFileTemplateV2(
          pContext.FileSystem().FileName(
              pContext.State().mBootstrap.mBootstrapArchivePath),
          pOutTemplate.mPrefix,
          aIgnoredIndex,
          pOutTemplate.mSuffix,
          pOutTemplate.mDigits)) {
    return false;
  }
  return true;
}

std::uint64_t ComputeTotalFamilyBlocks(const DecodeStageContextV2& pContext) {
  const DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  const std::uint64_t aAdvertised =
      aBootstrap.mExpectedEmptyFolderBlockCount +
      aBootstrap.mExpectedPreviewManifestBlockCount +
      aBootstrap.mExpectedArchiveDataBlockCount +
      aBootstrap.mExpectedRepairBlockCount;
  if (aAdvertised > 0u) {
    return aAdvertised;
  }
  return pContext.State().mDiscovery.mTotalReadableBlocks;
}

std::uint64_t DetermineNominalBlocksPerArchive(const DecodeStageContextV2& pContext,
                                               std::uint64_t pExpectedArchiveCount,
                                               std::uint64_t pTotalFamilyBlocks) {
  std::uint64_t aMaxNonLast = 0u;
  std::uint64_t aMaxAny = 0u;

  for (const DiscoveredArchiveFileV2& aArchive : pContext.State().mDiscovery.mArchives) {
    const std::uint64_t aBlockCount =
        aArchive.mHasReadableSection ? aArchive.mArchiveBlockCount
                                     : aArchive.mReadableBlockCount;
    if (aBlockCount == 0u) {
      continue;
    }

    aMaxAny = std::max(aMaxAny, aBlockCount);
    if (pExpectedArchiveCount > 0u &&
        aArchive.mArchiveIndex + 1u < pExpectedArchiveCount) {
      aMaxNonLast = std::max(aMaxNonLast, aBlockCount);
    }
  }

  if (pContext.State().mInspection.mHasValidSection) {
    const DecodeInspectionStateV2& aInspection = pContext.State().mInspection;
    const std::uint64_t aInspectionBlocks = aInspection.mSectionHeader.mArchiveBlockCount;
    aMaxAny = std::max(aMaxAny, aInspectionBlocks);
    if (pExpectedArchiveCount > 0u &&
        aInspection.mArchiveIndex + 1u < pExpectedArchiveCount) {
      aMaxNonLast = std::max(aMaxNonLast, aInspectionBlocks);
    }
  }

  std::uint64_t aNominal = aMaxNonLast != 0u ? aMaxNonLast : aMaxAny;
  if (aNominal == 0u && pExpectedArchiveCount > 0u && pTotalFamilyBlocks > 0u) {
    aNominal = CeilingDivideU64(pTotalFamilyBlocks, pExpectedArchiveCount);
  }
  if (aNominal != 0u &&
      pExpectedArchiveCount > 0u &&
      MultiplyClampedU64(aNominal, pExpectedArchiveCount) < pTotalFamilyBlocks) {
    aNominal = CeilingDivideU64(pTotalFamilyBlocks, pExpectedArchiveCount);
  }

  return aNominal;
}

std::uint64_t DetermineExpectedBlocksForArchive(const DiscoveredArchiveFileV2* pArchive,
                                                std::uint64_t pArchiveSlot,
                                                std::uint64_t pExpectedArchiveCount,
                                                std::uint64_t pNominalBlocksPerArchive,
                                                std::uint64_t pTotalFamilyBlocks) {
  if (pArchive != nullptr &&
      pArchive->mHasReadableSection &&
      pArchive->mArchiveBlockCount != 0u) {
    return pArchive->mArchiveBlockCount;
  }

  if (pExpectedArchiveCount == 0u) {
    return pArchive != nullptr ? pArchive->mReadableBlockCount : 0u;
  }

  if (pTotalFamilyBlocks == 0u) {
    return pNominalBlocksPerArchive;
  }

  const std::uint64_t aConsumedBefore =
      std::min(pTotalFamilyBlocks,
               MultiplyClampedU64(pArchiveSlot, pNominalBlocksPerArchive));
  if (aConsumedBefore >= pTotalFamilyBlocks) {
    return 0u;
  }
  return std::min(pNominalBlocksPerArchive, pTotalFamilyBlocks - aConsumedBefore);
}

bool BuildSyntheticArchiveHeader(const DecodeStageContextV2& pContext,
                                 std::uint64_t pArchiveIndex,
                                 ArchiveHeaderV2& pOutHeader,
                                 std::string& pOutError) {
  pOutError.clear();
  pOutHeader = pContext.State().mBootstrap.mFirstHeader;
  pOutHeader.mDirtyState =
      static_cast<std::uint8_t>(ArchiveDirtyStateV2::kFinishedWithError);

  if (!TrySetPackedUint48(pOutHeader.mArchiveIndex,
                          pArchiveIndex,
                          nullptr,
                          "ArchiveIndex") ||
      !TrySetPackedUint48(pOutHeader.mArchiveCount,
                          pContext.State().mBootstrap.mExpectedArchiveCount,
                          nullptr,
                          "ArchiveCount") ||
      !TrySetPackedUint48(pOutHeader.mArchiveDataBlockCount,
                          pContext.State().mBootstrap.mExpectedArchiveDataBlockCount,
                          nullptr,
                          "ArchiveDataBlockCount") ||
      !TrySetPackedUint48(pOutHeader.mEmptyFolderBlockCount,
                          pContext.State().mBootstrap.mExpectedEmptyFolderBlockCount,
                          nullptr,
                          "EmptyFolderBlockCount") ||
      !TrySetPackedUint48(pOutHeader.mPreviewManifestBlockCount,
                          pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount,
                          nullptr,
                          "PreviewManifestBlockCount") ||
      !TrySetPackedUint48(pOutHeader.mRepairSectorBlockCount,
                          pContext.State().mBootstrap.mExpectedRepairBlockCount,
                          nullptr,
                          "RepairSectorBlockCount")) {
    pOutError = "synthetic archive header values were out of range";
    return false;
  }
  return true;
}

bool WriteOrPatchArchiveHeader(FileSystemV2& pFileSystem,
                               const std::string& pDestinationPath,
                               const ArchiveHeaderV2& pHeader,
                               bool pWriteFreshFile,
                               std::string& pOutError) {
  pOutError.clear();
  std::array<unsigned char, kArchiveHeaderBytesV2> aHeaderBytes{};
  if (!WriteArchiveHeader(pHeader,
                          aHeaderBytes.data(),
                          aHeaderBytes.size(),
                          nullptr)) {
    pOutError = "failed serializing synthetic archive header";
    return false;
  }

  if (pWriteFreshFile) {
    if (!pFileSystem.WriteFile(
            pDestinationPath, aHeaderBytes.data(), aHeaderBytes.size())) {
      pOutError = "failed writing synthetic archive header";
      return false;
    }
    return true;
  }

  if (!pFileSystem.OverwriteFileRegion(
          pDestinationPath, 0u, aHeaderBytes.data(), aHeaderBytes.size())) {
    pOutError = "failed patching synthetic archive header";
    return false;
  }
  return true;
}

std::string BuildRepairProgressLabel(const DecodeRepairStateV2& pRepairState) {
  std::ostringstream aOut;
  aOut << "Repairing archive family: "
       << pRepairState.mPatchedBlocks << "/" << pRepairState.mRepairableBlocks
       << " blocks, " << FormatHumanBytesV2(pRepairState.mPatchedBytes) << " / "
       << FormatHumanBytesV2(pRepairState.mRepairableBytes);
  if (pRepairState.mArchivesTotal > 0u) {
    aOut << ", archives " << pRepairState.mArchivesCompleted << "/"
         << pRepairState.mArchivesTotal;
  }
  return aOut.str();
}

std::string MakeArchiveNameFromCursor(const DecodeRepairApplyCursorV2& pCursor,
                                      std::uint64_t pArchiveIndex) {
  std::ostringstream aOut;
  aOut << pCursor.mArchiveNamePrefix
       << std::setw(static_cast<int>(pCursor.mArchiveNameDigits))
       << std::setfill('0') << pArchiveIndex
       << pCursor.mArchiveNameSuffix;
  return aOut.str();
}

double CurrentRepairArchiveFraction(const DecodeRepairApplyCursorV2& pCursor) {
  if (pCursor.mCurrentPlanIndex >= pCursor.mPlans.size()) {
    return 0.0;
  }

  const DecodeRepairApplyCursorV2::ArchivePlanV2& aPlan =
      pCursor.mPlans[pCursor.mCurrentPlanIndex];
  if (aPlan.mExpectedFileBytes == 0u) {
    return pCursor.mStage == DecodeRepairApplyCursorV2::StageV2::kFinishArchive
               ? 1.0
               : 0.0;
  }

  double aFraction =
      static_cast<double>(pCursor.mBytesCopied) /
      static_cast<double>(aPlan.mExpectedFileBytes);
  aFraction = std::max(0.0, std::min(1.0, aFraction));
  if (pCursor.mStage != DecodeRepairApplyCursorV2::StageV2::kFinishArchive &&
      aFraction >= 1.0) {
    aFraction = 0.999;
  }
  return aFraction;
}

void EmitRepairApplyProgress(DecodeStageContextV2& pContext,
                             const DecodeRepairApplyCursorV2& pCursor) {
  const DecodeRepairStateV2& aRepair = pContext.State().mRepair;
  const double aArchiveFraction =
      CurrentRepairArchiveFraction(pCursor);
  const double aLocalFraction =
      (static_cast<double>(aRepair.mArchivesCompleted) + aArchiveFraction) /
      static_cast<double>(std::max<std::uint64_t>(1u, aRepair.mArchivesTotal));
  pContext.EmitPhaseProgress(aLocalFraction, BuildRepairProgressLabel(aRepair));
}

void ObserveRepairApplyCancel(DecodeStageContextV2& pContext,
                              DecodeRepairApplyCursorV2& pCursor,
                              const DecodeRepairApplyCursorV2::ArchivePlanV2& pPlan) {
  if (!pContext.IsCancelRequested() || pCursor.mStopAfterCurrentArchive) {
    return;
  }

  pCursor.mStopAfterCurrentArchive = true;
  pContext.State().mCancel.mObserved = true;
  pContext.State().mCancel.mCancelFileReference = pPlan.mOutputName;
  if (pCursor.mCancelLoggedForCurrentArchive) {
    return;
  }

  pCursor.mCancelLoggedForCurrentArchive = true;
  pContext.EmitLog(
      LogLevelV2::kWarning,
      "[Repair][Repair Apply] Cancel requested; finishing archive " +
          pPlan.mOutputName + " before stopping.");
}

void RequestRepairApplyFinalize(DecodeStageContextV2& pContext,
                                const std::string& pReason) {
  pContext.State().mCancel.mObserved = true;
  pContext.State().mCancel.mShouldFinalizeAfterCancel = true;
  if (!pReason.empty()) {
    pContext.EmitLog(LogLevelV2::kWarning, pReason);
  }
  pContext.State().mCursor.mRepairApply.reset();
}

void AdoptRepairPlanningSectionTruth(DiscoveredArchiveFileV2& pArchive,
                                     const SectionHeaderV2& pSectionHeader) {
  pArchive.mHasReadableSection = true;
  pArchive.mFirstSectionHeader = pSectionHeader;
  pArchive.mArchiveIndex = pSectionHeader.mArchiveIndex;
  pArchive.mArchiveBlockCount = pSectionHeader.mArchiveBlockCount;
}

void EmitRepairPlanningProgress(DecodeStageContextV2& pContext,
                                const DecodeRepairApplyCursorV2& pCursor,
                                const std::string& pLabel) {
  double aLocalFraction = 0.0;
  if (pCursor.mExpectedArchiveCount > 0u) {
    aLocalFraction =
        static_cast<double>(pCursor.mPlanArchiveIndex) /
        static_cast<double>(pCursor.mExpectedArchiveCount);
    if (pCursor.mPlanningArchiveSlot != std::numeric_limits<std::size_t>::max() &&
        pCursor.mPlanningArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
      const DiscoveredArchiveFileV2& aArchive =
          pContext.State().mDiscovery.mArchives[pCursor.mPlanningArchiveSlot];
      if (aArchive.mReadableBlockCount > 0u) {
        aLocalFraction =
            (static_cast<double>(pCursor.mPlanArchiveIndex) +
             (static_cast<double>(pCursor.mPlanningBlockIndex) /
              static_cast<double>(aArchive.mReadableBlockCount))) /
            static_cast<double>(pCursor.mExpectedArchiveCount);
      }
    }
  }
  pContext.EmitPhaseProgress(std::max(0.0, std::min(0.999, aLocalFraction)), pLabel);
}

bool BuildRepairArchivePlan(
    DecodeStageContextV2& pContext,
    DecodeRepairApplyCursorV2& pCursor,
    std::uint64_t pArchiveIndex,
    DecodeRepairApplyCursorV2::ArchivePlanV2& pOutPlan,
    std::string& pOutError) {
  pOutError.clear();
  pOutPlan = DecodeRepairApplyCursorV2::ArchivePlanV2{};
  pOutPlan.mArchiveIndex = pArchiveIndex;

  DiscoveredArchiveFileV2* aArchive = nullptr;
  if (pArchiveIndex < pContext.State().mDiscovery.mArchives.size()) {
    const std::size_t aArchiveSlot = static_cast<std::size_t>(pArchiveIndex);
    aArchive = &pContext.State().mDiscovery.mArchives[aArchiveSlot];
    pOutPlan.mArchiveSlot = aArchiveSlot;
  }

  pOutPlan.mExpectedBlocks =
      DetermineExpectedBlocksForArchive(aArchive,
                                        pArchiveIndex,
                                        pCursor.mExpectedArchiveCount,
                                        pCursor.mNominalBlocksPerArchive,
                                        pCursor.mTotalFamilyBlocks);
  pOutPlan.mNonRepairBlocks =
      DetermineExpectedBlocksForArchive(aArchive,
                                        pArchiveIndex,
                                        pCursor.mExpectedArchiveCount,
                                        pCursor.mNominalBlocksPerArchive,
                                        pCursor.mTotalNonRepairBlocks);
  if (pOutPlan.mNonRepairBlocks > pOutPlan.mExpectedBlocks) {
    pOutPlan.mNonRepairBlocks = pOutPlan.mExpectedBlocks;
  }
  pOutPlan.mExpectedRepairBlocks = pOutPlan.mExpectedBlocks - pOutPlan.mNonRepairBlocks;
  pOutPlan.mExpectedFileBytes =
      static_cast<std::uint64_t>(kArchiveHeaderBytesV2) +
      (pOutPlan.mExpectedBlocks *
       static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes));

  pOutPlan.mHasSourceFile =
      aArchive != nullptr && aArchive->mIsPresent && !aArchive->mPath.empty();
  const std::string aPreferredOutputName =
      pOutPlan.mHasSourceFile ? pContext.FileSystem().FileName(aArchive->mPath)
                              : MakeArchiveNameFromCursor(pCursor, pArchiveIndex);
  if (!ResolveNoOverwritePathV2(pContext.FileSystem(),
                                pContext.Request().mDestinationDirectory,
                                aPreferredOutputName,
                                pOutPlan.mOutputPath,
                                &pCursor.mReservedOutputPaths)) {
    pOutError = "unable to resolve non-overwrite output archive path for " +
                aPreferredOutputName;
    return false;
  }
  pOutPlan.mOutputName = pContext.FileSystem().FileName(pOutPlan.mOutputPath);
  pCursor.mReservedOutputPaths.push_back(pOutPlan.mOutputPath);

  pOutPlan.mSourcePath = pOutPlan.mHasSourceFile ? aArchive->mPath : std::string();
  pOutPlan.mSourceFileBytes = pOutPlan.mHasSourceFile ? aArchive->mFileLength : 0u;
  pOutPlan.mNeedsSyntheticHeader =
      !pOutPlan.mHasSourceFile ||
      (aArchive != nullptr &&
       (!aArchive->mHasReadableHeader ||
        aArchive->mFileLength < static_cast<std::uint64_t>(kArchiveHeaderBytesV2)));

  const std::uint64_t aCoveredFileBytes =
      std::max<std::uint64_t>(pOutPlan.mSourceFileBytes,
                              static_cast<std::uint64_t>(kArchiveHeaderBytesV2));
  pOutPlan.mRepairableBytes =
      pOutPlan.mExpectedFileBytes > aCoveredFileBytes
          ? (pOutPlan.mExpectedFileBytes - aCoveredFileBytes)
          : 0u;
  pOutPlan.mFullReadableBlocks =
      pOutPlan.mSourceFileBytes > static_cast<std::uint64_t>(kArchiveHeaderBytesV2)
          ? ((pOutPlan.mSourceFileBytes -
              static_cast<std::uint64_t>(kArchiveHeaderBytesV2)) /
             static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes))
          : 0u;
  pOutPlan.mReadableRepairBlocks =
      pOutPlan.mFullReadableBlocks > pOutPlan.mNonRepairBlocks
          ? std::min(pOutPlan.mExpectedRepairBlocks,
                     pOutPlan.mFullReadableBlocks - pOutPlan.mNonRepairBlocks)
          : 0u;
  pOutPlan.mRepairableBlocks =
      pOutPlan.mExpectedBlocks > pOutPlan.mFullReadableBlocks
          ? (pOutPlan.mExpectedBlocks - pOutPlan.mFullReadableBlocks)
          : 0u;
  return true;
}

bool ReadArchiveBlockRaw(FileReadStreamV2& pRead,
                         std::uint64_t pBlockIndex,
                         std::size_t pArchiveBlockBytes,
                         unsigned char* pOutBlockBytes,
                         std::string& pOutError) {
  pOutError.clear();
  if (pOutBlockBytes == nullptr) {
    pOutError = "null destination block buffer";
    return false;
  }

  const std::size_t aOffset = static_cast<std::size_t>(
      kArchiveHeaderBytesV2 +
      (pBlockIndex * static_cast<std::uint64_t>(pArchiveBlockBytes)));
  if (!pRead.Read(aOffset, pOutBlockBytes, pArchiveBlockBytes)) {
    pOutError = "failed reading archive block at index " + std::to_string(pBlockIndex);
    return false;
  }
  return true;
}

bool DecodeValidatedSectionHeaderFromRawBlock(DecodeStageContextV2& pContext,
                                              const unsigned char* pRawBlockBytes,
                                              FixedBlockBufferV2& pDecodeBuffer,
                                              SectionHeaderV2& pOutHeader,
                                              std::string& pOutError) {
  pOutError.clear();
  if (pRawBlockBytes == nullptr || pDecodeBuffer.Empty()) {
    pOutError = "block buffers were unavailable for decode validation";
    return false;
  }

  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (pDecodeBuffer.Size() < aArchiveBlockBytes) {
    pOutError = "decode buffer was too small for archive block bytes";
    return false;
  }
  const bool aEncrypted = pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u;
  const unsigned char* aParseBytes = pRawBlockBytes;
  if (aEncrypted) {
    std::memcpy(pDecodeBuffer.Data(), pRawBlockBytes, aArchiveBlockBytes);
    std::string aUnsealError;
    if (pContext.State().mCipher.mWorkerBuffer.Size() < aArchiveBlockBytes ||
        !pContext.State().mCipher.mCipher.Unseal(
            pDecodeBuffer.Data(),
            pContext.State().mCipher.mWorkerBuffer.Data(),
            pDecodeBuffer.Data(),
            aArchiveBlockBytes,
            &aUnsealError)) {
      pOutError = aUnsealError.empty() ? "block failed decryption/checksum validation"
                                       : "block failed decryption/checksum validation: " +
                                             aUnsealError;
      return false;
    }
    aParseBytes = pDecodeBuffer.Data();
  }

  if (!ReadSectionHeader(aParseBytes, kSectionHeaderBytesV2, pOutHeader, nullptr) ||
      !ValidateSectionCheckSum(
          pOutHeader, aParseBytes + kSectionHeaderBytesV2, aSectionPayloadBytes)) {
    pOutError = "block header checksum/validation failed";
    return false;
  }
  return true;
}

bool BuildPatchedRawTargetBlockFromRepair(DecodeStageContextV2& pContext,
                                          const unsigned char* pRawRepairBlockBytes,
                                          const FixedBlockBufferV2& pDecodeBuffer,
                                          const SectionHeaderV2& pRepairHeader,
                                          std::uint64_t pTargetArchiveIndex,
                                          std::uint64_t pTargetBlockIndex,
                                          FixedBlockBufferV2& pOutPatchedRawBlock,
                                          std::string& pOutError) {
  pOutError.clear();
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (pRawRepairBlockBytes == nullptr || pOutPatchedRawBlock.Empty() ||
      pOutPatchedRawBlock.Size() < aArchiveBlockBytes) {
    pOutError = "repair patch buffers were unavailable";
    return false;
  }

  const bool aEncrypted = pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u;
  const unsigned char* aRepairPlainBlockBytes = pRawRepairBlockBytes;
  if (aEncrypted) {
    if (pDecodeBuffer.Empty() || pDecodeBuffer.Size() < aArchiveBlockBytes) {
      pOutError = "decode buffer was too small for repair patch assembly";
      return false;
    }
    aRepairPlainBlockBytes = pDecodeBuffer.Data();
  }

  std::memcpy(
      pOutPatchedRawBlock.Data(), aRepairPlainBlockBytes, aArchiveBlockBytes);

  SectionHeaderV2 aPatchedHeader = pRepairHeader;
  aPatchedHeader.mRepairRecord.mRepairPointerArchive =
      static_cast<std::uint32_t>(pTargetArchiveIndex);
  aPatchedHeader.mRepairRecord.mRepairPointerBlock =
      static_cast<std::uint32_t>(pTargetBlockIndex);
  aPatchedHeader.mCheckSum = ComputeSectionCheckSum(
      pOutPatchedRawBlock.Data() + kSectionHeaderBytesV2,
      aSectionPayloadBytes,
      aPatchedHeader);

  if (!WriteSectionHeader(aPatchedHeader,
                          pOutPatchedRawBlock.Data(),
                          kSectionHeaderBytesV2,
                          nullptr)) {
    pOutError = "failed writing patched section header";
    return false;
  }

  if (aEncrypted) {
    if (!pContext.State().mCipher.mAssembled) {
      pOutError = "repair apply expected assembled cipher while resealing patched block";
      return false;
    }
    if (pContext.State().mCipher.mWorkerBuffer.Size() < aArchiveBlockBytes) {
      pOutError = "cipher worker buffer is too small while resealing patched block";
      return false;
    }
    std::string aSealError;
    if (!pContext.State().mCipher.mCipher.Seal(
            pOutPatchedRawBlock.Data(),
            pContext.State().mCipher.mWorkerBuffer.Data(),
            pOutPatchedRawBlock.Data(),
            aArchiveBlockBytes,
            &aSealError)) {
      pOutError = aSealError.empty() ? "failed resealing patched block"
                                     : "failed resealing patched block: " + aSealError;
      return false;
    }
  }

  return true;
}

bool CheckIfTargetBlockAlreadyValidForRepair(DecodeStageContextV2& pContext,
                                             const std::string& pTargetArchivePath,
                                             std::uint64_t pTargetBlockIndex,
                                             FixedBlockBufferV2& pRawTargetBlockBuffer,
                                             FixedBlockBufferV2& pDecodeBuffer,
                                             bool& pOutIsValid,
                                             std::string& pOutError) {
  pOutIsValid = false;
  pOutError.clear();
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  if (pRawTargetBlockBuffer.Empty() ||
      pRawTargetBlockBuffer.Size() < aArchiveBlockBytes) {
    pOutError = "target block buffer was too small";
    return false;
  }

  std::unique_ptr<FileReadStreamV2> aTargetRead =
      pContext.FileSystem().OpenReadStream(pTargetArchivePath);
  if (aTargetRead == nullptr || !aTargetRead->IsReady()) {
    pOutError = "failed opening target archive for read";
    return false;
  }

  if (!ReadArchiveBlockRaw(*aTargetRead,
                           pTargetBlockIndex,
                           aArchiveBlockBytes,
                           pRawTargetBlockBuffer.Data(),
                           pOutError)) {
    return false;
  }

  SectionHeaderV2 aObservedHeader;
  std::string aValidationError;
  pOutIsValid = DecodeValidatedSectionHeaderFromRawBlock(
      pContext,
      pRawTargetBlockBuffer.Data(),
      pDecodeBuffer,
      aObservedHeader,
      aValidationError);
  return true;
}

bool FinalizeRepairApplySuccess(DecodeStageContextV2& pContext) {
  const DecodeRepairStateV2& aRepair = pContext.State().mRepair;
  std::ostringstream aSummary;
  aSummary << "[Repair][Repair Apply] Successfully patched "
           << aRepair.mPatchedBlocks << "/" << aRepair.mRepairableBlocks
           << " repairable blocks (" << FormatHumanBytesV2(aRepair.mPatchedBytes)
           << " / " << FormatHumanBytesV2(aRepair.mRepairableBytes)
           << ") across " << aRepair.mArchivesCompleted << "/"
           << aRepair.mArchivesTotal << " archives.";
  aSummary << " Applied " << aRepair.mRepairBlocksApplied
           << " repair replacements";
  if (aRepair.mRepairBlocksSkippedValidTarget > 0u) {
    aSummary << ", skipped " << aRepair.mRepairBlocksSkippedValidTarget
             << " already-valid targets (non-aggressive mode)";
  }
  aSummary << ".";
  if (aRepair.mArchivesSynthesized > 0u || aRepair.mArchivesExpanded > 0u) {
    aSummary << " Synthesized " << aRepair.mArchivesSynthesized
             << ", expanded " << aRepair.mArchivesExpanded << ".";
  }
  pContext.EmitLog(LogLevelV2::kInfo, aSummary.str());
  pContext.EmitPhaseProgress(1.0, "Repair apply complete");
  pContext.State().mCursor.mRepairApply.reset();
  return !pContext.IsCancelRequested();
}

void EmitRepairFileEvent(DecodeStageContextV2& pContext,
                         RuntimeEventKindV2 pKind,
                         const std::string& pPath,
                         std::uint64_t pArchiveIndex,
                         std::uint64_t pOldSize,
                         std::uint64_t pNewSize) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = pKind;
  aEvent.mStage = ProgressStageV2::kRepairApply;
  aEvent.SetInfo("archive_index", pArchiveIndex);
  aEvent.SetInfo("path", pPath);
  aEvent.SetInfo("old_size", pOldSize);
  aEvent.SetInfo("new_size", pNewSize);
  if (pKind == RuntimeEventKindV2::kRepairFileCreated) {
    aEvent.mLabel = "Repair created " + pPath;
  } else {
    aEvent.mLabel = "Repair resized " + pPath;
  }
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitRepairBlockEvent(DecodeStageContextV2& pContext,
                          RuntimeEventKindV2 pKind,
                          const std::string& pPath,
                          std::uint64_t pArchiveIndex,
                          std::uint64_t pBlockIndex) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = pKind;
  aEvent.mStage = ProgressStageV2::kRepairApply;
  aEvent.SetInfo("archive_index", pArchiveIndex);
  aEvent.SetInfo("path", pPath);
  aEvent.SetInfo("block_index", pBlockIndex);

  switch (pKind) {
    case RuntimeEventKindV2::kRepairBlockStarted:
      aEvent.mLabel = "Repair started block " + std::to_string(pBlockIndex);
      break;
    case RuntimeEventKindV2::kRepairBlockFinished:
      aEvent.mLabel = "Repair finished block " + std::to_string(pBlockIndex);
      break;
    case RuntimeEventKindV2::kRepairBlockMatched:
      aEvent.mLabel = "Repair matched block " + std::to_string(pBlockIndex);
      break;
    case RuntimeEventKindV2::kRepairBlockUnmatched:
      aEvent.mLabel = "Repair failed to match block " + std::to_string(pBlockIndex);
      break;
    default:
      aEvent.mLabel = RuntimeEventKindLabelV2(pKind);
      break;
  }

  pContext.EmitRuntimeEvent(aEvent);
}

void EmitDecodeInspectionBlockEvent(DecodeStageContextV2& pContext,
                                    const DiscoveredArchiveFileV2& pArchive,
                                    std::size_t pArchiveSlot,
                                    std::uint64_t pBlockIndex,
                                    bool pValidSection,
                                    const SectionHeaderV2* pSectionHeader) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeInspectionBlockScanned;
  aEvent.mStage = ProgressStageV2::kInspection;
  aEvent.mLabel =
      "Decode inspection scanned block " + std::to_string(pBlockIndex) +
      " in archive " + std::to_string(pArchiveSlot);
  aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("block_index", pBlockIndex);
  aEvent.SetInfo("valid_section", pValidSection);
  if (pValidSection && pSectionHeader != nullptr) {
    aEvent.SetInfo("section_type",
                   static_cast<std::uint64_t>(pSectionHeader->mSectionType));
    aEvent.SetInfo("payload_bytes_used",
                   static_cast<std::uint64_t>(pSectionHeader->mPayloadBytesUsed));
  }
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitRepairArchiveEvent(DecodeStageContextV2& pContext,
                            RuntimeEventKindV2 pKind,
                            const std::string& pPath,
                            std::uint64_t pArchiveIndex,
                            std::uint64_t pExpectedBlocks,
                            std::uint64_t pFileBytes) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = pKind;
  aEvent.mStage = ProgressStageV2::kRepairApply;
  aEvent.SetInfo("archive_index", pArchiveIndex);
  aEvent.SetInfo("path", pPath);
  aEvent.SetInfo("expected_blocks", pExpectedBlocks);
  aEvent.SetInfo("file_bytes", pFileBytes);
  if (pKind == RuntimeEventKindV2::kRepairArchiveStarted) {
    aEvent.mLabel = "Repair started archive " + pPath;
  } else {
    aEvent.mLabel = "Repair finished archive " + pPath;
  }
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitRepairArchiveHeaderEvent(DecodeStageContextV2& pContext,
                                  const std::string& pPath,
                                  std::uint64_t pArchiveIndex,
                                  bool pCreatedNewFile,
                                  const ArchiveHeaderV2& pHeader) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kRepairArchiveHeaderWritten;
  aEvent.mStage = ProgressStageV2::kRepairApply;
  aEvent.mLabel = "Repair wrote archive header for " + pPath;
  aEvent.SetInfo("archive_index", pArchiveIndex);
  aEvent.SetInfo("path", pPath);
  aEvent.SetInfo("mode", pCreatedNewFile ? "created" : "patched");
  aEvent.SetInfo("archive_count", PackedUint48ToUInt64(pHeader.mArchiveCount));
  aEvent.SetInfo("archive_family_id", pHeader.mArchiveFamilyId);
  aEvent.SetInfo("archive_data_block_count",
                 PackedUint48ToUInt64(pHeader.mArchiveDataBlockCount));
  aEvent.SetInfo("repair_block_count",
                 PackedUint48ToUInt64(pHeader.mRepairSectorBlockCount));
  pContext.EmitRuntimeEvent(aEvent);
}

bool FinalizeInspectionSuccess(DecodeStageContextV2& pContext) {
  const DecodeInspectionStateV2& aInspection = pContext.State().mInspection;
  pContext.State().mCursor.mInspection.reset();
  pContext.EmitLog(
      LogLevelV2::kInfo,
      "[" + LogActionLabelV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent)) +
          "][Inspection] Accepted archive " +
          std::to_string(aInspection.mArchiveIndex) + " block " +
          std::to_string(aInspection.mBlockIndex) + " as the first valid inspected block.");
  pContext.EmitPhaseProgress(1.0, "Inspection complete");
  return !pContext.IsCancelRequested();
}

}  // namespace

bool DecodeInspectionV2::Run(DecodeStageContextV2& pContext) {
  DecodeInspectionStateV2& aInspection = pContext.State().mInspection;
  std::shared_ptr<DecodeInspectionCursorV2>& aCursor =
      pContext.State().mCursor.mInspection;
  if (!aCursor) {
    aInspection = DecodeInspectionStateV2{};
    aCursor = std::make_shared<DecodeInspectionCursorV2>();
    aCursor->mTotalBlocksToInspect =
        std::max<std::uint64_t>(1u, pContext.State().mDiscovery.mTotalReadableBlocks);
  }

  auto aAccept = [&](std::size_t pArchiveSlot,
                     std::uint64_t pBlockIndex,
                     const SectionHeaderV2& pSectionHeader) {
    aInspection.mHasValidSection = true;
    aInspection.mArchiveIndex = pSectionHeader.mArchiveIndex;
    aInspection.mBlockIndex = pBlockIndex;
    aInspection.mSectionHeader = pSectionHeader;

    DiscoveredArchiveFileV2& aArchive =
        pContext.State().mDiscovery.mArchives[pArchiveSlot];
    aArchive.mHasReadableSection = true;
    aArchive.mFirstSectionHeader = pSectionHeader;
    aArchive.mArchiveIndex = pSectionHeader.mArchiveIndex;
    aArchive.mArchiveBlockCount = pSectionHeader.mArchiveBlockCount;

    RefineArchiveWindowFromInspection(pContext, pSectionHeader);
    return true;
  };

  if (pContext.Request().mIntent == DecodeIntentV2::kUnbundle) {
    if (pContext.State().mDiscovery.mArchives.empty() ||
        !pContext.State().mDiscovery.mArchives.front().mIsPresent) {
      aCursor.reset();
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                        ProgressStageV2::kInspection,
                                        "the first archive box is missing"));
      return false;
    }

    SectionHeaderV2 aHeader;
    if (!TryReadValidatedInspectionHeader(
            pContext, pContext.State().mDiscovery.mArchives.front(), 0u, aHeader)) {
      aCursor.reset();
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                        ProgressStageV2::kInspection,
                                        "the first block in the first archive did not validate"));
      return false;
    }
    ++aCursor->mScannedBlocks;
    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeInspectionBlockScanned)) {
      EmitDecodeInspectionBlockEvent(pContext,
                                     pContext.State().mDiscovery.mArchives.front(),
                                     0u,
                                     0u,
                                     true,
                                     &aHeader);
    }
    (void)aAccept(0u, 0u, aHeader);
    return FinalizeInspectionSuccess(pContext);
  }

  while (aCursor->mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
    const DiscoveredArchiveFileV2& aArchive =
        pContext.State().mDiscovery.mArchives[aCursor->mArchiveSlot];
    if (!aArchive.mIsPresent ||
        aCursor->mBlockIndex >= aArchive.mReadableBlockCount) {
      ++aCursor->mArchiveSlot;
      aCursor->mBlockIndex = 0u;
      continue;
    }
    break;
  }

  if (aCursor->mArchiveSlot >= pContext.State().mDiscovery.mArchives.size()) {
    aCursor.reset();
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kInspection,
                                      "no valid block was found during inspection"));
    return false;
  }

  const DiscoveredArchiveFileV2& aArchive =
      pContext.State().mDiscovery.mArchives[aCursor->mArchiveSlot];
  SectionHeaderV2 aHeader;
  const bool aValidHeader =
      TryReadValidatedInspectionHeader(pContext, aArchive, aCursor->mBlockIndex, aHeader);
  ++aCursor->mScannedBlocks;
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeInspectionBlockScanned)) {
    EmitDecodeInspectionBlockEvent(pContext,
                                   aArchive,
                                   aCursor->mArchiveSlot,
                                   aCursor->mBlockIndex,
                                   aValidHeader,
                                   aValidHeader ? &aHeader : nullptr);
  }
  pContext.EmitPhaseProgress(
      static_cast<double>(aCursor->mScannedBlocks) /
          static_cast<double>(std::max<std::uint64_t>(1u, aCursor->mTotalBlocksToInspect)),
      "Inspecting archive blocks");
  if (pContext.IsCancelRequested()) {
    return false;
  }
  if (aValidHeader) {
    (void)aAccept(aCursor->mArchiveSlot, aCursor->mBlockIndex, aHeader);
    return FinalizeInspectionSuccess(pContext);
  }

  ++aCursor->mBlockIndex;
  pContext.ContinuePhaseOnNextHeartbeat();
  return true;
}

bool DecodeManifestDiscoveryV2::Run(DecodeStageContextV2& pContext) {
  if (pContext.Request().mIntent == DecodeIntentV2::kManifest) {
    const DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
    const DecodeDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;

    std::ostringstream aOut;
    aOut << "[Read Manifest][Report] Family "
         << aBootstrap.mFirstHeader.mArchiveFamilyId
         << ", archives=" << aDiscovery.mArchives.size()
         << ", readable_blocks=" << aDiscovery.mTotalReadableBlocks
         << ", advertised_preview_manifest_blocks="
         << aBootstrap.mExpectedPreviewManifestBlockCount
         << ", advertised_archive_data_blocks="
         << aBootstrap.mExpectedArchiveDataBlockCount
         << ", advertised_repair_blocks="
         << aBootstrap.mExpectedRepairBlockCount
         << ", encrypted="
         << (aBootstrap.mFirstHeader.mIsEncrypted != 0u ? "true" : "false")
         << ", discovery_mode="
         << (aDiscovery.mMode == DecodeModeV2::kOptimistic ? "optimistic"
                                                           : "pessimistic")
         << ".";
    if (aBootstrap.mExpectedEmptyFolderBlockCount > 0u) {
      aOut << " legacy_empty_folder_blocks="
           << aBootstrap.mExpectedEmptyFolderBlockCount
           << ".";
    }
    pContext.EmitLog(LogLevelV2::kInfo, aOut.str());

    if (aBootstrap.mExpectedPreviewManifestBlockCount > 0u) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Read Manifest][Report] Preview-manifest block count is only an "
          "advertised header value here; this flow does not decode preview "
          "manifest payloads.");
    }
  } else {
    pContext.EmitLog(
        LogLevelV2::kInfo,
        LogDecodeManifestSummaryV2(
            LogActionFromDecodeIntentV2(pContext.Request().mIntent),
            pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount,
            pContext.State().mBootstrap.mExpectedRepairBlockCount));
  }
  pContext.EmitPhaseProgress(1.0, "Manifest discovery complete");
  return !pContext.IsCancelRequested();
}

bool DecodeRepairApplyV2::Run(DecodeStageContextV2& pContext) {
  DecodeRepairStateV2& aRepair = pContext.State().mRepair;
  std::shared_ptr<DecodeRepairApplyCursorV2>& aCursorPtr =
      pContext.State().mCursor.mRepairApply;
  if (!aCursorPtr) {
    aRepair = DecodeRepairStateV2{};
    pContext.State().mCancel = DecodeCancelStateV2{};

    const std::uint64_t aExpectedArchiveCount =
        std::max<std::uint64_t>(pContext.State().mBootstrap.mExpectedArchiveCount,
                                static_cast<std::uint64_t>(
                                    pContext.State().mDiscovery.mArchives.size()));
    if (aExpectedArchiveCount == 0u) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kRepair,
                                        ProgressStageV2::kRepairApply,
                                        "no archive boxes were available for repair"));
      return false;
    }

    ArchiveNameTemplateV2 aTemplate;
    if (!BuildArchiveNameTemplate(pContext, aTemplate)) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kRepair,
                           ProgressStageV2::kRepairApply,
                           "bootstrap archive filename did not match the archive template"));
      return false;
    }

    const std::uint64_t aTotalNonRepairBlocks =
        pContext.State().mBootstrap.mExpectedEmptyFolderBlockCount +
        pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount +
        pContext.State().mBootstrap.mExpectedArchiveDataBlockCount;
    const std::uint64_t aTotalFamilyBlocks = ComputeTotalFamilyBlocks(pContext);
    const std::uint64_t aNominalBlocksPerArchive =
        DetermineNominalBlocksPerArchive(
            pContext, aExpectedArchiveCount, aTotalFamilyBlocks);
    if (aNominalBlocksPerArchive == 0u && aTotalFamilyBlocks != 0u) {
      pContext.EmitLog(
          LogLevelV2::kError,
          LogPhaseFailedV2(LogActionV2::kRepair,
                           ProgressStageV2::kRepairApply,
                           "could not infer the per-archive block count for repair sizing"));
      return false;
    }

    aCursorPtr = std::make_shared<DecodeRepairApplyCursorV2>();
    aCursorPtr->mExpectedArchiveCount = aExpectedArchiveCount;
    aCursorPtr->mNominalBlocksPerArchive = aNominalBlocksPerArchive;
    aCursorPtr->mTotalNonRepairBlocks = aTotalNonRepairBlocks;
    aCursorPtr->mTotalFamilyBlocks = aTotalFamilyBlocks;
    aCursorPtr->mArchiveNamePrefix = aTemplate.mPrefix;
    aCursorPtr->mArchiveNameSuffix = aTemplate.mSuffix;
    aCursorPtr->mArchiveNameDigits = aTemplate.mDigits;
    const std::size_t aRepairBufferBytes =
        std::max<std::size_t>(knobs::kDefaultArchiveBlockBytesV2,
                              pContext.Layout().mArchiveBlockBytes);
    if (!aCursorPtr->mCopyBuffer.Resize(aRepairBufferBytes) ||
        !aCursorPtr->mZeroBuffer.Resize(aRepairBufferBytes) ||
        !aCursorPtr->mDecodeBuffer.Resize(pContext.Layout().mArchiveBlockBytes)) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kRepair,
                                        ProgressStageV2::kRepairApply,
                                        "could not allocate repair buffers"));
      aCursorPtr.reset();
      return false;
    }
    std::memset(aCursorPtr->mZeroBuffer.Data(), 0, aCursorPtr->mZeroBuffer.Size());

    aRepair.mArchivesTotal = aExpectedArchiveCount;
    pContext.EmitLog(
        LogLevelV2::kInfo,
        "[Repair][Repair Apply] Planning " + std::to_string(aExpectedArchiveCount) +
            " archive boxes, nominal blocks/archive=" +
            std::to_string(aNominalBlocksPerArchive) +
            ", family blocks=" + std::to_string(aTotalFamilyBlocks) + ".");
  }

  DecodeRepairApplyCursorV2& aCursor = *aCursorPtr;
  while (true) {
    if (aCursor.mStage == DecodeRepairApplyCursorV2::StageV2::kPlanArchives) {
      if (aCursor.mPlanArchiveIndex >= aCursor.mExpectedArchiveCount) {
        aCursor.mPlanningArchiveSlot = std::numeric_limits<std::size_t>::max();
        aCursor.mPlanningBlockIndex = 0u;
        aCursor.mStage = DecodeRepairApplyCursorV2::StageV2::kBeginArchive;
        continue;
      }

      if (pContext.IsCancelRequested()) {
        RequestRepairApplyFinalize(
            pContext,
            "[Repair][Repair Apply] Cancel requested before the next repair archive started.");
        return true;
      }

      if (aCursor.mPlanArchiveIndex < pContext.State().mDiscovery.mArchives.size()) {
        const std::size_t aArchiveSlot =
            static_cast<std::size_t>(aCursor.mPlanArchiveIndex);
        DiscoveredArchiveFileV2& aArchive =
            pContext.State().mDiscovery.mArchives[aArchiveSlot];
        if (aArchive.mIsPresent && !aArchive.mPath.empty() &&
            !aArchive.mHasReadableSection &&
            aArchive.mReadableBlockCount > 0u) {
          if (aCursor.mPlanningArchiveSlot != aArchiveSlot) {
            aCursor.mPlanningArchiveSlot = aArchiveSlot;
            aCursor.mPlanningBlockIndex = 0u;
          }
          if (aCursor.mPlanningBlockIndex < aArchive.mReadableBlockCount) {
            SectionHeaderV2 aSectionHeader;
            if (TryReadValidatedInspectionHeader(
                    pContext, aArchive, aCursor.mPlanningBlockIndex, aSectionHeader)) {
              AdoptRepairPlanningSectionTruth(aArchive, aSectionHeader);
              aCursor.mPlanningArchiveSlot = std::numeric_limits<std::size_t>::max();
              aCursor.mPlanningBlockIndex = 0u;
            } else {
              ++aCursor.mPlanningBlockIndex;
              if (aCursor.mPlanningBlockIndex >= aArchive.mReadableBlockCount) {
                aCursor.mPlanningArchiveSlot = std::numeric_limits<std::size_t>::max();
                aCursor.mPlanningBlockIndex = 0u;
              }
            }
            EmitRepairPlanningProgress(
                pContext, aCursor, "Planning repair archive sections");
            pContext.ContinuePhaseOnNextHeartbeat();
            return true;
          }
        } else {
          aCursor.mPlanningArchiveSlot = std::numeric_limits<std::size_t>::max();
          aCursor.mPlanningBlockIndex = 0u;
        }
      }

      DecodeRepairApplyCursorV2::ArchivePlanV2 aPlan;
      std::string aPlanError;
      if (!BuildRepairArchivePlan(
              pContext, aCursor, aCursor.mPlanArchiveIndex, aPlan, aPlanError)) {
        pContext.EmitLog(LogLevelV2::kError,
                         LogPhaseFailedV2(LogActionV2::kRepair,
                                          ProgressStageV2::kRepairApply,
                                          aPlanError));
        aCursorPtr.reset();
        return false;
      }

      aRepair.mRepairableBlocks += aPlan.mRepairableBlocks;
      aRepair.mRepairableBytes += aPlan.mRepairableBytes;
      aCursor.mPlans.push_back(std::move(aPlan));
      ++aCursor.mPlanArchiveIndex;
      EmitRepairPlanningProgress(
          pContext, aCursor, "Planning repair archive family");
      if (aCursor.mPlanArchiveIndex < aCursor.mExpectedArchiveCount) {
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }
      continue;
    }

    if (aCursor.mStage == DecodeRepairApplyCursorV2::StageV2::kBeginArchive) {
      if (aCursor.mCurrentPlanIndex >= aCursor.mPlans.size()) {
        aCursor.mRepairSourcePlanIndex = 0u;
        aCursor.mRepairSourceBlockIndex = 0u;
        aCursor.mRead.reset();
        aCursor.mStage = DecodeRepairApplyCursorV2::StageV2::kApplyRepairBlocks;
        continue;
      }

      const DecodeRepairApplyCursorV2::ArchivePlanV2& aPlan =
          aCursor.mPlans[aCursor.mCurrentPlanIndex];
      if (pContext.IsCancelRequested()) {
        RequestRepairApplyFinalize(
            pContext,
            "[Repair][Repair Apply] Cancel requested at archive boundary before " +
                aPlan.mOutputName + " started.");
        return true;
      }

      aCursor.mBytesCopied = 0u;
      aCursor.mZeroBytesRemaining = 0u;
      aCursor.mSizeBeforeResize = 0u;
      aCursor.mResizeEventPending = false;
      aCursor.mStopAfterCurrentArchive = false;
      aCursor.mCancelLoggedForCurrentArchive = false;
      aCursor.mRead.reset();
      aCursor.mWrite.reset();

      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairArchiveStarted)) {
        EmitRepairArchiveEvent(pContext,
                               RuntimeEventKindV2::kRepairArchiveStarted,
                               aPlan.mOutputPath,
                               aPlan.mArchiveIndex,
                               aPlan.mExpectedBlocks,
                               aPlan.mExpectedFileBytes);
      }
      aCursor.mStage = aPlan.mHasSourceFile
                           ? DecodeRepairApplyCursorV2::StageV2::kCopySource
                           : DecodeRepairApplyCursorV2::StageV2::kWriteHeader;
      EmitRepairApplyProgress(pContext, aCursor);
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    if (aCursor.mStage != DecodeRepairApplyCursorV2::StageV2::kApplyRepairBlocks &&
        aCursor.mCurrentPlanIndex >= aCursor.mPlans.size()) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kRepair,
                                        ProgressStageV2::kRepairApply,
                                        "repair cursor plan index moved out of range"));
      aCursorPtr.reset();
      return false;
    }
    const DecodeRepairApplyCursorV2::ArchivePlanV2 aEmptyPlan{};
    const DecodeRepairApplyCursorV2::ArchivePlanV2& aPlan =
        aCursor.mCurrentPlanIndex < aCursor.mPlans.size()
            ? aCursor.mPlans[aCursor.mCurrentPlanIndex]
            : aEmptyPlan;

    if (aCursor.mStage == DecodeRepairApplyCursorV2::StageV2::kCopySource) {
      if (aCursor.mRead == nullptr || aCursor.mWrite == nullptr) {
        aCursor.mRead = pContext.FileSystem().OpenReadStream(aPlan.mSourcePath);
        aCursor.mWrite = pContext.FileSystem().OpenWriteStream(aPlan.mOutputPath);
        if (aCursor.mRead == nullptr || aCursor.mWrite == nullptr ||
            !aCursor.mRead->IsReady() || !aCursor.mWrite->IsReady()) {
          pContext.EmitLog(
              LogLevelV2::kError,
              LogPhaseFailedV2(LogActionV2::kRepair,
                               ProgressStageV2::kRepairApply,
                               "failed opening repair archive streams for " +
                                   aPlan.mOutputName));
          aCursorPtr.reset();
          return false;
        }
      }

      if (aCursor.mBytesCopied < aPlan.mSourceFileBytes) {
        const std::size_t aChunkBytes = static_cast<std::size_t>(
            std::min<std::uint64_t>(aCursor.mCopyBuffer.Size(),
                                    aPlan.mSourceFileBytes - aCursor.mBytesCopied));
        if (!aCursor.mRead->Read(static_cast<std::size_t>(aCursor.mBytesCopied),
                                 aCursor.mCopyBuffer.Data(),
                                 aChunkBytes)) {
          pContext.EmitLog(LogLevelV2::kError,
                           LogPhaseFailedV2(LogActionV2::kRepair,
                                            ProgressStageV2::kRepairApply,
                                            "failed reading source archive bytes for " +
                                                aPlan.mOutputName));
          aCursorPtr.reset();
          return false;
        }
        if (!aCursor.mWrite->Write(aCursor.mCopyBuffer.Data(), aChunkBytes)) {
          pContext.EmitLog(
              LogLevelV2::kError,
              LogPhaseFailedV2(LogActionV2::kRepair,
                               ProgressStageV2::kRepairApply,
                               "failed writing destination archive bytes: " +
                                   aCursor.mWrite->LastErrorMessage() + " for " +
                                   aPlan.mOutputName));
          aCursorPtr.reset();
          return false;
        }
        aCursor.mBytesCopied += static_cast<std::uint64_t>(aChunkBytes);
        ObserveRepairApplyCancel(pContext, aCursor, aPlan);
        EmitRepairApplyProgress(pContext, aCursor);
        if (aCursor.mBytesCopied < aPlan.mSourceFileBytes) {
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
      }

      aCursor.mStage = DecodeRepairApplyCursorV2::StageV2::kCloseCopiedFile;
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    if (aCursor.mStage == DecodeRepairApplyCursorV2::StageV2::kCloseCopiedFile) {
      if (aCursor.mWrite != nullptr && !aCursor.mWrite->Close()) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(LogActionV2::kRepair,
                             ProgressStageV2::kRepairApply,
                             "failed closing destination archive: " +
                                 aCursor.mWrite->LastErrorMessage() + " for " +
                                 aPlan.mOutputName));
        aCursorPtr.reset();
        return false;
      }
      aCursor.mRead.reset();
      aCursor.mWrite.reset();
      aCursor.mStage = DecodeRepairApplyCursorV2::StageV2::kWriteHeader;
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    if (aCursor.mStage == DecodeRepairApplyCursorV2::StageV2::kWriteHeader) {
      std::string aHeaderError;
      if (!BuildSyntheticArchiveHeader(
              pContext,
              aPlan.mArchiveIndex,
              aCursor.mSyntheticHeader,
              aHeaderError)) {
        pContext.EmitLog(LogLevelV2::kError,
                         LogPhaseFailedV2(LogActionV2::kRepair,
                                          ProgressStageV2::kRepairApply,
                                          aHeaderError));
        aCursorPtr.reset();
        return false;
      }

      if (aPlan.mNeedsSyntheticHeader) {
        if (!WriteOrPatchArchiveHeader(pContext.FileSystem(),
                                       aPlan.mOutputPath,
                                       aCursor.mSyntheticHeader,
                                       !aPlan.mHasSourceFile,
                                       aHeaderError)) {
          pContext.EmitLog(LogLevelV2::kError,
                           LogPhaseFailedV2(LogActionV2::kRepair,
                                            ProgressStageV2::kRepairApply,
                                            aHeaderError + " for " +
                                                aPlan.mOutputName));
          aCursorPtr.reset();
          return false;
        }
        if (pContext.WantsRuntimeEvent(
                RuntimeEventKindV2::kRepairArchiveHeaderWritten)) {
          EmitRepairArchiveHeaderEvent(pContext,
                                       aPlan.mOutputPath,
                                       aPlan.mArchiveIndex,
                                       !aPlan.mHasSourceFile,
                                       aCursor.mSyntheticHeader);
        }
        if (!aPlan.mHasSourceFile) {
          aCursor.mBytesCopied = static_cast<std::uint64_t>(kArchiveHeaderBytesV2);
          if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairFileCreated)) {
            EmitRepairFileEvent(pContext,
                                RuntimeEventKindV2::kRepairFileCreated,
                                aPlan.mOutputPath,
                                aPlan.mArchiveIndex,
                                0u,
                                aCursor.mBytesCopied);
          }
        } else {
          aCursor.mBytesCopied =
              std::max<std::uint64_t>(aCursor.mBytesCopied,
                                      static_cast<std::uint64_t>(kArchiveHeaderBytesV2));
        }
      }

      aCursor.mSizeBeforeResize = aCursor.mBytesCopied;
      aCursor.mZeroBytesRemaining =
          aPlan.mExpectedFileBytes > aCursor.mBytesCopied
              ? (aPlan.mExpectedFileBytes - aCursor.mBytesCopied)
              : 0u;
      aCursor.mResizeEventPending = aCursor.mZeroBytesRemaining > 0u;
      ObserveRepairApplyCancel(pContext, aCursor, aPlan);
      EmitRepairApplyProgress(pContext, aCursor);
      aCursor.mStage = aCursor.mZeroBytesRemaining > 0u
                           ? DecodeRepairApplyCursorV2::StageV2::kZeroFill
                           : DecodeRepairApplyCursorV2::StageV2::kFinishArchive;
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    if (aCursor.mStage == DecodeRepairApplyCursorV2::StageV2::kZeroFill) {
      if (aCursor.mZeroBytesRemaining > 0u) {
        const std::size_t aChunkBytes = static_cast<std::size_t>(
            std::min<std::uint64_t>(aCursor.mZeroBuffer.Size(),
                                    aCursor.mZeroBytesRemaining));
        if (!pContext.FileSystem().AppendFile(
                aPlan.mOutputPath, aCursor.mZeroBuffer.Data(), aChunkBytes)) {
          pContext.EmitLog(LogLevelV2::kError,
                           LogPhaseFailedV2(LogActionV2::kRepair,
                                            ProgressStageV2::kRepairApply,
                                            "failed appending zero bytes for " +
                                                aPlan.mOutputName));
          aCursorPtr.reset();
          return false;
        }
        aCursor.mZeroBytesRemaining -= static_cast<std::uint64_t>(aChunkBytes);
        aCursor.mBytesCopied += static_cast<std::uint64_t>(aChunkBytes);
        ObserveRepairApplyCancel(pContext, aCursor, aPlan);
        EmitRepairApplyProgress(pContext, aCursor);
        if (aCursor.mZeroBytesRemaining > 0u) {
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
      }

      if (aCursor.mResizeEventPending &&
          pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairFileResized)) {
        EmitRepairFileEvent(pContext,
                            RuntimeEventKindV2::kRepairFileResized,
                            aPlan.mOutputPath,
                            aPlan.mArchiveIndex,
                            aCursor.mSizeBeforeResize,
                            aCursor.mBytesCopied);
      }
      aCursor.mResizeEventPending = false;
      aCursor.mStage = DecodeRepairApplyCursorV2::StageV2::kFinishArchive;
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    if (aCursor.mStage == DecodeRepairApplyCursorV2::StageV2::kFinishArchive) {
      aRepair.mPatchedBlocks += aPlan.mRepairableBlocks;
      aRepair.mPatchedBytes += aPlan.mRepairableBytes;
      if (!aPlan.mHasSourceFile) {
        ++aRepair.mArchivesSynthesized;
      } else if (aPlan.mRepairableBlocks > 0u || aPlan.mRepairableBytes > 0u) {
        ++aRepair.mArchivesExpanded;
      }
      ++aRepair.mArchivesCompleted;

      ++pContext.State().mOutput.mFilesWritten;
      pContext.State().mOutput.mBytesWritten += aCursor.mBytesCopied;
      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairArchiveFinished)) {
        EmitRepairArchiveEvent(pContext,
                               RuntimeEventKindV2::kRepairArchiveFinished,
                               aPlan.mOutputPath,
                               aPlan.mArchiveIndex,
                               aPlan.mExpectedBlocks,
                               aCursor.mBytesCopied);
      }

      if (!aPlan.mHasSourceFile) {
        pContext.EmitLog(
            LogLevelV2::kWarning,
            "[Repair][Repair Apply] Synthesized missing archive " + aPlan.mOutputName +
                " with " + std::to_string(aPlan.mExpectedBlocks) + " blocks (" +
                FormatHumanBytesV2(aPlan.mRepairableBytes) + ").");
      } else if (aPlan.mRepairableBlocks > 0u || aPlan.mRepairableBytes > 0u) {
        pContext.EmitLog(
            LogLevelV2::kWarning,
            "[Repair][Repair Apply] Expanded " + aPlan.mOutputName +
                " to exact size " + FormatHumanBytesV2(aPlan.mExpectedFileBytes) +
                " by patching " + std::to_string(aPlan.mRepairableBlocks) +
                " blocks (" + FormatHumanBytesV2(aPlan.mRepairableBytes) + ").");
      } else if (aPlan.mNeedsSyntheticHeader) {
        pContext.EmitLog(LogLevelV2::kInfo,
                         "[Repair][Repair Apply] Rebuilt archive header for " +
                             aPlan.mOutputName + ".");
      }

      pContext.EmitPhaseProgress(
          static_cast<double>(aRepair.mArchivesCompleted) /
              static_cast<double>(std::max<std::uint64_t>(1u, aRepair.mArchivesTotal)),
          BuildRepairProgressLabel(aRepair));

      ++aCursor.mCurrentPlanIndex;
      if (aCursor.mStopAfterCurrentArchive || pContext.IsCancelRequested()) {
        RequestRepairApplyFinalize(
            pContext,
            "[Repair][Repair Apply] Stopping after archive boundary at " +
                aPlan.mOutputName + ".");
        return true;
      }

      if (aCursor.mCurrentPlanIndex < aCursor.mPlans.size()) {
        aCursor.mStage = DecodeRepairApplyCursorV2::StageV2::kBeginArchive;
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }

      aCursor.mRepairSourcePlanIndex = 0u;
      aCursor.mRepairSourceBlockIndex = 0u;
      aCursor.mRead.reset();
      aCursor.mStage = DecodeRepairApplyCursorV2::StageV2::kApplyRepairBlocks;
      continue;
    }

    if (aCursor.mStage == DecodeRepairApplyCursorV2::StageV2::kApplyRepairBlocks) {
      if (pContext.IsCancelRequested()) {
        RequestRepairApplyFinalize(
            pContext,
            "[Repair][Repair Apply] Cancel requested during repair block application.");
        return true;
      }

      if (aCursor.mRepairSourcePlanIndex >= aCursor.mPlans.size()) {
        return FinalizeRepairApplySuccess(pContext);
      }

      const DecodeRepairApplyCursorV2::ArchivePlanV2& aSourcePlan =
          aCursor.mPlans[aCursor.mRepairSourcePlanIndex];
      if (aCursor.mRepairSourceBlockIndex >= aSourcePlan.mExpectedRepairBlocks) {
        ++aCursor.mRepairSourcePlanIndex;
        aCursor.mRepairSourceBlockIndex = 0u;
        aCursor.mRead.reset();
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }

      const std::uint64_t aSourceRepairOrdinal = aCursor.mRepairSourceBlockIndex;
      const std::uint64_t aSourceRepairBlockIndex =
          aSourcePlan.mNonRepairBlocks + aSourceRepairOrdinal;
      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairBlockStarted)) {
        EmitRepairBlockEvent(pContext,
                             RuntimeEventKindV2::kRepairBlockStarted,
                             aSourcePlan.mOutputPath,
                             aSourcePlan.mArchiveIndex,
                             aSourceRepairBlockIndex);
      }

      const bool aSourceRepairReadable =
          aSourcePlan.mHasSourceFile &&
          aSourceRepairOrdinal < aSourcePlan.mReadableRepairBlocks;
      if (!aSourceRepairReadable) {
        if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairBlockUnmatched)) {
          EmitRepairBlockEvent(pContext,
                               RuntimeEventKindV2::kRepairBlockUnmatched,
                               aSourcePlan.mOutputPath,
                               aSourcePlan.mArchiveIndex,
                               aSourceRepairBlockIndex);
        }
        if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairBlockFinished)) {
          EmitRepairBlockEvent(pContext,
                               RuntimeEventKindV2::kRepairBlockFinished,
                               aSourcePlan.mOutputPath,
                               aSourcePlan.mArchiveIndex,
                               aSourceRepairBlockIndex);
        }
        ++aCursor.mRepairSourceBlockIndex;
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }

      if (aCursor.mRead == nullptr) {
        aCursor.mRead = pContext.FileSystem().OpenReadStream(aSourcePlan.mSourcePath);
        if (aCursor.mRead == nullptr || !aCursor.mRead->IsReady()) {
          pContext.EmitLog(
              LogLevelV2::kError,
              LogPhaseFailedV2(LogActionV2::kRepair,
                               ProgressStageV2::kRepairApply,
                               "failed opening repair source archive for repair-block reads: " +
                                   aSourcePlan.mOutputName));
          aCursorPtr.reset();
          return false;
        }
      }

      std::string aReadError;
      if (!ReadArchiveBlockRaw(*aCursor.mRead,
                               aSourceRepairBlockIndex,
                               pContext.Layout().mArchiveBlockBytes,
                               aCursor.mCopyBuffer.Data(),
                               aReadError)) {
        pContext.EmitLog(
            LogLevelV2::kWarning,
            "[Repair][Repair Apply] Repair block was missing/unreadable at archive " +
                std::to_string(aSourcePlan.mArchiveIndex) + ", block " +
                std::to_string(aSourceRepairBlockIndex) + " (" + aReadError +
                "); skipping this repair block.");
        if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairBlockUnmatched)) {
          EmitRepairBlockEvent(pContext,
                               RuntimeEventKindV2::kRepairBlockUnmatched,
                               aSourcePlan.mOutputPath,
                               aSourcePlan.mArchiveIndex,
                               aSourceRepairBlockIndex);
        }
        if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairBlockFinished)) {
          EmitRepairBlockEvent(pContext,
                               RuntimeEventKindV2::kRepairBlockFinished,
                               aSourcePlan.mOutputPath,
                               aSourcePlan.mArchiveIndex,
                               aSourceRepairBlockIndex);
        }
        ++aCursor.mRepairSourceBlockIndex;
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }

      SectionHeaderV2 aRepairHeader;
      std::string aRepairValidationError;
      if (!DecodeValidatedSectionHeaderFromRawBlock(pContext,
                                                    aCursor.mCopyBuffer.Data(),
                                                    aCursor.mDecodeBuffer,
                                                    aRepairHeader,
                                                    aRepairValidationError)) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(
                LogActionV2::kRepair,
                ProgressStageV2::kRepairApply,
                "block_bad_checksum: repair block failed validation at archive " +
                    std::to_string(aSourcePlan.mArchiveIndex) + ", block " +
                    std::to_string(aSourceRepairBlockIndex) + " (" +
                    aRepairValidationError + ")"));
        aCursorPtr.reset();
        return false;
      }

      const std::uint64_t aExpectedFamilyId =
          pContext.State().mBootstrap.mFirstHeader.mArchiveFamilyId;
      if (aRepairHeader.mArchiveFamilyId != aExpectedFamilyId) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(
                LogActionV2::kRepair,
                ProgressStageV2::kRepairApply,
                "repair block family id mismatch at archive " +
                    std::to_string(aSourcePlan.mArchiveIndex) + ", block " +
                    std::to_string(aSourceRepairBlockIndex) + ": expected " +
                    std::to_string(aExpectedFamilyId) + ", observed " +
                    std::to_string(aRepairHeader.mArchiveFamilyId)));
        aCursorPtr.reset();
        return false;
      }

      const std::uint64_t aTargetArchiveIndex =
          static_cast<std::uint64_t>(aRepairHeader.mRepairRecord.mRepairPointerArchive);
      const std::uint64_t aTargetBlockIndex =
          static_cast<std::uint64_t>(aRepairHeader.mRepairRecord.mRepairPointerBlock);
      if (aTargetArchiveIndex >= aCursor.mPlans.size()) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(
                LogActionV2::kRepair,
                ProgressStageV2::kRepairApply,
                "repair block pointer archive index out of range at archive " +
                    std::to_string(aSourcePlan.mArchiveIndex) + ", block " +
                    std::to_string(aSourceRepairBlockIndex) + ": pointer archive=" +
                    std::to_string(aTargetArchiveIndex)));
        aCursorPtr.reset();
        return false;
      }

      const DecodeRepairApplyCursorV2::ArchivePlanV2& aTargetPlan =
          aCursor.mPlans[static_cast<std::size_t>(aTargetArchiveIndex)];
      if (aTargetBlockIndex >= aTargetPlan.mExpectedBlocks) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(
                LogActionV2::kRepair,
                ProgressStageV2::kRepairApply,
                "repair block pointer block index out of range at archive " +
                    std::to_string(aSourcePlan.mArchiveIndex) + ", block " +
                    std::to_string(aSourceRepairBlockIndex) + ": pointer block=" +
                    std::to_string(aTargetBlockIndex) + ", target blocks=" +
                    std::to_string(aTargetPlan.mExpectedBlocks)));
        aCursorPtr.reset();
        return false;
      }

      std::string aPatchBuildError;
      if (!BuildPatchedRawTargetBlockFromRepair(pContext,
                                                aCursor.mCopyBuffer.Data(),
                                                aCursor.mDecodeBuffer,
                                                aRepairHeader,
                                                aTargetArchiveIndex,
                                                aTargetBlockIndex,
                                                aCursor.mZeroBuffer,
                                                aPatchBuildError)) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(
                LogActionV2::kRepair,
                ProgressStageV2::kRepairApply,
                "failed preparing repaired target block at archive " +
                    std::to_string(aTargetArchiveIndex) + ", block " +
                    std::to_string(aTargetBlockIndex) + ": " + aPatchBuildError));
        aCursorPtr.reset();
        return false;
      }

      bool aSkipOverwriteForValidTarget = false;
      if (!pContext.Request().mAggressive) {
        std::string aTargetReadError;
        bool aTargetIsValid = false;
        if (!CheckIfTargetBlockAlreadyValidForRepair(
                pContext,
                aTargetPlan.mOutputPath,
                aTargetBlockIndex,
                aCursor.mCopyBuffer,
                aCursor.mDecodeBuffer,
                aTargetIsValid,
                aTargetReadError)) {
          pContext.EmitLog(
              LogLevelV2::kError,
              LogPhaseFailedV2(
                  LogActionV2::kRepair,
                  ProgressStageV2::kRepairApply,
                  "failed reading target block before replacement at archive " +
                      std::to_string(aTargetArchiveIndex) + ", block " +
                      std::to_string(aTargetBlockIndex) + ": " + aTargetReadError));
          aCursorPtr.reset();
          return false;
        }
        aSkipOverwriteForValidTarget = aTargetIsValid;
      }

      if (aSkipOverwriteForValidTarget) {
        ++aRepair.mRepairBlocksSkippedValidTarget;
      } else {
        const std::size_t aTargetOffset = static_cast<std::size_t>(
            kArchiveHeaderBytesV2 +
            (aTargetBlockIndex *
             static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes)));
        if (!pContext.FileSystem().OverwriteFileRegion(aTargetPlan.mOutputPath,
                                                       aTargetOffset,
                                                       aCursor.mZeroBuffer.Data(),
                                                       pContext.Layout().mArchiveBlockBytes)) {
          pContext.EmitLog(
              LogLevelV2::kError,
              LogPhaseFailedV2(LogActionV2::kRepair,
                               ProgressStageV2::kRepairApply,
                               "failed replacing target block at archive " +
                                   std::to_string(aTargetArchiveIndex) + ", block " +
                                   std::to_string(aTargetBlockIndex)));
          aCursorPtr.reset();
          return false;
        }
        ++aRepair.mRepairBlocksApplied;
      }

      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairBlockMatched)) {
        EmitRepairBlockEvent(pContext,
                             RuntimeEventKindV2::kRepairBlockMatched,
                             aSourcePlan.mOutputPath,
                             aSourcePlan.mArchiveIndex,
                             aSourceRepairBlockIndex);
      }
      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kRepairBlockFinished)) {
        EmitRepairBlockEvent(pContext,
                             RuntimeEventKindV2::kRepairBlockFinished,
                             aSourcePlan.mOutputPath,
                             aSourcePlan.mArchiveIndex,
                             aSourceRepairBlockIndex);
      }

      ++aCursor.mRepairSourceBlockIndex;
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }
  }
}

}  // namespace peanutbutter
