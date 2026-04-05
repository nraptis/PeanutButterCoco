#include "Bundle_ArchiveManifest.hpp"

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

bool BundleArchiveManifestV2::Run(BundleStageContextV2& pContext) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionV2::kBundle,
                                     ProgressStageV2::kArchiveManifest));
  BundleManifestStateV2& aManifest = pContext.State().mManifest;
  if (aManifest.mBlockCountPreview == 0u) {
    pContext.EmitLog(LogLevelV2::kInfo, LogBundleArchiveManifestNoneV2());
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseCompletedV2(LogActionV2::kBundle,
                                         ProgressStageV2::kArchiveManifest));
    pContext.EmitPhaseProgress(1.0, "Archive manifest complete");
    return !pContext.IsCancelRequested();
  }

  aManifest.mBlockCountPreview =
      pContext.State().mMemoryPlan.mBlockCountPreview;
  aManifest.mPreviewManifestBytes =
      static_cast<std::uint64_t>(aManifest.mPreviewManifestPayload.size());
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogBundleArchiveManifestSummaryV2(
                       aManifest.mPreviewManifestBytes,
                       aManifest.mBlockCountPreview));
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kArchiveManifest));
  pContext.EmitPhaseProgress(1.0, "Archive manifest complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
