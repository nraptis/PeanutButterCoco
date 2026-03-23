#ifndef PEANUT_BUTTER_ULTIMA_STRESS_CRYPT_HPP_
#define PEANUT_BUTTER_ULTIMA_STRESS_CRYPT_HPP_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "Encryption/Ciphers/Rotation/RotateMaskBlockCipher.hpp"
#include "Encryption/Crypt.hpp"

namespace peanutbutter {

namespace stress_crypt_internal {

inline std::uint8_t FoldPasswordMask(const std::string& pPassword,
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

inline int ExpansionBaseShift(ExpansionStrength pStrength) {
  switch (pStrength) {
    case ExpansionStrength::kHigh:
      return 17;
    case ExpansionStrength::kMedium:
      return 11;
    case ExpansionStrength::kLow:
      return 5;
  }
  return 5;
}

inline int PasswordShiftDelta(const std::string& pPassword) {
  std::uint32_t aState = 0u;
  for (unsigned char aByte : pPassword) {
    aState = (aState * 33u) + static_cast<std::uint32_t>(aByte);
  }
  return static_cast<int>(aState % 43u);
}

inline int NormalizeRotateMaskShift(int pShift) {
  constexpr int kCipherBlockSize = 48;
  int aNormalized = pShift % kCipherBlockSize;
  if (aNormalized < 0) {
    aNormalized += kCipherBlockSize;
  }
  if (aNormalized == 0) {
    aNormalized = 1;
  }
  return aNormalized;
}

}  // namespace stress_crypt_internal

class RotationMaskCrypt final : public Crypt {
 public:
  explicit RotationMaskCrypt(const CryptGeneratorRequest& pRequest)
      : mCipher(
            stress_crypt_internal::FoldPasswordMask(
                pRequest.mPassword,
                static_cast<std::uint8_t>(
                    0x5Au ^
                    static_cast<std::uint8_t>(pRequest.mEncryptionStrength))),
            stress_crypt_internal::NormalizeRotateMaskShift(
                stress_crypt_internal::ExpansionBaseShift(
                    pRequest.mExpansionStrength) +
                stress_crypt_internal::PasswordShiftDelta(
                    pRequest.mPassword))) {}

  bool SealData(const unsigned char* pSource,
                unsigned char* pWorker,
                unsigned char* pDestination,
                std::size_t pLength,
                std::string* pErrorMessage,
                CryptMode pMode) const override {
    return mCipher.SealData(
        pSource, pWorker, pDestination, pLength, pErrorMessage, pMode);
  }

  bool UnsealData(const unsigned char* pSource,
                  unsigned char* pWorker,
                  unsigned char* pDestination,
                  std::size_t pLength,
                  std::string* pErrorMessage,
                  CryptMode pMode) const override {
    return mCipher.UnsealData(
        pSource, pWorker, pDestination, pLength, pErrorMessage, pMode);
  }

 private:
  RotateMaskBlockCipher48 mCipher;
};

inline std::unique_ptr<Crypt> MakeRotationMaskCrypt(
    const CryptGeneratorRequest& pRequest) {
  return std::make_unique<RotationMaskCrypt>(pRequest);
}

}  // namespace peanutbutter

#endif  // PEANUT_BUTTER_ULTIMA_STRESS_CRYPT_HPP_
