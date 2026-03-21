#pragma once

#include <cstdint>
#include <string>

#include "Logging.hpp"
#include "Progress.hpp"

namespace peanutbutter {

enum class UiEffectTypeV2 {
  kShowLoading = 0,
  kUpdateLoading = 1,
  kHideLoading = 2,
  kShowDialog = 3,
  kShowPrompt = 4,
};

enum class UiDialogKindV2 {
  kInfo = 0,
  kWarning = 1,
  kError = 2,
};

struct UiDialogRequestV2 {
  UiDialogKindV2 mKind = UiDialogKindV2::kInfo;
  std::string mTitle;
  std::string mMessage;
};

enum class UiPromptKindV2 {
  kDestinationAction = 0,
};

enum class UiPromptChoiceV2 {
  kCancel = 0,
  kClear = 1,
  kMerge = 2,
};

struct UiPromptRequestV2 {
  std::uint64_t mPromptId = 0u;
  UiPromptKindV2 mKind = UiPromptKindV2::kDestinationAction;
  std::string mTitle;
  std::string mMessage;
  std::string mPrimaryLabel;
  std::string mSecondaryLabel;
  std::string mCancelLabel;
};

struct UiPromptResponseV2 {
  std::uint64_t mPromptId = 0u;
  UiPromptKindV2 mKind = UiPromptKindV2::kDestinationAction;
  UiPromptChoiceV2 mChoice = UiPromptChoiceV2::kCancel;
};

struct UiEffectV2 {
  UiEffectTypeV2 mType = UiEffectTypeV2::kShowLoading;
  std::string mLabel;
  UiDialogRequestV2 mDialog{};
  UiPromptRequestV2 mPrompt{};
};

class UiEventSinkV2 {
 public:
  virtual ~UiEventSinkV2() = default;
  virtual void HandleUiEffect(const UiEffectV2& pEffect) = 0;
  virtual void HandleLogEntry(const LogEntryV2& pEntry) = 0;
  virtual void HandleProgress(const ProgressSnapshotV2& pSnapshot) = 0;
};

}  // namespace peanutbutter
