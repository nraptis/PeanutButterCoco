#include "Bundle_FinalizingHeaders.hpp"

#include <array>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

class BundleFinalizingHeadersCursorV2 {
 public:
  static constexpr std::uint64_t kArchiveLogInterval = 64u;

  std::size_t mArchiveIndex = 0u;
  std::uint8_t mDirtyState = 0u;
  std::uint64_t mNextArchiveLog = kArchiveLogInterval;
  std::array<unsigned char, memory_layout::kArchiveHeaderBytesV2> mHeaderBytes{};
};

namespace {

bool BuildArchiveHeader(const BundleStageContextV2& pContext,
                        std::uint64_t pArchiveIndex,
                        std::uint8_t pDirtyState,
                        memory_layout::ArchiveHeaderV2& pOutHeader) {
  using namespace memory_layout;

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

void EmitBundleArchiveHeaderFinalizedEvent(
    BundleStageContextV2& pContext,
    const PlannedArchiveFileV2& pArchive,
    const memory_layout::ArchiveHeaderV2& pHeader) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kBundleArchiveHeaderFinalized;
  aEvent.mStage = ProgressStageV2::kFinalizingHeaders;
  aEvent.mLabel =
      "Bundle finalized archive header for archive " +
      std::to_string(pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("archive_count",
                 memory_layout::PackedUint48ToUInt64(pHeader.mArchiveCount));
  aEvent.SetInfo("archive_family_id", pHeader.mArchiveFamilyId);
  aEvent.SetInfo("dirty_state", static_cast<std::uint64_t>(pHeader.mDirtyState));
  aEvent.SetInfo("is_encrypted", pHeader.mIsEncrypted != 0u);
  pContext.EmitRuntimeEvent(aEvent);
}

std::string FinalizingHeadersLabel(std::size_t pCompleted,
                                   std::size_t pTotal) {
  if (pTotal == 0u) {
    return "Headers finalized";
  }
  return "Finalized archive headers " + std::to_string(pCompleted) + " / " +
         std::to_string(pTotal);
}

std::string BuildFinalizingHeadersSummary(std::size_t pCompleted,
                                          std::size_t pTotal) {
  return std::to_string(pCompleted) + " of " + std::to_string(pTotal) +
         " archives";
}

void EmitFinalizingHeadersProgress(BundleStageContextV2& pContext,
                                   std::size_t pCompleted,
                                   std::size_t pTotal) {
  const double aFraction =
      pTotal == 0u ? 1.0
                   : static_cast<double>(pCompleted) /
                         static_cast<double>(pTotal);
  pContext.EmitPhaseProgress(aFraction,
                             FinalizingHeadersLabel(pCompleted, pTotal));
}

void EmitFinalizingHeadersSliceLog(BundleStageContextV2& pContext,
                                   BundleFinalizingHeadersCursorV2& pCursor,
                                   std::size_t pTotal) {
  const std::uint64_t aCompleted =
      static_cast<std::uint64_t>(pCursor.mArchiveIndex);
  while (aCompleted >= pCursor.mNextArchiveLog) {
    pContext.EmitLog(LogLevelV2::kInfo,
                     "[Bundle][Finalizing Headers] " +
                         BuildFinalizingHeadersSummary(
                             static_cast<std::size_t>(aCompleted), pTotal) +
                         ".");
    pCursor.mNextArchiveLog += BundleFinalizingHeadersCursorV2::kArchiveLogInterval;
  }
}

bool FailFinalizingHeaders(BundleStageContextV2& pContext,
                           const std::string& pReason) {
  pContext.State().mCursor.mFinalizingHeaders.reset();
  pContext.EmitLog(LogLevelV2::kError,
                   LogPhaseFailedV2(LogActionV2::kBundle,
                                    ProgressStageV2::kFinalizingHeaders,
                                    pReason));
  return false;
}

bool CompleteFinalizingHeaders(BundleStageContextV2& pContext,
                               std::size_t pArchiveCountToFinalize) {
  pContext.State().mFinalize.mHeadersFinalized = true;
  pContext.State().mCursor.mFinalizingHeaders.reset();
  pContext.EmitLog(LogLevelV2::kInfo,
                   "[Bundle][Finalizing Headers] END. " +
                       BuildFinalizingHeadersSummary(pArchiveCountToFinalize,
                                                     pArchiveCountToFinalize) +
                       ".");
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kFinalizingHeaders));
  EmitFinalizingHeadersProgress(
      pContext, pArchiveCountToFinalize, pArchiveCountToFinalize);
  return true;
}

}  // namespace

bool BundleFinalizingHeadersV2::Run(BundleStageContextV2& pContext) {
  using namespace memory_layout;

  const std::size_t aArchiveCountToFinalize =
      std::min(pContext.State().mPacking.mArchivePaths.size(),
               pContext.State().mMemoryPlan.mArchives.size());
  std::shared_ptr<BundleFinalizingHeadersCursorV2>& aCursorPtr =
      pContext.State().mCursor.mFinalizingHeaders;
  if (!aCursorPtr) {
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseStartedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kFinalizingHeaders));
    aCursorPtr = std::make_shared<BundleFinalizingHeadersCursorV2>();
    aCursorPtr->mDirtyState = static_cast<std::uint8_t>(
        pContext.State().mCancel.mShouldFinalizeAfterCancel
            ? ArchiveDirtyStateV2::kFinishedWithCancel
            : ArchiveDirtyStateV2::kFinished);
    pContext.EmitLog(LogLevelV2::kInfo,
                     "[Bundle][Finalizing Headers] " +
                         BuildFinalizingHeadersSummary(0u,
                                                       aArchiveCountToFinalize) +
                         ".");
    if (aArchiveCountToFinalize > 0u) {
      EmitFinalizingHeadersProgress(pContext, 0u, aArchiveCountToFinalize);
    }
  }

  BundleFinalizingHeadersCursorV2& aCursor = *aCursorPtr;
  if (aCursor.mArchiveIndex >= aArchiveCountToFinalize) {
    return CompleteFinalizingHeaders(pContext, aArchiveCountToFinalize);
  }

  const PlannedArchiveFileV2& aArchive =
      pContext.State().mMemoryPlan.mArchives[aCursor.mArchiveIndex];
  ArchiveHeaderV2 aHeader;
  if (!BuildArchiveHeader(
          pContext,
          aArchive.mArchiveIndex,
          aCursor.mDirtyState,
          aHeader)) {
    return FailFinalizingHeaders(pContext,
                                 "archive header values were out of range");
  }
  if (!WriteArchiveHeader(
          aHeader,
          aCursor.mHeaderBytes.data(),
          aCursor.mHeaderBytes.size(),
          nullptr)) {
    return FailFinalizingHeaders(pContext,
                                 "archive header bytes could not be written");
  }
  if (!pContext.FileSystem().OverwriteFileRegion(aArchive.mPath,
                                                 0u,
                                                 aCursor.mHeaderBytes.data(),
                                                 aCursor.mHeaderBytes.size())) {
    return FailFinalizingHeaders(pContext, "archive header patch write failed");
  }
  if (pContext.WantsRuntimeEvent(
          RuntimeEventKindV2::kBundleArchiveHeaderFinalized)) {
    EmitBundleArchiveHeaderFinalizedEvent(pContext, aArchive, aHeader);
  }

  ++aCursor.mArchiveIndex;
  EmitFinalizingHeadersSliceLog(pContext, aCursor, aArchiveCountToFinalize);
  EmitFinalizingHeadersProgress(
      pContext, aCursor.mArchiveIndex, aArchiveCountToFinalize);
  if (aCursor.mArchiveIndex < aArchiveCountToFinalize) {
    pContext.ContinuePhaseOnNextHeartbeat();
    return true;
  }

  return CompleteFinalizingHeaders(pContext, aArchiveCountToFinalize);
}

}  // namespace peanutbutter
