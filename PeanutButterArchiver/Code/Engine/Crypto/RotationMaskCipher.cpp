#include "RotationMaskCipher.hpp"

#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {
namespace {

constexpr std::size_t kCipherTileBytesV2 = 48u;

std::uint8_t FoldPasswordMask(const std::string& pPassword,
                              std::uint8_t pSeed) {
  std::uint32_t aState = static_cast<std::uint32_t>(pSeed);
  for (unsigned char aByte : pPassword) {
    aState = (aState * 131u) ^ static_cast<std::uint32_t>(aByte);
  }

  std::uint8_t aMask = static_cast<std::uint8_t>(aState & 0xFFu);
  if (aMask == 0u) {
    aMask = static_cast<std::uint8_t>(pSeed ^ 0x5Au);
  }
  return aMask;
}

int StrengthBaseShift(StrengthPresetV2 pStrength) {
  switch (pStrength) {
    case StrengthPresetV2::kHigh:
      return 17;
    case StrengthPresetV2::kMedium:
      return 11;
    case StrengthPresetV2::kLow:
      return 5;
  }
  return 5;
}

int PasswordShiftDelta(const std::string& pPassword) {
  std::uint32_t aState = 0u;
  for (unsigned char aByte : pPassword) {
    aState = (aState * 33u) + static_cast<std::uint32_t>(aByte);
  }
  return static_cast<int>(aState % 43u);
}

bool AssignCipherError(std::string* pOutError,
                       const std::string& pMessage) {
  if (pOutError != nullptr) {
    *pOutError = pMessage;
  }
  return false;
}

}  // namespace

RotationMaskCipherV2::RotationMaskCipherV2(const std::string& pPassword,
                                           StrengthPresetV2 pEncryptionStrength,
                                           StrengthPresetV2 pTableStrength) {
  mMask = FoldPasswordMask(
      pPassword,
      static_cast<std::uint8_t>(0x5Au ^
                                static_cast<std::uint8_t>(pEncryptionStrength)));
  mShift = NormalizeShift(StrengthBaseShift(pTableStrength) +
                          PasswordShiftDelta(pPassword));
  mConfigured = true;
}

bool RotationMaskCipherV2::IsConfigured() const {
  return mConfigured;
}

bool RotationMaskCipherV2::Seal(const unsigned char* pSource,
                                unsigned char* pDestination,
                                std::size_t pLength,
                                std::string* pOutError) const {
  return Apply(pSource, pDestination, pLength, mShift, pOutError);
}

bool RotationMaskCipherV2::Unseal(const unsigned char* pSource,
                                  unsigned char* pDestination,
                                  std::size_t pLength,
                                  std::string* pOutError) const {
  return Apply(pSource, pDestination, pLength, -mShift, pOutError);
}

bool RotationMaskCipherV2::Apply(const unsigned char* pSource,
                                 unsigned char* pDestination,
                                 std::size_t pLength,
                                 int pSignedShift,
                                 std::string* pOutError) const {
  if (!mConfigured) {
    return AssignCipherError(pOutError, "rotation mask cipher is not configured.");
  }
  if (pLength == 0u) {
    return true;
  }
  if (pSource == nullptr || pDestination == nullptr) {
    return AssignCipherError(pOutError, "rotation mask cipher received null buffer.");
  }
  if (pSource == pDestination) {
    return AssignCipherError(pOutError, "rotation mask cipher requires distinct source and destination buffers.");
  }
  if ((pLength % kCipherTileBytesV2) != 0u) {
    return AssignCipherError(pOutError, "rotation mask cipher length must be divisible by 48.");
  }

  const std::size_t aRotation = static_cast<std::size_t>(NormalizeShift(pSignedShift));
  const unsigned char aAntiMask = static_cast<unsigned char>(~mMask);
  const std::size_t aBlockCount = pLength / kCipherTileBytesV2;
  for (std::size_t aBlock = 0u; aBlock < aBlockCount; ++aBlock) {
    const std::size_t aBase = aBlock * kCipherTileBytesV2;
    for (std::size_t aIndex = 0u; aIndex < kCipherTileBytesV2; ++aIndex) {
      const std::size_t aSourceIndex =
          (aIndex + aRotation < kCipherTileBytesV2)
              ? (aIndex + aRotation)
              : (aIndex + aRotation - kCipherTileBytesV2);
      const unsigned char aBaseByte =
          static_cast<unsigned char>(pSource[aBase + aIndex] & aAntiMask);
      const unsigned char aMasked =
          static_cast<unsigned char>(pSource[aBase + aSourceIndex] & mMask);
      pDestination[aBase + aIndex] =
          static_cast<unsigned char>(aBaseByte | aMasked);
    }
  }

  return true;
}

int RotationMaskCipherV2::NormalizeShift(int pShift) {
  int aRotation = pShift % static_cast<int>(kCipherTileBytesV2);
  if (aRotation < 0) {
    aRotation += static_cast<int>(kCipherTileBytesV2);
  }
  if (aRotation == 0) {
    aRotation = 1;
  }
  return aRotation;
}

}  // namespace peanutbutter
