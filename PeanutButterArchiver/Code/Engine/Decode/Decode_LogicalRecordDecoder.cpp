#include "Decode_LogicalRecordDecoder.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

DecodeLogicalRecordDecoderV2::DecodeLogicalRecordDecoderV2(
    const std::string& pDestinationDirectory,
    FileSystemV2& pFileSystem)
    : mDestinationDirectory(pDestinationDirectory),
      mFileSystem(pFileSystem) {}

bool DecodeLogicalRecordDecoderV2::Consume(const unsigned char* pData,
                                           std::size_t pStart,
                                           std::size_t pEnd,
                                           bool pTreatZeroLengthAsPadding,
                                           bool& pOutTerminated,
                                           bool& pOutStoppedAtPadding,
                                           bool& pOutParseError,
                                           std::string& pOutParseErrorMessage,
                                           std::uint64_t& pOutDataBytesWritten) {
  pOutTerminated = false;
  pOutStoppedAtPadding = false;
  pOutParseError = false;
  pOutParseErrorMessage.clear();
  pOutDataBytesWritten = 0u;

  if (pData == nullptr || pStart > pEnd || pEnd > memory_layout::kSectionPayloadBytesV2) {
    pOutParseError = true;
    pOutParseErrorMessage = "invalid payload span while decoding.";
    return false;
  }

  std::size_t aOffset = pStart;
  while (aOffset < pEnd) {
    switch (mStage) {
      case Stage::kPathLength: {
        while (mPathLengthBytesUsed < 2u && aOffset < pEnd) {
          mPathLengthLe[mPathLengthBytesUsed++] = pData[aOffset++];
        }
        if (mPathLengthBytesUsed < 2u) {
          return true;
        }

        mCurrentPathLength = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(mPathLengthLe[0]) |
            (static_cast<std::uint16_t>(mPathLengthLe[1]) << 8u));
        if (mCurrentPathLength == 0u) {
          ResetRecordState();
          if (pTreatZeroLengthAsPadding) {
            pOutStoppedAtPadding = true;
          } else {
            pOutTerminated = true;
          }
          return true;
        }
        if (mCurrentPathLength > memory_layout::kMaxPathLengthV2) {
          pOutParseError = true;
          pOutParseErrorMessage = "path length exceeds supported maximum.";
          return false;
        }

        mCurrentPath.clear();
        mCurrentPath.reserve(mCurrentPathLength);
        mPathBytesUsed = 0u;
        mStage = Stage::kPathBytes;
        break;
      }

      case Stage::kPathBytes: {
        const std::size_t aRemaining = mCurrentPathLength - mPathBytesUsed;
        const std::size_t aChunk = std::min<std::size_t>(aRemaining, pEnd - aOffset);
        mCurrentPath.append(reinterpret_cast<const char*>(pData + aOffset), aChunk);
        mPathBytesUsed += aChunk;
        aOffset += aChunk;
        if (mPathBytesUsed < mCurrentPathLength) {
          return true;
        }
        if (!IsSafeRelativePath(mCurrentPath)) {
          pOutParseError = true;
          pOutParseErrorMessage = "path payload failed safety validation.";
          return false;
        }
        mContentLengthBytesUsed = 0u;
        std::memset(mContentLengthLe, 0, sizeof(mContentLengthLe));
        mStage = Stage::kContentLength;
        break;
      }

      case Stage::kContentLength: {
        while (mContentLengthBytesUsed < 8u && aOffset < pEnd) {
          mContentLengthLe[mContentLengthBytesUsed++] = pData[aOffset++];
        }
        if (mContentLengthBytesUsed < 8u) {
          return true;
        }

        mCurrentContentLength = 0u;
        for (int aByte = 0; aByte < 8; ++aByte) {
          mCurrentContentLength |=
              static_cast<std::uint64_t>(
                  mContentLengthLe[static_cast<std::size_t>(aByte)])
              << (8u * aByte);
        }

        if (mCurrentContentLength == memory_layout::kDirectoryRecordContentMarkerV2) {
          const std::string aDirPath =
              mFileSystem.JoinPath(mDestinationDirectory, mCurrentPath);
          if (!mFileSystem.EnsureDirectory(aDirPath)) {
            pOutParseError = true;
            pOutParseErrorMessage = "failed creating directory: " + aDirPath;
            return false;
          }
          ++mFoldersCreated;
          ResetRecordState();
          break;
        }

        const std::string aOutPath =
            mFileSystem.JoinPath(mDestinationDirectory, mCurrentPath);
        const std::string aOutParent = mFileSystem.ParentPath(aOutPath);
        if (!aOutParent.empty() && !mFileSystem.EnsureDirectory(aOutParent)) {
          pOutParseError = true;
          pOutParseErrorMessage =
              "failed creating parent directory for output file.";
          return false;
        }

        mCurrentWrite = mFileSystem.OpenWriteStream(aOutPath);
        if (mCurrentWrite == nullptr || !mCurrentWrite->IsReady()) {
          pOutParseError = true;
          pOutParseErrorMessage = "failed opening output file for writing.";
          return false;
        }
        mCurrentOutputPath = aOutPath;
        mContentBytesRemaining = mCurrentContentLength;
        mStage = Stage::kContentBytes;
        break;
      }

      case Stage::kContentBytes: {
        const std::size_t aChunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(mContentBytesRemaining,
                                    static_cast<std::uint64_t>(pEnd - aOffset)));
        if (aChunk == 0u) {
          FinishFileRecord();
          break;
        }
        if (!mCurrentWrite->Write(pData + aOffset, aChunk)) {
          pOutParseError = true;
          pOutParseErrorMessage = "failed writing decoded file bytes.";
          return false;
        }
        aOffset += aChunk;
        mContentBytesRemaining -= static_cast<std::uint64_t>(aChunk);
        mBytesWritten += static_cast<std::uint64_t>(aChunk);
        pOutDataBytesWritten += static_cast<std::uint64_t>(aChunk);
        if (mContentBytesRemaining == 0u) {
          FinishFileRecord();
        }
        break;
      }
    }
  }

  return true;
}

bool DecodeLogicalRecordDecoderV2::Finalize(std::string& pOutErrorMessage) const {
  pOutErrorMessage.clear();
  if (IsInsideFile()) {
    pOutErrorMessage = "decode ended while still writing a file.";
    return false;
  }
  if (!IsAtRecordBoundary()) {
    pOutErrorMessage = "decode ended in the middle of a logical record.";
    return false;
  }
  return true;
}

bool DecodeLogicalRecordDecoderV2::IsAtRecordBoundary() const {
  return mStage == Stage::kPathLength &&
         mPathLengthBytesUsed == 0u &&
         mCurrentWrite == nullptr;
}

bool DecodeLogicalRecordDecoderV2::IsInsideFile() const {
  return mStage == Stage::kContentBytes && mCurrentWrite != nullptr;
}

std::uint64_t DecodeLogicalRecordDecoderV2::FilesWritten() const {
  return mFilesWritten;
}

std::uint64_t DecodeLogicalRecordDecoderV2::FoldersCreated() const {
  return mFoldersCreated;
}

std::uint64_t DecodeLogicalRecordDecoderV2::BytesWritten() const {
  return mBytesWritten;
}

void DecodeLogicalRecordDecoderV2::ResetRecordState() {
  mStage = Stage::kPathLength;
  mPathLengthBytesUsed = 0u;
  mPathBytesUsed = 0u;
  mContentLengthBytesUsed = 0u;
  mCurrentPathLength = 0u;
  mCurrentContentLength = 0u;
  mContentBytesRemaining = 0u;
  std::memset(mPathLengthLe, 0, sizeof(mPathLengthLe));
  std::memset(mContentLengthLe, 0, sizeof(mContentLengthLe));
  mCurrentPath.clear();
  mCurrentOutputPath.clear();
  mCurrentWrite.reset();
}

void DecodeLogicalRecordDecoderV2::FinishFileRecord() {
  if (mCurrentWrite != nullptr) {
    (void)mCurrentWrite->Close();
    mCurrentWrite.reset();
  }
  ++mFilesWritten;
  ResetRecordState();
}

bool DecodeLogicalRecordDecoderV2::IsSafeRelativePath(const std::string& pPath) {
  if (pPath.empty() || pPath.size() > memory_layout::kMaxPathLengthV2) {
    return false;
  }
  if (pPath[0] == '/' || pPath[0] == '\\') {
    return false;
  }
  if (pPath.size() > 2u &&
      std::isalpha(static_cast<unsigned char>(pPath[0])) != 0 &&
      pPath[1] == ':') {
    return false;
  }

  std::size_t aStart = 0u;
  while (aStart < pPath.size()) {
    std::size_t aEnd = pPath.find_first_of("/\\", aStart);
    if (aEnd == std::string::npos) {
      aEnd = pPath.size();
    }
    if (aEnd == aStart) {
      return false;
    }
    const std::string aPart = pPath.substr(aStart, aEnd - aStart);
    if (aPart == "." || aPart == "..") {
      return false;
    }
    for (char aChar : aPart) {
      const unsigned char aByte = static_cast<unsigned char>(aChar);
      if (aByte < 32u || aByte == 127u) {
        return false;
      }
    }
    aStart = aEnd + 1u;
  }
  return true;
}

}  // namespace peanutbutter
