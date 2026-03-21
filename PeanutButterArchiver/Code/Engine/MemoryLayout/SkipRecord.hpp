#pragma once

#include <cstddef>
#include <cstdint>

#include "MemoryLayoutError.hpp"
#include "Primatives.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kSkipRecordBytesV2 = 7u;

#pragma pack(push, 1)
struct SkipRecordV2 {
  std::uint16_t mArchiveDistance = 0u;
  std::uint16_t mBlockDistance = 0u;
  PackedUint24V2 mByteDistance{};
};
#pragma pack(pop)

std::uint32_t GetSkipRecordByteDistance(const SkipRecordV2& pSkipRecord);

bool SetSkipRecordByteDistance(SkipRecordV2& pSkipRecord,
                               std::uint32_t pByteDistance,
                               MemoryLayoutErrorInfo* pOutError = nullptr);

bool ReadSkipRecord(const unsigned char* pBytes,
                    std::size_t pByteCount,
                    SkipRecordV2& pOutSkipRecord,
                    MemoryLayoutErrorInfo* pOutError = nullptr);

bool WriteSkipRecord(const SkipRecordV2& pSkipRecord,
                     unsigned char* pOutBytes,
                     std::size_t pByteCount,
                     MemoryLayoutErrorInfo* pOutError = nullptr);

static_assert(sizeof(SkipRecordV2) == kSkipRecordBytesV2,
              "SkipRecordV2 must remain 7 bytes.");

}  // namespace peanutbutter::memory_layout
