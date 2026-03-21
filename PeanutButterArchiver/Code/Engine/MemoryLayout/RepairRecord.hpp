#pragma once

#include <cstddef>
#include <cstdint>

#include "MemoryLayoutError.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kRepairRecordBytesV2 = 8u;

#pragma pack(push, 1)
struct RepairRecordV2 {
  std::uint32_t mRepairPointerArchive = 0u;
  std::uint32_t mRepairPointerBlock = 0u;
};
#pragma pack(pop)

bool ReadRepairRecord(const unsigned char* pBytes,
                      std::size_t pByteCount,
                      RepairRecordV2& pOutRepairRecord,
                      MemoryLayoutErrorInfo* pOutError = nullptr);

bool WriteRepairRecord(const RepairRecordV2& pRepairRecord,
                       unsigned char* pOutBytes,
                       std::size_t pByteCount,
                       MemoryLayoutErrorInfo* pOutError = nullptr);

static_assert(sizeof(RepairRecordV2) == kRepairRecordBytesV2,
              "RepairRecordV2 must remain 8 bytes.");

}  // namespace peanutbutter::memory_layout
