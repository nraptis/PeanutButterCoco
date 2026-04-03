#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "../../Common/Progress.hpp"
#include "../../Common/RuntimeEvent.hpp"
#include "../FileAccess/FileSystem.hpp"
#include "../MemoryLayout/ArchiveLayoutConfig.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

enum class DecodeLogicalZoneV2 {
  kPreviewManifest = 0,
  kFolderManifest = 1,
  kData = 2,
};

using DecodeLogicalRecordObserverV2 =
    std::function<bool(const RuntimeEventV2&)>;

class DecodeLogicalRecordDecoderV2 final {
 public:
  DecodeLogicalRecordDecoderV2(const std::string& pDestinationDirectory,
                               FileSystemV2& pFileSystem,
                               const memory_layout::ArchiveLayoutConfigV2& pLayout,
                               DecodeLogicalZoneV2 pZone,
                               ProgressStageV2 pStage = ProgressStageV2::kArchiveDecode,
                               RuntimeEventKindV2 pStartEventKind =
                                   RuntimeEventKindV2::kDecodeFileStarted,
                               RuntimeEventKindV2 pFinishEventKind =
                                   RuntimeEventKindV2::kDecodeFileFinished,
                               DecodeLogicalRecordObserverV2 pObserver = {});

  bool Consume(const unsigned char* pData,
               std::size_t pStart,
               std::size_t pEnd,
               bool pTreatZeroLengthAsPadding,
               bool& pOutTerminated,
               bool& pOutStoppedAtPadding,
               bool& pOutParseError,
               std::string& pOutParseErrorMessage,
               std::uint64_t& pOutDataBytesWritten,
               bool& pOutPausedAtBoundary,
               std::size_t& pOutResumeOffset,
               std::string& pOutPausedRecordReference);

  bool Finalize(std::string& pOutErrorMessage) const;
  bool IsAtRecordBoundary() const;
  bool IsInsideFile() const;
  const std::string& CurrentFileReference() const;
  bool AbortCurrentFile();
  void ResetAfterParseError();
  std::uint64_t FilesWritten() const;
  std::uint64_t FoldersCreated() const;
  std::uint64_t BytesWritten() const;

 private:
  enum class Stage {
    kPathLength = 0,
    kPathBytes = 1,
    kTypeFlag = 2,
    kReferenceKind = 3,
    kReferenceTargetLength = 4,
    kReferenceTargetBytes = 5,
    kPreviewPlaceholder = 6,
    kFileSize = 7,
    kContentBytes = 8,
  };

  void ResetRecordState();
  bool FinishFileRecord(bool& pOutShouldContinue,
                        std::string& pOutFinishedReference,
                        std::string& pOutErrorMessage);
  bool IsSafeRelativePath(const std::string& pPath) const;
  bool IsTypeAllowed(std::uint8_t pTypeFlag) const;
  bool ShouldMaterializeFolder(std::uint8_t pTypeFlag) const;
  bool ShouldMaterializeFile(std::uint8_t pTypeFlag) const;
  bool ShouldMaterializeReference(std::uint8_t pTypeFlag) const;
  bool FinishReferenceRecord(bool& pOutShouldContinue,
                             std::string& pOutFinishedReference,
                             std::string& pOutErrorMessage);
  bool ResolveReferenceOutputPath(const std::string& pRelativePath,
                                  std::string& pOutFinalPath) const;
  bool IsAtIllegalPartialScalarBoundary() const;
  bool ResolveOutputPaths(const std::string& pRelativePath,
                          std::string& pOutWritingPath,
                          std::string& pOutFinalPath,
                          std::string& pOutPartialPath) const;
  bool IsOutputPathInsideDestination(const std::string& pOutputPath) const;
  bool ResolveCanonicalOrAbsolutePath(const std::string& pPath,
                                      std::string& pOutResolvedPath) const;
  bool EnsureResolvedDestinationDirectory() const;
  bool IsResolvedPathInsideResolvedDestination(
      const std::string& pResolvedPath) const;
  bool PromoteCurrentOutputToPartial();
  void EmitRecordStartEvent() const;
  bool EmitRecordFinishEvent() const;

 private:
  std::string mDestinationDirectory;
  FileSystemV2& mFileSystem;
  const memory_layout::ArchiveLayoutConfigV2& mLayout;
  DecodeLogicalZoneV2 mZone = DecodeLogicalZoneV2::kData;
  Stage mStage = Stage::kPathLength;
  unsigned char mPathLengthLe[2] = {};
  unsigned char mReferenceTargetLengthLe[2] = {};
  unsigned char mFileSizeLe[8] = {};
  std::size_t mPathLengthBytesUsed = 0u;
  std::size_t mPathBytesUsed = 0u;
  std::size_t mReferenceTargetLengthBytesUsed = 0u;
  std::size_t mReferenceTargetBytesUsed = 0u;
  std::size_t mFileSizeBytesUsed = 0u;
  std::uint16_t mCurrentPathLength = 0u;
  std::uint16_t mCurrentReferenceTargetLength = 0u;
  std::uint8_t mCurrentTypeFlag = 0u;
  std::uint8_t mCurrentReferenceKind = 0u;
  std::uint64_t mCurrentFileSize = 0u;
  std::uint64_t mContentBytesRemaining = 0u;
  std::string mCurrentPath;
  std::string mCurrentReferenceTargetPath;
  std::string mCurrentOutputPath;
  std::string mCurrentFinalPath;
  std::string mCurrentPartialPath;
  std::unique_ptr<FileWriteStreamV2> mCurrentWrite;
  std::uint64_t mCurrentFileBytesWritten = 0u;
  std::uint64_t mFilesWritten = 0u;
  std::uint64_t mFoldersCreated = 0u;
  std::uint64_t mBytesWritten = 0u;
  ProgressStageV2 mRuntimeStage = ProgressStageV2::kArchiveDecode;
  RuntimeEventKindV2 mStartEventKind = RuntimeEventKindV2::kDecodeFileStarted;
  RuntimeEventKindV2 mFinishEventKind = RuntimeEventKindV2::kDecodeFileFinished;
  DecodeLogicalRecordObserverV2 mObserver;
  mutable bool mResolvedDestinationDirectoryInitialized = false;
  mutable bool mResolvedDestinationDirectoryValid = false;
  mutable std::string mResolvedDestinationDirectory;
};

}  // namespace peanutbutter
