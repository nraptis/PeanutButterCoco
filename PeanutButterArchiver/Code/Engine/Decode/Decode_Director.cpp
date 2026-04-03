#include "Decode_Director.hpp"

#include "../../Common/LogCatalog.hpp"
#include "../Repair/Repair_Workflow.hpp"

namespace peanutbutter {

DecodeDirector::DecodeDirector(const DecodeRequestV2& pRequest,
                               DecodeRuntimeV2* pRuntime,
                               FileSystemV2* pFileSystem,
                               const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mContext(pRequest, pRuntime, pFileSystem, pLayout) {
  BuildPhaseList();
}

bool DecodeDirector::Step() {
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
    const ProgressStageV2 aStage = mPhases.mEntries[mCurrentPhaseIndex].mStage;
    if (mContext.State().mCancel.mShouldFinalizeAfterCancel &&
        aStage != ProgressStageV2::kFinalize) {
      mCurrentPhaseIndex = FindPhaseIndex(ProgressStageV2::kFinalize);
      return true;
    }
    mWasCanceled = true;
    mIsFinished = true;
    return false;
  }

  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    mIsFinished = true;
  }
  return true;
}

bool DecodeDirector::IsFinished() const {
  return mIsFinished;
}

bool DecodeDirector::HasFailed() const {
  return mHasFailed;
}

bool DecodeDirector::WasCanceled() const {
  return mWasCanceled;
}

const DecodeWorkStateV2& DecodeDirector::State() const {
  return mContext.State();
}

const std::string& DecodeDirector::FailureMessage() const {
  return mContext.LastErrorLog();
}

void DecodeDirector::BuildPhaseList() {
  mPhases = decode_workflow::BuildDecodePhaseListV2(
      decode_workflow::DecodePhasePlanV2::kDecode);
}

bool DecodeDirector::RunCurrentPhase() {
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

bool DecodeDirector::ShouldDeferCancelForCurrentPhase() const {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }

  return decode_workflow::ShouldDeferDecodeCancelForPhaseV2(
      mContext.State(),
      mPhases.mEntries[mCurrentPhaseIndex].mStage,
      ProgressStageV2::kArchiveDecode);
}

std::size_t DecodeDirector::FindPhaseIndex(ProgressStageV2 pStage) const {
  return decode_workflow::FindDecodePhaseIndexV2(mPhases, pStage);
}

RepairDirector::RepairDirector(const RepairRequestV2& pRequest,
                               DecodeRuntimeV2* pRuntime,
                               FileSystemV2* pFileSystem,
                               const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mContext(repair_workflow::MakeRepairDecodeRequestV2(pRequest),
               pRuntime,
               pFileSystem,
               pLayout) {
  BuildPhaseList();
}

bool RepairDirector::Step() {
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
    const ProgressStageV2 aStage = mPhases.mEntries[mCurrentPhaseIndex].mStage;
    if (mContext.State().mCancel.mShouldFinalizeAfterCancel &&
        aStage != ProgressStageV2::kFinalize) {
      mCurrentPhaseIndex = FindPhaseIndex(ProgressStageV2::kFinalize);
      return true;
    }
    mWasCanceled = true;
    mIsFinished = true;
    return false;
  }

  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    mIsFinished = true;
  }
  return true;
}

bool RepairDirector::IsFinished() const {
  return mIsFinished;
}

bool RepairDirector::HasFailed() const {
  return mHasFailed;
}

bool RepairDirector::WasCanceled() const {
  return mWasCanceled;
}

const DecodeWorkStateV2& RepairDirector::State() const {
  return mContext.State();
}

const std::string& RepairDirector::FailureMessage() const {
  return mContext.LastErrorLog();
}

void RepairDirector::BuildPhaseList() {
  mPhases = repair_workflow::BuildRepairPhaseListV2();
}

bool RepairDirector::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }

  return repair_workflow::RunRepairPhaseV2(
      mContext,
      mPhases.mEntries[mCurrentPhaseIndex],
      mCurrentPhaseIndex,
      mPhases.mCount);
}

bool RepairDirector::ShouldDeferCancelForCurrentPhase() const {
  if (mCurrentPhaseIndex >= mPhases.mCount) {
    return false;
  }

  return repair_workflow::ShouldDeferRepairCancelForPhaseV2(
      mContext.State(), mPhases.mEntries[mCurrentPhaseIndex].mStage);
}

std::size_t RepairDirector::FindPhaseIndex(ProgressStageV2 pStage) const {
  return repair_workflow::FindRepairPhaseIndexV2(mPhases, pStage);
}

ManifestDirector::ManifestDirector(const DecodeRequestV2& pRequest,
                                   DecodeRuntimeV2* pRuntime,
                                   FileSystemV2* pFileSystem,
                                   const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mContext(pRequest, pRuntime, pFileSystem, pLayout) {
  BuildPhaseList();
}

bool ManifestDirector::Step() {
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

bool ManifestDirector::IsFinished() const {
  return mIsFinished;
}

bool ManifestDirector::HasFailed() const {
  return mHasFailed;
}

bool ManifestDirector::WasCanceled() const {
  return mWasCanceled;
}

const DecodeWorkStateV2& ManifestDirector::State() const {
  return mContext.State();
}

const std::string& ManifestDirector::FailureMessage() const {
  return mContext.LastErrorLog();
}

void ManifestDirector::BuildPhaseList() {
  mPhases = decode_workflow::BuildDecodePhaseListV2(
      decode_workflow::DecodePhasePlanV2::kManifest);
}

bool ManifestDirector::RunCurrentPhase() {
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

}  // namespace peanutbutter
