#include "Decode_Execution.hpp"

#include "Decode_Director.hpp"

namespace peanutbutter {
namespace {

template <typename DirectorT>
DecodeExecutionResultV2 ExecuteDirectorV2(DirectorT& pDirector) {
  while (!pDirector.IsFinished() &&
         !pDirector.HasFailed() &&
         !pDirector.WasCanceled()) {
    (void)pDirector.Step();
  }

  DecodeExecutionResultV2 aResult;
  aResult.mState = pDirector.State();
  if (pDirector.HasFailed()) {
    aResult.mStatus = DecodeExecutionStatusV2::kFailed;
    aResult.mFailureMessage = pDirector.FailureMessage();
    if (aResult.mFailureMessage.empty()) {
      aResult.mFailureMessage = "operation failed without explicit failure detail";
    }
    return aResult;
  }
  if (pDirector.WasCanceled()) {
    aResult.mStatus = DecodeExecutionStatusV2::kCanceled;
    return aResult;
  }
  aResult.mStatus = DecodeExecutionStatusV2::kCompleted;
  return aResult;
}

}  // namespace

bool DecodeExecutionResultV2::Succeeded() const {
  return mStatus == DecodeExecutionStatusV2::kCompleted;
}

bool DecodeExecutionResultV2::Failed() const {
  return mStatus == DecodeExecutionStatusV2::kFailed;
}

bool DecodeExecutionResultV2::Canceled() const {
  return mStatus == DecodeExecutionStatusV2::kCanceled;
}

DecodeExecutionResultV2 ExecuteDecodeV2(
    const DecodeRequestV2& pRequest,
    DecodeRuntimeV2* pRuntime,
    FileSystemV2* pFileSystem,
    const memory_layout::ArchiveLayoutConfigV2* pLayout) {
  DecodeDirector aDirector(pRequest, pRuntime, pFileSystem, pLayout);
  return ExecuteDirectorV2(aDirector);
}

DecodeExecutionResultV2 ExecuteManifestV2(
    const DecodeRequestV2& pRequest,
    DecodeRuntimeV2* pRuntime,
    FileSystemV2* pFileSystem,
    const memory_layout::ArchiveLayoutConfigV2* pLayout) {
  DecodeRequestV2 aRequest = pRequest;
  aRequest.mIntent = DecodeIntentV2::kManifest;
  ManifestDirector aDirector(aRequest, pRuntime, pFileSystem, pLayout);
  return ExecuteDirectorV2(aDirector);
}

DecodeExecutionResultV2 ExecuteRepairV2(
    const RepairRequestV2& pRequest,
    DecodeRuntimeV2* pRuntime,
    FileSystemV2* pFileSystem,
    const memory_layout::ArchiveLayoutConfigV2* pLayout) {
  RepairDirector aDirector(pRequest, pRuntime, pFileSystem, pLayout);
  return ExecuteDirectorV2(aDirector);
}

}  // namespace peanutbutter
