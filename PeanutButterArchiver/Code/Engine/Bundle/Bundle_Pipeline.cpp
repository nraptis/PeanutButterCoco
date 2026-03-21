#include "Bundle_Pipeline.hpp"

#include "Bundle_ArchiveManifest.hpp"
#include "Bundle_ArchivePacking.hpp"
#include "Bundle_Discovery.hpp"
#include "Bundle_FinalizingHeaders.hpp"
#include "Bundle_FolderManifest.hpp"
#include "Bundle_MemoryPlanning.hpp"
#include "Bundle_Preflight.hpp"
#include "Bundle_RepairPacking.hpp"

namespace peanutbutter {

BundlePipelineV2::BundlePipelineV2(const BundleRequestV2& pRequest,
                                   BundleRuntimeV2* pRuntime)
    : mContext(pRequest, pRuntime) {}

bool BundlePipelineV2::Step() {
  if (mIsFinished || mHasFailed) {
    return false;
  }

  if (mContext.IsCancelRequested()) {
    mIsFinished = true;
    return false;
  }

  const bool aSucceeded = RunCurrentPhase();
  if (!aSucceeded) {
    mHasFailed = true;
    return false;
  }

  ++mCurrentPhaseIndex;
  if (mCurrentPhaseIndex >= kPhaseCount) {
    mIsFinished = true;
  }
  return true;
}

bool BundlePipelineV2::IsFinished() const {
  return mIsFinished;
}

bool BundlePipelineV2::HasFailed() const {
  return mHasFailed;
}

const BundleWorkStateV2& BundlePipelineV2::State() const {
  return mContext.State();
}

bool BundlePipelineV2::RunCurrentPhase() {
  switch (mCurrentPhaseIndex) {
    case 0u:
      return BundlePreflightV2::Run(mContext);
    case 1u:
      return BundleDiscoveryV2::Run(mContext);
    case 2u:
      return BundleMemoryPlanningV2::Run(mContext);
    case 3u:
      return BundleArchiveManifestV2::Run(mContext);
    case 4u:
      return BundleFolderPackingV2::Run(mContext);
    case 5u:
      return BundleArchivePackingV2::Run(mContext);
    case 6u:
      return BundleRepairPackingV2::Run(mContext);
    case 7u:
      return BundleFinalizingHeadersV2::Run(mContext);
    default:
      return false;
  }
}

}  // namespace peanutbutter
