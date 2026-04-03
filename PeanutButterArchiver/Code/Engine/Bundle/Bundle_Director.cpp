#include "Bundle_Director.hpp"

#include "Bundle_Workflow.hpp"

namespace peanutbutter {

BundleDirector::BundleDirector(const BundleRequestV2& pRequest,
                               BundleRuntimeV2* pRuntime,
                               FileSystemV2* pFileSystem,
                               const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mContext(pRequest, pRuntime, pFileSystem, pLayout) {
  BuildPhaseList();
}

bool BundleDirector::Step() {
  if (mIsFinished || mHasFailed || mWasCanceled) {
    return false;
  }

  if (mContext.IsCancelRequested() && !ShouldDeferCancelForCurrentPhase()) {
    mWasCanceled = true;
    mIsFinished = true;
    return false;
  }

  const bool aSucceeded = RunCurrentPhase();
  if (!aSucceeded) {
    if (mContext.IsCancelRequested() && !ShouldDeferCancelForCurrentPhase()) {
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

  if (mContext.IsCancelRequested()) {
    const ProgressStageV2 aStage = mPhases[mCurrentPhaseIndex].mStage;
    if (mContext.State().mCancel.mShouldFinalizeAfterCancel &&
        aStage != ProgressStageV2::kFinalizingHeaders) {
      mCurrentPhaseIndex = FindPhaseIndex(ProgressStageV2::kFinalizingHeaders);
      return true;
    }
    mWasCanceled = true;
    mIsFinished = true;
    return false;
  }

  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= mPhases.size()) {
    mIsFinished = true;
  }
  return true;
}

bool BundleDirector::IsFinished() const {
  return mIsFinished;
}

bool BundleDirector::HasFailed() const {
  return mHasFailed;
}

bool BundleDirector::WasCanceled() const {
  return mWasCanceled;
}

const BundleWorkStateV2& BundleDirector::State() const {
  return mContext.State();
}

const std::string& BundleDirector::FailureMessage() const {
  return mContext.LastErrorLog();
}

void BundleDirector::BuildPhaseList() {
  const std::vector<bundle_workflow::BundlePhaseEntryV2> aWorkflowPhases =
      bundle_workflow::BuildBundlePhaseListV2(mContext.Request());
  mPhases.clear();
  mPhases.reserve(aWorkflowPhases.size());
  for (const bundle_workflow::BundlePhaseEntryV2& aPhase : aWorkflowPhases) {
    mPhases.push_back({aPhase.mStage, aPhase.mRun});
  }
}

bool BundleDirector::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }

  const PhaseEntry& aEntry = mPhases[mCurrentPhaseIndex];
  return bundle_workflow::RunBundlePhaseV2(
      mContext,
      {aEntry.mStage, aEntry.mRun},
      mCurrentPhaseIndex,
      mPhases.size());
}

bool BundleDirector::ShouldDeferCancelForCurrentPhase() const {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }

  return bundle_workflow::ShouldDeferBundleCancelForPhaseV2(
      mContext.State(), mPhases[mCurrentPhaseIndex].mStage);
}

std::size_t BundleDirector::FindPhaseIndex(ProgressStageV2 pStage) const {
  for (std::size_t aIndex = 0u; aIndex < mPhases.size(); ++aIndex) {
    if (mPhases[aIndex].mStage == pStage) {
      return aIndex;
    }
  }
  return mPhases.size();
}

}  // namespace peanutbutter
