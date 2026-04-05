#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../FileAccess/FileSystem.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "Bundle_Context.hpp"

namespace peanutbutter {

using BundleLogicalRecordObserverV2 =
    std::function<bool(const RuntimeEventV2&)>;

class BundleLogicalRecordEncoderV2 final {
 public:
  BundleLogicalRecordEncoderV2(const std::vector<BundleRecordEntryV2>& pRecords,
                               FileSystemV2& pFileSystem,
                               memory_layout::TypedRecordTypeV2 pFileType,
                               memory_layout::TypedRecordTypeV2 pFolderType,
                               memory_layout::TypedRecordTypeV2 pReferenceType =
                                   memory_layout::TypedRecordTypeV2::kDataReference,
                               ProgressStageV2 pStage = ProgressStageV2::kArchivePacking,
                               RuntimeEventKindV2 pStartEventKind =
                                   RuntimeEventKindV2::kBundleFileStarted,
                               RuntimeEventKindV2 pFinishEventKind =
                                   RuntimeEventKindV2::kBundleFileFinished,
                               bool pWritePreviewPlaceholderByte = false,
                               BundleLogicalRecordObserverV2 pObserver = {});

  bool Fill(unsigned char* pDestination,
            std::size_t pCapacity,
            bool pPauseAfterCurrentFileBoundary,
            std::size_t& pOutBytesWritten,
            std::uint64_t& pOutLogicalBytesWritten,
            std::uint64_t& pOutFileBytesWritten,
            bool& pOutPausedAtBoundary,
            std::string& pOutFailureMessage,
            bool pZeroPadRemainder = false);

  bool IsDone() const;
  bool HasRemainingRecords() const;
  bool IsInsideFile() const;
  const std::string& CurrentFileReference() const;
  std::size_t PackedItemCount() const;
  std::size_t PackedFileCount() const;
  std::size_t PackedFolderCount() const;
  void Reset();

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

  void StartNextRecord();
  void FinishRecord();
  bool EmitRecordEvent(RuntimeEventKindV2 pKind) const;

 private:
  const std::vector<BundleRecordEntryV2>& mRecords;
  FileSystemV2& mFileSystem;
  std::uint8_t mFileType = static_cast<std::uint8_t>(memory_layout::TypedRecordTypeV2::kDataFile);
  std::uint8_t mFolderType =
      static_cast<std::uint8_t>(memory_layout::TypedRecordTypeV2::kManifestFolder);
  std::uint8_t mReferenceType =
      static_cast<std::uint8_t>(memory_layout::TypedRecordTypeV2::kDataReference);
  std::size_t mRecordIndex = 0u;
  Stage mStage = Stage::kPathLength;
  std::unique_ptr<FileReadStreamV2> mCurrentRead;
  std::string mCurrentRecordRelativePath;
  std::string mCurrentReferenceTargetRelativePath;
  std::uint16_t mCurrentPathLength = 0u;
  std::uint16_t mCurrentReferenceTargetLength = 0u;
  std::uint64_t mCurrentRecordContentLength = 0u;
  std::uint64_t mContentBytesRemaining = 0u;
  std::uint64_t mCurrentFileReadOffset = 0u;
  unsigned char mPathLengthLe[2] = {};
  unsigned char mCurrentReferenceKind = 0u;
  unsigned char mReferenceTargetLengthLe[2] = {};
  unsigned char mCurrentTypeFlag = 0u;
  unsigned char mFileSizeLe[8] = {};
  std::size_t mPathLengthBytesUsed = 0u;
  std::size_t mPathBytesUsed = 0u;
  std::size_t mReferenceTargetLengthBytesUsed = 0u;
  std::size_t mReferenceTargetBytesUsed = 0u;
  std::size_t mFileSizeBytesUsed = 0u;
  bool mCurrentRecordIsDirectory = false;
  bool mCurrentRecordIsReference = false;
  bool mDone = false;
  bool mPauseAfterCurrentFileRequested = false;
  bool mPausedAtBoundary = false;
  bool mNeedsStartNextRecord = false;
  std::size_t mPackedFileCount = 0u;
  std::size_t mPackedFolderCount = 0u;
  ProgressStageV2 mRuntimeStage = ProgressStageV2::kArchivePacking;
  RuntimeEventKindV2 mStartEventKind = RuntimeEventKindV2::kBundleFileStarted;
  RuntimeEventKindV2 mFinishEventKind = RuntimeEventKindV2::kBundleFileFinished;
  bool mWritePreviewPlaceholderByte = false;
  BundleLogicalRecordObserverV2 mObserver;
};

}  // namespace peanutbutter
