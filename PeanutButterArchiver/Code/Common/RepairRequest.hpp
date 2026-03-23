#pragma once

#include <string>

namespace peanutbutter {

struct RepairRequestV2 {
  std::string mSourcePath;
  std::string mDestinationDirectory;
  bool mEncryptionEnabled = true;
  std::uint64_t mCancelFinishBlocks = 8u;
  std::string mPassword;
};

}  // namespace peanutbutter
