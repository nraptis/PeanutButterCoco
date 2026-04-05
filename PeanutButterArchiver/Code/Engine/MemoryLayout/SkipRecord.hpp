#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "MemoryLayoutError.hpp"
#include "Primatives.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kSkipRecordBytesV2 = 8u;

#pragma pack(push, 1)
struct SkipRecordV2 {
  // Skip pointers identify the first payload byte of the next logical record.
  // mArchiveIndex is the absolute archive slot index (packed uint24),
  // mBlockIndex is the local block index inside that archive (uint16), and
  // mByteIndex is the payload byte offset inside that target block (packed uint24).
  PackedUint24V2 mArchiveIndex{};
  std::uint16_t mBlockIndex = 0u;
  PackedUint24V2 mByteIndex{};
};
#pragma pack(pop)

std::uint32_t GetSkipRecordArchiveIndex(const SkipRecordV2& pSkipRecord);

bool SetSkipRecordArchiveIndex(SkipRecordV2& pSkipRecord,
                               std::uint32_t pArchiveIndex,
                               MemoryLayoutErrorInfo* pOutError = nullptr);

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
              "SkipRecordV2 must remain 8 bytes.");
static_assert(std::is_same_v<decltype(SkipRecordV2::mArchiveIndex), PackedUint24V2>,
              "SkipRecordV2::mArchiveIndex must remain packed uint24.");
static_assert(std::is_same_v<decltype(SkipRecordV2::mBlockIndex), std::uint16_t>,
              "SkipRecordV2::mBlockIndex must remain uint16_t.");
static_assert(std::is_same_v<decltype(SkipRecordV2::mByteIndex), PackedUint24V2>,
              "SkipRecordV2::mByteIndex must remain packed uint24.");

}  // namespace peanutbutter::memory_layout
