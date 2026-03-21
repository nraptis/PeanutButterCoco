#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "../FileAccess/FileSystem.hpp"

namespace peanutbutter {

class DecodeLogicalRecordDecoderV2 final {
 public:
  DecodeLogicalRecordDecoderV2(const std::string& pDestinationDirectory,
                               FileSystemV2& pFileSystem);

  bool Consume(const unsigned char* pData,
               std::size_t pStart,
               std::size_t pEnd,
               bool pTreatZeroLengthAsPadding,
               bool& pOutTerminated,
               bool& pOutStoppedAtPadding,
               bool& pOutParseError,
               std::string& pOutParseErrorMessage,
               std::uint64_t& pOutDataBytesWritten);

  bool Finalize(std::string& pOutErrorMessage) const;
  bool IsAtRecordBoundary() const;
  bool IsInsideFile() const;
  std::uint64_t FilesWritten() const;
  std::uint64_t FoldersCreated() const;
  std::uint64_t BytesWritten() const;

 private:
  enum class Stage {
    kPathLength = 0,
    kPathBytes = 1,
    kContentLength = 2,
    kContentBytes = 3,
  };

  void ResetRecordState();
  void FinishFileRecord();
  static bool IsSafeRelativePath(const std::string& pPath);

 private:
  std::string mDestinationDirectory;
  FileSystemV2& mFileSystem;
  Stage mStage = Stage::kPathLength;
  unsigned char mPathLengthLe[2] = {};
  unsigned char mContentLengthLe[8] = {};
  std::size_t mPathLengthBytesUsed = 0u;
  std::size_t mPathBytesUsed = 0u;
  std::size_t mContentLengthBytesUsed = 0u;
  std::uint16_t mCurrentPathLength = 0u;
  std::uint64_t mCurrentContentLength = 0u;
  std::uint64_t mContentBytesRemaining = 0u;
  std::string mCurrentPath;
  std::string mCurrentOutputPath;
  std::unique_ptr<FileWriteStreamV2> mCurrentWrite;
  std::uint64_t mFilesWritten = 0u;
  std::uint64_t mFoldersCreated = 0u;
  std::uint64_t mBytesWritten = 0u;
};

}  // namespace peanutbutter
