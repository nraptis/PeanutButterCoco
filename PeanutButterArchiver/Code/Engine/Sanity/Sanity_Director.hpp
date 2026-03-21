#pragma once

#include <cstddef>
#include <vector>

#include "Sanity_Context.hpp"

namespace peanutbutter {

class SanityDirector final {
 public:
  SanityDirector(const SanityRequestV2& pRequest,
                 SanityRuntimeV2* pRuntime);

  bool Step();
  bool IsFinished() const;
  bool HasFailed() const;
  bool WasCanceled() const;

 private:
  using PhaseRunner = bool (*)(SanityStageContextV2&);
  struct PhaseEntry {
    ProgressStageV2 mStage = ProgressStageV2::kIdle;
    PhaseRunner mRun = nullptr;
  };
  void BuildPhaseList();
  bool RunCurrentPhase();

  SanityStageContextV2 mContext;
  std::vector<PhaseEntry> mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  bool mIsFinished = false;
  bool mHasFailed = false;
  bool mWasCanceled = false;
};

}  // namespace peanutbutter
