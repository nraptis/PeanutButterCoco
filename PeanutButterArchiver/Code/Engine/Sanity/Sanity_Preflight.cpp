#include "Sanity_Preflight.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool SanityPreflightV2::Run(SanityStageContextV2& pContext) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionV2::kSanity, ProgressStageV2::kPreflight));

  if (pContext.Request().mLeftDirectory.empty() ||
      pContext.Request().mRightDirectory.empty()) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kSanity,
                                      ProgressStageV2::kPreflight,
                                      "both compare directories are required"));
    return false;
  }
  if (!pContext.FileSystem().IsDirectory(pContext.Request().mLeftDirectory) ||
      !pContext.FileSystem().IsDirectory(pContext.Request().mRightDirectory)) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kSanity,
                                      ProgressStageV2::kPreflight,
                                      "both compare paths must be existing directories"));
    return false;
  }

  pContext.EmitPhaseProgress(1.0, ProgressStageLabelV2(ProgressStageV2::kPreflight));
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kSanity, ProgressStageV2::kPreflight));
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
