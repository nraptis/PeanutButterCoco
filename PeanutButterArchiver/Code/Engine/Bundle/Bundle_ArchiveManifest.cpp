#include "Bundle_ArchiveManifest.hpp"

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

bool BundleArchiveManifestV2::Run(BundleStageContextV2& pContext) {
  BundleManifestStateV2& aManifest = pContext.State().mManifest;
  if (aManifest.mPreviewManifestBlockCount == 0u ||
      aManifest.mPreviewManifestPayload.empty()) {
    pContext.EmitLog(LogLevelV2::kInfo, LogBundleArchiveManifestNoneV2());
    pContext.EmitPhaseProgress(1.0, "Archive manifest complete");
    return !pContext.IsCancelRequested();
  }

  aManifest.mPreviewManifestBlockCount =
      pContext.State().mMemoryPlan.mPreviewManifestBlockCount;
  aManifest.mPreviewManifestBytes =
      static_cast<std::uint64_t>(aManifest.mPreviewManifestPayload.size());
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogBundleArchiveManifestSummaryV2(
                       aManifest.mPreviewManifestBytes,
                       aManifest.mPreviewManifestBlockCount));
  pContext.EmitPhaseProgress(1.0, "Archive manifest complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
