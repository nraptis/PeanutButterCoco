#include "FormatUtilities.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

namespace peanutbutter::memory_layout {
namespace {

constexpr std::array<std::uint32_t, 64u> kSha256RoundConstantsV2 = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

inline std::uint32_t RotateRight32V2(std::uint32_t pValue, std::uint32_t pBits) {
  return (pValue >> pBits) | (pValue << (32u - pBits));
}

inline std::uint32_t Sha256ChoiceV2(std::uint32_t pX,
                                    std::uint32_t pY,
                                    std::uint32_t pZ) {
  return (pX & pY) ^ ((~pX) & pZ);
}

inline std::uint32_t Sha256MajorityV2(std::uint32_t pX,
                                      std::uint32_t pY,
                                      std::uint32_t pZ) {
  return (pX & pY) ^ (pX & pZ) ^ (pY & pZ);
}

inline std::uint32_t Sha256BigSigma0V2(std::uint32_t pValue) {
  return RotateRight32V2(pValue, 2u) ^ RotateRight32V2(pValue, 13u) ^
         RotateRight32V2(pValue, 22u);
}

inline std::uint32_t Sha256BigSigma1V2(std::uint32_t pValue) {
  return RotateRight32V2(pValue, 6u) ^ RotateRight32V2(pValue, 11u) ^
         RotateRight32V2(pValue, 25u);
}

inline std::uint32_t Sha256SmallSigma0V2(std::uint32_t pValue) {
  return RotateRight32V2(pValue, 7u) ^ RotateRight32V2(pValue, 18u) ^
         (pValue >> 3u);
}

inline std::uint32_t Sha256SmallSigma1V2(std::uint32_t pValue) {
  return RotateRight32V2(pValue, 17u) ^ RotateRight32V2(pValue, 19u) ^
         (pValue >> 10u);
}

inline std::uint32_t ReadUint32BEV2(const std::uint8_t* pBytes) {
  return (static_cast<std::uint32_t>(pBytes[0u]) << 24u) |
         (static_cast<std::uint32_t>(pBytes[1u]) << 16u) |
         (static_cast<std::uint32_t>(pBytes[2u]) << 8u) |
         static_cast<std::uint32_t>(pBytes[3u]);
}

inline void WriteUint32BEV2(std::uint32_t pValue, std::uint8_t* pOutBytes) {
  pOutBytes[0u] = static_cast<std::uint8_t>((pValue >> 24u) & 0xffu);
  pOutBytes[1u] = static_cast<std::uint8_t>((pValue >> 16u) & 0xffu);
  pOutBytes[2u] = static_cast<std::uint8_t>((pValue >> 8u) & 0xffu);
  pOutBytes[3u] = static_cast<std::uint8_t>(pValue & 0xffu);
}

struct Sha256StateV2 {
  std::array<std::uint32_t, 8u> mHash = {
      0x6a09e667u,
      0xbb67ae85u,
      0x3c6ef372u,
      0xa54ff53au,
      0x510e527fu,
      0x9b05688cu,
      0x1f83d9abu,
      0x5be0cd19u,
  };
  std::array<std::uint8_t, 64u> mBuffer = {};
  std::size_t mBufferBytes = 0u;
  std::uint64_t mTotalBytes = 0u;
};

void Sha256ProcessBlockV2(Sha256StateV2& pState, const std::uint8_t* pBlock) {
  std::array<std::uint32_t, 64u> aSchedule = {};
  for (std::size_t aIndex = 0u; aIndex < 16u; ++aIndex) {
    aSchedule[aIndex] = ReadUint32BEV2(pBlock + (aIndex * 4u));
  }
  for (std::size_t aIndex = 16u; aIndex < 64u; ++aIndex) {
    aSchedule[aIndex] = Sha256SmallSigma1V2(aSchedule[aIndex - 2u]) +
                        aSchedule[aIndex - 7u] +
                        Sha256SmallSigma0V2(aSchedule[aIndex - 15u]) +
                        aSchedule[aIndex - 16u];
  }

  std::uint32_t aA = pState.mHash[0u];
  std::uint32_t aB = pState.mHash[1u];
  std::uint32_t aC = pState.mHash[2u];
  std::uint32_t aD = pState.mHash[3u];
  std::uint32_t aE = pState.mHash[4u];
  std::uint32_t aF = pState.mHash[5u];
  std::uint32_t aG = pState.mHash[6u];
  std::uint32_t aH = pState.mHash[7u];

  for (std::size_t aRound = 0u; aRound < 64u; ++aRound) {
    const std::uint32_t aTemp1 = aH + Sha256BigSigma1V2(aE) +
                                 Sha256ChoiceV2(aE, aF, aG) +
                                 kSha256RoundConstantsV2[aRound] + aSchedule[aRound];
    const std::uint32_t aTemp2 =
        Sha256BigSigma0V2(aA) + Sha256MajorityV2(aA, aB, aC);
    aH = aG;
    aG = aF;
    aF = aE;
    aE = aD + aTemp1;
    aD = aC;
    aC = aB;
    aB = aA;
    aA = aTemp1 + aTemp2;
  }

  pState.mHash[0u] += aA;
  pState.mHash[1u] += aB;
  pState.mHash[2u] += aC;
  pState.mHash[3u] += aD;
  pState.mHash[4u] += aE;
  pState.mHash[5u] += aF;
  pState.mHash[6u] += aG;
  pState.mHash[7u] += aH;
}

void Sha256UpdateV2(Sha256StateV2& pState,
                    const unsigned char* pBytes,
                    std::size_t pByteCount) {
  if (pBytes == nullptr || pByteCount == 0u) {
    return;
  }

  pState.mTotalBytes += static_cast<std::uint64_t>(pByteCount);
  std::size_t aOffset = 0u;
  if (pState.mBufferBytes > 0u) {
    const std::size_t aCopyBytes = std::min(
        pState.mBuffer.size() - pState.mBufferBytes, pByteCount);
    std::memcpy(pState.mBuffer.data() + pState.mBufferBytes, pBytes, aCopyBytes);
    pState.mBufferBytes += aCopyBytes;
    aOffset += aCopyBytes;
    if (pState.mBufferBytes == pState.mBuffer.size()) {
      Sha256ProcessBlockV2(pState, pState.mBuffer.data());
      pState.mBufferBytes = 0u;
    }
  }

  while ((aOffset + pState.mBuffer.size()) <= pByteCount) {
    Sha256ProcessBlockV2(pState, pBytes + aOffset);
    aOffset += pState.mBuffer.size();
  }

  const std::size_t aRemainder = pByteCount - aOffset;
  if (aRemainder > 0u) {
    std::memcpy(pState.mBuffer.data(), pBytes + aOffset, aRemainder);
    pState.mBufferBytes = aRemainder;
  }
}

void Sha256FinalizeV2(Sha256StateV2& pState,
                      std::uint8_t* pOutBytes,
                      std::size_t pOutByteCount) {
  if (pOutBytes == nullptr || pOutByteCount < 32u) {
    return;
  }

  const std::uint64_t aBitLength = pState.mTotalBytes * 8u;
  pState.mBuffer[pState.mBufferBytes++] = 0x80u;
  if (pState.mBufferBytes > 56u) {
    std::memset(pState.mBuffer.data() + pState.mBufferBytes,
                0,
                pState.mBuffer.size() - pState.mBufferBytes);
    Sha256ProcessBlockV2(pState, pState.mBuffer.data());
    pState.mBufferBytes = 0u;
  }

  std::memset(
      pState.mBuffer.data() + pState.mBufferBytes, 0, 56u - pState.mBufferBytes);
  pState.mBuffer[56u] = static_cast<std::uint8_t>((aBitLength >> 56u) & 0xffu);
  pState.mBuffer[57u] = static_cast<std::uint8_t>((aBitLength >> 48u) & 0xffu);
  pState.mBuffer[58u] = static_cast<std::uint8_t>((aBitLength >> 40u) & 0xffu);
  pState.mBuffer[59u] = static_cast<std::uint8_t>((aBitLength >> 32u) & 0xffu);
  pState.mBuffer[60u] = static_cast<std::uint8_t>((aBitLength >> 24u) & 0xffu);
  pState.mBuffer[61u] = static_cast<std::uint8_t>((aBitLength >> 16u) & 0xffu);
  pState.mBuffer[62u] = static_cast<std::uint8_t>((aBitLength >> 8u) & 0xffu);
  pState.mBuffer[63u] = static_cast<std::uint8_t>(aBitLength & 0xffu);
  Sha256ProcessBlockV2(pState, pState.mBuffer.data());
  pState.mBufferBytes = 0u;

  for (std::size_t aIndex = 0u; aIndex < pState.mHash.size(); ++aIndex) {
    WriteUint32BEV2(pState.mHash[aIndex], pOutBytes + (aIndex * 4u));
  }
}

}  // namespace

CheckSumV2 ComputeSectionCheckSum(const unsigned char* pPayloadBytes,
                                  std::size_t pPayloadLength,
                                  const SectionHeaderV2& pHeader) {
  static_assert(kCheckSumBytesV2 == 32u,
                "ComputeSectionCheckSum expects a 32-byte SHA-256 digest.");
  CheckSumV2 aCheckSum{};
  if (pPayloadBytes == nullptr || pPayloadLength == 0u) {
    return aCheckSum;
  }

  // Skip + repair + fixed metadata fields through archive family id.
  // Reserved bytes are intentionally excluded from checksum material.
  unsigned char aMetaBytes[kSkipRecordBytesV2 + kRepairRecordBytesV2 + 49u] = {};
  std::size_t aCursor = 0u;
  WriteSkipRecord(
      pHeader.mSkipRecord, aMetaBytes + aCursor, kSkipRecordBytesV2, nullptr);
  aCursor += kSkipRecordBytesV2;
  WriteRepairRecord(
      pHeader.mRepairRecord, aMetaBytes + aCursor, kRepairRecordBytesV2, nullptr);
  aCursor += kRepairRecordBytesV2;

  aMetaBytes[aCursor++] = pHeader.mCheckSumKind;
  aMetaBytes[aCursor++] = pHeader.mSectionType;
  aMetaBytes[aCursor++] = pHeader.mSectionFlags;
  WriteUint32LE(pHeader.mPayloadBytesUsed, aMetaBytes + aCursor);
  aCursor += 4u;
  WriteUint32LE(pHeader.mArchiveFileCount, aMetaBytes + aCursor);
  aCursor += 4u;
  WriteUint32LE(pHeader.mArchiveBlockCount, aMetaBytes + aCursor);
  aCursor += 4u;
  WriteUint32LE(pHeader.mArchiveIndex, aMetaBytes + aCursor);
  aCursor += 4u;
  WriteUint32LE(pHeader.mBlockIndex, aMetaBytes + aCursor);
  aCursor += 4u;
  std::memcpy(aMetaBytes + aCursor,
              pHeader.mBlockCountMain.mBytes.data(),
              kPackedUint48BytesV2);
  aCursor += kPackedUint48BytesV2;
  std::memcpy(aMetaBytes + aCursor,
              pHeader.mBlockCountPreview.mBytes.data(),
              kPackedUint48BytesV2);
  aCursor += kPackedUint48BytesV2;
  std::memcpy(aMetaBytes + aCursor,
              pHeader.mBlockCountRepair.mBytes.data(),
              kPackedUint48BytesV2);
  aCursor += kPackedUint48BytesV2;
  WriteUint64LE(pHeader.mArchiveFamilyId, aMetaBytes + aCursor);
  aCursor += 8u;

  Sha256StateV2 aState;
  Sha256UpdateV2(aState, pPayloadBytes, pPayloadLength);
  Sha256UpdateV2(aState, aMetaBytes, aCursor);
  Sha256FinalizeV2(aState, aCheckSum.mBytes.data(), aCheckSum.mBytes.size());
  return aCheckSum;
}

bool ValidateSectionCheckSum(const SectionHeaderV2& pHeader,
                             const unsigned char* pPayloadBytes,
                             std::size_t pPayloadLength) {
  const CheckSumV2 aExpected = ComputeSectionCheckSum(pPayloadBytes, pPayloadLength, pHeader);
  return CheckSumsEqual(aExpected, pHeader.mCheckSum);
}

std::string MakeArchiveFileNameV2(const std::string& pPrefix,
                                  std::size_t pArchiveOrdinal,
                                  std::size_t pArchiveCount,
                                  const std::string& pSuffix) {
  std::size_t aDigits = 1u;
  std::size_t aMax = pArchiveCount > 0u ? pArchiveCount : 1u;
  while (aMax >= 10u) {
    ++aDigits;
    aMax /= 10u;
  }

  std::ostringstream aStream;
  aStream << pPrefix << std::setw(static_cast<int>(aDigits))
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
