#pragma once

#include <cstdint>
#include <string>

#include "../MemoryLayout/ArchiveHeader.hpp"
#include "../MemoryLayout/SectionHeader.hpp"

namespace peanutbutter {

struct BaseArchiveBoxV2 {
  std::string mPath;
  std::uint64_t mArchiveIndex = 0u;
};

struct ArchiveBox_BundleV2 : public BaseArchiveBoxV2 {
  std::uint64_t mFamilyBlockStart = 0u;
  std::uint32_t mBlockCount = 0u;
};

struct ArchiveBox_DecodeV2 : public BaseArchiveBoxV2 {
  std::uint64_t mFileLength = 0u;
  std::uint64_t mReadableBlockCount = 0u;
  std::uint64_t mArchiveBlockCount = 0u;
  std::uint64_t mFilenameIndex = 0u;
  std::uint64_t mHeaderIndex = 0u;
  bool mIsPresent = true;
  bool mHasReadableHeader = false;
  bool mHasReadableSection = false;
  memory_layout::ArchiveHeaderV2 mHeader{};
  memory_layout::SectionHeaderV2 mFirstSectionHeader{};
};

struct ArchiveBox_RepairV2 : public BaseArchiveBoxV2 {
  bool mHasSourceFile = false;
  bool mNeedsSyntheticHeader = false;
  std::uint64_t mSourceFileBytes = 0u;
  std::uint64_t mExpectedBlocks = 0u;
  std::uint64_t mExpectedFileBytes = 0u;
};

}  // namespace peanutbutter
