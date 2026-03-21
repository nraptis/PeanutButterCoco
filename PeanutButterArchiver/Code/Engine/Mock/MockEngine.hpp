#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <queue>

#include "../../Common/EngineMessaging.hpp"

namespace peanutbutter {

enum class MockScenarioV2 {
  kGreenDelaySucceed = 0,
  kGreenDelayFail = 1,
  kYellowDelaySucceed = 2,
  kYellowDelayFail = 3,
  kRed = 4,
};

class MockEngine final {
 public:
  void EnqueueScenario(MockScenarioV2 pScenario);
  void EnqueuePromptResponse(const UiPromptResponseV2& pResponse);
  void EnqueueCancelRequest();
  EngineEventListV2 Poll();
  EngineSnapshotV2 Snapshot() const;

 private:
  enum class ActivePhaseV2 {
    kPrompting = 0,
    kRunning = 1,
    kCancelPending = 2,
  };

  struct ActiveScenarioV2 {
    MockScenarioV2 mScenario = MockScenarioV2::kGreenDelaySucceed;
    ActivePhaseV2 mPhase = ActivePhaseV2::kRunning;
    std::uint64_t mPromptId = 0u;
    std::chrono::steady_clock::time_point mRunStartedAt{};
    std::chrono::steady_clock::time_point mCancelRequestedAt{};
  };

  void ProcessIncomingLocked();
  void ProcessActiveLocked();
  void AcceptScenarioLocked(MockScenarioV2 pScenario);
  void StartScenarioExecutionLocked(ActiveScenarioV2& pScenario);
  void HandlePromptResponseLocked(const UiPromptResponseV2& pResponse);
  void AcceptCancelLocked();
  void RejectCancelLocked(const std::string& pReason);
  void RejectScenarioLocked(MockScenarioV2 pScenario, const std::string& pReason);
  void EmitUiEffectLocked(const UiEffectV2& pEffect,
                          EngineEventTypeV2 pType,
                          const std::string& pMessage);
  void EmitLogLocked(LogLevelV2 pLevel, const std::string& pMessage);
  void EmitProgressLocked(double pFraction, const std::string& pLabel);
  void FinishLocked(EngineEventTypeV2 pType, const std::string& pMessage);
  void PushEventLocked(EngineEventV2 pEvent);
  EngineSnapshotV2 BuildSnapshotLocked() const;

 private:
  mutable std::mutex mMutex;
  std::queue<MockScenarioV2> mIncomingScenarios;
  std::queue<UiPromptResponseV2> mIncomingPromptResponses;
  bool mIncomingCancelRequest = false;
  std::optional<ActiveScenarioV2> mActiveScenario;
  EngineEventListV2 mPendingEvents;
  std::uint64_t mNextPromptId = 1u;
};

}  // namespace peanutbutter
