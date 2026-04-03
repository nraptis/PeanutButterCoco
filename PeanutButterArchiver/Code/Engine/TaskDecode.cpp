#include "TaskCommon.hpp"

#include <algorithm>

#include "../Knobs.hpp"
#include "../Common/LogCatalog.hpp"
#include "Decode/Decode_Workflow.hpp"

namespace peanutbutter {

DecodeTaskV2::DecodeTaskV2(const DecodeRequestV2& pRequest,
                           DecodeRuntimeV2* pRuntime,
                           FileSystemV2* pFileSystem,
                           const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mContext(pRequest, pRuntime, pFileSystem, pLayout) {
  BuildPhaseList();
}

TaskDispositionV2 DecodeTaskV2::Heartbeat() {
  if (mDisposition != TaskDispositionV2::kRunning) {
    return mDisposition;
  }

  if (mContext.IsCancelRequested() && !ShouldDeferCancelForCurrentPhase()) {
    mDisposition = TaskDispositionV2::kCanceled;
    return mDisposition;
  }

  std::size_t aSliceCount = 0u;
  std::size_t aSliceBudget = 1u;
  if (mCurrentPhaseIndex < mPhases.mCount &&
      mPhases.mEntries[mCurrentPhaseIndex].mStage ==
          ProgressStageV2::kArchiveDecode) {
    if (mContext.Request().mIntent == DecodeIntentV2::kRecover) {
      aSliceBudget = std::max<std::size_t>(
          1u, static_cast<std::size_t>(knobs::kBatchSizeRepairV2));
    } else {
      aSliceBudget = std::max<std::size_t>(
          1u, static_cast<std::size_t>(knobs::kBatchSizeDecodeV2));
    }
  }

  while (true) {
    const bool aSucceeded = RunCurrentPhase();
    if (!aSucceeded) {
      if (mContext.IsCancelRequested() && !ShouldDeferCancelForCurrentPhase()) {
        mDisposition = TaskDispositionV2::kCanceled;
        return mDisposition;
      }
      MarkFailed();
      return mDisposition;
    }

    ++aSliceCount;
    if (!mContext.ActivePhaseNeedsMoreHeartbeats()) {
      break;
    }
    if (mContext.ActivePhaseBatchYieldRequested()) {
      return mDisposition;
    }
    if (aSliceCount >= aSliceBudget) {
      return mDisposition;
    }
    if (mContext.IsCancelRequested() && !ShouldDeferCancelForCurrentPhase()) {
      mDisposition = TaskDispositionV2::kCanceled;
      return mDisposition;
    }
  }

  if (mContext.IsCancelRequested()) {
    const ProgressStageV2 aStage = mPhases.mEntries[mCurrentPhaseIndex].mStage;
    if (mContext.State().mCancel.mShouldFinalizeAfterCancel &&
        aStage != ProgressStageV2::kFinalize) {
      mCurrentPhaseIndex = FindPhaseIndex(ProgressStageV2::kFinalize);
      return mDisposition;
    }
    mDisposition = TaskDispositionV2::kCanceled;
    return mDisposition;
  }

  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    mDisposition = TaskDispositionV2::kCompleted;
  }
  return mDisposition;
}

TaskDispositionV2 DecodeTaskV2::Disposition() const {
  return mDisposition;
}

const std::string& DecodeTaskV2::FailureMessage() const {
  return mFailureMessage;
}

EngineTaskTerminalSnapshotV2 DecodeTaskV2::BuildTerminalSnapshot() const {
  EngineTaskTerminalSnapshotV2 aSnapshot;
  const DecodeWorkStateV2& aState = State();
  aSnapshot.mDecodeState = std::make_shared<DecodeWorkStateV2>(aState);
  aSnapshot.mFailureMessage = FailureMessage();
  aSnapshot.mFailure = aState.mFailure;
  return aSnapshot;
}

const DecodeWorkStateV2& DecodeTaskV2::State() const {
  return mContext.State();
}

void DecodeTaskV2::BuildPhaseList() {
  mPhases = decode_workflow::BuildDecodePhaseListV2(
      decode_workflow::DecodePhasePlanV2::kDecode);
}

bool DecodeTaskV2::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }

  return decode_workflow::RunDecodePhaseV2(
      mContext,
      mPhases.mEntries[mCurrentPhaseIndex],
      mCurrentPhaseIndex,
      mPhases.mCount,
      LogActionFromDecodeIntentV2(mContext.Request().mIntent));
}

bool DecodeTaskV2::ShouldDeferCancelForCurrentPhase() const {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }

  return decode_workflow::ShouldDeferDecodeCancelForPhaseV2(
      mContext.State(),
      mPhases.mEntries[mCurrentPhaseIndex].mStage,
      ProgressStageV2::kArchiveDecode);
}

std::size_t DecodeTaskV2::FindPhaseIndex(ProgressStageV2 pStage) const {
  return decode_workflow::FindDecodePhaseIndexV2(mPhases, pStage);
}

void DecodeTaskV2::MarkFailed() {
  mDisposition = TaskDispositionV2::kFailed;
  mFailureMessage = mContext.LastErrorLog();
  if (mFailureMessage.empty()) {
    mFailureMessage = "decode failed without explicit failure detail";
  }
}

ManifestTaskV2::ManifestTaskV2(const DecodeRequestV2& pRequest,
                               DecodeRuntimeV2* pRuntime,
                               FileSystemV2* pFileSystem,
                               const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mContext(pRequest, pRuntime, pFileSystem, pLayout) {
  BuildPhaseList();
}

TaskDispositionV2 ManifestTaskV2::Heartbeat() {
  if (mDisposition != TaskDispositionV2::kRunning) {
    return mDisposition;
  }

  if (mContext.IsCancelRequested()) {
    mDisposition = TaskDispositionV2::kCanceled;
    return mDisposition;
  }

  const bool aSucceeded = RunCurrentPhase();
  if (!aSucceeded) {
    if (mContext.IsCancelRequested()) {
      mDisposition = TaskDispositionV2::kCanceled;
      return mDisposition;
    }
    MarkFailed();
    return mDisposition;
  }

  if (mContext.ActivePhaseNeedsMoreHeartbeats()) {
    return mDisposition;
  }

  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    mDisposition = TaskDispositionV2::kCompleted;
  }
  return mDisposition;
}

TaskDispositionV2 ManifestTaskV2::Disposition() const {
  return mDisposition;
}

const std::string& ManifestTaskV2::FailureMessage() const {
  return mFailureMessage;
}

EngineTaskTerminalSnapshotV2 ManifestTaskV2::BuildTerminalSnapshot() const {
  EngineTaskTerminalSnapshotV2 aSnapshot;
  const DecodeWorkStateV2& aState = State();
  aSnapshot.mDecodeState = std::make_shared<DecodeWorkStateV2>(aState);
  aSnapshot.mFailureMessage = FailureMessage();
  aSnapshot.mFailure = aState.mFailure;
  return aSnapshot;
}

const DecodeWorkStateV2& ManifestTaskV2::State() const {
  return mContext.State();
}

void ManifestTaskV2::BuildPhaseList() {
  mPhases = decode_workflow::BuildDecodePhaseListV2(
      decode_workflow::DecodePhasePlanV2::kManifest);
}

bool ManifestTaskV2::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }

  return decode_workflow::RunDecodePhaseV2(
      mContext,
      mPhases.mEntries[mCurrentPhaseIndex],
      mCurrentPhaseIndex,
      mPhases.mCount,
      LogActionFromDecodeIntentV2(mContext.Request().mIntent));
}

void ManifestTaskV2::MarkFailed() {
  mDisposition = TaskDispositionV2::kFailed;
  mFailureMessage = mContext.LastErrorLog();
  if (mFailureMessage.empty()) {
    mFailureMessage = "manifest read failed without explicit failure detail";
  }
}

}  // namespace peanutbutter
