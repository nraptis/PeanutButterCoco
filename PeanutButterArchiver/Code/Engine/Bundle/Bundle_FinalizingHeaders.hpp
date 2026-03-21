#pragma once

#include "Bundle_Context.hpp"

namespace peanutbutter {

class BundleFinalizingHeadersV2 {
 public:
  static bool Run(BundleStageContextV2& pContext);
};

}  // namespace peanutbutter
