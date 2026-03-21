#include "Bundle_RepairPacking.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool BundleRepairPackingV2::Run(BundleStageContextV2& pContext) {
  BundlePackingStateV2& aPacking = pContext.State().mPacking;

  aPacking.mRepairPackedBlockCount = 0u;

  if (pContext.Request().mRepairEnabled) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        LogPhaseSkippedV2(LogActionV2::kBundle, ProgressStageV2::kRepairPacking,
                          "repair packing is not emitted yet; header repair-sector count stays zero"));
  } else {
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseSkippedV2(LogActionV2::kBundle, ProgressStageV2::kRepairPacking,
                                       "repair is disabled"));
  }
  pContext.EmitPhaseProgress(1.0, "Repair packing complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
