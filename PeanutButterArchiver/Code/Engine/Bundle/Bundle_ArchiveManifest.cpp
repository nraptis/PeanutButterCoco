#include "Bundle_ArchiveManifest.hpp"

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

bool BundleArchiveManifestV2::Run(BundleStageContextV2& pContext) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionV2::kBundle,
                                     ProgressStageV2::kArchiveManifest));
  BundleManifestStateV2& aManifest = pContext.State().mManifest;
  if (aManifest.mPreviewManifestBlockCount == 0u) {
    pContext.EmitLog(LogLevelV2::kInfo, LogBundleArchiveManifestNoneV2());
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseCompletedV2(LogActionV2::kBundle,
                                         ProgressStageV2::kArchiveManifest));
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
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kArchiveManifest));
  pContext.EmitPhaseProgress(1.0, "Archive manifest complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
