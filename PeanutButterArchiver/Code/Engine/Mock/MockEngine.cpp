#include "MockEngine.hpp"

#include <algorithm>

namespace peanutbutter {

namespace {

constexpr auto kMockRunDuration = std::chrono::seconds(10);
constexpr auto kMockCancelDelay = std::chrono::seconds(3);

const char* ScenarioLabel(MockScenarioV2 pScenario) {
  switch (pScenario) {
    case MockScenarioV2::kGreenDelaySucceed:
      return "Test-Green-Delay-Succeed";
    case MockScenarioV2::kGreenDelayFail:
      return "Test-Green-Delay-Fail";
    case MockScenarioV2::kYellowDelaySucceed:
      return "Test-Yellow-Delay-Succeed";
    case MockScenarioV2::kYellowDelayFail:
      return "Test-Yellow-Delay-Fail";
    case MockScenarioV2::kRed:
      return "Test-Red";
  }
  return "Unknown";
}

bool ScenarioIsYellow(MockScenarioV2 pScenario) {
  switch (pScenario) {
    case MockScenarioV2::kYellowDelaySucceed:
    case MockScenarioV2::kYellowDelayFail:
      return true;
    case MockScenarioV2::kGreenDelaySucceed:
    case MockScenarioV2::kGreenDelayFail:
    case MockScenarioV2::kRed:
      return false;
  }
  return false;
}

}  // namespace

void MockEngine::EnqueueScenario(MockScenarioV2 pScenario) {
  std::lock_guard<std::mutex> aLock(mMutex);
  mIncomingScenarios.push(pScenario);
}

void MockEngine::EnqueuePromptResponse(const UiPromptResponseV2& pResponse) {
  std::lock_guard<std::mutex> aLock(mMutex);
  mIncomingPromptResponses.push(pResponse);
}

void MockEngine::EnqueueCancelRequest() {
  std::lock_guard<std::mutex> aLock(mMutex);
  mIncomingCancelRequest = true;
}

EngineEventListV2 MockEngine::Poll() {
  std::lock_guard<std::mutex> aLock(mMutex);
  ProcessIncomingLocked();
  ProcessActiveLocked();
  EngineEventListV2 aEvents = std::move(mPendingEvents);
  mPendingEvents.clear();
  return aEvents;
}

EngineSnapshotV2 MockEngine::Snapshot() const {
  std::lock_guard<std::mutex> aLock(mMutex);
  return BuildSnapshotLocked();
}

void MockEngine::ProcessIncomingLocked() {
  while (!mIncomingPromptResponses.empty()) {
    const UiPromptResponseV2 aResponse = mIncomingPromptResponses.front();
    mIncomingPromptResponses.pop();
    HandlePromptResponseLocked(aResponse);
  }

  if (mIncomingCancelRequest) {
    mIncomingCancelRequest = false;
    AcceptCancelLocked();
  }

  while (!mIncomingScenarios.empty()) {
    const MockScenarioV2 aScenario = mIncomingScenarios.front();
    mIncomingScenarios.pop();

    if (!mActiveScenario.has_value()) {
      AcceptScenarioLocked(aScenario);
    } else {
      RejectScenarioLocked(aScenario, "Primary action already locked.");
    }
  }
}

void MockEngine::ProcessActiveLocked() {
  if (!mActiveScenario.has_value()) {
    return;
  }

  ActiveScenarioV2& aScenario = *mActiveScenario;
  if (aScenario.mPhase == ActivePhaseV2::kPrompting) {
    return;
  }

  const auto aNow = std::chrono::steady_clock::now();
  const auto aElapsed = aNow - aScenario.mRunStartedAt;
  const bool aNaturalFinishReached = aElapsed >= kMockRunDuration;
  const double aFraction =
      std::clamp(std::chrono::duration<double>(aElapsed).count() /
                     std::chrono::duration<double>(kMockRunDuration).count(),
                 0.0, 1.0);
  EmitProgressLocked(aFraction, ScenarioLabel(aScenario.mScenario));

  if (aScenario.mPhase == ActivePhaseV2::kCancelPending) {
    if (aNaturalFinishReached) {
      switch (aScenario.mScenario) {
        case MockScenarioV2::kGreenDelaySucceed:
        case MockScenarioV2::kYellowDelaySucceed:
          FinishLocked(EngineEventTypeV2::kActionCompleted,
                       std::string(ScenarioLabel(aScenario.mScenario)) +
                           " completed while draining cancel.");
          break;
        case MockScenarioV2::kGreenDelayFail:
        case MockScenarioV2::kYellowDelayFail:
        case MockScenarioV2::kRed:
          FinishLocked(EngineEventTypeV2::kActionFailed,
                       std::string(ScenarioLabel(aScenario.mScenario)) +
                           " failed while draining cancel.");
          break;
      }
      return;
    }

    if (aNow - aScenario.mCancelRequestedAt >= kMockCancelDelay) {
      FinishLocked(EngineEventTypeV2::kActionCanceled,
                   std::string(ScenarioLabel(aScenario.mScenario)) + " canceled.");
    }
    return;
  }

  if (!aNaturalFinishReached) {
    return;
  }

  switch (aScenario.mScenario) {
    case MockScenarioV2::kGreenDelaySucceed:
    case MockScenarioV2::kYellowDelaySucceed:
      FinishLocked(EngineEventTypeV2::kActionCompleted,
                   std::string(ScenarioLabel(aScenario.mScenario)) + " completed.");
      break;
    case MockScenarioV2::kGreenDelayFail:
    case MockScenarioV2::kYellowDelayFail:
    case MockScenarioV2::kRed:
      FinishLocked(EngineEventTypeV2::kActionFailed,
                   std::string(ScenarioLabel(aScenario.mScenario)) + " failed.");
      break;
  }
}

void MockEngine::AcceptScenarioLocked(MockScenarioV2 pScenario) {
  if (pScenario == MockScenarioV2::kRed) {
    RejectScenarioLocked(pScenario, "Mock red-light flow blocked before execution.");
    UiEffectV2 aShowDialog;
    aShowDialog.mType = UiEffectTypeV2::kShowDialog;
    aShowDialog.mDialog.mKind = UiDialogKindV2::kError;
    aShowDialog.mDialog.mTitle = "Mock Red Light";
    aShowDialog.mDialog.mMessage = "Mock red-light flow blocked before execution.";
    EmitUiEffectLocked(aShowDialog, EngineEventTypeV2::kUiStateChanged,
                       "Mock red-light flow blocked before execution.");
    return;
  }

  ActiveScenarioV2 aScenario;
  aScenario.mScenario = pScenario;
  aScenario.mPhase = ScenarioIsYellow(pScenario) ? ActivePhaseV2::kPrompting
                                                 : ActivePhaseV2::kRunning;
  aScenario.mPromptId = mNextPromptId++;
  mActiveScenario = aScenario;

  EngineEventV2 aAccepted;
  aAccepted.mType = EngineEventTypeV2::kActionAccepted;
  aAccepted.mMessage = std::string(ScenarioLabel(pScenario)) + " accepted.";
  PushEventLocked(std::move(aAccepted));

  if (ScenarioIsYellow(pScenario)) {
    UiEffectV2 aShowPrompt;
    aShowPrompt.mType = UiEffectTypeV2::kShowPrompt;
    aShowPrompt.mPrompt.mPromptId = aScenario.mPromptId;
    aShowPrompt.mPrompt.mKind = UiPromptKindV2::kDestinationAction;
    aShowPrompt.mPrompt.mTitle = "Mock Yellow Light";
    aShowPrompt.mPrompt.mMessage = std::string("Choose how to continue:\n") +
                                   ScenarioLabel(pScenario);
    aShowPrompt.mPrompt.mPrimaryLabel = "Clear";
    aShowPrompt.mPrompt.mSecondaryLabel = "Merge";
    aShowPrompt.mPrompt.mCancelLabel = "Cancel";
    EmitUiEffectLocked(aShowPrompt, EngineEventTypeV2::kUiStateChanged,
                       "Mock yellow-light prompt requested.");
    EmitLogLocked(LogLevelV2::kInfo,
                  std::string("MockEngine waiting on prompt for ") +
                      ScenarioLabel(pScenario) + ".");
    return;
  }

  StartScenarioExecutionLocked(*mActiveScenario);
}

void MockEngine::StartScenarioExecutionLocked(ActiveScenarioV2& pScenario) {
  pScenario.mPhase = ActivePhaseV2::kRunning;
  pScenario.mRunStartedAt = std::chrono::steady_clock::now();

  UiEffectV2 aShowLoading;
  aShowLoading.mType = UiEffectTypeV2::kShowLoading;
  aShowLoading.mLabel = ScenarioLabel(pScenario.mScenario);
  EmitUiEffectLocked(aShowLoading, EngineEventTypeV2::kUiStateChanged,
                     "UI locked.");

  EmitLogLocked(LogLevelV2::kInfo,
                std::string("MockEngine started ") +
                    ScenarioLabel(pScenario.mScenario) + ".");
}

void MockEngine::HandlePromptResponseLocked(const UiPromptResponseV2& pResponse) {
  if (!mActiveScenario.has_value()) {
    return;
  }

  ActiveScenarioV2& aScenario = *mActiveScenario;
  if (aScenario.mPhase != ActivePhaseV2::kPrompting ||
      pResponse.mPromptId != aScenario.mPromptId ||
      pResponse.mKind != UiPromptKindV2::kDestinationAction) {
    return;
  }

  if (pResponse.mChoice == UiPromptChoiceV2::kCancel) {
    FinishLocked(EngineEventTypeV2::kActionCanceled,
                 std::string(ScenarioLabel(aScenario.mScenario)) +
                     " canceled from prompt.");
    return;
  }

  StartScenarioExecutionLocked(aScenario);
}

void MockEngine::AcceptCancelLocked() {
  if (!mActiveScenario.has_value()) {
    RejectCancelLocked("Cancel request rejected because no primary action is active.");
    return;
  }

  ActiveScenarioV2& aScenario = *mActiveScenario;
  if (aScenario.mPhase == ActivePhaseV2::kPrompting) {
    RejectCancelLocked("Cancel request rejected because the prompt has not been answered.");
    return;
  }
  if (aScenario.mPhase == ActivePhaseV2::kCancelPending) {
    RejectCancelLocked("Cancel request rejected because cancel is already pending.");
    return;
  }

  aScenario.mPhase = ActivePhaseV2::kCancelPending;
  aScenario.mCancelRequestedAt = std::chrono::steady_clock::now();

  EngineEventV2 aAccepted;
  aAccepted.mType = EngineEventTypeV2::kCancelAccepted;
  aAccepted.mMessage = std::string("Cancel accepted for ") +
                       ScenarioLabel(aScenario.mScenario) + ".";
  PushEventLocked(std::move(aAccepted));

  EmitLogLocked(LogLevelV2::kWarning,
                std::string("MockEngine cancel pending for ") +
                    ScenarioLabel(aScenario.mScenario) +
                    "; finishing current work for 3 seconds.");
}

void MockEngine::RejectCancelLocked(const std::string& pReason) {
  EngineEventV2 aRejected;
  aRejected.mType = EngineEventTypeV2::kCancelRejected;
  aRejected.mMessage = pReason;
  PushEventLocked(std::move(aRejected));
}

void MockEngine::RejectScenarioLocked(MockScenarioV2 pScenario,
                                      const std::string& pReason) {
  EngineEventV2 aRejected;
  aRejected.mType = EngineEventTypeV2::kActionRejected;
  aRejected.mMessage = std::string(ScenarioLabel(pScenario)) + " rejected: " + pReason;
  PushEventLocked(std::move(aRejected));
}

void MockEngine::EmitUiEffectLocked(const UiEffectV2& pEffect,
                                    EngineEventTypeV2 pType,
                                    const std::string& pMessage) {
  EngineEventV2 aEvent;
  aEvent.mType = pType;
  aEvent.mMessage = pMessage;
  aEvent.mUiEffect = pEffect;
  PushEventLocked(std::move(aEvent));
}

void MockEngine::EmitLogLocked(LogLevelV2 pLevel, const std::string& pMessage) {
  EngineEventV2 aEvent;
  aEvent.mType = EngineEventTypeV2::kLog;
  aEvent.mMessage = pMessage;
  aEvent.mLogEntry.mLevel = pLevel;
  aEvent.mLogEntry.mMessage = pMessage;
  PushEventLocked(std::move(aEvent));
}

void MockEngine::EmitProgressLocked(double pFraction, const std::string& pLabel) {
  EngineEventV2 aEvent;
  aEvent.mType = EngineEventTypeV2::kProgress;
  aEvent.mMessage = pLabel;
  aEvent.mProgress.mStage = ProgressStageV2::kArchivePacking;
  aEvent.mProgress.mLocalFraction = std::max(0.0, std::min(1.0, pFraction));
  aEvent.mProgress.mOverallFraction = aEvent.mProgress.mLocalFraction;
  aEvent.mProgress.mLabel = pLabel;
  PushEventLocked(std::move(aEvent));
}

void MockEngine::FinishLocked(EngineEventTypeV2 pType, const std::string& pMessage) {
  const bool aWasRunning =
      mActiveScenario.has_value() &&
      mActiveScenario->mPhase != ActivePhaseV2::kPrompting;
  mActiveScenario.reset();

  EngineEventV2 aFinished;
  aFinished.mType = pType;
  aFinished.mMessage = pMessage;
  PushEventLocked(std::move(aFinished));

  if (aWasRunning) {
    UiEffectV2 aHideLoading;
    aHideLoading.mType = UiEffectTypeV2::kHideLoading;
    EmitUiEffectLocked(aHideLoading, EngineEventTypeV2::kUiStateChanged,
                       "UI unlocked.");
  }
}

void MockEngine::PushEventLocked(EngineEventV2 pEvent) {
  pEvent.mSnapshot = BuildSnapshotLocked();
  mPendingEvents.push_back(std::move(pEvent));
}

EngineSnapshotV2 MockEngine::BuildSnapshotLocked() const {
  EngineSnapshotV2 aSnapshot;
  aSnapshot.mIsBusy = mActiveScenario.has_value();
  aSnapshot.mIsUiLocked = mActiveScenario.has_value();
  aSnapshot.mCurrentPrimaryAction =
      mActiveScenario.has_value() ? EnginePrimaryActionV2::kBundle
                                  : EnginePrimaryActionV2::kNone;
  aSnapshot.mIsCancelPending =
      mActiveScenario.has_value() &&
      mActiveScenario->mPhase == ActivePhaseV2::kCancelPending;
  return aSnapshot;
}

}  // namespace peanutbutter
