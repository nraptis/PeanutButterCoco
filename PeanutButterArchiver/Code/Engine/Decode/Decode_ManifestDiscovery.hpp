#pragma once

#include "Decode_Context.hpp"

namespace peanutbutter {

class DecodeInspectionV2 {
 public:
  static bool Run(DecodeStageContextV2& pContext);
};

class DecodeManifestDiscoveryV2 {
 public:
  static bool Run(DecodeStageContextV2& pContext);
};

class DecodeRepairApplyV2 {
 public:
  static bool Run(DecodeStageContextV2& pContext);
};

}  // namespace peanutbutter
