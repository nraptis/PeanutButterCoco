#pragma once

#include <cstddef>
#include <vector>

#include "../../Common/BundleRequest.hpp"
#include "Bundle_Context.hpp"

namespace peanutbutter {

class BundleDirector final {
 public:
  BundleDirector(const BundleRequestV2& pRequest,
                 BundleRuntimeV2* pRuntime);

  bool Step();
  bool IsFinished() const;
  bool HasFailed() const;
  bool WasCanceled() const;
  const BundleWorkStateV2& State() const;

 private:
  using PhaseRunner = bool (*)(BundleStageContextV2&);

  struct PhaseEntry {
    ProgressStageV2 mStage = ProgressStageV2::kIdle;
    PhaseRunner mRun = nullptr;
  };

  void BuildPhaseList();
  bool RunCurrentPhase();

 private:
  BundleStageContextV2 mContext;
  std::vector<PhaseEntry> mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  bool mIsFinished = false;
  bool mHasFailed = false;
  bool mWasCanceled = false;
};

}  // namespace peanutbutter
