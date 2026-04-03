#include "Bundle_AssembleCipherStack.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool BundleAssembleCipherStackV2::Run(BundleStageContextV2& pContext) {
  if (!pContext.Request().mEncryptionEnabled) {
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseSkippedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kAssembleCipherStack,
                                       "encryption is disabled"));
    pContext.EmitPhaseProgress(1.0, "Cipher stack skipped");
    return !pContext.IsCancelRequested();
  }
  if (!pContext.State().mCipher.mDerived) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kBundle,
                                      ProgressStageV2::kAssembleCipherStack,
                                      "cipher material was not derived"));
    return false;
  }

  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionV2::kBundle,
                                     ProgressStageV2::kAssembleCipherStack));
  pContext.State().mCipher.mCipher = RotationMaskCipherV2(
      pContext.Request().mPassword,
      pContext.Request().mEncryptionStrength,
      pContext.Request().mTableStrength);
  pContext.State().mCipher.mAssembled =
      pContext.State().mCipher.mCipher.IsConfigured();
  if (pContext.State().mCipher.mAssembled &&
      !pContext.State().mCipher.mWorkerBuffer.Resize(
          pContext.Layout().mArchiveBlockBytes)) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionV2::kBundle,
                         ProgressStageV2::kAssembleCipherStack,
                         "failed allocating worker buffer for cipher operations"));
    pContext.State().mCipher.mAssembled = false;
    return false;
  }
  pContext.EmitLog(LogLevelV2::kInfo, LogBundleCipherAssembledV2());
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kAssembleCipherStack));
  pContext.EmitPhaseProgress(1.0, "Cipher stack assembled");
  return pContext.State().mCipher.mAssembled && !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
