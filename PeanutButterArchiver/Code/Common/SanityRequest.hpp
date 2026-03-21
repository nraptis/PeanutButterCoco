#pragma once

#include <string>

namespace peanutbutter {

struct SanityRequestV2 {
  std::string mLeftDirectory;
  std::string mRightDirectory;
  bool mIgnoreHidden = false;
};

}  // namespace peanutbutter
