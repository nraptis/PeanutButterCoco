#pragma once

#include <cstddef>
#include <cstdint>

#include "../../Knobs.hpp"
#include "CheckSum.hpp"
#include "MemoryLayoutError.hpp"
#include "Primatives.hpp"
#include "RepairRecord.hpp"
#include "SkipRecord.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kSectionHeaderBytesV2 = knobs::kSectionHeaderBytesV2;

enum class SectionTypeV2 : std::uint8_t {
  kArchiveData = 0u,
  kPreviewManifest = 1u,
  kRepairData = 3u,
};

#pragma pack(push, 1)
struct SectionHeaderV2 {
  CheckSumV2 mCheckSum{};
  SkipRecordV2 mSkipRecord{};
  RepairRecordV2 mRepairRecord{};
  std::uint8_t mCheckSumKind = specs_verified::kSectionCheckSumKindSha256V2;
  std::uint8_t mSectionType = static_cast<std::uint8_t>(SectionTypeV2::kArchiveData);
  std::uint8_t mSectionFlags = 0u;
  std::uint32_t mPayloadBytesUsed = 0u;
  std::uint32_t mArchiveFileCount = 0u;
  std::uint32_t mArchiveBlockCount = 0u;
  std::uint32_t mArchiveIndex = 0u;
  std::uint32_t mBlockIndex = 0u;
  PackedUint48V2 mBlockCountMain{};
  PackedUint48V2 mBlockCountPreview{};
  PackedUint48V2 mBlockCountRepair{};
  std::uint64_t mArchiveFamilyId = 0u;
  std::uint8_t mReserved[specs_verified::kSectionReservedBytesV2] = {};
};
#pragma pack(pop)

inline std::uint64_t SectionHeaderBlockCountMain(const SectionHeaderV2& pHeader) {
  return PackedUint48ToUInt64(pHeader.mBlockCountMain);
}

inline std::uint64_t SectionHeaderBlockCountPreview(const SectionHeaderV2& pHeader) {
  return PackedUint48ToUInt64(pHeader.mBlockCountPreview);
}

inline std::uint64_t SectionHeaderBlockCountRepair(const SectionHeaderV2& pHeader) {
  return PackedUint48ToUInt64(pHeader.mBlockCountRepair);
}

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
