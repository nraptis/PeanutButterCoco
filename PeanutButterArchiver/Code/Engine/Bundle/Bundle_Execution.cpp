#include "Bundle_Execution.hpp"

#include "Bundle_Workflow.hpp"

namespace peanutbutter {
namespace {

BundleExecutionResultV2 BuildBundleExecutionResultV2(
    const BundleStageContextV2& pContext,
    BundleExecutionStatusV2 pStatus) {
  BundleExecutionResultV2 aResult;
  aResult.mStatus = pStatus;
  aResult.mState = pContext.State();
  if (pStatus == BundleExecutionStatusV2::kFailed) {
    aResult.mFailureMessage = pContext.LastErrorLog();
  }
  return aResult;
}

}  // namespace

bool BundleExecutionResultV2::Succeeded() const {
  return mStatus == BundleExecutionStatusV2::kCompleted;
}

bool BundleExecutionResultV2::Failed() const {
  return mStatus == BundleExecutionStatusV2::kFailed;
}

bool BundleExecutionResultV2::Canceled() const {
  return mStatus == BundleExecutionStatusV2::kCanceled;
}

const std::vector<std::string>& BundleExecutionResultV2::ArchivePaths() const {
  return mState.mPacking.mArchivePaths;
}

BundleExecutionResultV2 ExecuteBundleV2(BundleStageContextV2& pContext) {
  const std::vector<bundle_workflow::BundlePhaseEntryV2> aPhases =
      bundle_workflow::BuildBundlePhaseListV2(pContext.Request());

  std::size_t aCurrentPhaseIndex = 0u;
  while (aCurrentPhaseIndex < aPhases.size()) {
    const bundle_workflow::BundlePhaseEntryV2& aPhase = aPhases[aCurrentPhaseIndex];

    if (pContext.IsCancelRequested() &&
        !bundle_workflow::ShouldDeferBundleCancelForPhaseV2(pContext.State(),
                                                            aPhase.mStage)) {
      return BuildBundleExecutionResultV2(pContext,
                                          BundleExecutionStatusV2::kCanceled);
    }

    if (!bundle_workflow::RunBundlePhaseV2(pContext,
                                           aPhase,
                                           aCurrentPhaseIndex,
                                           aPhases.size())) {
      if (pContext.IsCancelRequested() &&
          !bundle_workflow::ShouldDeferBundleCancelForPhaseV2(pContext.State(),
                                                              aPhase.mStage)) {
        return BuildBundleExecutionResultV2(
            pContext, BundleExecutionStatusV2::kCanceled);
      }
      return BuildBundleExecutionResultV2(pContext,
                                          BundleExecutionStatusV2::kFailed);
    }

    if (pContext.ActivePhaseNeedsMoreHeartbeats()) {
      continue;
    }

    if (pContext.IsCancelRequested()) {
      if (pContext.State().mCancel.mShouldFinalizeAfterCancel &&
          aPhase.mStage != ProgressStageV2::kFinalizingHeaders) {
        aCurrentPhaseIndex = bundle_workflow::FindBundlePhaseIndexV2(
            aPhases, ProgressStageV2::kFinalizingHeaders);
        continue;
      }
      return BuildBundleExecutionResultV2(pContext,
                                          BundleExecutionStatusV2::kCanceled);
    }

    ++aCurrentPhaseIndex;
  }

  return BuildBundleExecutionResultV2(pContext,
                                      BundleExecutionStatusV2::kCompleted);
}

BundleExecutionResultV2 ExecuteBundleV2(
    const BundleRequestV2& pRequest,
    BundleRuntimeV2* pRuntime,
    FileSystemV2* pFileSystem,
    const memory_layout::ArchiveLayoutConfigV2* pLayout) {
  BundleStageContextV2 aContext(pRequest, pRuntime, pFileSystem, pLayout);
  return ExecuteBundleV2(aContext);
}

}  // namespace peanutbutter
