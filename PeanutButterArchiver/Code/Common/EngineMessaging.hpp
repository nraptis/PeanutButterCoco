#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "BundleRequest.hpp"
#include "DecodeRequest.hpp"
#include "Logging.hpp"
#include "Progress.hpp"
#include "RepairRequest.hpp"
#include "RuntimeEvent.hpp"
#include "SanityRequest.hpp"
#include "UiContracts.hpp"

namespace peanutbutter {

enum class EnginePrimaryActionV2 {
  kNone = 0,
  kBundle = 1,
  kDecode = 2,
  kManifest = 3,
  kRepair = 4,
  kSanity = 5,
};

enum class EngineCommandTypeV2 {
  kStartBundle = 0,
  kStartDecode = 1,
  kStartManifest = 2,
  kStartRepair = 3,
  kStartSanity = 4,
  kCancel = 5,
  kPromptResponse = 6,
  kCheckpointDecision = 7,
};

enum class EngineCheckpointDecisionKindV2 {
  kContinue = 0,
  kCancel = 1,
};

struct EngineCheckpointDecisionV2 {
  std::uint64_t mCheckpointId = 0u;
  EngineCheckpointDecisionKindV2 mKind =
      EngineCheckpointDecisionKindV2::kContinue;
};

struct EngineCheckpointRequestV2 {
  std::uint64_t mCheckpointId = 0u;
  RuntimeEventV2 mRuntimeEvent{};
};

struct EngineCommandV2 {
  EngineCommandTypeV2 mType = EngineCommandTypeV2::kCancel;
  std::optional<BundleRequestV2> mBundleRequest;
  std::optional<DecodeRequestV2> mDecodeRequest;
  std::optional<DecodeRequestV2> mManifestRequest;
  std::optional<RepairRequestV2> mRepairRequest;
  std::optional<SanityRequestV2> mSanityRequest;
  std::optional<UiPromptResponseV2> mPromptResponse;
  std::optional<EngineCheckpointDecisionV2> mCheckpointDecision;
};

inline EngineCommandV2 MakeBundleCommandV2(const BundleRequestV2& pRequest) {
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartBundle;
  aCommand.mBundleRequest = pRequest;
  return aCommand;
}

inline EngineCommandV2 MakeDecodeCommandV2(const DecodeRequestV2& pRequest) {
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartDecode;
  aCommand.mDecodeRequest = pRequest;
  return aCommand;
}

inline EngineCommandV2 MakeManifestCommandV2(const DecodeRequestV2& pRequest) {
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartManifest;
  aCommand.mManifestRequest = pRequest;
  return aCommand;
}

inline EngineCommandV2 MakeRepairCommandV2(const RepairRequestV2& pRequest) {
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartRepair;
  aCommand.mRepairRequest = pRequest;
  return aCommand;
}

inline EngineCommandV2 MakeSanityCommandV2(const SanityRequestV2& pRequest) {
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartSanity;
  aCommand.mSanityRequest = pRequest;
  return aCommand;
}

inline EngineCommandV2 MakePromptResponseCommandV2(
    const UiPromptResponseV2& pResponse) {
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kPromptResponse;
  aCommand.mPromptResponse = pResponse;
  return aCommand;
}

inline EngineCommandV2 MakeCancelCommandV2() {
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kCancel;
  return aCommand;
}

inline EngineCommandV2 MakeCheckpointDecisionCommandV2(
    const EngineCheckpointDecisionV2& pDecision) {
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kCheckpointDecision;
  aCommand.mCheckpointDecision = pDecision;
  return aCommand;
}

inline EngineCommandV2 MakeContinueCheckpointCommandV2(
    std::uint64_t pCheckpointId) {
  EngineCheckpointDecisionV2 aDecision;
  aDecision.mCheckpointId = pCheckpointId;
  aDecision.mKind = EngineCheckpointDecisionKindV2::kContinue;
  return MakeCheckpointDecisionCommandV2(aDecision);
}

inline EngineCommandV2 MakeCancelCheckpointCommandV2(
    std::uint64_t pCheckpointId) {
  EngineCheckpointDecisionV2 aDecision;
  aDecision.mCheckpointId = pCheckpointId;
  aDecision.mKind = EngineCheckpointDecisionKindV2::kCancel;
  return MakeCheckpointDecisionCommandV2(aDecision);
}

enum class EngineEventTypeV2 {
  kActionAccepted = 0,
  kActionRejected = 1,
  kCancelAccepted = 2,
  kCancelRejected = 3,
  kUiStateChanged = 4,
  kLog = 5,
  kProgress = 6,
  kActionCompleted = 7,
  kActionFailed = 8,
  kActionCanceled = 9,
  kRuntimeEvent = 10,
  kCheckpointRequested = 11,
};

struct EngineSnapshotV2 {
  bool mIsBusy = false;
  bool mIsUiLocked = false;
  bool mIsCancelPending = false;
  bool mIsAwaitingCheckpointDecision = false;
  std::uint64_t mPendingCheckpointId = 0u;
  EnginePrimaryActionV2 mCurrentPrimaryAction = EnginePrimaryActionV2::kNone;
};

struct EngineEventV2 {
  EngineEventTypeV2 mType = EngineEventTypeV2::kLog;
  EngineSnapshotV2 mSnapshot{};
  std::string mMessage;
  UiEffectV2 mUiEffect{};
  LogEntryV2 mLogEntry{};
  ProgressSnapshotV2 mProgress{};
  RuntimeEventV2 mRuntimeEvent{};
  EngineCheckpointRequestV2 mCheckpointRequest{};
};

using EngineEventListV2 = std::vector<EngineEventV2>;

}  // namespace peanutbutter
