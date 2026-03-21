#pragma once

#include <cstddef>
#include <string>

namespace peanutbutter {

class FileWriteStreamV2 {
 public:
  virtual ~FileWriteStreamV2() = default;
  virtual bool IsReady() const = 0;
  virtual bool Write(const unsigned char* pData, std::size_t pLength) = 0;
  virtual std::size_t GetBytesWritten() const = 0;
  virtual bool Close() = 0;
  virtual std::string LastErrorMessage() const = 0;
};

}  // namespace peanutbutter
