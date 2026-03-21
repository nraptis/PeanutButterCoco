#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../FileAccess/FileSystem.hpp"
#include "Bundle_Context.hpp"

namespace peanutbutter {

class BundleLogicalRecordEncoderV2 final {
 public:
  BundleLogicalRecordEncoderV2(const std::vector<BundleRecordEntryV2>& pRecords,
                               FileSystemV2& pFileSystem);

  bool Fill(unsigned char* pDestination,
            std::size_t pCapacity,
            bool pPauseAfterCurrentFileBoundary,
            std::size_t& pOutBytesWritten,
            std::uint64_t& pOutLogicalBytesWritten,
            std::uint64_t& pOutFileBytesWritten,
            bool& pOutPausedAtBoundary,
            std::string& pOutFailureMessage);

  bool IsDone() const;
  bool HasRemainingRecords() const;
  bool IsInsideFile() const;
  std::size_t PackedItemCount() const;

 private:
  enum class Stage {
    kPathLength = 0,
    kPathBytes = 1,
    kContentLength = 2,
    kContentBytes = 3,
  };

  void StartNextRecord();
  void FinishRecord();

 private:
  const std::vector<BundleRecordEntryV2>& mRecords;
  FileSystemV2& mFileSystem;
  std::size_t mRecordIndex = 0u;
  Stage mStage = Stage::kPathLength;
  std::unique_ptr<FileReadStreamV2> mCurrentRead;
  std::string mCurrentRecordRelativePath;
  std::uint16_t mCurrentPathLength = 0u;
  std::uint64_t mContentBytesRemaining = 0u;
  std::uint64_t mCurrentFileReadOffset = 0u;
  unsigned char mPathLengthLe[2] = {};
  unsigned char mContentLengthLe[8] = {};
  std::size_t mPathLengthBytesUsed = 0u;
  std::size_t mPathBytesUsed = 0u;
  std::size_t mContentLengthBytesUsed = 0u;
  bool mCurrentRecordIsDirectory = false;
  bool mDone = false;
  bool mPauseAfterCurrentFileRequested = false;
  bool mPausedAtBoundary = false;
};

}  // namespace peanutbutter
