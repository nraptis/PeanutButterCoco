#pragma once

#include <string>

namespace peanutbutter {

enum class DecodeIntentV2 {
  kUnbundle = 0,
  kRecover = 1,
  kManifest = 2,
  kRepair = 3,
};

inline bool DecodeIntentAllowsSalvageV2(DecodeIntentV2 pIntent) {
  return pIntent == DecodeIntentV2::kRecover;
}

struct DecodeRequestV2 {
  std::string mSourcePath;
  std::string mDestinationDirectory;
  bool mClearDestinationBeforeWrite = false;
  bool mEncryptionEnabled = true;
  // Used by repair apply policy when intent is kRepair.
  bool mAggressive = false;
  std::uint64_t mCancelFinishBlocks = 8u;
  std::string mPassword;
  DecodeIntentV2 mIntent = DecodeIntentV2::kUnbundle;
};

}  // namespace peanutbutter
