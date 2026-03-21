#include "CheckSum.hpp"

#include <cstring>

namespace peanutbutter::memory_layout {

bool ReadCheckSum(const unsigned char* pBytes,
                  std::size_t pByteCount,
                  CheckSumV2& pOutCheckSum,
                  MemoryLayoutErrorInfo* pOutError) {
  pOutCheckSum = CheckSumV2{};
  ClearMemoryLayoutError(pOutError);

  if (pBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "ReadCheckSum",
                            "bytes",
                            0u,
                            0u,
                            kCheckSumBytesV2);
    return false;
  }
  if (pByteCount < kCheckSumBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "ReadCheckSum",
                            "bytes",
                            0u,
                            pByteCount,
                            kCheckSumBytesV2);
    return false;
  }

  std::memcpy(pOutCheckSum.mBytes.data(), pBytes, kCheckSumBytesV2);
  return true;
}

bool WriteCheckSum(const CheckSumV2& pCheckSum,
                   unsigned char* pOutBytes,
                   std::size_t pByteCount,
                   MemoryLayoutErrorInfo* pOutError) {
  ClearMemoryLayoutError(pOutError);

  if (pOutBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "WriteCheckSum",
                            "bytes",
                            0u,
                            0u,
                            kCheckSumBytesV2);
    return false;
  }
  if (pByteCount < kCheckSumBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "WriteCheckSum",
                            "bytes",
                            0u,
                            pByteCount,
                            kCheckSumBytesV2);
    return false;
  }

  std::memcpy(pOutBytes, pCheckSum.mBytes.data(), kCheckSumBytesV2);
  return true;
}

bool CheckSumsEqual(const CheckSumV2& pLeft,
                    const CheckSumV2& pRight) {
  return pLeft.mBytes == pRight.mBytes;
}

}  // namespace peanutbutter::memory_layout
