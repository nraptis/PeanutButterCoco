#pragma once

#include "Bundle_Context.hpp"

namespace peanutbutter {

class BundlePreflightV2 {
 public:
  static bool Run(BundleStageContextV2& pContext);
};

}  // namespace peanutbutter
