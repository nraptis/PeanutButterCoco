#include "Sanity_Director.hpp"

#include "Sanity_Compare.hpp"
#include "Sanity_Discovery.hpp"
#include "Sanity_Finalize.hpp"
#include "Sanity_Preflight.hpp"

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
  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= mPhases.size()) {
    mIsFinished = true;
  }
  return true;
}

bool SanityDirector::IsFinished() const { return mIsFinished; }
bool SanityDirector::HasFailed() const { return mHasFailed; }
bool SanityDirector::WasCanceled() const { return mWasCanceled; }

void SanityDirector::BuildPhaseList() {
  mPhases = {
      {ProgressStageV2::kPreflight, &SanityPreflightV2::Run},
      {ProgressStageV2::kDiscovery, &SanityDiscoveryV2::Run},
      {ProgressStageV2::kCompare, &SanityCompareV2::Run},
      {ProgressStageV2::kFinalize, &SanityFinalizeV2::Run},
  };
}

bool SanityDirector::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }
  const PhaseEntry& aEntry = mPhases[mCurrentPhaseIndex];
  mContext.SetActivePhase(aEntry.mStage, mCurrentPhaseIndex, mPhases.size());
  return aEntry.mRun != nullptr && aEntry.mRun(mContext);
}

}  // namespace peanutbutter
