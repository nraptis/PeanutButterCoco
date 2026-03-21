#include "Bundle_Director.hpp"

#include "Bundle_ArchiveManifest.hpp"
#include "Bundle_ArchivePacking.hpp"
#include "Bundle_AssembleCipherStack.hpp"
#include "Bundle_DeriveCipherMaterial.hpp"
#include "Bundle_Discovery.hpp"
#include "Bundle_FinalizingHeaders.hpp"
#include "Bundle_FolderManifest.hpp"
#include "Bundle_MemoryPlanning.hpp"
#include "Bundle_Preflight.hpp"
#include "Bundle_RepairPacking.hpp"
#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

BundleDirector::BundleDirector(const BundleRequestV2& pRequest,
                               BundleRuntimeV2* pRuntime)
    : mContext(pRequest, pRuntime) {
  BuildPhaseList();
}

bool BundleDirector::Step() {
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

void BundleDirector::BuildPhaseList() {
  mPhases.clear();
  mPhases.push_back({ProgressStageV2::kPreflight, &BundlePreflightV2::Run});
  mPhases.push_back({ProgressStageV2::kDiscovery, &BundleDiscoveryV2::Run});
  mPhases.push_back({ProgressStageV2::kMemoryPlanning, &BundleMemoryPlanningV2::Run});

  if (mContext.Request().mEncryptionEnabled) {
    mPhases.push_back({ProgressStageV2::kDeriveCipherMaterial,
                       &BundleDeriveCipherMaterialV2::Run});
    mPhases.push_back({ProgressStageV2::kAssembleCipherStack,
                       &BundleAssembleCipherStackV2::Run});
  }

  mPhases.push_back({ProgressStageV2::kArchiveManifest, &BundleArchiveManifestV2::Run});
  mPhases.push_back({ProgressStageV2::kFolderPacking, &BundleFolderPackingV2::Run});
  mPhases.push_back({ProgressStageV2::kArchivePacking, &BundleArchivePackingV2::Run});

  if (mContext.Request().mRepairEnabled) {
    mPhases.push_back({ProgressStageV2::kRepairPacking, &BundleRepairPackingV2::Run});
  }

  mPhases.push_back({ProgressStageV2::kFinalizingHeaders, &BundleFinalizingHeadersV2::Run});
}

bool BundleDirector::RunCurrentPhase() {
  if (mCurrentPhaseIndex >= mPhases.size()) {
    return false;
  }

  const PhaseEntry& aEntry = mPhases[mCurrentPhaseIndex];
  if (aEntry.mRun == nullptr) {
    return false;
  }
  mContext.SetActivePhase(aEntry.mStage, mCurrentPhaseIndex, mPhases.size());
  const bool aSucceeded = aEntry.mRun(mContext);
  if (!aSucceeded && !mContext.IsCancelRequested() && !mContext.ActivePhaseHasError()) {
    mContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kBundle,
                                      aEntry.mStage,
                                      "phase returned false without explicit failure detail"));
  }
  return aSucceeded;
}

}  // namespace peanutbutter
