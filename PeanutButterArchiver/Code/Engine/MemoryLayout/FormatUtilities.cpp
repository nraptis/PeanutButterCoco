#include "FormatUtilities.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>

namespace peanutbutter::memory_layout {
namespace {

constexpr std::uint64_t kFnvOffsetBasis64 = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime64 = 1099511628211ULL;

inline std::uint64_t Fnv1aUpdate(std::uint64_t pState,
                                 unsigned char pByte) {
  pState ^= static_cast<std::uint64_t>(pByte);
  pState *= kFnvPrime64;
  return pState;
}

inline std::uint64_t HashBytes(const unsigned char* pData,
                               std::size_t pLength,
                               std::uint64_t pSeed,
                               unsigned char pTag) {
  std::uint64_t aState = pSeed;
  aState = Fnv1aUpdate(aState, pTag);
  if (pData == nullptr) {
    return aState;
  }
  for (std::size_t aIndex = 0u; aIndex < pLength; ++aIndex) {
    aState = Fnv1aUpdate(aState, pData[aIndex]);
  }
  return aState;
}

inline std::uint64_t MixU64(std::uint64_t pValue) {
  pValue ^= (pValue >> 33u);
  pValue *= 0xff51afd7ed558ccdULL;
  pValue ^= (pValue >> 33u);
  pValue *= 0xc4ceb9fe1a85ec53ULL;
  pValue ^= (pValue >> 33u);
  return pValue;
}

}  // namespace

CheckSumV2 ComputeSectionCheckSum(const unsigned char* pPayloadBytes,
                                  std::size_t pPayloadLength,
                                  const SkipRecordV2& pSkipRecord,
                                  std::uint8_t pSectionType,
                                  std::uint8_t pSectionFlags,
                                  const RepairRecordV2& pRepairRecord) {
  CheckSumV2 aCheckSum{};
  if (pPayloadBytes == nullptr || pPayloadLength == 0u) {
    return aCheckSum;
  }

  std::uint64_t aState0 = kFnvOffsetBasis64 ^ 0x1020304050607080ULL;
  std::uint64_t aState1 = kFnvOffsetBasis64 ^ 0x8877665544332211ULL;
  std::uint64_t aState2 = kFnvOffsetBasis64 ^ 0xA5A5A5A5A5A5A5A5ULL;
  std::uint64_t aState3 = kFnvOffsetBasis64 ^ 0x5A5A5A5A5A5A5A5AULL;

  for (std::size_t aIndex = 0u; aIndex < pPayloadLength; ++aIndex) {
    const unsigned char aByte = pPayloadBytes[aIndex];
    aState0 = Fnv1aUpdate(aState0, aByte);
    aState1 = Fnv1aUpdate(aState1, static_cast<unsigned char>(aByte ^ 0x5Au));
    aState2 = Fnv1aUpdate(
        aState2,
        static_cast<unsigned char>(aByte + static_cast<unsigned char>(aIndex & 0xFFu)));
    aState3 = Fnv1aUpdate(
        aState3,
        static_cast<unsigned char>(aByte ^ static_cast<unsigned char>((aIndex * 131u) & 0xFFu)));
  }

  unsigned char aMetaBytes[17] = {};
  WriteSkipRecord(pSkipRecord, aMetaBytes + 0u, kSkipRecordBytesV2, nullptr);
  aMetaBytes[7u] = pSectionType;
  aMetaBytes[8u] = pSectionFlags;
  WriteRepairRecord(pRepairRecord, aMetaBytes + 9u, kRepairRecordBytesV2, nullptr);

  aState0 = HashBytes(aMetaBytes, sizeof(aMetaBytes), aState0, 1u);
  aState1 = HashBytes(aMetaBytes, sizeof(aMetaBytes), aState1, 2u);
  aState2 = HashBytes(aMetaBytes, sizeof(aMetaBytes), aState2, 3u);
  aState3 = HashBytes(aMetaBytes, sizeof(aMetaBytes), aState3, 4u);

  WriteUint64LE(MixU64(aState0), aCheckSum.mBytes.data() + 0u);
  WriteUint64LE(MixU64(aState1), aCheckSum.mBytes.data() + 8u);
  WriteUint64LE(MixU64(aState2), aCheckSum.mBytes.data() + 16u);
  WriteUint64LE(MixU64(aState3), aCheckSum.mBytes.data() + 24u);
  return aCheckSum;
}

bool ValidateSectionCheckSum(const SectionHeaderV2& pHeader,
                             const unsigned char* pPayloadBytes,
                             std::size_t pPayloadLength) {
  const CheckSumV2 aExpected = ComputeSectionCheckSum(
      pPayloadBytes,
      pPayloadLength,
      pHeader.mSkipRecord,
      pHeader.mSectionType,
      pHeader.mSectionFlags,
      pHeader.mRepairRecord);
  return CheckSumsEqual(aExpected, pHeader.mCheckSum);
}

RepairRecordV2 MakeIgnoredRepairRecord(std::uint64_t pArchiveFamilyId,
                                       std::uint64_t pArchiveIndex,
                                       std::uint64_t pBlockIndex) {
  const std::uint64_t aSeed =
      MixU64((pArchiveFamilyId << 1u) ^ (pArchiveIndex * 1315423911ULL) ^
             (pBlockIndex * 2654435761ULL));
  RepairRecordV2 aRecord;
  aRecord.mRepairPointerArchive =
      static_cast<std::uint32_t>((aSeed & 0xFFFFFFFFULL) ^ 0xA5A5A5A5u);
  aRecord.mRepairPointerBlock =
      static_cast<std::uint32_t>(((aSeed >> 32u) & 0xFFFFFFFFULL) ^ 0x5A5A5A5Au);
  return aRecord;
}

std::string MakeArchiveFileNameV2(const std::string& pPrefix,
                                  const std::string& pSourceStem,
                                  std::size_t pArchiveOrdinal,
                                  std::size_t pArchiveCount,
                                  const std::string& pSuffix) {
  std::size_t aDigits = 1u;
  std::size_t aMax = pArchiveCount > 0u ? (pArchiveCount - 1u) : 0u;
  while (aMax >= 10u) {
    ++aDigits;
    aMax /= 10u;
  }

  std::ostringstream aStream;
  aStream << pPrefix << pSourceStem << "_" << std::setw(static_cast<int>(aDigits))
          << std::setfill('0') << pArchiveOrdinal;
  if (!pSuffix.empty()) {
    if (pSuffix[0] == '.') {
      aStream << pSuffix;
    } else {
      aStream << "." << pSuffix;
    }
  }
  return aStream.str();
}

bool ParseArchiveFileTemplateV2(const std::string& pFileName,
                                std::string& pOutPrefix,
                                std::uint32_t& pOutIndex,
                                std::string& pOutSuffix,
                                std::size_t& pOutDigits) {
  pOutPrefix.clear();
  pOutIndex = 0u;
  pOutSuffix.clear();
  pOutDigits = 0u;

  if (pFileName.empty()) {
    return false;
  }

  const std::size_t aDot = pFileName.find_last_of('.');
  const std::string aBase =
      aDot == std::string::npos ? pFileName : pFileName.substr(0u, aDot);
  pOutSuffix = aDot == std::string::npos ? std::string() : pFileName.substr(aDot);
  if (aBase.empty()) {
    return false;
  }

  std::size_t aDigitsStart = aBase.size();
  while (aDigitsStart > 0u &&
         std::isdigit(static_cast<unsigned char>(aBase[aDigitsStart - 1u])) != 0) {
    --aDigitsStart;
  }
  if (aDigitsStart == aBase.size()) {
    return false;
  }

  const std::string aDigits = aBase.substr(aDigitsStart);
  if (aDigits.size() > 9u) {
    return false;
  }

  std::uint64_t aIndex = 0u;
  for (char aChar : aDigits) {
    if (std::isdigit(static_cast<unsigned char>(aChar)) == 0) {
      return false;
    }
    aIndex = (aIndex * 10u) + static_cast<std::uint64_t>(aChar - '0');
    if (aIndex > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
      return false;
    }
  }

  pOutPrefix = aBase.substr(0u, aDigitsStart);
  pOutIndex = static_cast<std::uint32_t>(aIndex);
  pOutDigits = aDigits.size();
  return true;
}

}  // namespace peanutbutter::memory_layout
