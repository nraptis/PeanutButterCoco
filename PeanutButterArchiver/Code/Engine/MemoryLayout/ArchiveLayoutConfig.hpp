#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ArchiveHeader.hpp"
#include "SectionHeader.hpp"

namespace peanutbutter::memory_layout {

struct ArchiveLayoutConfigV2 {
  std::size_t mArchiveBlockBytes = 1044480u;
  std::size_t mMaxPathLength = 16384u;
  std::uint64_t mMaxArchiveCount = 1048576u;
  std::uint32_t mMaxBlocksPerArchive = 2048u;

  std::size_t ArchiveHeaderBytes() const {
    return kArchiveHeaderBytesV2;
  }

  std::size_t SectionHeaderBytes() const {
    return kSectionHeaderBytesV2;
  }

  std::size_t SectionPayloadBytes() const {
    return mArchiveBlockBytes > kSectionHeaderBytesV2
               ? (mArchiveBlockBytes - kSectionHeaderBytesV2)
               : 0u;
  }

  bool IsValid(std::string* pOutError = nullptr) const {
    if (mArchiveBlockBytes <= kSectionHeaderBytesV2) {
      if (pOutError != nullptr) {
        *pOutError =
            "archive block bytes must exceed the fixed section header size";
      }
      return false;
    }
    if (mMaxPathLength == 0u) {
      if (pOutError != nullptr) {
        *pOutError = "max path length must be at least 1";
      }
      return false;
    }
    if (mMaxArchiveCount == 0u) {
      if (pOutError != nullptr) {
        *pOutError = "max archive count must be at least 1";
      }
      return false;
    }
    if (mMaxBlocksPerArchive == 0u) {
      if (pOutError != nullptr) {
        *pOutError = "max blocks per archive must be at least 1";
      }
      return false;
    }
    return true;
  }
};

inline const ArchiveLayoutConfigV2& DefaultArchiveLayoutConfigV2() {
  static const ArchiveLayoutConfigV2 kDefault{};
  return kDefault;
}

}  // namespace peanutbutter::memory_layout
