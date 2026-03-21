#pragma once

#include "Sanity_Context.hpp"

namespace peanutbutter {

class SanityDiscoveryV2 final {
 public:
  static bool Run(SanityStageContextV2& pContext);
};

}  // namespace peanutbutter
