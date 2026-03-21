#pragma once

#include <optional>
#include <string>
#include <vector>

#include "BundleRequest.hpp"
#include "DecodeRequest.hpp"
#include "Logging.hpp"
#include "Progress.hpp"
#include "RepairRequest.hpp"
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
};

struct EngineCommandV2 {
  EngineCommandTypeV2 mType = EngineCommandTypeV2::kCancel;
  std::optional<BundleRequestV2> mBundleRequest;
  std::optional<DecodeRequestV2> mDecodeRequest;
  std::optional<DecodeRequestV2> mManifestRequest;
  std::optional<RepairRequestV2> mRepairRequest;
  std::optional<SanityRequestV2> mSanityRequest;
  std::optional<UiPromptResponseV2> mPromptResponse;
};

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
};

struct EngineSnapshotV2 {
  bool mIsBusy = false;
  bool mIsUiLocked = false;
  bool mIsCancelPending = false;
  EnginePrimaryActionV2 mCurrentPrimaryAction = EnginePrimaryActionV2::kNone;
};

struct EngineEventV2 {
  EngineEventTypeV2 mType = EngineEventTypeV2::kLog;
  EngineSnapshotV2 mSnapshot{};
  std::string mMessage;
  UiEffectV2 mUiEffect{};
  LogEntryV2 mLogEntry{};
  ProgressSnapshotV2 mProgress{};
};

using EngineEventListV2 = std::vector<EngineEventV2>;

}  // namespace peanutbutter
