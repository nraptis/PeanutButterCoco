#include "Bundle_Workflow.hpp"

#include "Bundle_ArchiveManifest.hpp"
#include "Bundle_ArchivePacking.hpp"
#include "Bundle_AssembleCipherStack.hpp"
#include "Bundle_DeriveCipherMaterial.hpp"
#include "Bundle_Discovery.hpp"
#include "Bundle_FinalizingHeaders.hpp"
#include "Bundle_MemoryPlanning.hpp"
#include "Bundle_Preflight.hpp"
#include "Bundle_RepairPacking.hpp"
#include "../../Common/LogCatalog.hpp"

namespace peanutbutter::bundle_workflow {

std::vector<BundlePhaseEntryV2> BuildBundlePhaseListV2(
    const BundleRequestV2& pRequest) {
  std::vector<BundlePhaseEntryV2> aPhases;
  aPhases.push_back({ProgressStageV2::kPreflight, &BundlePreflightV2::Run});
  aPhases.push_back({ProgressStageV2::kDiscovery, &BundleDiscoveryV2::Run});
  aPhases.push_back(
      {ProgressStageV2::kMemoryPlanning, &BundleMemoryPlanningV2::Run});

  if (pRequest.mEncryptionEnabled) {
    aPhases.push_back({ProgressStageV2::kDeriveCipherMaterial,
                       &BundleDeriveCipherMaterialV2::Run});
    aPhases.push_back({ProgressStageV2::kAssembleCipherStack,
                       &BundleAssembleCipherStackV2::Run});
  }

  aPhases.push_back(
      {ProgressStageV2::kArchiveManifest, &BundleArchiveManifestV2::Run});
  aPhases.push_back(
      {ProgressStageV2::kArchivePacking, &BundleArchivePackingV2::Run});

  if (pRequest.mRepairEnabled) {
    aPhases.push_back(
        {ProgressStageV2::kRepairPacking, &BundleRepairPackingV2::Run});
  }

  aPhases.push_back({ProgressStageV2::kFinalizingHeaders,
                     &BundleFinalizingHeadersV2::Run});
  return aPhases;
}

bool RunBundlePhaseV2(BundleStageContextV2& pContext,
                      const BundlePhaseEntryV2& pPhase,
                      std::size_t pPhaseIndex,
                      std::size_t pPhaseCount) {
  if (pPhase.mRun == nullptr) {
    return false;
  }

  pContext.SetActivePhase(pPhase.mStage, pPhaseIndex, pPhaseCount);
  pContext.BeginWorkUnit();
  const bool aSucceeded = pPhase.mRun(pContext);
  if (!aSucceeded && !pContext.IsCancelRequested() &&
      !pContext.ActivePhaseHasError()) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(
            LogActionV2::kBundle,
            pPhase.mStage,
            "phase returned false without explicit failure detail"));
  }
  if (aSucceeded && pContext.ActivePhaseHasError()) {
    return false;
  }
  return aSucceeded;
}

bool ShouldDeferBundleCancelForPhaseV2(const BundleWorkStateV2& pState,
                                       ProgressStageV2 pStage) {
  if (pState.mCancel.mShouldFinalizeAfterCancel) {
    return pStage == ProgressStageV2::kFinalizingHeaders;
  }
  return pStage == ProgressStageV2::kArchivePacking;
}

std::size_t FindBundlePhaseIndexV2(
    const std::vector<BundlePhaseEntryV2>& pPhases,
    ProgressStageV2 pStage) {
  for (std::size_t aIndex = 0u; aIndex < pPhases.size(); ++aIndex) {
    if (pPhases[aIndex].mStage == pStage) {
      return aIndex;
    }
  }
  return pPhases.size();
}

}  // namespace peanutbutter::bundle_workflow
