#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "MemoryLayoutError.hpp"

namespace peanutbutter::memory_layout {

inline constexpr std::size_t kPackedUint24BytesV2 = 3u;
inline constexpr std::size_t kPackedUint48BytesV2 = 6u;

struct PackedUint24V2 {
  std::array<std::uint8_t, kPackedUint24BytesV2> mBytes{};
};

struct PackedUint48V2 {
  std::array<std::uint8_t, kPackedUint48BytesV2> mBytes{};
};

inline std::uint16_t ReadUint16LE(const unsigned char* pBytes) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(pBytes[0]) |
                                    (static_cast<std::uint16_t>(pBytes[1]) << 8));
}

inline std::uint32_t ReadUint24LE(const unsigned char* pBytes) {
  return static_cast<std::uint32_t>(static_cast<std::uint32_t>(pBytes[0]) |
                                    (static_cast<std::uint32_t>(pBytes[1]) << 8) |
                                    (static_cast<std::uint32_t>(pBytes[2]) << 16));
}

inline std::uint32_t ReadUint32LE(const unsigned char* pBytes) {
  return static_cast<std::uint32_t>(static_cast<std::uint32_t>(pBytes[0]) |
                                    (static_cast<std::uint32_t>(pBytes[1]) << 8) |
                                    (static_cast<std::uint32_t>(pBytes[2]) << 16) |
                                    (static_cast<std::uint32_t>(pBytes[3]) << 24));
}

inline std::uint64_t ReadUint64LE(const unsigned char* pBytes) {
  std::uint64_t aValue = 0u;
  for (std::size_t aIndex = 0u; aIndex < 8u; ++aIndex) {
    aValue |= (static_cast<std::uint64_t>(pBytes[aIndex]) << (aIndex * 8u));
  }
  return aValue;
}

inline void WriteUint16LE(std::uint16_t pValue, unsigned char* pOutBytes) {
  pOutBytes[0] = static_cast<unsigned char>(pValue & 0xFFu);
  pOutBytes[1] = static_cast<unsigned char>((pValue >> 8u) & 0xFFu);
}

inline void WriteUint24LE(std::uint32_t pValue, unsigned char* pOutBytes) {
  pOutBytes[0] = static_cast<unsigned char>(pValue & 0xFFu);
  pOutBytes[1] = static_cast<unsigned char>((pValue >> 8u) & 0xFFu);
  pOutBytes[2] = static_cast<unsigned char>((pValue >> 16u) & 0xFFu);
}

inline void WriteUint32LE(std::uint32_t pValue, unsigned char* pOutBytes) {
  pOutBytes[0] = static_cast<unsigned char>(pValue & 0xFFu);
  pOutBytes[1] = static_cast<unsigned char>((pValue >> 8u) & 0xFFu);
  pOutBytes[2] = static_cast<unsigned char>((pValue >> 16u) & 0xFFu);
  pOutBytes[3] = static_cast<unsigned char>((pValue >> 24u) & 0xFFu);
}

inline void WriteUint64LE(std::uint64_t pValue, unsigned char* pOutBytes) {
  for (std::size_t aIndex = 0u; aIndex < 8u; ++aIndex) {
    pOutBytes[aIndex] = static_cast<unsigned char>((pValue >> (aIndex * 8u)) & 0xFFu);
  }
}

inline std::uint32_t PackedUint24ToUInt32(const PackedUint24V2& pValue) {
  return ReadUint24LE(pValue.mBytes.data());
}

inline std::uint64_t PackedUint48ToUInt64(const PackedUint48V2& pValue) {
  std::uint64_t aOut = 0u;
  for (std::size_t aIndex = 0u; aIndex < kPackedUint48BytesV2; ++aIndex) {
    aOut |= (static_cast<std::uint64_t>(pValue.mBytes[aIndex]) << (aIndex * 8u));
  }
  return aOut;
}

inline bool TrySetPackedUint24(PackedUint24V2& pOutValue,
                               std::uint32_t pValue,
                               MemoryLayoutErrorInfo* pOutError = nullptr,
                               const std::string& pFieldName = std::string()) {
  if (pValue > 0x00FFFFFFu) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kIntegerOutOfRange,
                                            "TrySetPackedUint24",
                                            pFieldName,
                                            0u,
                                            0u,
                                            kPackedUint24BytesV2,
                                            static_cast<std::uint64_t>(pValue),
                                            0x00FFFFFFu);
    return false;
  }

  WriteUint24LE(pValue, pOutValue.mBytes.data());
  return true;
}

inline bool TrySetPackedUint48(PackedUint48V2& pOutValue,
                               std::uint64_t pValue,
                               MemoryLayoutErrorInfo* pOutError = nullptr,
                               const std::string& pFieldName = std::string()) {
  if (pValue > 0x0000FFFFFFFFFFFFULL) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kIntegerOutOfRange,
                                            "TrySetPackedUint48",
                                            pFieldName,
                                            0u,
                                            0u,
                                            kPackedUint48BytesV2,
                                            pValue,
                                            0x0000FFFFFFFFFFFFULL);
    return false;
  }

  for (std::size_t aIndex = 0u; aIndex < kPackedUint48BytesV2; ++aIndex) {
    pOutValue.mBytes[aIndex] =
        static_cast<std::uint8_t>((pValue >> (aIndex * 8u)) & 0xFFu);
  }
  return true;
}

static_assert(sizeof(PackedUint24V2) == kPackedUint24BytesV2,
              "PackedUint24V2 must remain 3 bytes.");
static_assert(sizeof(PackedUint48V2) == kPackedUint48BytesV2,
              "PackedUint48V2 must remain 6 bytes.");

}  // namespace peanutbutter::memory_layout
