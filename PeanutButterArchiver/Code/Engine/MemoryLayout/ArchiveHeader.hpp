#pragma once

#include <cstddef>
#include <cstdint>

#include "MemoryLayoutError.hpp"
#include "Primatives.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kArchiveHeaderBytesV2 = 64u;
inline constexpr std::uint64_t kArchiveMagicV2 = 0x5045414E55544254ULL;

enum class ArchiveDirtyStateV2 : std::uint8_t {
  kInvalid = 0u,
  kFinishedWithCancel = 1u,
  kFinishedWithError = 2u,
  kFinishedWithCancelAndError = 3u,
  kFinished = 4u,
};

#pragma pack(push, 1)
struct ArchiveHeaderV2 {
  std::uint64_t mMagic = kArchiveMagicV2;
  std::uint8_t mArchiveFormatVersion = 2u;
  std::uint8_t mCipherVersion = 1u;
  std::uint8_t mExpanderVersion = 1u;
  std::uint8_t mDirtyState = static_cast<std::uint8_t>(ArchiveDirtyStateV2::kInvalid);
  std::uint8_t mIsEncrypted = 1u;
  std::uint8_t mCipherProfile = 0u;
  std::uint8_t mExpanderProfile = 0u;
  std::uint8_t mReserved0 = 0u;
  PackedUint48V2 mArchiveIndex{};
  PackedUint48V2 mArchiveCount{};
  PackedUint48V2 mArchiveDataBlockCount{};
  PackedUint48V2 mEmptyFolderBlockCount{};
  PackedUint48V2 mPreviewManifestBlockCount{};
  PackedUint48V2 mRepairSectorBlockCount{};
  std::uint64_t mArchiveFamilyId = 0u;
  std::uint32_t mReserved1 = 0u;
};
#pragma pack(pop)

bool ValidateArchiveHeader(const ArchiveHeaderV2& pHeader,
                           MemoryLayoutErrorInfo* pOutError = nullptr);

bool ReadArchiveHeader(const unsigned char* pBytes,
                       std::size_t pByteCount,
                       ArchiveHeaderV2& pOutHeader,
                       MemoryLayoutErrorInfo* pOutError = nullptr);

bool WriteArchiveHeader(const ArchiveHeaderV2& pHeader,
                        unsigned char* pOutBytes,
                        std::size_t pByteCount,
                        MemoryLayoutErrorInfo* pOutError = nullptr);

static_assert(sizeof(ArchiveHeaderV2) == kArchiveHeaderBytesV2,
              "ArchiveHeaderV2 must remain 64 bytes.");

}  // namespace peanutbutter::memory_layout
