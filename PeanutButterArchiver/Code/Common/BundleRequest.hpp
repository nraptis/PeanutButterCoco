#pragma once

#include <cstdint>
#include <string>

namespace peanutbutter {

enum class StrengthPresetV2 {
  kLow = 0,
  kMedium = 1,
  kHigh = 2,
};

struct BundleRequestV2 {
  std::string mSourceDirectory;
  std::string mDestinationDirectory;
  bool mClearDestinationBeforeWrite = false;
  bool mEncryptionEnabled = true;
  StrengthPresetV2 mEncryptionStrength = StrengthPresetV2::kHigh;
  StrengthPresetV2 mTableStrength = StrengthPresetV2::kHigh;
  bool mRepairEnabled = true;
  std::uint8_t mRepairPercent = 25u;
  bool mIncludePreviewManifest = true;
  bool mSafeModeEnabled = true;
  std::uint32_t mBlockCount = 4u;
  std::uint64_t mCancelFinishBlocks = 8u;
  std::string mFilePrefix = "archive";
  std::string mPassword;
};

}  // namespace peanutbutter
