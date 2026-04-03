#pragma once

#include <cstddef>

#include "Sanity_Compare.hpp"
#include "Sanity_Discovery.hpp"
#include "Sanity_Finalize.hpp"
#include "Sanity_Preflight.hpp"

namespace peanutbutter::sanity_workflow {

using SanityPhaseRunnerV2 = bool (*)(SanityStageContextV2&);

struct SanityPhaseEntryV2 {
  ProgressStageV2 mStage = ProgressStageV2::kIdle;
  SanityPhaseRunnerV2 mRun = nullptr;
};

struct SanityPhaseListViewV2 {
  const SanityPhaseEntryV2* mEntries = nullptr;
  std::size_t mCount = 0u;
};

inline SanityPhaseListViewV2 BuildSanityPhaseListV2() {
  static const SanityPhaseEntryV2 kPhases[] = {
      {ProgressStageV2::kPreflight, &SanityPreflightV2::Run},
      {ProgressStageV2::kDiscovery, &SanityDiscoveryV2::Run},
      {ProgressStageV2::kCompare, &SanityCompareV2::Run},
      {ProgressStageV2::kFinalize, &SanityFinalizeV2::Run},
  };
  return {kPhases, sizeof(kPhases) / sizeof(kPhases[0])};
}

inline bool RunSanityPhaseV2(SanityStageContextV2& pContext,
                             const SanityPhaseEntryV2& pPhase,
                             std::size_t pPhaseIndex,
                             std::size_t pPhaseCount) {
  if (pPhase.mRun == nullptr) {
    return false;
  }

  pContext.SetActivePhase(pPhase.mStage, pPhaseIndex, pPhaseCount);
  pContext.BeginWorkUnit();
  const bool aSucceeded = pPhase.mRun(pContext);
  if (aSucceeded && pContext.ActivePhaseHasError()) {
    return false;
  }
  return aSucceeded;
}

}  // namespace peanutbutter::sanity_workflow
