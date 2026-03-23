#include "Decode_Finalize.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool DecodeFinalizeV2::Run(DecodeStageContextV2& pContext) {
  pContext.EmitLog(
      LogLevelV2::kInfo,
      LogDecodeFinalizeSummaryV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                 pContext.State().mOutput.mBytesWritten));
  pContext.EmitPhaseProgress(1.0, "Finalize complete");
  return true;
}

}  // namespace peanutbutter
