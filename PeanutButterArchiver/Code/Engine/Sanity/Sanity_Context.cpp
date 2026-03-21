#include "Sanity_Context.hpp"

#include <algorithm>

namespace peanutbutter {

SanityStageContextV2::SanityStageContextV2(const SanityRequestV2& pRequest,
                                           SanityRuntimeV2* pRuntime)
    : mRequest(pRequest),
      mRuntime(pRuntime) {}

const SanityRequestV2& SanityStageContextV2::Request() const {
  return mRequest;
}

SanityWorkStateV2& SanityStageContextV2::State() {
  return mState;
}

const SanityWorkStateV2& SanityStageContextV2::State() const {
  return mState;
}

FileSystemV2& SanityStageContextV2::FileSystem() {
  return mFileSystem;
}

const FileSystemV2& SanityStageContextV2::FileSystem() const {
  return mFileSystem;
}

bool SanityStageContextV2::IsCancelRequested() const {
  return mRuntime != nullptr && mRuntime->IsCancelRequested();
}

void SanityStageContextV2::EmitLog(LogLevelV2 pLevel,
                                   const std::string& pMessage) const {
  if (mRuntime != nullptr) {
    mRuntime->EmitLog(pLevel, pMessage);
  }
}

void SanityStageContextV2::SetActivePhase(ProgressStageV2 pStage,
                                          std::size_t pPhaseIndex,
                                          std::size_t pPhaseCount) {
  mActiveStage = pStage;
  mActivePhaseIndex = pPhaseIndex;
  mActivePhaseCount = std::max<std::size_t>(1u, pPhaseCount);
}

void SanityStageContextV2::EmitPhaseProgress(double pLocalFraction,
                                             const std::string& pLabel) const {
  if (mRuntime == nullptr) {
    return;
  }
  const double aClampedLocal = std::max(0.0, std::min(1.0, pLocalFraction));
  const double aOverall =
      (static_cast<double>(mActivePhaseIndex) + aClampedLocal) /
      static_cast<double>(std::max<std::size_t>(1u, mActivePhaseCount));
  mRuntime->EmitProgress(mActiveStage, aClampedLocal, aOverall, pLabel);
}

}  // namespace peanutbutter
