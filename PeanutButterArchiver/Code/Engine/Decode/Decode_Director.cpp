#include "Decode_Director.hpp"

#include "Decode_ArchiveDecode.hpp"
#include "Decode_AssembleCipherStack.hpp"
#include "Decode_DeriveCipherMaterial.hpp"
#include "Decode_Discovery.hpp"
#include "Decode_Finalize.hpp"
#include "Decode_HeaderBootstrap.hpp"
#include "Decode_ManifestDiscovery.hpp"
#include "Decode_Preflight.hpp"

namespace peanutbutter {

namespace {

DecodeRequestV2 MakeRepairDecodeRequest(const RepairRequestV2& pRequest) {
  DecodeRequestV2 aDecodeRequest;
  aDecodeRequest.mSourcePath = pRequest.mSourcePath;
  aDecodeRequest.mDestinationDirectory = pRequest.mDestinationDirectory;
  aDecodeRequest.mEncryptionEnabled = pRequest.mEncryptionEnabled;
  aDecodeRequest.mCancelFinishBlocks = pRequest.mCancelFinishBlocks;
  aDecodeRequest.mPassword = pRequest.mPassword;
  aDecodeRequest.mIntent = DecodeIntentV2::kRecover;
  return aDecodeRequest;
}

}  // namespace

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

  if (mContext.IsCancelRequested()) {
    const ProgressStageV2 aStage = mPhases[mCurrentPhaseIndex].mStage;
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
  if (mCurrentPhaseIndex >= mPhases.size()) {
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

void DecodeDirector::BuildPhaseList() {
  mPhases.clear();
  mPhases.push_back({ProgressStageV2::kPreflight, &DecodePreflightV2::Run});
  mPhases.push_back({ProgressStageV2::kHeaderBootstrap,
                     &DecodeHeaderBootstrapV2::Run});
  mPhases.push_back({ProgressStageV2::kDiscovery, &DecodeDiscoveryV2::Run});
  mPhases.push_back({ProgressStageV2::kDeriveCipherMaterial,
                     &DecodeDeriveCipherMaterialV2::Run});
  mPhases.push_back({ProgressStageV2::kAssembleCipherStack,
                     &DecodeAssembleCipherStackV2::Run});
  mPhases.push_back({ProgressStageV2::kInspection, &DecodeInspectionV2::Run});
  mPhases.push_back({ProgressStageV2::kManifestDiscovery,
                     &DecodeManifestDiscoveryV2::Run});
  mPhases.push_back({ProgressStageV2::kArchiveDecode,
                     &DecodeArchiveDecodeV2::Run});
  mPhases.push_back({ProgressStageV2::kFinalize, &DecodeFinalizeV2::Run});
}

bool DecodeDirector::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }

  const PhaseEntry& aEntry = mPhases[mCurrentPhaseIndex];
  if (aEntry.mRun == nullptr) {
    return false;
  }

  mContext.SetActivePhase(aEntry.mStage, mCurrentPhaseIndex, mPhases.size());
  return aEntry.mRun(mContext);
}

bool DecodeDirector::ShouldDeferCancelForCurrentPhase() const {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }

  const ProgressStageV2 aStage = mPhases[mCurrentPhaseIndex].mStage;
  if (mContext.State().mCancel.mShouldFinalizeAfterCancel) {
    return aStage == ProgressStageV2::kFinalize;
  }
  return aStage == ProgressStageV2::kArchiveDecode;
}

std::size_t DecodeDirector::FindPhaseIndex(ProgressStageV2 pStage) const {
  for (std::size_t aIndex = 0u; aIndex < mPhases.size(); ++aIndex) {
    if (mPhases[aIndex].mStage == pStage) {
      return aIndex;
    }
  }
  return mPhases.size();
}

RepairDirector::RepairDirector(const RepairRequestV2& pRequest,
                               DecodeRuntimeV2* pRuntime,
                               FileSystemV2* pFileSystem,
                               const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mContext(MakeRepairDecodeRequest(pRequest), pRuntime, pFileSystem, pLayout) {
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

  if (mContext.IsCancelRequested()) {
    const ProgressStageV2 aStage = mPhases[mCurrentPhaseIndex].mStage;
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
  if (mCurrentPhaseIndex >= mPhases.size()) {
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

void RepairDirector::BuildPhaseList() {
  mPhases.clear();
  mPhases.push_back({ProgressStageV2::kPreflight, &DecodePreflightV2::Run});
  mPhases.push_back({ProgressStageV2::kHeaderBootstrap,
                     &DecodeHeaderBootstrapV2::Run});
  mPhases.push_back({ProgressStageV2::kDiscovery, &DecodeDiscoveryV2::Run});
  mPhases.push_back({ProgressStageV2::kDeriveCipherMaterial,
                     &DecodeDeriveCipherMaterialV2::Run});
  mPhases.push_back({ProgressStageV2::kAssembleCipherStack,
                     &DecodeAssembleCipherStackV2::Run});
  mPhases.push_back({ProgressStageV2::kInspection, &DecodeInspectionV2::Run});
  mPhases.push_back({ProgressStageV2::kManifestDiscovery,
                     &DecodeManifestDiscoveryV2::Run});
  mPhases.push_back({ProgressStageV2::kRepairApply, &DecodeRepairApplyV2::Run});
  mPhases.push_back({ProgressStageV2::kFinalize, &DecodeFinalizeV2::Run});
}

bool RepairDirector::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }

  const PhaseEntry& aEntry = mPhases[mCurrentPhaseIndex];
  if (aEntry.mRun == nullptr) {
    return false;
  }

  mContext.SetActivePhase(aEntry.mStage, mCurrentPhaseIndex, mPhases.size());
  return aEntry.mRun(mContext);
}

bool RepairDirector::ShouldDeferCancelForCurrentPhase() const {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }

  const ProgressStageV2 aStage = mPhases[mCurrentPhaseIndex].mStage;
  if (mContext.State().mCancel.mShouldFinalizeAfterCancel) {
    return aStage == ProgressStageV2::kFinalize;
  }
  return aStage == ProgressStageV2::kRepairApply;
}

std::size_t RepairDirector::FindPhaseIndex(ProgressStageV2 pStage) const {
  for (std::size_t aIndex = 0u; aIndex < mPhases.size(); ++aIndex) {
    if (mPhases[aIndex].mStage == pStage) {
      return aIndex;
    }
  }
  return mPhases.size();
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

  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= mPhases.size()) {
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

void ManifestDirector::BuildPhaseList() {
  mPhases.clear();
  mPhases.push_back({ProgressStageV2::kPreflight, &DecodePreflightV2::Run});
  mPhases.push_back({ProgressStageV2::kHeaderBootstrap,
                     &DecodeHeaderBootstrapV2::Run});
  mPhases.push_back({ProgressStageV2::kDiscovery, &DecodeDiscoveryV2::Run});
  mPhases.push_back({ProgressStageV2::kManifestDiscovery,
                     &DecodeManifestDiscoveryV2::Run});
}

bool ManifestDirector::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }

  const PhaseEntry& aEntry = mPhases[mCurrentPhaseIndex];
  if (aEntry.mRun == nullptr) {
    return false;
  }

  mContext.SetActivePhase(aEntry.mStage, mCurrentPhaseIndex, mPhases.size());
  return aEntry.mRun(mContext);
}

}  // namespace peanutbutter
