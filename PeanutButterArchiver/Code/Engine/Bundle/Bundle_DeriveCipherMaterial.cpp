#include "Bundle_DeriveCipherMaterial.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool BundleDeriveCipherMaterialV2::Run(BundleStageContextV2& pContext) {
  if (!pContext.Request().mEncryptionEnabled) {
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseSkippedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kDeriveCipherMaterial,
                                       "encryption is disabled"));
    pContext.EmitPhaseProgress(1.0, "Derive cipher material skipped");
    return !pContext.IsCancelRequested();
  }

  pContext.State().mCipher.mDerived = true;
  pContext.EmitLog(LogLevelV2::kInfo, LogBundleCipherDeriveDetailV2());
  pContext.EmitPhaseProgress(1.0, "Cipher material derived");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
