#include "Bundle_LogicalRecordEncoder.hpp"

#include <algorithm>
#include <cstring>

#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

BundleLogicalRecordEncoderV2::BundleLogicalRecordEncoderV2(
    const std::vector<BundleRecordEntryV2>& pRecords,
    FileSystemV2& pFileSystem,
    memory_layout::TypedRecordTypeV2 pFileType,
    memory_layout::TypedRecordTypeV2 pFolderType,
    memory_layout::TypedRecordTypeV2 pReferenceType,
    ProgressStageV2 pStage,
    RuntimeEventKindV2 pStartEventKind,
    RuntimeEventKindV2 pFinishEventKind,
    bool pWritePreviewPlaceholderByte,
    BundleLogicalRecordObserverV2 pObserver)
    : mRecords(pRecords),
      mFileSystem(pFileSystem),
      mFileType(static_cast<std::uint8_t>(pFileType)),
      mFolderType(static_cast<std::uint8_t>(pFolderType)),
      mReferenceType(static_cast<std::uint8_t>(pReferenceType)),
      mRuntimeStage(pStage),
      mStartEventKind(pStartEventKind),
      mFinishEventKind(pFinishEventKind),
      mWritePreviewPlaceholderByte(pWritePreviewPlaceholderByte),
      mObserver(std::move(pObserver)) {
  StartNextRecord();
}

bool BundleLogicalRecordEncoderV2::Fill(unsigned char* pDestination,
                                        std::size_t pCapacity,
                                        bool pPauseAfterCurrentFileBoundary,
                                        std::size_t& pOutBytesWritten,
                                        std::uint64_t& pOutLogicalBytesWritten,
                                        std::uint64_t& pOutFileBytesWritten,
                                        bool& pOutPausedAtBoundary,
                                        std::string& pOutFailureMessage,
                                        bool pZeroPadRemainder) {
  pOutBytesWritten = 0u;
  pOutLogicalBytesWritten = 0u;
  pOutFileBytesWritten = 0u;
  pOutPausedAtBoundary = false;
  pOutFailureMessage.clear();

  if (pDestination == nullptr && pCapacity > 0u) {
    pOutFailureMessage = "null destination payload buffer.";
    return false;
  }

  mPauseAfterCurrentFileRequested = pPauseAfterCurrentFileBoundary;
  mPausedAtBoundary = false;
  if (mNeedsStartNextRecord) {
    mNeedsStartNextRecord = false;
    StartNextRecord();
  }

  while (pOutBytesWritten < pCapacity && !mDone && !mPausedAtBoundary) {
    const std::size_t aWritable = pCapacity - pOutBytesWritten;
    switch (mStage) {
      case Stage::kPathLength: {
        const std::size_t aRemaining =
            sizeof(mPathLengthLe) - mPathLengthBytesUsed;
        const std::size_t aChunk = std::min<std::size_t>(aWritable, aRemaining);
        if (aChunk == 0u) {
          pOutPausedAtBoundary = true;
          return true;
        }
        std::memcpy(pDestination + pOutBytesWritten,
                    mPathLengthLe + mPathLengthBytesUsed,
                    aChunk);
        pOutBytesWritten += aChunk;
        pOutLogicalBytesWritten += static_cast<std::uint64_t>(aChunk);
        mPathLengthBytesUsed += aChunk;
        if (mPathLengthBytesUsed == sizeof(mPathLengthLe)) {
          mStage = Stage::kPathBytes;
        }
        break;
      }

      case Stage::kPathBytes: {
        const std::size_t aRemaining = mCurrentPathLength - mPathBytesUsed;
        const std::size_t aChunk = std::min<std::size_t>(aWritable, aRemaining);
        if (aChunk == 0u) {
          mStage = Stage::kTypeFlag;
          break;
        }
        std::memcpy(pDestination + pOutBytesWritten,
                    mCurrentRecordRelativePath.data() + mPathBytesUsed,
                    aChunk);
        pOutBytesWritten += aChunk;
        pOutLogicalBytesWritten += static_cast<std::uint64_t>(aChunk);
        mPathBytesUsed += aChunk;
        if (mPathBytesUsed == mCurrentPathLength) {
          mStage = Stage::kTypeFlag;
        }
        break;
      }

      case Stage::kTypeFlag: {
        if (aWritable == 0u) {
          pOutPausedAtBoundary = true;
          return true;
        }
        pDestination[pOutBytesWritten++] = mCurrentTypeFlag;
        ++pOutLogicalBytesWritten;
        if (memory_layout::TypedRecordTypeIsReferenceV2(mCurrentTypeFlag)) {
          mStage = Stage::kReferenceKind;
        } else {
          mStage = mWritePreviewPlaceholderByte ? Stage::kPreviewPlaceholder
                                                : Stage::kFileSize;
        }
        if (!memory_layout::TypedRecordTypeIsReferenceV2(mCurrentTypeFlag) &&
            !mWritePreviewPlaceholderByte &&
            !memory_layout::TypedRecordTypeHasFileSizeV2(mCurrentTypeFlag)) {
          FinishRecord();
        }
        break;
      }

      case Stage::kReferenceKind: {
        if (aWritable == 0u) {
          pOutPausedAtBoundary = true;
          return true;
        }
        pDestination[pOutBytesWritten++] = mCurrentReferenceKind;
        ++pOutLogicalBytesWritten;
        mStage = Stage::kReferenceTargetLength;
        break;
      }

      case Stage::kReferenceTargetLength: {
        const std::size_t aRemaining =
            sizeof(mReferenceTargetLengthLe) - mReferenceTargetLengthBytesUsed;
        const std::size_t aChunk = std::min<std::size_t>(aWritable, aRemaining);
        if (aChunk == 0u) {
          pOutPausedAtBoundary = true;
          return true;
        }
        std::memcpy(pDestination + pOutBytesWritten,
                    mReferenceTargetLengthLe + mReferenceTargetLengthBytesUsed,
                    aChunk);
        pOutBytesWritten += aChunk;
        pOutLogicalBytesWritten += static_cast<std::uint64_t>(aChunk);
        mReferenceTargetLengthBytesUsed += aChunk;
        if (mReferenceTargetLengthBytesUsed == sizeof(mReferenceTargetLengthLe)) {
          if (mCurrentReferenceTargetLength == 0u) {
            FinishRecord();
          } else {
            mStage = Stage::kReferenceTargetBytes;
          }
        }
        break;
      }

      case Stage::kReferenceTargetBytes: {
        const std::size_t aRemaining =
            mCurrentReferenceTargetLength - mReferenceTargetBytesUsed;
        const std::size_t aChunk = std::min<std::size_t>(aWritable, aRemaining);
        if (aChunk == 0u) {
          if (aRemaining == 0u) {
            FinishRecord();
          } else {
            pOutPausedAtBoundary = true;
            return true;
          }
          break;
        }
        std::memcpy(pDestination + pOutBytesWritten,
                    mCurrentReferenceTargetRelativePath.data() +
                        mReferenceTargetBytesUsed,
                    aChunk);
        pOutBytesWritten += aChunk;
        pOutLogicalBytesWritten += static_cast<std::uint64_t>(aChunk);
        mReferenceTargetBytesUsed += aChunk;
        if (mReferenceTargetBytesUsed == mCurrentReferenceTargetLength) {
          FinishRecord();
        }
        break;
      }

      case Stage::kPreviewPlaceholder: {
        if (aWritable == 0u) {
          pOutPausedAtBoundary = true;
          return true;
        }
        pDestination[pOutBytesWritten++] =
            memory_layout::specs_verified::kPreviewRecordPlaceholderValueV2;
        pOutLogicalBytesWritten +=
            static_cast<std::uint64_t>(
                memory_layout::specs_verified::kPreviewRecordPlaceholderBytesV2);
        if (!memory_layout::TypedRecordTypeHasFileSizeV2(mCurrentTypeFlag)) {
          FinishRecord();
        } else {
          mStage = Stage::kFileSize;
        }
        break;
      }

      case Stage::kFileSize: {
        const std::size_t aRemaining =
            sizeof(mFileSizeLe) - mFileSizeBytesUsed;
        const std::size_t aChunk = std::min<std::size_t>(aWritable, aRemaining);
        if (aChunk == 0u) {
          pOutPausedAtBoundary = true;
          return true;
        }
        std::memcpy(pDestination + pOutBytesWritten,
                    mFileSizeLe + mFileSizeBytesUsed,
                    aChunk);
        pOutBytesWritten += aChunk;
        pOutLogicalBytesWritten += static_cast<std::uint64_t>(aChunk);
        mFileSizeBytesUsed += aChunk;
        if (mFileSizeBytesUsed == sizeof(mFileSizeLe)) {
          if (!memory_layout::TypedRecordTypeHasContentBytesV2(mCurrentTypeFlag) ||
              mContentBytesRemaining == 0u) {
            FinishRecord();
          } else {
            mStage = Stage::kContentBytes;
          }
        }
        break;
      }

      case Stage::kContentBytes: {
        const std::size_t aChunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(static_cast<std::uint64_t>(aWritable),
                                    mContentBytesRemaining));
        if (aChunk == 0u) {
          FinishRecord();
          break;
        }
        bool aReadSucceeded = false;
        if (mCurrentRead != nullptr && mCurrentRead->IsReady()) {
          aReadSucceeded = mCurrentRead->Read(
              static_cast<std::size_t>(mCurrentFileReadOffset),
              pDestination + pOutBytesWritten,
              aChunk);
          if (!aReadSucceeded) {
            // Keep packing linear and deterministic even when source reads fail.
            mCurrentRead.reset();
          }
        }
        if (!aReadSucceeded) {
          std::memset(pDestination + pOutBytesWritten, 0, aChunk);
        }

        pOutBytesWritten += aChunk;
        pOutLogicalBytesWritten += static_cast<std::uint64_t>(aChunk);
        pOutFileBytesWritten += static_cast<std::uint64_t>(aChunk);
        mCurrentFileReadOffset += static_cast<std::uint64_t>(aChunk);
        mContentBytesRemaining -= static_cast<std::uint64_t>(aChunk);
        if (mContentBytesRemaining == 0u) {
          FinishRecord();
        }
        break;
      }
    }
  }

  if (pZeroPadRemainder &&
      pOutBytesWritten < pCapacity &&
      pDestination != nullptr) {
    std::memset(
        pDestination + pOutBytesWritten, 0, pCapacity - pOutBytesWritten);
  }

  pOutPausedAtBoundary = mPausedAtBoundary;
  return true;
}

bool BundleLogicalRecordEncoderV2::IsDone() const {
  return mDone;
}

bool BundleLogicalRecordEncoderV2::HasRemainingRecords() const {
  return mRecordIndex < mRecords.size();
}

bool BundleLogicalRecordEncoderV2::IsInsideFile() const {
  if (mNeedsStartNextRecord) {
    return false;
  }
  return mStage == Stage::kContentBytes &&
         memory_layout::TypedRecordTypeHasContentBytesV2(mCurrentTypeFlag);
}

const std::string& BundleLogicalRecordEncoderV2::CurrentFileReference() const {
  static const std::string kEmpty;
  if (mDone || mNeedsStartNextRecord || mCurrentRecordIsDirectory ||
      mCurrentRecordRelativePath.empty()) {
    return kEmpty;
  }
  return mCurrentRecordRelativePath;
}

std::size_t BundleLogicalRecordEncoderV2::PackedItemCount() const {
  return mRecordIndex;
}

std::size_t BundleLogicalRecordEncoderV2::PackedFileCount() const {
  return mPackedFileCount;
}

std::size_t BundleLogicalRecordEncoderV2::PackedFolderCount() const {
  return mPackedFolderCount;
}

void BundleLogicalRecordEncoderV2::Reset() {
  mRecordIndex = 0u;
  mStage = Stage::kPathLength;
  mCurrentRead.reset();
  mCurrentRecordRelativePath.clear();
  mCurrentReferenceTargetRelativePath.clear();
  mCurrentPathLength = 0u;
  mCurrentReferenceTargetLength = 0u;
  mCurrentRecordContentLength = 0u;
  mContentBytesRemaining = 0u;
  mCurrentFileReadOffset = 0u;
  std::memset(mPathLengthLe, 0, sizeof(mPathLengthLe));
  mCurrentReferenceKind = 0u;
  std::memset(mReferenceTargetLengthLe, 0, sizeof(mReferenceTargetLengthLe));
  mCurrentTypeFlag = 0u;
  std::memset(mFileSizeLe, 0, sizeof(mFileSizeLe));
  mPathLengthBytesUsed = 0u;
  mPathBytesUsed = 0u;
  mReferenceTargetLengthBytesUsed = 0u;
  mReferenceTargetBytesUsed = 0u;
  mFileSizeBytesUsed = 0u;
  mCurrentRecordIsDirectory = false;
  mCurrentRecordIsReference = false;
  mDone = false;
  mPauseAfterCurrentFileRequested = false;
  mPausedAtBoundary = false;
  mNeedsStartNextRecord = false;
  mPackedFileCount = 0u;
  mPackedFolderCount = 0u;
  StartNextRecord();
}

void BundleLogicalRecordEncoderV2::StartNextRecord() {
  mPathLengthBytesUsed = 0u;
  mPathBytesUsed = 0u;
  mReferenceTargetLengthBytesUsed = 0u;
  mReferenceTargetBytesUsed = 0u;
  mFileSizeBytesUsed = 0u;
  mCurrentRecordContentLength = 0u;
  mContentBytesRemaining = 0u;
  mCurrentFileReadOffset = 0u;
  mCurrentRead.reset();
  mCurrentRecordRelativePath.clear();
  mCurrentReferenceTargetRelativePath.clear();
  mCurrentReferenceTargetLength = 0u;
  mCurrentReferenceKind = 0u;
  std::memset(mReferenceTargetLengthLe, 0, sizeof(mReferenceTargetLengthLe));
  mCurrentTypeFlag = 0u;
  mCurrentRecordIsReference = false;

  if (mRecordIndex >= mRecords.size()) {
    mCurrentPathLength = 0u;
    mDone = true;
    return;
  }
  mDone = false;

  const BundleRecordEntryV2& aRecord = mRecords[mRecordIndex];
  mCurrentRecordRelativePath = aRecord.mRelativePath;
  mCurrentRecordIsDirectory = aRecord.mIsDirectory;
  mCurrentRecordIsReference = aRecord.mIsReference;
  mCurrentPathLength =
      static_cast<std::uint16_t>(mCurrentRecordRelativePath.size());
  std::uint16_t aPathLengthLe = mCurrentPathLength;
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  aPathLengthLe = static_cast<std::uint16_t>((aPathLengthLe << 8u) |
                                             (aPathLengthLe >> 8u));
#endif
  std::memcpy(mPathLengthLe, &aPathLengthLe, sizeof(aPathLengthLe));

  mCurrentTypeFlag =
      mCurrentRecordIsReference && !mWritePreviewPlaceholderByte
          ? mReferenceType
          : (aRecord.mIsDirectory ? mFolderType : mFileType);
  std::uint64_t aContentLengthLe = aRecord.mContentLength;
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  aContentLengthLe = __builtin_bswap64(aContentLengthLe);
#endif
  std::memcpy(mFileSizeLe, &aContentLengthLe, sizeof(aContentLengthLe));
  mContentBytesRemaining =
      memory_layout::TypedRecordTypeHasContentBytesV2(mCurrentTypeFlag)
          ? aRecord.mContentLength
          : 0u;
  mCurrentRecordContentLength = aRecord.mContentLength;
  if (memory_layout::TypedRecordTypeIsReferenceV2(mCurrentTypeFlag)) {
    mCurrentReferenceKind = aRecord.mReferenceKind;
    mCurrentReferenceTargetRelativePath = aRecord.mReferenceTargetRelativePath;
    mCurrentReferenceTargetLength = static_cast<std::uint16_t>(
        mCurrentReferenceTargetRelativePath.size());
    std::uint16_t aTargetLengthLe = mCurrentReferenceTargetLength;
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    aTargetLengthLe = static_cast<std::uint16_t>((aTargetLengthLe << 8u) |
                                                 (aTargetLengthLe >> 8u));
#endif
    std::memcpy(mReferenceTargetLengthLe,
                &aTargetLengthLe,
                sizeof(aTargetLengthLe));
  }

  if (memory_layout::TypedRecordTypeHasContentBytesV2(mCurrentTypeFlag) &&
      mContentBytesRemaining > 0u) {
    mCurrentRead = mFileSystem.OpenReadStream(aRecord.mSourcePath);
  }

  mStage = Stage::kPathLength;
  EmitRecordEvent(mStartEventKind);
}

void BundleLogicalRecordEncoderV2::FinishRecord() {
  const bool aWasDirectory = mCurrentRecordIsDirectory;
  const bool aShouldContinue = EmitRecordEvent(mFinishEventKind);
  if (aWasDirectory) {
    ++mPackedFolderCount;
  } else {
    ++mPackedFileCount;
  }
  ++mRecordIndex;

  const bool aShouldPauseAtBoundary =
      !aShouldContinue || (!aWasDirectory && mPauseAfterCurrentFileRequested);
  mPauseAfterCurrentFileRequested = false;
  if (aShouldPauseAtBoundary) {
    mPausedAtBoundary = true;
    mNeedsStartNextRecord = true;
    return;
  }

  StartNextRecord();
}

bool BundleLogicalRecordEncoderV2::EmitRecordEvent(RuntimeEventKindV2 pKind) const {
  if (!mObserver || mCurrentRecordRelativePath.empty()) {
    return true;
  }

  RuntimeEventV2 aEvent;
  aEvent.mKind = pKind;
  aEvent.mStage = mRuntimeStage;
  aEvent.SetInfo("relative_path", mCurrentRecordRelativePath);
  aEvent.SetInfo("file_name", mCurrentRecordRelativePath);
  aEvent.SetInfo("is_directory", mCurrentRecordIsDirectory);
  aEvent.SetInfo("content_length", mCurrentRecordContentLength);

  switch (pKind) {
    case RuntimeEventKindV2::kBundleFolderStarted:
      aEvent.mLabel = "Bundle started folder " + mCurrentRecordRelativePath;
      break;
    case RuntimeEventKindV2::kBundleFolderFinished:
      aEvent.mLabel = "Bundle finished folder " + mCurrentRecordRelativePath;
      break;
    case RuntimeEventKindV2::kBundleManifestItemStarted:
      aEvent.mLabel = "Bundle started manifest item " + mCurrentRecordRelativePath;
      break;
    case RuntimeEventKindV2::kBundleManifestItemFinished:
      aEvent.mLabel = "Bundle finished manifest item " + mCurrentRecordRelativePath;
      break;
    case RuntimeEventKindV2::kBundleFileStarted:
      aEvent.mLabel = "Bundle started file " + mCurrentRecordRelativePath;
      break;
    case RuntimeEventKindV2::kBundleFileFinished:
      aEvent.mLabel = "Bundle finished file " + mCurrentRecordRelativePath;
      break;
    default:
      aEvent.mLabel = RuntimeEventKindLabelV2(pKind);
      break;
  }

  return mObserver(aEvent);
}

}  // namespace peanutbutter
