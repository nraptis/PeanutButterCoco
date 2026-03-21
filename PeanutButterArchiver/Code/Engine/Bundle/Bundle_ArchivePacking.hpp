#pragma once

#include "Bundle_Context.hpp"

namespace peanutbutter {

class BundleArchivePackingV2 {
 public:
  static bool Run(BundleStageContextV2& pContext);
};

}  // namespace peanutbutter
