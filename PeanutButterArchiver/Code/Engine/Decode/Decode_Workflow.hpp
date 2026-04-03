#pragma once

#include <cstddef>

#include "../../Common/LogCatalog.hpp"
#include "Decode_ArchiveDecode.hpp"
#include "Decode_AssembleCipherStack.hpp"
#include "Decode_DeriveCipherMaterial.hpp"
#include "Decode_Discovery.hpp"
#include "Decode_Finalize.hpp"
#include "Decode_HeaderBootstrap.hpp"
#include "Decode_ManifestDiscovery.hpp"
#include "Decode_Preflight.hpp"

namespace peanutbutter::decode_workflow {

using DecodePhaseRunnerV2 = bool (*)(DecodeStageContextV2&);

struct DecodePhaseEntryV2 {
  ProgressStageV2 mStage = ProgressStageV2::kIdle;
  DecodePhaseRunnerV2 mRun = nullptr;
};

struct DecodePhaseListViewV2 {
  const DecodePhaseEntryV2* mEntries = nullptr;
  std::size_t mCount = 0u;
};

enum class DecodePhasePlanV2 {
  kDecode = 0,
  kRepair = 1,
  kManifest = 2,
};

inline DecodePhaseListViewV2 BuildDecodePhaseListV2(
    DecodePhasePlanV2 pPlan) {
  static const DecodePhaseEntryV2 kManifestPhases[] = {
      {ProgressStageV2::kPreflight, &DecodePreflightV2::Run},
      {ProgressStageV2::kHeaderBootstrap, &DecodeHeaderBootstrapV2::Run},
      {ProgressStageV2::kDiscovery, &DecodeDiscoveryV2::Run},
      {ProgressStageV2::kManifestDiscovery, &DecodeManifestDiscoveryV2::Run},
  };

  static const DecodePhaseEntryV2 kRepairPhases[] = {
      {ProgressStageV2::kPreflight, &DecodePreflightV2::Run},
      {ProgressStageV2::kHeaderBootstrap, &DecodeHeaderBootstrapV2::Run},
      {ProgressStageV2::kDiscovery, &DecodeDiscoveryV2::Run},
      {ProgressStageV2::kDeriveCipherMaterial, &DecodeDeriveCipherMaterialV2::Run},
      {ProgressStageV2::kAssembleCipherStack, &DecodeAssembleCipherStackV2::Run},
      {ProgressStageV2::kInspection, &DecodeInspectionV2::Run},
      {ProgressStageV2::kManifestDiscovery, &DecodeManifestDiscoveryV2::Run},
      {ProgressStageV2::kRepairApply, &DecodeRepairApplyV2::Run},
      {ProgressStageV2::kFinalize, &DecodeFinalizeV2::Run},
  };

  static const DecodePhaseEntryV2 kDecodePhases[] = {
      {ProgressStageV2::kPreflight, &DecodePreflightV2::Run},
      {ProgressStageV2::kHeaderBootstrap, &DecodeHeaderBootstrapV2::Run},
      {ProgressStageV2::kDiscovery, &DecodeDiscoveryV2::Run},
      {ProgressStageV2::kDeriveCipherMaterial, &DecodeDeriveCipherMaterialV2::Run},
      {ProgressStageV2::kAssembleCipherStack, &DecodeAssembleCipherStackV2::Run},
      {ProgressStageV2::kInspection, &DecodeInspectionV2::Run},
      {ProgressStageV2::kManifestDiscovery, &DecodeManifestDiscoveryV2::Run},
      {ProgressStageV2::kArchiveDecode, &DecodeArchiveDecodeV2::Run},
      {ProgressStageV2::kFinalize, &DecodeFinalizeV2::Run},
  };

  switch (pPlan) {
    case DecodePhasePlanV2::kManifest:
      return {kManifestPhases, sizeof(kManifestPhases) / sizeof(kManifestPhases[0])};

    case DecodePhasePlanV2::kRepair:
      return {kRepairPhases, sizeof(kRepairPhases) / sizeof(kRepairPhases[0])};

    case DecodePhasePlanV2::kDecode:
      return {kDecodePhases, sizeof(kDecodePhases) / sizeof(kDecodePhases[0])};
  }

  return {};
}

inline bool RunDecodePhaseV2(DecodeStageContextV2& pContext,
                             const DecodePhaseEntryV2& pPhase,
                             std::size_t pPhaseIndex,
                             std::size_t pPhaseCount,
                             LogActionV2 pLogAction) {
  if (pPhase.mRun == nullptr) {
    return false;
  }

  pContext.SetActivePhase(pPhase.mStage, pPhaseIndex, pPhaseCount);
  pContext.BeginWorkUnit();
  const bool aSucceeded = pPhase.mRun(pContext);
  if (!aSucceeded &&
      !pContext.IsCancelRequested() &&
      !pContext.ActivePhaseHasError()) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(
            pLogAction,
            pPhase.mStage,
            "phase returned false without explicit failure detail"));
  }
  if (aSucceeded && pContext.ActivePhaseHasError()) {
    return false;
  }
  return aSucceeded;
}

inline bool ShouldDeferDecodeCancelForPhaseV2(
    const DecodeWorkStateV2& pState,
    ProgressStageV2 pStage,
    ProgressStageV2 pDataStage) {
  if (pState.mCancel.mShouldFinalizeAfterCancel) {
    return pStage == ProgressStageV2::kFinalize;
  }
  return pStage == pDataStage;
}

inline std::size_t FindDecodePhaseIndexV2(DecodePhaseListViewV2 pPhases,
                                          ProgressStageV2 pStage) {
  for (std::size_t aIndex = 0u; aIndex < pPhases.mCount; ++aIndex) {
    if (pPhases.mEntries[aIndex].mStage == pStage) {
      return aIndex;
    }
  }
  return pPhases.mCount;
}

}  // namespace peanutbutter::decode_workflow
