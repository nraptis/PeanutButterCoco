#pragma once

#include <cstddef>
#include <cstdint>

#include "../../Knobs.hpp"
#include "MemoryLayoutError.hpp"
#include "Primatives.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kArchiveHeaderBytesV2 = knobs::kArchiveHeaderBytesV2;
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

inline std::uint8_t ArchiveHeaderFileCountMod256(const ArchiveHeaderV2& pHeader) {
  return pHeader.mReserved0;
}

inline std::uint8_t ArchiveHeaderFolderCountMod256(const ArchiveHeaderV2& pHeader) {
  return static_cast<std::uint8_t>(pHeader.mReserved1 & 0xFFu);
}

inline void SetArchiveHeaderCountMods(ArchiveHeaderV2& pHeader,
                                      std::uint8_t pFileCountMod256,
                                      std::uint8_t pFolderCountMod256) {
  pHeader.mReserved0 = pFileCountMod256;
  pHeader.mReserved1 =
      (pHeader.mReserved1 & 0xFFFFFF00u) | static_cast<std::uint32_t>(pFolderCountMod256);
}

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
