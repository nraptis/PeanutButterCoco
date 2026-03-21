#include "Decode_Finalize.hpp"

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

bool DecodeFinalizeV2::Run(DecodeStageContextV2& pContext) {
  pContext.EmitLog(
      LogLevelV2::kInfo,
      LogDecodeFinalizeSummaryV2(pContext.State().mOutput.mBytesWritten));
  pContext.EmitPhaseProgress(1.0, "Finalize complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
