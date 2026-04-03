#pragma once

#include <string>

namespace peanutbutter {

struct RepairRequestV2 {
  std::string mSourcePath;
  std::string mDestinationDirectory;
  bool mEncryptionEnabled = true;
  // true: always apply repair payload over target block
  // false: keep target block when it already validates
  bool mAggressive = false;
  std::uint64_t mCancelFinishBlocks = 8u;
  std::string mPassword;
};

}  // namespace peanutbutter
