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
inline constexpr std::size_t kMaxPathLengthV2 = 2048u;
inline constexpr std::uint64_t kDirectoryRecordContentMarkerV2 =
    0xFFFFFFFFFFFFFFFFULL;
inline constexpr char kArchiveFileSuffixV2[] = ".PBTR";

CheckSumV2 ComputeSectionCheckSum(const unsigned char* pPayloadBytes,
                                  std::size_t pPayloadLength,
                                  const SkipRecordV2& pSkipRecord,
                                  std::uint8_t pSectionType,
                                  std::uint8_t pSectionFlags,
                                  const RepairRecordV2& pRepairRecord);

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
