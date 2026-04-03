#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../../Knobs.hpp"
#include "ArchiveHeader.hpp"
#include "SectionHeader.hpp"

namespace peanutbutter::memory_layout {

// Real Password Expander Size 7680
// Real L1 Size: 261120
// Real L2 Size: 522240
// Real L3 Size: 1044480

inline constexpr std::size_t kDefaultArchiveBlockBytesV2 =
    knobs::kDefaultArchiveBlockBytesV2;
inline constexpr std::size_t kDefaultMaxPathLengthV2 = knobs::kMaxPathLengthV2;
inline constexpr std::uint64_t kDefaultMaxArchiveCountV2 = 1048576u;
inline constexpr std::uint32_t kDefaultMaxBlocksPerArchiveV2 = 2048u;

struct ArchiveLayoutConfigV2 {
    
    // kSectionHeaderBytesV2
    // inline constexpr std::size_t kArchiveHeaderBytesV2 = 64u;
    
    std::size_t mArchiveBlockBytes = kDefaultArchiveBlockBytesV2;
    std::size_t mMaxPathLength = kDefaultMaxPathLengthV2;
    std::uint64_t mMaxArchiveCount = kDefaultMaxArchiveCountV2;
    std::uint32_t mMaxBlocksPerArchive = kDefaultMaxBlocksPerArchiveV2;
    
    void SetPayloadSizePerBlock(std::size_t pSize) {
        mArchiveBlockBytes = pSize + SectionHeaderBytes();
    }
    
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
        if (mArchiveBlockBytes > knobs::kDefaultArchiveBlockBytesV2) {
            if (pOutError != nullptr) {
                *pOutError =
                "archive block bytes must not exceed the fixed default block size";
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
    static ArchiveLayoutConfigV2 kDefault{};
    kDefault.mArchiveBlockBytes = knobs::kArchiveBlockBytesV2;
    kDefault.mMaxPathLength = knobs::kMaxPathLengthV2;
    return kDefault;
}

inline ArchiveLayoutConfigV2 MakeArchiveLayoutConfigV2(
    std::size_t pPayloadBytesPerBlock,
    std::uint32_t pMaxBlocksPerArchive = kDefaultMaxBlocksPerArchiveV2) {
    ArchiveLayoutConfigV2 aConfig = DefaultArchiveLayoutConfigV2();
    aConfig.SetPayloadSizePerBlock(pPayloadBytesPerBlock);
    aConfig.mMaxBlocksPerArchive =
        pMaxBlocksPerArchive == 0u ? 1u : pMaxBlocksPerArchive;
    return aConfig;
}

}  // namespace peanutbutter::memory_layout
