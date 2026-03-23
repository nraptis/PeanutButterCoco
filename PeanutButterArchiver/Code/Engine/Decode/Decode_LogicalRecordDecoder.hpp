#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "../FileAccess/FileSystem.hpp"
#include "../MemoryLayout/ArchiveLayoutConfig.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

enum class DecodeLogicalZoneV2 {
  kPreviewManifest = 0,
  kFolderManifest = 1,
  kData = 2,
};

class DecodeLogicalRecordDecoderV2 final {
 public:
  DecodeLogicalRecordDecoderV2(const std::string& pDestinationDirectory,
                               FileSystemV2& pFileSystem,
                               const memory_layout::ArchiveLayoutConfigV2& pLayout,
                               DecodeLogicalZoneV2 pZone);

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
  std::string CurrentFileReference() const;
  bool AbortCurrentFile();
  std::uint64_t FilesWritten() const;
  std::uint64_t FoldersCreated() const;
  std::uint64_t BytesWritten() const;

 private:
  enum class Stage {
    kPathLength = 0,
    kPathBytes = 1,
    kTypeFlag = 2,
    kFileSize = 3,
    kContentBytes = 4,
  };

  void ResetRecordState();
  bool FinishFileRecord(std::string& pOutErrorMessage);
  bool IsSafeRelativePath(const std::string& pPath) const;
  bool IsTypeAllowed(std::uint8_t pTypeFlag) const;
  bool ShouldMaterializeFolder(std::uint8_t pTypeFlag) const;
  bool ShouldMaterializeFile(std::uint8_t pTypeFlag) const;
  bool IsAtIllegalPartialScalarBoundary() const;
  bool ResolveOutputPaths(const std::string& pRelativePath,
                          std::string& pOutWritingPath,
                          std::string& pOutFinalPath,
                          std::string& pOutPartialPath) const;
  bool PromoteCurrentOutputToPartial();

 private:
  std::string mDestinationDirectory;
  FileSystemV2& mFileSystem;
  const memory_layout::ArchiveLayoutConfigV2& mLayout;
  DecodeLogicalZoneV2 mZone = DecodeLogicalZoneV2::kData;
  Stage mStage = Stage::kPathLength;
  unsigned char mPathLengthLe[2] = {};
  unsigned char mFileSizeLe[8] = {};
  std::size_t mPathLengthBytesUsed = 0u;
  std::size_t mPathBytesUsed = 0u;
  std::size_t mFileSizeBytesUsed = 0u;
  std::uint16_t mCurrentPathLength = 0u;
  std::uint8_t mCurrentTypeFlag = 0u;
  std::uint64_t mCurrentFileSize = 0u;
  std::uint64_t mContentBytesRemaining = 0u;
  std::string mCurrentPath;
  std::string mCurrentOutputPath;
  std::string mCurrentFinalPath;
  std::string mCurrentPartialPath;
  std::unique_ptr<FileWriteStreamV2> mCurrentWrite;
  std::uint64_t mCurrentFileBytesWritten = 0u;
  std::uint64_t mFilesWritten = 0u;
  std::uint64_t mFoldersCreated = 0u;
  std::uint64_t mBytesWritten = 0u;
};

}  // namespace peanutbutter
