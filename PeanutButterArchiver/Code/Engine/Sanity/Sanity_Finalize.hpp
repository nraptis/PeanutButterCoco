#pragma once

#include "Sanity_Context.hpp"

namespace peanutbutter {

class SanityFinalizeV2 final {
 public:
  static bool Run(SanityStageContextV2& pContext);
};

}  // namespace peanutbutter
