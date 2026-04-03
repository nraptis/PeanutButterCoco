#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "MemoryLayoutError.hpp"
#include "Specs_Verified.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kCheckSumBytesV2 =
    specs_verified::kSectionCheckSumBytesV2;

struct CheckSumV2 {
  std::array<std::uint8_t, kCheckSumBytesV2> mBytes{};
};

bool ReadCheckSum(const unsigned char* pBytes,
                  std::size_t pByteCount,
                  CheckSumV2& pOutCheckSum,
                  MemoryLayoutErrorInfo* pOutError = nullptr);

bool WriteCheckSum(const CheckSumV2& pCheckSum,
                   unsigned char* pOutBytes,
                   std::size_t pByteCount,
                   MemoryLayoutErrorInfo* pOutError = nullptr);

bool CheckSumsEqual(const CheckSumV2& pLeft,
                    const CheckSumV2& pRight);

}  // namespace peanutbutter::memory_layout
