#pragma once

#include "../../Common/BundleRequest.hpp"
#include "Bundle_Context.hpp"

namespace peanutbutter {

class BundlePipelineV2 {
 public:
  static constexpr std::size_t kPhaseCount = 8u;

  BundlePipelineV2(const BundleRequestV2& pRequest,
                   BundleRuntimeV2* pRuntime);

  bool Step();
  bool IsFinished() const;
  bool HasFailed() const;
  const BundleWorkStateV2& State() const;

 private:
  bool RunCurrentPhase();

 private:
  BundleStageContextV2 mContext;
  std::size_t mCurrentPhaseIndex = 0u;
  bool mIsFinished = false;
  bool mHasFailed = false;
};

}  // namespace peanutbutter
