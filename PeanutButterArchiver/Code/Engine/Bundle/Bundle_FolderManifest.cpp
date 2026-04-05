#include "Bundle_FolderManifest.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool BundleFolderPackingV2::Run(BundleStageContextV2& pContext) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionV2::kBundle,
                                     ProgressStageV2::kFolderPacking));
  BundleManifestStateV2& aManifest = pContext.State().mManifest;
  const BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;
  aManifest.mFolderPackingBytes = aMemoryPlan.mEmptyFolderLogicalBytes;
  aManifest.mFolderPackingBlockCount = 0u;

  pContext.EmitLog(LogLevelV2::kInfo,
                   LogBundleFolderPackingSummaryV2(aManifest.mFolderPackingBytes));
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kFolderPacking));
  pContext.EmitPhaseProgress(1.0, "Folder packing complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
