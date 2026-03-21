#include "Decode_DeriveCipherMaterial.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool DecodeDeriveCipherMaterialV2::Run(DecodeStageContextV2& pContext) {
  if (!pContext.State().mBootstrap.mHeaderRead) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kDeriveCipherMaterial,
                                      "bootstrap header was not read"));
    return false;
  }

  if (pContext.State().mBootstrap.mFirstHeader.mIsEncrypted == 0u) {
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseSkippedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                       ProgressStageV2::kDeriveCipherMaterial,
                                       "archive header says plaintext"));
    pContext.EmitPhaseProgress(1.0, "Derive cipher material skipped");
    return true;
  }

  if (!pContext.Request().mEncryptionEnabled) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kDeriveCipherMaterial,
                                      "encryption is disabled in the request"));
    return false;
  }

  pContext.State().mCipher.mDerived = true;
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                     ProgressStageV2::kDeriveCipherMaterial));
  pContext.EmitPhaseProgress(1.0, "Cipher material derived");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
