#include "Bundle_Context.hpp"

#include <algorithm>

namespace peanutbutter {

BundleStageContextV2::BundleStageContextV2(const BundleRequestV2& pRequest,
                                           BundleRuntimeV2* pRuntime,
                                           FileSystemV2* pFileSystem,
                                           const memory_layout::ArchiveLayoutConfigV2* pLayout)
    : mRequest(pRequest),
      mRuntime(pRuntime),
      mFileSystem(pFileSystem != nullptr ? pFileSystem : &mLocalFileSystem),
      mLayout(pLayout != nullptr ? pLayout : &memory_layout::DefaultArchiveLayoutConfigV2()) {}

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
  return *mFileSystem;
}

const FileSystemV2& BundleStageContextV2::FileSystem() const {
  return *mFileSystem;
}

const memory_layout::ArchiveLayoutConfigV2& BundleStageContextV2::Layout() const {
  return *mLayout;
}

bool BundleStageContextV2::IsCancelRequested() const {
  return mRuntime != nullptr && mRuntime->IsCancelRequested();
}

void BundleStageContextV2::EmitLog(LogLevelV2 pLevel,
                                   const std::string& pMessage) const {
  if (pLevel == LogLevelV2::kError) {
    const_cast<BundleStageContextV2*>(this)->mActivePhaseHasError = true;
    BundleStageContextV2* const aMutableThis =
        const_cast<BundleStageContextV2*>(this);
    if (aMutableThis->mLastErrorLog.empty()) {
      aMutableThis->mLastErrorLog = pMessage;
    }
    if (!aMutableThis->mState.mFailure.HasFailure()) {
      aMutableThis->mState.mFailure.mFamily =
          InferFailureFamilyV2(aMutableThis->mActiveStage, pMessage);
      aMutableThis->mState.mFailure.mStage = aMutableThis->mActiveStage;
      aMutableThis->mState.mFailure.mWorkUnit = aMutableThis->mState.mWorkUnitsProcessed;
      aMutableThis->mState.mFailure.mMessage = pMessage;
    }
  }
  if (mRuntime != nullptr) {
    mRuntime->EmitLog(pLevel, pMessage);
  }
}

bool BundleStageContextV2::WantsRuntimeEvent(RuntimeEventKindV2 pKind) const {
  return mRuntime != nullptr && mRuntime->WantsRuntimeEvent(pKind);
}

bool BundleStageContextV2::EmitRuntimeEvent(const RuntimeEventV2& pEvent) const {
  if (mRuntime == nullptr || !mRuntime->WantsRuntimeEvent(pEvent.mKind)) {
    return true;
  }

  RuntimeEventV2 aEvent = pEvent;
  TrackRuntimeEventTransferV2(
      aEvent,
      const_cast<BundleStageContextV2*>(this)->mState.mTransfers);
  ScrubRuntimeEventFileInfoV2(aEvent);
  if (aEvent.mStage == ProgressStageV2::kIdle) {
    aEvent.mStage = mActiveStage;
  }
  return mRuntime->EmitRuntimeEvent(aEvent);
}

void BundleStageContextV2::SetActivePhase(ProgressStageV2 pStage,
                                          std::size_t pPhaseIndex,
                                          std::size_t pPhaseCount) {
  mActiveStage = pStage;
  mActivePhaseIndex = pPhaseIndex;
  mActivePhaseCount = std::max<std::size_t>(1u, pPhaseCount);
  mActivePhaseNeedsMoreHeartbeats = false;
  mActivePhaseHasError = false;
  mLastErrorLog.clear();
}

void BundleStageContextV2::BeginWorkUnit() {
  ++mState.mWorkUnitsProcessed;
}

void BundleStageContextV2::ContinuePhaseOnNextHeartbeat() {
  mActivePhaseNeedsMoreHeartbeats = true;
}

bool BundleStageContextV2::ActivePhaseNeedsMoreHeartbeats() const {
  return mActivePhaseNeedsMoreHeartbeats;
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
  if (mState.mFailure.HasFailure() && !mState.mFailure.mMessage.empty()) {
    return mState.mFailure.mMessage;
  }
  return mLastErrorLog;
}

const FailureInfoV2& BundleStageContextV2::Failure() const {
  return mState.mFailure;
}

}  // namespace peanutbutter
