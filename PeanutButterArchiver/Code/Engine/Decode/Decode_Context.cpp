#include "Decode_Context.hpp"

#include <algorithm>

namespace peanutbutter {

DecodeStageContextV2::DecodeStageContextV2(const DecodeRequestV2& pRequest,
                                           DecodeRuntimeV2* pRuntime)
    : mRequest(pRequest),
      mRuntime(pRuntime) {}

const DecodeRequestV2& DecodeStageContextV2::Request() const {
  return mRequest;
}

DecodeWorkStateV2& DecodeStageContextV2::State() {
  return mState;
}

const DecodeWorkStateV2& DecodeStageContextV2::State() const {
  return mState;
}

FileSystemV2& DecodeStageContextV2::FileSystem() {
  return mFileSystem;
}

const FileSystemV2& DecodeStageContextV2::FileSystem() const {
  return mFileSystem;
}

bool DecodeStageContextV2::IsCancelRequested() const {
  return mRuntime != nullptr && mRuntime->IsCancelRequested();
}

void DecodeStageContextV2::EmitLog(LogLevelV2 pLevel,
                                   const std::string& pMessage) const {
  if (mRuntime == nullptr) {
    return;
  }
  mRuntime->EmitLog(pLevel, pMessage);
}

void DecodeStageContextV2::SetActivePhase(ProgressStageV2 pStage,
                                          std::size_t pPhaseIndex,
                                          std::size_t pPhaseCount) {
  mActiveStage = pStage;
  mActivePhaseIndex = pPhaseIndex;
  mActivePhaseCount = std::max<std::size_t>(1u, pPhaseCount);
}

void DecodeStageContextV2::EmitPhaseProgress(double pLocalFraction,
                                             const std::string& pLabel) const {
  const double aClampedLocal = std::max(0.0, std::min(1.0, pLocalFraction));
  const double aOverall =
      (static_cast<double>(mActivePhaseIndex) + aClampedLocal) /
      static_cast<double>(std::max<std::size_t>(1u, mActivePhaseCount));
  EmitProgress(mActiveStage, aClampedLocal, aOverall, pLabel);
}

void DecodeStageContextV2::EmitProgress(ProgressStageV2 pStage,
                                        double pLocalFraction,
                                        double pOverallFraction,
                                        const std::string& pLabel) const {
  if (mRuntime == nullptr) {
    return;
  }
  mRuntime->EmitProgress(pStage, pLocalFraction, pOverallFraction, pLabel);
}

}  // namespace peanutbutter
