#ifndef PEANUT_BUTTER_ULTIMA_HEADERS_V2_HPP_
#define PEANUT_BUTTER_ULTIMA_HEADERS_V2_HPP_

#include <cstddef>
#include <cstdint>

namespace peanutbutter {
namespace headers_v2 {

inline constexpr std::size_t kArchiveHeaderV2Length = 48u;
inline constexpr std::size_t kSectionHeaderV2Length = 64u;

inline constexpr std::uint64_t kArchiveMagicV2 = 0x5045414E55544254ull;  // "PEANUTBT"

enum class ArchiveFormatVersionV2 : std::uint8_t {
  kVersion2 = 2u,
};

enum class CipherVersionV2 : std::uint8_t {
  kVersion1 = 1u,
};

enum class ExpanderVersionV2 : std::uint8_t {
  kVersion1 = 1u,
};

enum class EncryptionProfileV2 : std::uint8_t {
  kHigh = 1u,
  kMedium = 2u,
  kLow = 3u,
};

enum class ExpanderProfileV2 : std::uint8_t {
  kHigh = 1u,
  kMedium = 2u,
  kLow = 3u,
};

enum class ArchiveStateV2 : std::uint8_t {
  kInvalid = 0u,
  kFinishedWithCancel = 1u,
  kFinishedWithError = 2u,
  kFinishedWithCancelAndError = 3u,
  kFinished = 4u,
};

enum class SectionTypeV2 : std::uint8_t {
  kArchiveData = 0u,
  kManifest = 1u,
  kRepairData = 2u,
};

#pragma pack(push, 1)

struct PackedUint48V2 {
  std::uint8_t mBytes[6] = {};
};

struct PackedUint24V2 {
  std::uint8_t mBytes[3] = {};
};

struct ArchiveHeaderV2 {
  std::uint64_t mMagic = kArchiveMagicV2;
  std::uint8_t mArchiveFormatVersion =
      static_cast<std::uint8_t>(ArchiveFormatVersionV2::kVersion2);
  std::uint8_t mCipherVersion =
      static_cast<std::uint8_t>(CipherVersionV2::kVersion1);
  std::uint8_t mExpanderVersion =
      static_cast<std::uint8_t>(ExpanderVersionV2::kVersion1);
  std::uint8_t mIsEncrypted = 1u;
  std::uint8_t mCipherProfile =
      static_cast<std::uint8_t>(EncryptionProfileV2::kHigh);
  std::uint8_t mExpanderProfile =
      static_cast<std::uint8_t>(ExpanderProfileV2::kLow);
  std::uint8_t mArchiveState =
      static_cast<std::uint8_t>(ArchiveStateV2::kInvalid);
  std::uint8_t mReservedA = 0u;
  std::uint8_t mReservedB = 0u;
  std::uint8_t mReservedC = 0u;
  PackedUint48V2 mArchiveIndex{};
  PackedUint48V2 mArchiveCount{};
  std::uint64_t mArchiveFamilyId = 0u;
  std::uint8_t mReservedTail[10] = {};
};

struct ChecksumV2 {
  std::uint64_t mWord1 = 0u;
  std::uint64_t mWord2 = 0u;
  std::uint64_t mWord3 = 0u;
  std::uint64_t mWord4 = 0u;
  std::uint64_t mWord5 = 0u;
  std::uint64_t mWord6 = 0u;
};

struct SkipRecordV2 {
  std::uint16_t mArchiveDistance = 0u;
  std::uint16_t mBlockDistance = 0u;
  PackedUint24V2 mByteDistance{};
};

struct RepairRecordV2 {
  std::uint32_t mRepairPointerArchive = 0u;
  std::uint32_t mRepairPointerBlock = 0u;
};

struct SectionHeaderV2 {
  ChecksumV2 mChecksum{};
  SkipRecordV2 mSkip{};
  std::uint8_t mSectionType =
      static_cast<std::uint8_t>(SectionTypeV2::kArchiveData);
  RepairRecordV2 mRepair{};
};

#pragma pack(pop)

static_assert(sizeof(PackedUint48V2) == 6u,
              "PackedUint48V2 must be 6 bytes.");
static_assert(sizeof(PackedUint24V2) == 3u,
              "PackedUint24V2 must be 3 bytes.");
static_assert(sizeof(ArchiveHeaderV2) == kArchiveHeaderV2Length,
              "ArchiveHeaderV2 must be 48 bytes.");
static_assert(sizeof(ChecksumV2) == 48u,
              "ChecksumV2 must be 48 bytes.");
static_assert(sizeof(SkipRecordV2) == 7u,
              "SkipRecordV2 must be 7 bytes.");
static_assert(sizeof(RepairRecordV2) == 8u,
              "RepairRecordV2 must be 8 bytes.");
static_assert(sizeof(SectionHeaderV2) == kSectionHeaderV2Length,
              "SectionHeaderV2 must be 64 bytes.");

}  // namespace headers_v2
}  // namespace peanutbutter

#endif  // PEANUT_BUTTER_ULTIMA_HEADERS_V2_HPP_
