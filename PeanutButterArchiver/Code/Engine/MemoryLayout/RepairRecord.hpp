#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "MemoryLayoutError.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kRepairRecordBytesV2 = 4u;

#pragma pack(push, 1)
struct RepairRecordV2 {
  // Repair pointers identify an archive-local block position directly.
  std::uint16_t mArchiveIndex = 0u;
  std::uint16_t mBlockIndex = 0u;
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
              "RepairRecordV2 must remain 4 bytes.");
static_assert(std::is_same_v<decltype(RepairRecordV2::mArchiveIndex), std::uint16_t>,
              "RepairRecordV2::mArchiveIndex must remain uint16_t.");
static_assert(std::is_same_v<decltype(RepairRecordV2::mBlockIndex), std::uint16_t>,
              "RepairRecordV2::mBlockIndex must remain uint16_t.");

}  // namespace peanutbutter::memory_layout
