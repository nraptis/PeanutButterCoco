#include "Sanity_Director.hpp"

namespace peanutbutter {

SanityDirector::SanityDirector(const SanityRequestV2& pRequest,
                               SanityRuntimeV2* pRuntime)
    : mContext(pRequest, pRuntime) {
  BuildPhaseList();
}

bool SanityDirector::Step() {
  if (mIsFinished || mHasFailed || mWasCanceled) {
    return false;
  }
  if (mContext.IsCancelRequested()) {
    mWasCanceled = true;
    mIsFinished = true;
    return false;
  }
  const bool aSucceeded = RunCurrentPhase();
  if (!aSucceeded) {
    if (mContext.IsCancelRequested()) {
      mWasCanceled = true;
      mIsFinished = true;
      return false;
    }
    mHasFailed = true;
    return false;
  }
  if (mContext.ActivePhaseNeedsMoreHeartbeats()) {
    return true;
  }
  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    mIsFinished = true;
  }
  return true;
}

bool SanityDirector::IsFinished() const { return mIsFinished; }
bool SanityDirector::HasFailed() const { return mHasFailed; }
bool SanityDirector::WasCanceled() const { return mWasCanceled; }

void SanityDirector::BuildPhaseList() {
  mPhases = sanity_workflow::BuildSanityPhaseListV2();
}

bool SanityDirector::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }
  return sanity_workflow::RunSanityPhaseV2(
      mContext,
      mPhases.mEntries[mCurrentPhaseIndex],
      mCurrentPhaseIndex,
      mPhases.mCount);
}

}  // namespace peanutbutter
