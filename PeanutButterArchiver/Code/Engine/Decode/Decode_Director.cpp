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
  aDecodeRequest.mPassword = pRequest.mPassword;
  aDecodeRequest.mIntent = DecodeIntentV2::kRecover;
  return aDecodeRequest;
}

}  // namespace

DecodeDirector::DecodeDirector(const DecodeRequestV2& pRequest,
                               DecodeRuntimeV2* pRuntime)
    : mContext(pRequest, pRuntime) {
  BuildPhaseList();
}

bool DecodeDirector::Step() {
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

RepairDirector::RepairDirector(const RepairRequestV2& pRequest,
                               DecodeRuntimeV2* pRuntime)
    : mDecodeDirector(MakeRepairDecodeRequest(pRequest), pRuntime) {}

bool RepairDirector::Step() {
  return mDecodeDirector.Step();
}

bool RepairDirector::IsFinished() const {
  return mDecodeDirector.IsFinished();
}

bool RepairDirector::HasFailed() const {
  return mDecodeDirector.HasFailed();
}

bool RepairDirector::WasCanceled() const {
  return mDecodeDirector.WasCanceled();
}

const DecodeWorkStateV2& RepairDirector::State() const {
  return mDecodeDirector.State();
}

ManifestDirector::ManifestDirector(const DecodeRequestV2& pRequest,
                                   DecodeRuntimeV2* pRuntime)
    : mContext(pRequest, pRuntime) {
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
