#include "Decode_AssembleCipherStack.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool DecodeAssembleCipherStackV2::Run(DecodeStageContextV2& pContext) {
  if (pContext.State().mBootstrap.mFirstHeader.mIsEncrypted == 0u) {
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseSkippedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                       ProgressStageV2::kAssembleCipherStack,
                                       "archive header says plaintext"));
    pContext.EmitPhaseProgress(1.0, "Cipher stack skipped");
    return true;
  }
  if (!pContext.State().mCipher.mDerived) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kAssembleCipherStack,
                                      "decode cipher material was not derived"));
    return false;
  }

  pContext.State().mCipher.mCipher = RotationMaskCipherV2(
      pContext.Request().mPassword,
      static_cast<StrengthPresetV2>(pContext.State().mBootstrap.mFirstHeader.mCipherProfile),
      static_cast<StrengthPresetV2>(pContext.State().mBootstrap.mFirstHeader.mExpanderProfile));
  pContext.State().mCipher.mAssembled =
      pContext.State().mCipher.mCipher.IsConfigured();
  if (pContext.State().mCipher.mAssembled &&
      !pContext.State().mCipher.mWorkerBuffer.Resize(
          pContext.Layout().mArchiveBlockBytes)) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                         ProgressStageV2::kAssembleCipherStack,
                         "failed allocating worker buffer for decode cipher operations"));
    pContext.State().mCipher.mAssembled = false;
    return false;
  }

  pContext.EmitLog(LogLevelV2::kInfo, LogBundleCipherAssembledV2());
  pContext.EmitPhaseProgress(1.0, "Cipher stack assembled");
  return pContext.State().mCipher.mAssembled && !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
