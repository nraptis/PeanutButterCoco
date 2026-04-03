#include "TaskCommon.hpp"

#include "Bundle/Bundle_Workflow.hpp"

namespace peanutbutter {

BundleTaskV2::BundleTaskV2(const BundleRequestV2& pRequest,
                           BundleRuntimeV2* pRuntime,
                           FileSystemV2* pFileSystem,
                           const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mContext(pRequest, pRuntime, pFileSystem, pLayout) {
  BuildPhaseList();
}

TaskDispositionV2 BundleTaskV2::Heartbeat() {
  if (mDisposition != TaskDispositionV2::kRunning) {
    return mDisposition;
  }

  if (mContext.IsCancelRequested() && !ShouldDeferCancelForCurrentPhase()) {
    mDisposition = TaskDispositionV2::kCanceled;
    return mDisposition;
  }

  const bool aSucceeded = RunCurrentPhase();
  if (!aSucceeded) {
    if (mContext.IsCancelRequested() && !ShouldDeferCancelForCurrentPhase()) {
      mDisposition = TaskDispositionV2::kCanceled;
      return mDisposition;
    }
    MarkFailed();
    return mDisposition;
  }

  if (mContext.ActivePhaseNeedsMoreHeartbeats()) {
    return mDisposition;
  }

  if (mContext.IsCancelRequested()) {
    const ProgressStageV2 aStage = mPhases[mCurrentPhaseIndex].mStage;
    if (mContext.State().mCancel.mShouldFinalizeAfterCancel &&
        aStage != ProgressStageV2::kFinalizingHeaders) {
      mCurrentPhaseIndex = FindPhaseIndex(ProgressStageV2::kFinalizingHeaders);
      return mDisposition;
    }
    mDisposition = TaskDispositionV2::kCanceled;
    return mDisposition;
  }

  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= mPhases.size()) {
    mDisposition = TaskDispositionV2::kCompleted;
  }
  return mDisposition;
}

TaskDispositionV2 BundleTaskV2::Disposition() const {
  return mDisposition;
}

const std::string& BundleTaskV2::FailureMessage() const {
  return mFailureMessage;
}

EngineTaskTerminalSnapshotV2 BundleTaskV2::BuildTerminalSnapshot() const {
  EngineTaskTerminalSnapshotV2 aSnapshot;
  const BundleWorkStateV2& aState = State();
  aSnapshot.mBundleState = std::make_shared<BundleWorkStateV2>(aState);
  aSnapshot.mFailureMessage = FailureMessage();
  aSnapshot.mFailure = aState.mFailure;
  return aSnapshot;
}

const BundleWorkStateV2& BundleTaskV2::State() const {
  return mContext.State();
}

void BundleTaskV2::BuildPhaseList() {
  const std::vector<bundle_workflow::BundlePhaseEntryV2> aWorkflowPhases =
      bundle_workflow::BuildBundlePhaseListV2(mContext.Request());
  mPhases.clear();
  mPhases.reserve(aWorkflowPhases.size());
  for (const bundle_workflow::BundlePhaseEntryV2& aPhase : aWorkflowPhases) {
    mPhases.push_back({aPhase.mStage, aPhase.mRun});
  }
}

bool BundleTaskV2::RunCurrentPhase() {
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

bool BundleTaskV2::ShouldDeferCancelForCurrentPhase() const {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }

  return bundle_workflow::ShouldDeferBundleCancelForPhaseV2(
      mContext.State(), mPhases[mCurrentPhaseIndex].mStage);
}

std::size_t BundleTaskV2::FindPhaseIndex(ProgressStageV2 pStage) const {
  for (std::size_t aIndex = 0u; aIndex < mPhases.size(); ++aIndex) {
    if (mPhases[aIndex].mStage == pStage) {
      return aIndex;
    }
  }
  return mPhases.size();
}

void BundleTaskV2::MarkFailed() {
  mDisposition = TaskDispositionV2::kFailed;
  mFailureMessage = mContext.LastErrorLog();
  if (mFailureMessage.empty()) {
    mFailureMessage = "bundle failed without explicit failure detail";
  }
}

}  // namespace peanutbutter
