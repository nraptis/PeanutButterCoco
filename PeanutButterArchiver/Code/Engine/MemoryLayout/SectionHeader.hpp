#pragma once

#include <cstddef>
#include <cstdint>

#include "CheckSum.hpp"
#include "MemoryLayoutError.hpp"
#include "RepairRecord.hpp"
#include "SkipRecord.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kSectionHeaderBytesV2 = 96u;

enum class SectionTypeV2 : std::uint8_t {
  kArchiveData = 0u,
  kPreviewManifest = 1u,
  kEmptyFolderManifest = 2u,
  kRepairData = 3u,
};

#pragma pack(push, 1)
struct SectionHeaderV2 {
  CheckSumV2 mCheckSum{};
  SkipRecordV2 mSkipRecord{};
  RepairRecordV2 mRepairRecord{};
  std::uint8_t mSectionType = static_cast<std::uint8_t>(SectionTypeV2::kArchiveData);
  std::uint8_t mSectionFlags = 0u;
  std::uint32_t mPayloadBytesUsed = 0u;
  std::uint32_t mArchiveFileCount = 0u;
  std::uint32_t mArchiveBlockCount = 0u;
  std::uint32_t mArchiveIndex = 0u;
  std::uint32_t mBlockIndex = 0u;
  std::uint32_t mArchiveDataBlockCount = 0u;
  std::uint32_t mPreviewManifestBlockCount = 0u;
  std::uint32_t mFolderManifestBlockCount = 0u;
  std::uint32_t mRepairDataBlockCount = 0u;
  std::uint64_t mArchiveFamilyId = 0u;
  std::uint8_t mReserved[3] = {};
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
              "SectionHeaderV2 must remain 96 bytes.");

}  // namespace peanutbutter::memory_layout
