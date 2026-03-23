#include "Decode_LogicalRecordDecoder.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace peanutbutter {

DecodeLogicalRecordDecoderV2::DecodeLogicalRecordDecoderV2(
    const std::string& pDestinationDirectory,
    FileSystemV2& pFileSystem,
    const memory_layout::ArchiveLayoutConfigV2& pLayout,
    DecodeLogicalZoneV2 pZone)
    : mDestinationDirectory(pDestinationDirectory),
      mFileSystem(pFileSystem),
      mLayout(pLayout),
      mZone(pZone) {}

bool DecodeLogicalRecordDecoderV2::Consume(const unsigned char* pData,
                                           std::size_t pStart,
                                           std::size_t pEnd,
                                           bool pTreatZeroLengthAsPadding,
                                           bool& pOutTerminated,
                                           bool& pOutStoppedAtPadding,
                                           bool& pOutParseError,
                                           std::string& pOutParseErrorMessage,
                                           std::uint64_t& pOutDataBytesWritten) {
  (void)pTreatZeroLengthAsPadding;
  pOutTerminated = false;
  pOutStoppedAtPadding = false;
  pOutParseError = false;
  pOutParseErrorMessage.clear();
  pOutDataBytesWritten = 0u;

  if (pData == nullptr || pStart > pEnd || pEnd > mLayout.SectionPayloadBytes()) {
    pOutParseError = true;
    pOutParseErrorMessage = "invalid payload span while decoding.";
    return false;
  }

  std::size_t aOffset = pStart;
  while (aOffset < pEnd) {
    switch (mStage) {
      case Stage::kPathLength: {
        while (mPathLengthBytesUsed < sizeof(mPathLengthLe) && aOffset < pEnd) {
          mPathLengthLe[mPathLengthBytesUsed++] = pData[aOffset++];
        }
        if (mPathLengthBytesUsed < sizeof(mPathLengthLe)) {
          break;
        }

        mCurrentPathLength = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(mPathLengthLe[0]) |
            (static_cast<std::uint16_t>(mPathLengthLe[1]) << 8u));
        if (mCurrentPathLength == 0u) {
          pOutParseError = true;
          pOutParseErrorMessage = "path length must not be zero.";
          return false;
        }
        if (mCurrentPathLength > mLayout.mMaxPathLength) {
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
        if (aChunk == 0u) {
          break;
        }
        mCurrentPath.append(reinterpret_cast<const char*>(pData + aOffset), aChunk);
        mPathBytesUsed += aChunk;
        aOffset += aChunk;
        if (mPathBytesUsed < mCurrentPathLength) {
          break;
        }
        if (!IsSafeRelativePath(mCurrentPath)) {
          pOutParseError = true;
          pOutParseErrorMessage = "path payload failed safety validation.";
          return false;
        }
        mCurrentTypeFlag = 0u;
        mStage = Stage::kTypeFlag;
        break;
      }

      case Stage::kTypeFlag: {
        if (aOffset >= pEnd) {
          break;
        }
        mCurrentTypeFlag = pData[aOffset++];
        if (!memory_layout::IsKnownTypedRecordTypeV2(mCurrentTypeFlag)) {
          pOutParseError = true;
          pOutParseErrorMessage = "record type flag is unknown.";
          return false;
        }
        if (!IsTypeAllowed(mCurrentTypeFlag)) {
          pOutParseError = true;
          pOutParseErrorMessage = "record type is not allowed in this section.";
          return false;
        }
        if (memory_layout::TypedRecordTypeIsFolderV2(mCurrentTypeFlag)) {
          if (ShouldMaterializeFolder(mCurrentTypeFlag)) {
            const std::string aDirPath =
                mFileSystem.JoinPath(mDestinationDirectory, mCurrentPath);
            if (!mFileSystem.EnsureDirectory(aDirPath)) {
              pOutParseError = true;
              pOutParseErrorMessage = "failed creating directory: " + aDirPath;
              return false;
            }
            ++mFoldersCreated;
          }
          ResetRecordState();
          break;
        }
        mFileSizeBytesUsed = 0u;
        std::memset(mFileSizeLe, 0, sizeof(mFileSizeLe));
        mStage = Stage::kFileSize;
        break;
      }

      case Stage::kFileSize: {
        while (mFileSizeBytesUsed < sizeof(mFileSizeLe) && aOffset < pEnd) {
          mFileSizeLe[mFileSizeBytesUsed++] = pData[aOffset++];
        }
        if (mFileSizeBytesUsed < sizeof(mFileSizeLe)) {
          break;
        }

        mCurrentFileSize = 0u;
        for (int aByte = 0; aByte < 8; ++aByte) {
          mCurrentFileSize |=
              static_cast<std::uint64_t>(mFileSizeLe[static_cast<std::size_t>(aByte)])
              << (8u * aByte);
        }

        if (!memory_layout::TypedRecordTypeHasContentBytesV2(mCurrentTypeFlag)) {
          ResetRecordState();
          break;
        }

        if (ShouldMaterializeFile(mCurrentTypeFlag)) {
          std::string aOutPath;
          std::string aFinalPath;
          std::string aPartialPath;
          if (!ResolveOutputPaths(mCurrentPath, aOutPath, aFinalPath, aPartialPath)) {
            pOutParseError = true;
            pOutParseErrorMessage = "failed reserving a visible output path.";
            return false;
          }
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
          mCurrentFinalPath = aFinalPath;
          mCurrentPartialPath = aPartialPath;
        }
        mContentBytesRemaining = mCurrentFileSize;
        mStage = Stage::kContentBytes;
        break;
      }

      case Stage::kContentBytes: {
        const std::size_t aChunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(mContentBytesRemaining,
                                    static_cast<std::uint64_t>(pEnd - aOffset)));
        if (aChunk == 0u) {
          if (!FinishFileRecord(pOutParseErrorMessage)) {
            pOutParseError = true;
            return false;
          }
          break;
        }
        if (mCurrentWrite != nullptr &&
            !mCurrentWrite->Write(pData + aOffset, aChunk)) {
          pOutParseError = true;
          pOutParseErrorMessage = "failed writing decoded file bytes.";
          return false;
        }
        aOffset += aChunk;
        mContentBytesRemaining -= static_cast<std::uint64_t>(aChunk);
        if (mCurrentWrite != nullptr) {
          mBytesWritten += static_cast<std::uint64_t>(aChunk);
          mCurrentFileBytesWritten += static_cast<std::uint64_t>(aChunk);
          pOutDataBytesWritten += static_cast<std::uint64_t>(aChunk);
        }
        if (mContentBytesRemaining == 0u) {
          if (!FinishFileRecord(pOutParseErrorMessage)) {
            pOutParseError = true;
            return false;
          }
        }
        break;
      }
    }
  }

  if (IsAtIllegalPartialScalarBoundary()) {
    pOutParseError = true;
    pOutParseErrorMessage = "fixed-width scalar crossed a block boundary.";
    return false;
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

std::string DecodeLogicalRecordDecoderV2::CurrentFileReference() const {
  return mCurrentPath;
}

bool DecodeLogicalRecordDecoderV2::AbortCurrentFile() {
  if (mCurrentWrite != nullptr) {
    (void)mCurrentWrite->Close();
    mCurrentWrite.reset();
  }
  if (mCurrentFileBytesWritten <= mBytesWritten) {
    mBytesWritten -= mCurrentFileBytesWritten;
  } else {
    mBytesWritten = 0u;
  }
  const bool aRenamed = PromoteCurrentOutputToPartial();
  ResetRecordState();
  return aRenamed;
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
  mFileSizeBytesUsed = 0u;
  mCurrentPathLength = 0u;
  mCurrentTypeFlag = 0u;
  mCurrentFileSize = 0u;
  mContentBytesRemaining = 0u;
  mCurrentFileBytesWritten = 0u;
  std::memset(mPathLengthLe, 0, sizeof(mPathLengthLe));
  std::memset(mFileSizeLe, 0, sizeof(mFileSizeLe));
  mCurrentPath.clear();
  mCurrentOutputPath.clear();
  mCurrentFinalPath.clear();
  mCurrentPartialPath.clear();
  mCurrentWrite.reset();
}

bool DecodeLogicalRecordDecoderV2::FinishFileRecord(std::string& pOutErrorMessage) {
  if (mCurrentWrite != nullptr) {
    if (!mCurrentWrite->Close()) {
      pOutErrorMessage = "failed closing decoded output file.";
      return false;
    }
    mCurrentWrite.reset();
  }
  if (!mCurrentOutputPath.empty() && !mCurrentFinalPath.empty() &&
      !mFileSystem.RenamePath(mCurrentOutputPath, mCurrentFinalPath)) {
    pOutErrorMessage = "failed renaming decoded output file into place.";
    return false;
  }
  if (!mCurrentFinalPath.empty()) {
    ++mFilesWritten;
  }
  ResetRecordState();
  return true;
}

bool DecodeLogicalRecordDecoderV2::IsSafeRelativePath(const std::string& pPath) const {
  if (pPath.empty() || pPath.size() > mLayout.mMaxPathLength) {
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
      if (aByte < 32u || aByte == 127u || aByte == 0u) {
        return false;
      }
    }
    aStart = aEnd + 1u;
  }
  return true;
}

bool DecodeLogicalRecordDecoderV2::IsTypeAllowed(std::uint8_t pTypeFlag) const {
  switch (mZone) {
    case DecodeLogicalZoneV2::kPreviewManifest:
      return pTypeFlag ==
                 static_cast<std::uint8_t>(memory_layout::TypedRecordTypeV2::kManifestFile) ||
             pTypeFlag == static_cast<std::uint8_t>(
                              memory_layout::TypedRecordTypeV2::kManifestFolder);
    case DecodeLogicalZoneV2::kFolderManifest:
      return pTypeFlag == static_cast<std::uint8_t>(
                              memory_layout::TypedRecordTypeV2::kManifestFolder);
    case DecodeLogicalZoneV2::kData:
      return pTypeFlag ==
                 static_cast<std::uint8_t>(memory_layout::TypedRecordTypeV2::kDataFile) ||
             pTypeFlag ==
                 static_cast<std::uint8_t>(memory_layout::TypedRecordTypeV2::kDataFolder);
  }
  return false;
}

bool DecodeLogicalRecordDecoderV2::ShouldMaterializeFolder(std::uint8_t pTypeFlag) const {
  if (mZone == DecodeLogicalZoneV2::kPreviewManifest) {
    return false;
  }
  return pTypeFlag == static_cast<std::uint8_t>(
                          memory_layout::TypedRecordTypeV2::kManifestFolder) ||
         pTypeFlag ==
             static_cast<std::uint8_t>(memory_layout::TypedRecordTypeV2::kDataFolder);
}

bool DecodeLogicalRecordDecoderV2::ShouldMaterializeFile(std::uint8_t pTypeFlag) const {
  return mZone == DecodeLogicalZoneV2::kData &&
         pTypeFlag ==
             static_cast<std::uint8_t>(memory_layout::TypedRecordTypeV2::kDataFile);
}

bool DecodeLogicalRecordDecoderV2::IsAtIllegalPartialScalarBoundary() const {
  switch (mStage) {
    case Stage::kPathLength:
      return mPathLengthBytesUsed != 0u;
    case Stage::kFileSize:
      return mFileSizeBytesUsed != 0u;
    case Stage::kTypeFlag:
    case Stage::kPathBytes:
    case Stage::kContentBytes:
      return false;
  }
  return false;
}

bool DecodeLogicalRecordDecoderV2::ResolveOutputPaths(
    const std::string& pRelativePath,
    std::string& pOutWritingPath,
    std::string& pOutFinalPath,
    std::string& pOutPartialPath) const {
  pOutWritingPath.clear();
  pOutFinalPath.clear();
  pOutPartialPath.clear();
  if (pRelativePath.empty()) {
    return false;
  }

  const std::string aRequestedFinalPath =
      mFileSystem.JoinPath(mDestinationDirectory, pRelativePath);
  const std::string aParentPath = mFileSystem.ParentPath(aRequestedFinalPath);
  const std::string aLeafName = mFileSystem.FileName(aRequestedFinalPath);
  const std::string aExtension = mFileSystem.Extension(aLeafName);
  std::string aStem = mFileSystem.StemName(aLeafName);
  if (aStem.empty()) {
    aStem = "output";
  }

  for (std::uint32_t aOrdinal = 0u; aOrdinal < 1000000u; ++aOrdinal) {
    const std::string aCandidateLeaf =
        aOrdinal == 0u ? (aStem + aExtension)
                       : (aStem + "_" + std::to_string(aOrdinal) + aExtension);
    const std::string aWritingLeaf = "$WRITING_" + aCandidateLeaf;
    const std::string aPartialLeaf = "$PARTIAL_" + aCandidateLeaf;
    const std::string aFinalPath = mFileSystem.JoinPath(aParentPath, aCandidateLeaf);
    const std::string aWritingPath = mFileSystem.JoinPath(aParentPath, aWritingLeaf);
    const std::string aPartialPath = mFileSystem.JoinPath(aParentPath, aPartialLeaf);

    if (!mFileSystem.Exists(aFinalPath) &&
        !mFileSystem.Exists(aWritingPath) &&
        !mFileSystem.Exists(aPartialPath)) {
      pOutWritingPath = aWritingPath;
      pOutFinalPath = aFinalPath;
      pOutPartialPath = aPartialPath;
      return true;
    }
  }

  return false;
}

bool DecodeLogicalRecordDecoderV2::PromoteCurrentOutputToPartial() {
  if (mCurrentOutputPath.empty()) {
    return true;
  }
  if (mCurrentPartialPath.empty()) {
    return false;
  }
  return mFileSystem.RenamePath(mCurrentOutputPath, mCurrentPartialPath);
}

}  // namespace peanutbutter
