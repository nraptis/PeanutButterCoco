#include "TaskCommon.hpp"

#include "Sanity/Sanity_Workflow.hpp"

namespace peanutbutter {

SanityTaskV2::SanityTaskV2(const SanityRequestV2& pRequest,
                           SanityRuntimeV2* pRuntime)
    : mContext(pRequest, pRuntime) {
  BuildPhaseList();
}

TaskDispositionV2 SanityTaskV2::Heartbeat() {
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

TaskDispositionV2 SanityTaskV2::Disposition() const {
  return mDisposition;
}

const std::string& SanityTaskV2::FailureMessage() const {
  return mFailureMessage;
}

EngineTaskTerminalSnapshotV2 SanityTaskV2::BuildTerminalSnapshot() const {
  EngineTaskTerminalSnapshotV2 aSnapshot;
  const SanityWorkStateV2& aState = State();
  aSnapshot.mSanityState = std::make_shared<SanityWorkStateV2>(aState);
  aSnapshot.mFailureMessage = FailureMessage();
  aSnapshot.mFailure = aState.mFailure;
  return aSnapshot;
}

const SanityWorkStateV2& SanityTaskV2::State() const {
  return mContext.State();
}

void SanityTaskV2::BuildPhaseList() {
  mPhases = sanity_workflow::BuildSanityPhaseListV2();
}

bool SanityTaskV2::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }

  return sanity_workflow::RunSanityPhaseV2(
      mContext,
      mPhases.mEntries[mCurrentPhaseIndex],
      mCurrentPhaseIndex,
      mPhases.mCount);
}

void SanityTaskV2::MarkFailed() {
  mDisposition = TaskDispositionV2::kFailed;
  mFailureMessage = mContext.LastErrorLog();
  if (mFailureMessage.empty()) {
    mFailureMessage = "sanity check failed";
  }
}

}  // namespace peanutbutter
