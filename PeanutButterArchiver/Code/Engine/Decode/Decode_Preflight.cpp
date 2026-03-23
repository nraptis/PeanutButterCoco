#include "Decode_Preflight.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool DecodePreflightV2::Run(DecodeStageContextV2& pContext) {
  std::string aLayoutError;
  if (!pContext.Layout().IsValid(&aLayoutError)) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                         ProgressStageV2::kPreflight,
                         "archive layout is invalid: " + aLayoutError));
    return false;
  }

  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                     ProgressStageV2::kPreflight));

  if (pContext.Request().mSourcePath.empty()) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kPreflight, "source path is empty"));
    return false;
  }
  if (pContext.Request().mDestinationDirectory.empty()) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kPreflight, "destination directory is empty"));
    return false;
  }
  if (!pContext.FileSystem().Exists(pContext.Request().mSourcePath)) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kPreflight, "source path does not exist"));
    return false;
  }
  if (!pContext.FileSystem().EnsureDirectory(
          pContext.Request().mDestinationDirectory)) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kPreflight, "destination directory could not be created"));
    return false;
  }

  pContext.EmitPhaseProgress(1.0, "Preflight complete");
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                       ProgressStageV2::kPreflight));
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
