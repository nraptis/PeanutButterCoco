#pragma once

#include <cstddef>

#include "Sanity_Workflow.hpp"

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
  void BuildPhaseList();
  bool RunCurrentPhase();

  SanityStageContextV2 mContext;
  sanity_workflow::SanityPhaseListViewV2 mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  bool mIsFinished = false;
  bool mHasFailed = false;
  bool mWasCanceled = false;
};

}  // namespace peanutbutter
