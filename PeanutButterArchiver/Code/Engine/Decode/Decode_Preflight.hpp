#pragma once

#include "Decode_Context.hpp"

namespace peanutbutter {

class DecodePreflightV2 {
 public:
  static bool Run(DecodeStageContextV2& pContext);
};

}  // namespace peanutbutter
