#include "Bundle_Preflight.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

namespace {

std::string BoolLabel(bool value) {
  return value ? "true" : "false";
}

}

bool BundlePreflightV2::Run(BundleStageContextV2& pContext) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionV2::kBundle, ProgressStageV2::kPreflight));

  if (pContext.Request().mSourceDirectory.empty()) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kPreflight,
                                      "source directory is empty"));
    return false;
  }

  if (pContext.Request().mDestinationDirectory.empty()) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kPreflight,
                                      "destination directory is empty"));
    return false;
  }

  const bool aSourceExists = pContext.FileSystem().Exists(pContext.Request().mSourceDirectory);
  const bool aSourceIsDirectory = pContext.FileSystem().IsDirectory(pContext.Request().mSourceDirectory);
  const bool aSourceIsFile = pContext.FileSystem().IsFile(pContext.Request().mSourceDirectory);
  pContext.EmitLog(LogLevelV2::kInfo,
                   "[Bundle][Preflight] Source probe: path='" +
                       pContext.Request().mSourceDirectory + "', exists=" +
                       BoolLabel(aSourceExists) + ", is_directory=" +
                       BoolLabel(aSourceIsDirectory) + ", is_file=" +
                       BoolLabel(aSourceIsFile) + ".");

  if (!pContext.FileSystem().Exists(pContext.Request().mSourceDirectory) ||
      (!pContext.FileSystem().IsDirectory(pContext.Request().mSourceDirectory) &&
       !pContext.FileSystem().IsFile(pContext.Request().mSourceDirectory))) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kPreflight,
                                      "source path is not a readable file or folder: '" +
                                          pContext.Request().mSourceDirectory + "'"));
    return false;
  }

  const bool aDestinationExists =
      pContext.FileSystem().Exists(pContext.Request().mDestinationDirectory);
  const bool aDestinationIsDirectory =
      pContext.FileSystem().IsDirectory(pContext.Request().mDestinationDirectory);
  pContext.EmitLog(LogLevelV2::kInfo,
                   "[Bundle][Preflight] Destination probe: path='" +
                       pContext.Request().mDestinationDirectory + "', exists=" +
                       BoolLabel(aDestinationExists) + ", is_directory=" +
                       BoolLabel(aDestinationIsDirectory) + ".");

  if (aDestinationExists && !aDestinationIsDirectory) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kPreflight,
                                      "destination path exists and is not a directory"));
    return false;
  }

  if (pContext.Request().mClearDestinationBeforeWrite &&
      aDestinationExists && aDestinationIsDirectory &&
      !pContext.FileSystem().ClearDirectory(
          pContext.Request().mDestinationDirectory)) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kPreflight,
                                      "destination directory could not be cleared"));
    return false;
  }

  if (!pContext.FileSystem().EnsureDirectory(
          pContext.Request().mDestinationDirectory)) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kPreflight,
                                      "destination directory could not be created"));
    return false;
  }

  pContext.EmitPhaseProgress(1.0, "Preflight complete");
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle, ProgressStageV2::kPreflight));
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
