#include "Bundle_LogicalRecordEncoder.hpp"

#include <algorithm>
#include <cstring>

#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

BundleLogicalRecordEncoderV2::BundleLogicalRecordEncoderV2(
    const std::vector<BundleRecordEntryV2>& pRecords,
    FileSystemV2& pFileSystem,
    memory_layout::TypedRecordTypeV2 pFileType,
    memory_layout::TypedRecordTypeV2 pFolderType)
    : mRecords(pRecords),
      mFileSystem(pFileSystem),
      mFileType(static_cast<std::uint8_t>(pFileType)),
      mFolderType(static_cast<std::uint8_t>(pFolderType)) {
  StartNextRecord();
}

bool BundleLogicalRecordEncoderV2::Fill(unsigned char* pDestination,
                                        std::size_t pCapacity,
                                        bool pPauseAfterCurrentFileBoundary,
                                        std::size_t& pOutBytesWritten,
                                        std::uint64_t& pOutLogicalBytesWritten,
                                        std::uint64_t& pOutFileBytesWritten,
                                        bool& pOutPausedAtBoundary,
                                        std::string& pOutFailureMessage) {
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

  while (pOutBytesWritten < pCapacity && !mDone && !mPausedAtBoundary) {
    const std::size_t aWritable = pCapacity - pOutBytesWritten;
    switch (mStage) {
      case Stage::kPathLength: {
        if (mPathLengthBytesUsed == 0u && aWritable < sizeof(mPathLengthLe)) {
          pOutPausedAtBoundary = true;
          return true;
        }
        while (mPathLengthBytesUsed < 2u && pOutBytesWritten < pCapacity) {
          pDestination[pOutBytesWritten++] = mPathLengthLe[mPathLengthBytesUsed++];
          ++pOutLogicalBytesWritten;
        }
        if (mPathLengthBytesUsed == 2u) {
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
        if (!memory_layout::TypedRecordTypeHasFileSizeV2(mCurrentTypeFlag)) {
          FinishRecord();
        } else {
          mStage = Stage::kFileSize;
        }
        break;
      }

      case Stage::kFileSize: {
        if (mFileSizeBytesUsed == 0u && aWritable < sizeof(mFileSizeLe)) {
          pOutPausedAtBoundary = true;
          return true;
        }
        while (mFileSizeBytesUsed < 8u && pOutBytesWritten < pCapacity) {
          pDestination[pOutBytesWritten++] = mFileSizeLe[mFileSizeBytesUsed++];
          ++pOutLogicalBytesWritten;
        }
        if (mFileSizeBytesUsed == 8u) {
          if (!memory_layout::TypedRecordTypeHasContentBytesV2(mCurrentTypeFlag)) {
            FinishRecord();
          } else {
            mStage = Stage::kContentBytes;
          }
        }
        break;
      }

      case Stage::kContentBytes: {
        if (mCurrentRead == nullptr || !mCurrentRead->IsReady()) {
          pOutFailureMessage = "source file stream is not readable.";
          return false;
        }

        const std::size_t aChunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(static_cast<std::uint64_t>(aWritable),
                                    mContentBytesRemaining));
        if (aChunk == 0u) {
          FinishRecord();
          break;
        }
        if (!mCurrentRead->Read(static_cast<std::size_t>(mCurrentFileReadOffset),
                                pDestination + pOutBytesWritten,
                                aChunk)) {
          pOutFailureMessage = "failed reading source file bytes.";
          return false;
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
  return mStage == Stage::kContentBytes &&
         memory_layout::TypedRecordTypeHasContentBytesV2(mCurrentTypeFlag);
}

std::string BundleLogicalRecordEncoderV2::CurrentFileReference() const {
  if (mDone || mCurrentRecordIsDirectory || mCurrentRecordRelativePath.empty()) {
    return {};
  }
  return mCurrentRecordRelativePath;
}

std::size_t BundleLogicalRecordEncoderV2::PackedItemCount() const {
  return mRecordIndex;
}

void BundleLogicalRecordEncoderV2::StartNextRecord() {
  mPathLengthBytesUsed = 0u;
  mPathBytesUsed = 0u;
  mFileSizeBytesUsed = 0u;
  mContentBytesRemaining = 0u;
  mCurrentFileReadOffset = 0u;
  mCurrentRead.reset();
  mCurrentRecordRelativePath.clear();
  mCurrentTypeFlag = 0u;

  if (mRecordIndex >= mRecords.size()) {
    mCurrentPathLength = 0u;
    mDone = true;
    return;
  }
  mDone = false;

  const BundleRecordEntryV2& aRecord = mRecords[mRecordIndex];
  mCurrentRecordRelativePath = aRecord.mRelativePath;
  mCurrentRecordIsDirectory = aRecord.mIsDirectory;
  mCurrentPathLength =
      static_cast<std::uint16_t>(mCurrentRecordRelativePath.size());
  mPathLengthLe[0] = static_cast<unsigned char>(mCurrentPathLength & 0xFFu);
  mPathLengthLe[1] =
      static_cast<unsigned char>((mCurrentPathLength >> 8u) & 0xFFu);

  mCurrentTypeFlag = aRecord.mIsDirectory ? mFolderType : mFileType;
  for (int aByte = 0; aByte < 8; ++aByte) {
    mFileSizeLe[static_cast<std::size_t>(aByte)] =
        static_cast<unsigned char>((aRecord.mContentLength >> (8 * aByte)) & 0xFFu);
  }
  mContentBytesRemaining =
      memory_layout::TypedRecordTypeHasContentBytesV2(mCurrentTypeFlag)
          ? aRecord.mContentLength
          : 0u;

  if (memory_layout::TypedRecordTypeHasContentBytesV2(mCurrentTypeFlag)) {
    mCurrentRead = mFileSystem.OpenReadStream(aRecord.mSourcePath);
  }

  mStage = Stage::kPathLength;
}

void BundleLogicalRecordEncoderV2::FinishRecord() {
  const bool aWasDirectory = mCurrentRecordIsDirectory;
  ++mRecordIndex;
  StartNextRecord();

  if (!aWasDirectory && mPauseAfterCurrentFileRequested) {
    mPausedAtBoundary = true;
    mPauseAfterCurrentFileRequested = false;
  }
}

}  // namespace peanutbutter
