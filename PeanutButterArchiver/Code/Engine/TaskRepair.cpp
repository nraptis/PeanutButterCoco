#include "TaskCommon.hpp"

#include <algorithm>

#include "../Knobs.hpp"
#include "Repair/Repair_Workflow.hpp"

namespace peanutbutter {

RepairTaskV2::RepairTaskV2(const RepairRequestV2& pRequest,
                           DecodeRuntimeV2* pRuntime,
                           FileSystemV2* pFileSystem,
                           const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mContext(repair_workflow::MakeRepairDecodeRequestV2(pRequest),
               pRuntime,
               pFileSystem,
               pLayout) {
  BuildPhaseList();
}

TaskDispositionV2 RepairTaskV2::Heartbeat() {
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
          ProgressStageV2::kRepairApply) {
    aSliceBudget = std::max<std::size_t>(
        1u, static_cast<std::size_t>(knobs::kBatchSizeRepairV2));
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

TaskDispositionV2 RepairTaskV2::Disposition() const {
  return mDisposition;
}

const std::string& RepairTaskV2::FailureMessage() const {
  return mFailureMessage;
}

EngineTaskTerminalSnapshotV2 RepairTaskV2::BuildTerminalSnapshot() const {
  EngineTaskTerminalSnapshotV2 aSnapshot;
  const DecodeWorkStateV2& aState = State();
  aSnapshot.mDecodeState = std::make_shared<DecodeWorkStateV2>(aState);
  aSnapshot.mFailureMessage = FailureMessage();
  aSnapshot.mFailure = aState.mFailure;
  return aSnapshot;
}

const DecodeWorkStateV2& RepairTaskV2::State() const {
  return mContext.State();
}

void RepairTaskV2::BuildPhaseList() {
  mPhases = repair_workflow::BuildRepairPhaseListV2();
}

bool RepairTaskV2::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }

  return repair_workflow::RunRepairPhaseV2(
      mContext,
      mPhases.mEntries[mCurrentPhaseIndex],
      mCurrentPhaseIndex,
      mPhases.mCount);
}

bool RepairTaskV2::ShouldDeferCancelForCurrentPhase() const {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }

  return repair_workflow::ShouldDeferRepairCancelForPhaseV2(
      mContext.State(), mPhases.mEntries[mCurrentPhaseIndex].mStage);
}

std::size_t RepairTaskV2::FindPhaseIndex(ProgressStageV2 pStage) const {
  return repair_workflow::FindRepairPhaseIndexV2(mPhases, pStage);
}

void RepairTaskV2::MarkFailed() {
  mDisposition = TaskDispositionV2::kFailed;
  mFailureMessage = mContext.LastErrorLog();
  if (mFailureMessage.empty()) {
    mFailureMessage = "repair failed without explicit failure detail";
  }
}

}  // namespace peanutbutter
