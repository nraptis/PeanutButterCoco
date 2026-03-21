#include "Sanity_Finalize.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool SanityFinalizeV2::Run(SanityStageContextV2& pContext) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kSanity, ProgressStageV2::kFinalize));
  pContext.EmitPhaseProgress(1.0, ProgressStageLabelV2(ProgressStageV2::kFinalize));
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
