#pragma once

#include <cstdint>
#include <string>

#include "../Knobs.hpp"

namespace peanutbutter {

inline constexpr std::uint32_t kDefaultBlocksPerArchiveV2 =
    knobs::kDefaultBlocksPerArchiveV2;

enum class StrengthPresetV2 {
  kLow = 0,
  kMedium = 1,
  kHigh = 2,
};

enum class RepairCoveragePresetV2 : std::uint8_t {
  k20 = 20u,
  k40 = 40u,
  k60 = 60u,
  k80 = 80u,
};

inline constexpr std::uint8_t RepairCoveragePercentV2(
    RepairCoveragePresetV2 pPreset) {
  return static_cast<std::uint8_t>(pPreset);
}

struct BundleRequestV2 {
  std::string mSourceDirectory;
  std::string mDestinationDirectory;
  bool mClearDestinationBeforeWrite = false;
  bool mEncryptionEnabled = true;
  StrengthPresetV2 mEncryptionStrength = StrengthPresetV2::kHigh;
  StrengthPresetV2 mTableStrength = StrengthPresetV2::kHigh;
  bool mRepairEnabled = true;
  RepairCoveragePresetV2 mRepairCoverage = RepairCoveragePresetV2::k20;
  bool mIncludePreviewManifest = true;
  bool mSafeModeEnabled = true;
  std::uint32_t mBlockCount = kDefaultBlocksPerArchiveV2;
  std::uint64_t mCancelFinishBlocks = 8u;
  std::string mFilePrefix = "archive";
  std::string mPassword;
};

}  // namespace peanutbutter
