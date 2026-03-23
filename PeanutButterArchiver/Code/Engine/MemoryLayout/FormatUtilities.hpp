#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ArchiveHeader.hpp"
#include "CheckSum.hpp"
#include "RepairRecord.hpp"
#include "SectionHeader.hpp"
#include "SkipRecord.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kArchiveBlockBytesV2 = 1044480u;
inline constexpr std::size_t kSectionPayloadBytesV2 =
    kArchiveBlockBytesV2 - kSectionHeaderBytesV2;
inline constexpr std::size_t kMaxPathLengthV2 = 16384u;
inline constexpr std::uint64_t kDirectoryRecordContentMarkerV2 =
    0xFFFFFFFFFFFFFFFFULL;
inline constexpr char kArchiveFileSuffixV2[] = ".PBTR";

enum class TypedRecordTypeV2 : std::uint8_t {
  kManifestFile = 1u,
  kManifestFolder = 2u,
  kDataFile = 3u,
  kDataFolder = 4u,
};

inline constexpr bool IsKnownTypedRecordTypeV2(std::uint8_t pValue) {
  return pValue >= static_cast<std::uint8_t>(TypedRecordTypeV2::kManifestFile) &&
         pValue <= static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFolder);
}

inline constexpr bool TypedRecordTypeHasFileSizeV2(std::uint8_t pValue) {
  return pValue == static_cast<std::uint8_t>(TypedRecordTypeV2::kManifestFile) ||
         pValue == static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFile);
}

inline constexpr bool TypedRecordTypeHasContentBytesV2(std::uint8_t pValue) {
  return pValue == static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFile);
}

inline constexpr bool TypedRecordTypeIsFolderV2(std::uint8_t pValue) {
  return pValue == static_cast<std::uint8_t>(TypedRecordTypeV2::kManifestFolder) ||
         pValue == static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFolder);
}

CheckSumV2 ComputeSectionCheckSum(const unsigned char* pPayloadBytes,
                                  std::size_t pPayloadLength,
                                  const SectionHeaderV2& pHeader);

bool ValidateSectionCheckSum(const SectionHeaderV2& pHeader,
                             const unsigned char* pPayloadBytes,
                             std::size_t pPayloadLength);

RepairRecordV2 MakeIgnoredRepairRecord(std::uint64_t pArchiveFamilyId,
                                       std::uint64_t pArchiveIndex,
                                       std::uint64_t pBlockIndex);

std::string MakeArchiveFileNameV2(const std::string& pPrefix,
                                  const std::string& pSourceStem,
                                  std::size_t pArchiveOrdinal,
                                  std::size_t pArchiveCount,
                                  const std::string& pSuffix = kArchiveFileSuffixV2);

bool ParseArchiveFileTemplateV2(const std::string& pFileName,
                                std::string& pOutPrefix,
                                std::uint32_t& pOutIndex,
                                std::string& pOutSuffix,
                                std::size_t& pOutDigits);

}  // namespace peanutbutter::memory_layout
