#pragma once

#include <cstddef>

namespace peanutbutter {

class FileReadStreamV2 {
 public:
  virtual ~FileReadStreamV2() = default;
  virtual bool IsReady() const = 0;
  virtual std::size_t GetLength() const = 0;
  virtual bool Read(std::size_t pOffset,
                    unsigned char* pDestination,
                    std::size_t pLength) const = 0;
};

}  // namespace peanutbutter



