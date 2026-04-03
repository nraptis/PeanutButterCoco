#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../../Common/BundleRequest.hpp"

namespace peanutbutter {

class RotationMaskCipherV2 final {
 public:
  RotationMaskCipherV2() = default;
  RotationMaskCipherV2(const std::string& pPassword,
                       StrengthPresetV2 pEncryptionStrength,
                       StrengthPresetV2 pTableStrength);

  bool IsConfigured() const;
  bool Seal(const unsigned char* pSource,
            unsigned char* pWorker,
            unsigned char* pDestination,
            std::size_t pLength,
            std::string* pOutError = nullptr) const;
  bool Seal(const unsigned char* pSource,
            unsigned char* pDestination,
            std::size_t pLength,
            std::string* pOutError = nullptr) const;
  bool Unseal(const unsigned char* pSource,
              unsigned char* pWorker,
              unsigned char* pDestination,
              std::size_t pLength,
              std::string* pOutError = nullptr) const;
  bool Unseal(const unsigned char* pSource,
              unsigned char* pDestination,
              std::size_t pLength,
              std::string* pOutError = nullptr) const;
  static std::size_t WorkerBufferBytes();

 private:
  bool Apply(const unsigned char* pSource,
             unsigned char* pWorker,
             unsigned char* pDestination,
             std::size_t pLength,
             int pSignedShift,
             std::string* pOutError) const;
  static int NormalizeShift(int pShift);

 private:
  std::uint8_t mMask = 0u;
  int mShift = 0;
  bool mConfigured = false;
};

}  // namespace peanutbutter
