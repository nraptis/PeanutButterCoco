#pragma once

#include <cstddef>
#include <vector>

#include "../../Common/Progress.hpp"
#include "Bundle_Context.hpp"

namespace peanutbutter::bundle_workflow {

using BundlePhaseRunnerV2 = bool (*)(BundleStageContextV2&);

struct BundlePhaseEntryV2 {
  ProgressStageV2 mStage = ProgressStageV2::kIdle;
  BundlePhaseRunnerV2 mRun = nullptr;
};

std::vector<BundlePhaseEntryV2> BuildBundlePhaseListV2(
    const BundleRequestV2& pRequest);

bool RunBundlePhaseV2(BundleStageContextV2& pContext,
                      const BundlePhaseEntryV2& pPhase,
                      std::size_t pPhaseIndex,
                      std::size_t pPhaseCount);

bool ShouldDeferBundleCancelForPhaseV2(const BundleWorkStateV2& pState,
                                       ProgressStageV2 pStage);

std::size_t FindBundlePhaseIndexV2(
    const std::vector<BundlePhaseEntryV2>& pPhases,
    ProgressStageV2 pStage);

}  // namespace peanutbutter::bundle_workflow
