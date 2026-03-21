#include "Bundle_FolderManifest.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool BundleFolderPackingV2::Run(BundleStageContextV2& pContext) {
  BundleManifestStateV2& aManifest = pContext.State().mManifest;
  const BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;
  aManifest.mFolderPackingBytes = aMemoryPlan.mEmptyFolderLogicalBytes;
  aManifest.mFolderPackingBlockCount = aMemoryPlan.mEmptyFolderBlockCount;

  pContext.EmitLog(LogLevelV2::kInfo,
                   LogBundleFolderPackingSummaryV2(aManifest.mFolderPackingBytes));
  pContext.EmitPhaseProgress(1.0, "Folder packing complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
