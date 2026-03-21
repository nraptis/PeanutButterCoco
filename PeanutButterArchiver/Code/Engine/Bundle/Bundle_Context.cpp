#include "Bundle_Context.hpp"

#include <algorithm>

namespace peanutbutter {

BundleStageContextV2::BundleStageContextV2(const BundleRequestV2& pRequest,
                                           BundleRuntimeV2* pRuntime)
    : mRequest(pRequest),
      mRuntime(pRuntime) {}

const BundleRequestV2& BundleStageContextV2::Request() const {
  return mRequest;
}

BundleWorkStateV2& BundleStageContextV2::State() {
  return mState;
}

const BundleWorkStateV2& BundleStageContextV2::State() const {
  return mState;
}

FileSystemV2& BundleStageContextV2::FileSystem() {
  return mFileSystem;
}

const FileSystemV2& BundleStageContextV2::FileSystem() const {
  return mFileSystem;
}

bool BundleStageContextV2::IsCancelRequested() const {
  return mRuntime != nullptr && mRuntime->IsCancelRequested();
}

void BundleStageContextV2::EmitLog(LogLevelV2 pLevel,
                                   const std::string& pMessage) const {
  if (mRuntime == nullptr) {
    return;
  }
  if (pLevel == LogLevelV2::kError) {
    const_cast<BundleStageContextV2*>(this)->mActivePhaseHasError = true;
    const_cast<BundleStageContextV2*>(this)->mLastErrorLog = pMessage;
  }
  mRuntime->EmitLog(pLevel, pMessage);
}

void BundleStageContextV2::SetActivePhase(ProgressStageV2 pStage,
                                          std::size_t pPhaseIndex,
                                          std::size_t pPhaseCount) {
  mActiveStage = pStage;
  mActivePhaseIndex = pPhaseIndex;
  mActivePhaseCount = std::max<std::size_t>(1u, pPhaseCount);
  mActivePhaseHasError = false;
  mLastErrorLog.clear();
}

void BundleStageContextV2::EmitPhaseProgress(double pLocalFraction,
                                             const std::string& pLabel) const {
  const double aClampedLocal = std::max(0.0, std::min(1.0, pLocalFraction));
  const double aOverall =
      (static_cast<double>(mActivePhaseIndex) + aClampedLocal) /
      static_cast<double>(std::max<std::size_t>(1u, mActivePhaseCount));
  EmitProgress(mActiveStage, aClampedLocal, aOverall, pLabel);
}

void BundleStageContextV2::EmitProgress(ProgressStageV2 pStage,
                                        double pLocalFraction,
                                        double pOverallFraction,
                                        const std::string& pLabel) const {
  if (mRuntime == nullptr) {
    return;
  }
  mRuntime->EmitProgress(pStage, pLocalFraction, pOverallFraction, pLabel);
}

bool BundleStageContextV2::ActivePhaseHasError() const {
  return mActivePhaseHasError;
}

const std::string& BundleStageContextV2::LastErrorLog() const {
  return mLastErrorLog;
}

}  // namespace peanutbutter
