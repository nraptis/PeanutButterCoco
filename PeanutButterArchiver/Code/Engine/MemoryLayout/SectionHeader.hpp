#pragma once

#include <cstddef>
#include <cstdint>

#include "CheckSum.hpp"
#include "MemoryLayoutError.hpp"
#include "RepairRecord.hpp"
#include "SkipRecord.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kSectionHeaderBytesV2 = 64u;

enum class SectionTypeV2 : std::uint8_t {
  kArchiveData = 0u,
  kPreviewManifest = 1u,
  kEmptyFolderManifest = 2u,
  kRepairData = 3u,
};

inline constexpr std::uint8_t kSectionFlagSparsePaddingHasNextRecordV2 = 0x01u;

#pragma pack(push, 1)
struct SectionHeaderV2 {
  CheckSumV2 mCheckSum{};
  SkipRecordV2 mSkipRecord{};
  std::uint8_t mSectionType = static_cast<std::uint8_t>(SectionTypeV2::kArchiveData);
  std::uint8_t mSectionFlags = 0u;
  std::uint8_t mReserved[15] = {};
  RepairRecordV2 mRepairRecord{};
};
#pragma pack(pop)

bool ValidateSectionHeader(const SectionHeaderV2& pHeader,
                           MemoryLayoutErrorInfo* pOutError = nullptr);

bool ReadSectionHeader(const unsigned char* pBytes,
                       std::size_t pByteCount,
                       SectionHeaderV2& pOutHeader,
                       MemoryLayoutErrorInfo* pOutError = nullptr);

bool WriteSectionHeader(const SectionHeaderV2& pHeader,
                        unsigned char* pOutBytes,
                        std::size_t pByteCount,
                        MemoryLayoutErrorInfo* pOutError = nullptr);

static_assert(sizeof(SectionHeaderV2) == kSectionHeaderBytesV2,
              "SectionHeaderV2 must remain 64 bytes.");

}  // namespace peanutbutter::memory_layout
