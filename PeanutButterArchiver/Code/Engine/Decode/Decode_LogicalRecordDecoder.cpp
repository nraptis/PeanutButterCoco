#include "Decode_LogicalRecordDecoder.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "../FileAccess/ConflictNamePolicy.hpp"

namespace peanutbutter {
namespace {

bool IsAbsolutePathLikeV2(const std::string& pPath) {
  if (pPath.empty()) {
    return false;
  }
  if (pPath[0] == '/' || pPath[0] == '\\') {
    return true;
  }
  return pPath.size() > 1u &&
         std::isalpha(static_cast<unsigned char>(pPath[0])) != 0 &&
         pPath[1] == ':';
}

bool IsSafeSymlinkTargetPathV2(const std::string& pPath) {
  if (pPath.empty() || IsAbsolutePathLikeV2(pPath)) {
    return false;
  }
  for (char aChar : pPath) {
    const unsigned char aByte = static_cast<unsigned char>(aChar);
    if (aByte < 32u || aByte == 127u || aByte == 0u) {
      return false;
    }
  }
  return true;
}

std::string JoinAndNormalizePathV2(const std::string& pLeft,
                                   const std::string& pRight) {
  if (pRight.empty()) {
    return std::filesystem::path(pLeft).lexically_normal().generic_string();
  }
  return (std::filesystem::path(pLeft) / std::filesystem::path(pRight))
      .lexically_normal()
      .generic_string();
}

bool TryResolveAliasTargetDescriptorV2(const std::string& pEncodedTarget,
                                       const std::string& pDestinationDirectory,
                                       std::string& pOutResolvedTargetPath,
                                       std::string& pOutErrorMessage) {
  pOutResolvedTargetPath.clear();
  pOutErrorMessage.clear();
  if (pEncodedTarget.empty()) {
    pOutErrorMessage = "alias target descriptor is empty.";
    return false;
  }

  auto BuildAbsoluteFromDescriptor =
      [&](char pDescriptor, const std::string& pRestPath) -> bool {
    switch (pDescriptor) {
      case 'r': {
        pOutResolvedTargetPath =
            pRestPath.empty()
                ? std::filesystem::path(pDestinationDirectory)
                      .lexically_normal()
                      .generic_string()
                : JoinAndNormalizePathV2(pDestinationDirectory, pRestPath);
        return true;
      }
      case 'h': {
        const char* aHomeValue = std::getenv("HOME");
        if (aHomeValue == nullptr || aHomeValue[0] == '\0') {
          pOutErrorMessage = "HOME is unavailable for alias target mapping.";
          return false;
        }
        const std::string aHomePath(aHomeValue);
        pOutResolvedTargetPath =
            pRestPath.empty() ? aHomePath : JoinAndNormalizePathV2(aHomePath, pRestPath);
        return true;
      }
      case 'a': {
        pOutResolvedTargetPath =
            pRestPath.empty() ? "/" : (std::string("/") + pRestPath);
        return true;
      }
      default:
        return false;
    }
  };

  if (pEncodedTarget.size() == 1u) {
    if (BuildAbsoluteFromDescriptor(pEncodedTarget[0], "")) {
      return true;
    }
  } else if (pEncodedTarget.size() > 2u && pEncodedTarget[1] == '/') {
    if (BuildAbsoluteFromDescriptor(pEncodedTarget[0], pEncodedTarget.substr(2u))) {
      return true;
    }
  }

  // Backward compatibility with earlier alias payloads that were root-relative.
  pOutResolvedTargetPath = JoinAndNormalizePathV2(pDestinationDirectory, pEncodedTarget);
  return true;
}

}  // namespace

DecodeLogicalRecordDecoderV2::DecodeLogicalRecordDecoderV2(
    const std::string& pDestinationDirectory,
    FileSystemV2& pFileSystem,
    const memory_layout::ArchiveLayoutConfigV2& pLayout,
    DecodeLogicalZoneV2 pZone,
    ProgressStageV2 pStage,
    RuntimeEventKindV2 pStartEventKind,
    RuntimeEventKindV2 pFinishEventKind,
    DecodeLogicalRecordObserverV2 pObserver)
    : mDestinationDirectory(pDestinationDirectory),
      mFileSystem(pFileSystem),
      mLayout(pLayout),
      mZone(pZone),
      mRuntimeStage(pStage),
      mStartEventKind(pStartEventKind),
      mFinishEventKind(pFinishEventKind),
      mObserver(std::move(pObserver)) {}

bool DecodeLogicalRecordDecoderV2::Consume(const unsigned char* pData,
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
                                           std::string& pOutPausedRecordReference) {
  (void)pTreatZeroLengthAsPadding;
  pOutTerminated = false;
  pOutStoppedAtPadding = false;
  pOutParseError = false;
  pOutParseErrorMessage.clear();
  pOutDataBytesWritten = 0u;
  pOutPausedAtBoundary = false;
  pOutResumeOffset = pStart;
  pOutPausedRecordReference.clear();

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
        if (memory_layout::TypedRecordTypeIsReferenceV2(mCurrentTypeFlag)) {
          mStage = Stage::kReferenceKind;
          break;
        }
        if (mZone == DecodeLogicalZoneV2::kPreviewManifest) {
          mStage = Stage::kPreviewPlaceholder;
          break;
        }
        if (memory_layout::TypedRecordTypeIsFolderV2(mCurrentTypeFlag)) {
          EmitRecordStartEvent();
          if (ShouldMaterializeFolder(mCurrentTypeFlag)) {
            const std::string aDirPath =
                mFileSystem.JoinPath(mDestinationDirectory, mCurrentPath);
            if (!IsOutputPathInsideDestination(aDirPath)) {
              pOutParseError = true;
              pOutParseErrorMessage =
                  "decoded folder path escapes destination directory.";
              return false;
            }
            if (!mFileSystem.EnsureDirectory(aDirPath)) {
              pOutParseError = true;
              pOutParseErrorMessage = "failed creating directory: " + aDirPath;
              return false;
            }
            ++mFoldersCreated;
          }
          const std::string aFinishedReference = mCurrentPath;
          const bool aShouldContinue = EmitRecordFinishEvent();
          ResetRecordState();
          if (!aShouldContinue) {
            pOutPausedAtBoundary = true;
            pOutResumeOffset = aOffset;
            pOutPausedRecordReference = aFinishedReference;
            return true;
          }
          break;
        }
        mFileSizeBytesUsed = 0u;
        std::memset(mFileSizeLe, 0, sizeof(mFileSizeLe));
        mStage = Stage::kFileSize;
        break;
      }

      case Stage::kReferenceKind: {
        if (aOffset >= pEnd) {
          break;
        }
        mCurrentReferenceKind = pData[aOffset++];
        if (!memory_layout::IsKnownReferenceRecordKindV2(mCurrentReferenceKind)) {
          pOutParseError = true;
          pOutParseErrorMessage = "reference record kind is unknown.";
          return false;
        }
        mReferenceTargetLengthBytesUsed = 0u;
        std::memset(mReferenceTargetLengthLe, 0, sizeof(mReferenceTargetLengthLe));
        mStage = Stage::kReferenceTargetLength;
        break;
      }

      case Stage::kReferenceTargetLength: {
        while (mReferenceTargetLengthBytesUsed < sizeof(mReferenceTargetLengthLe) &&
               aOffset < pEnd) {
          mReferenceTargetLengthLe[mReferenceTargetLengthBytesUsed++] = pData[aOffset++];
        }
        if (mReferenceTargetLengthBytesUsed < sizeof(mReferenceTargetLengthLe)) {
          break;
        }

        mCurrentReferenceTargetLength = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(mReferenceTargetLengthLe[0]) |
            (static_cast<std::uint16_t>(mReferenceTargetLengthLe[1]) << 8u));
        if (mCurrentReferenceTargetLength == 0u ||
            mCurrentReferenceTargetLength > mLayout.mMaxPathLength) {
          pOutParseError = true;
          pOutParseErrorMessage =
              "reference target path length exceeds supported maximum.";
          return false;
        }

        mCurrentReferenceTargetPath.clear();
        mCurrentReferenceTargetPath.reserve(mCurrentReferenceTargetLength);
        mReferenceTargetBytesUsed = 0u;
        mStage = Stage::kReferenceTargetBytes;
        break;
      }

      case Stage::kReferenceTargetBytes: {
        const std::size_t aRemaining =
            mCurrentReferenceTargetLength - mReferenceTargetBytesUsed;
        const std::size_t aChunk = std::min<std::size_t>(aRemaining, pEnd - aOffset);
        if (aChunk == 0u) {
          break;
        }
        mCurrentReferenceTargetPath.append(
            reinterpret_cast<const char*>(pData + aOffset), aChunk);
        mReferenceTargetBytesUsed += aChunk;
        aOffset += aChunk;
        if (mReferenceTargetBytesUsed < mCurrentReferenceTargetLength) {
          break;
        }
        if (!IsSafeRelativePath(mCurrentReferenceTargetPath)) {
          pOutParseError = true;
          pOutParseErrorMessage =
              "reference target path failed safety validation.";
          return false;
        }

        bool aShouldContinue = true;
        std::string aFinishedReference;
        if (!FinishReferenceRecord(
                aShouldContinue, aFinishedReference, pOutParseErrorMessage)) {
          pOutParseError = true;
          return false;
        }
        if (!aShouldContinue) {
          pOutPausedAtBoundary = true;
          pOutResumeOffset = aOffset;
          pOutPausedRecordReference = aFinishedReference;
          return true;
        }
        break;
      }

      case Stage::kPreviewPlaceholder: {
        if (aOffset >= pEnd) {
          break;
        }
        if (pData[aOffset++] !=
            memory_layout::specs_verified::kPreviewRecordPlaceholderValueV2) {
          pOutParseError = true;
          pOutParseErrorMessage = "preview placeholder byte was non-zero.";
          return false;
        }

        if (memory_layout::TypedRecordTypeIsFolderV2(mCurrentTypeFlag)) {
          EmitRecordStartEvent();
          if (ShouldMaterializeFolder(mCurrentTypeFlag)) {
            const std::string aDirPath =
                mFileSystem.JoinPath(mDestinationDirectory, mCurrentPath);
            if (!IsOutputPathInsideDestination(aDirPath)) {
              pOutParseError = true;
              pOutParseErrorMessage =
                  "decoded folder path escapes destination directory.";
              return false;
            }
            if (!mFileSystem.EnsureDirectory(aDirPath)) {
              pOutParseError = true;
              pOutParseErrorMessage = "failed creating directory: " + aDirPath;
              return false;
            }
            ++mFoldersCreated;
          }
          const std::string aFinishedReference = mCurrentPath;
          const bool aShouldContinue = EmitRecordFinishEvent();
          ResetRecordState();
          if (!aShouldContinue) {
            pOutPausedAtBoundary = true;
            pOutResumeOffset = aOffset;
            pOutPausedRecordReference = aFinishedReference;
            return true;
          }
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
          if (!IsOutputPathInsideDestination(aOutPath) ||
              !IsOutputPathInsideDestination(aFinalPath) ||
              !IsOutputPathInsideDestination(aPartialPath)) {
            pOutParseError = true;
            pOutParseErrorMessage =
                "decoded file path escapes destination directory.";
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
        EmitRecordStartEvent();
        mContentBytesRemaining = mCurrentFileSize;
        mStage = Stage::kContentBytes;
        break;
      }

      case Stage::kContentBytes: {
        const std::size_t aChunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(mContentBytesRemaining,
                                    static_cast<std::uint64_t>(pEnd - aOffset)));
        if (aChunk == 0u) {
          bool aShouldContinue = true;
          std::string aFinishedReference;
          if (!FinishFileRecord(
                  aShouldContinue, aFinishedReference, pOutParseErrorMessage)) {
            pOutParseError = true;
            return false;
          }
          if (!aShouldContinue) {
            pOutPausedAtBoundary = true;
            pOutResumeOffset = aOffset;
            pOutPausedRecordReference = aFinishedReference;
            return true;
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
          bool aShouldContinue = true;
          std::string aFinishedReference;
          if (!FinishFileRecord(
                  aShouldContinue, aFinishedReference, pOutParseErrorMessage)) {
            pOutParseError = true;
            return false;
          }
          if (!aShouldContinue) {
            pOutPausedAtBoundary = true;
            pOutResumeOffset = aOffset;
            pOutPausedRecordReference = aFinishedReference;
            return true;
          }
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

const std::string& DecodeLogicalRecordDecoderV2::CurrentFileReference() const {
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

void DecodeLogicalRecordDecoderV2::ResetAfterParseError() {
  if (mCurrentWrite != nullptr) {
    (void)AbortCurrentFile();
    return;
  }
  ResetRecordState();
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
  mReferenceTargetLengthBytesUsed = 0u;
  mReferenceTargetBytesUsed = 0u;
  mFileSizeBytesUsed = 0u;
  mCurrentPathLength = 0u;
  mCurrentReferenceTargetLength = 0u;
  mCurrentTypeFlag = 0u;
  mCurrentReferenceKind = 0u;
  mCurrentFileSize = 0u;
  mContentBytesRemaining = 0u;
  mCurrentFileBytesWritten = 0u;
  std::memset(mPathLengthLe, 0, sizeof(mPathLengthLe));
  std::memset(mReferenceTargetLengthLe, 0, sizeof(mReferenceTargetLengthLe));
  std::memset(mFileSizeLe, 0, sizeof(mFileSizeLe));
  mCurrentPath.clear();
  mCurrentReferenceTargetPath.clear();
  mCurrentOutputPath.clear();
  mCurrentFinalPath.clear();
  mCurrentPartialPath.clear();
  mCurrentWrite.reset();
}

bool DecodeLogicalRecordDecoderV2::FinishFileRecord(
    bool& pOutShouldContinue,
    std::string& pOutFinishedReference,
    std::string& pOutErrorMessage) {
  pOutShouldContinue = true;
  pOutFinishedReference.clear();
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
  pOutFinishedReference = mCurrentPath;
  pOutShouldContinue = EmitRecordFinishEvent();
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
             pTypeFlag == static_cast<std::uint8_t>(
                              memory_layout::TypedRecordTypeV2::kDataReference) ||
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

bool DecodeLogicalRecordDecoderV2::ShouldMaterializeReference(
    std::uint8_t pTypeFlag) const {
  return mZone == DecodeLogicalZoneV2::kData &&
         pTypeFlag == static_cast<std::uint8_t>(
                          memory_layout::TypedRecordTypeV2::kDataReference);
}

bool DecodeLogicalRecordDecoderV2::ResolveReferenceOutputPath(
    const std::string& pRelativePath,
    std::string& pOutFinalPath) const {
  pOutFinalPath.clear();
  if (pRelativePath.empty()) {
    return false;
  }

  const std::string aRequestedFinalPath =
      mFileSystem.JoinPath(mDestinationDirectory, pRelativePath);
  const std::string aParentPath = mFileSystem.ParentPath(aRequestedFinalPath);
  const std::string aLeafName = mFileSystem.FileName(aRequestedFinalPath);
  std::string aPreferredLeaf = aLeafName;
  if (aPreferredLeaf.empty()) {
    aPreferredLeaf = "link";
  }
  return ResolveNoOverwritePathV2(
      mFileSystem,
      aParentPath,
      aPreferredLeaf,
      pOutFinalPath);
}

bool DecodeLogicalRecordDecoderV2::FinishReferenceRecord(
    bool& pOutShouldContinue,
    std::string& pOutFinishedReference,
    std::string& pOutErrorMessage) {
  pOutShouldContinue = true;
  pOutFinishedReference.clear();

  EmitRecordStartEvent();

  if (ShouldMaterializeReference(mCurrentTypeFlag)) {
    std::string aLinkPath;
    if (!ResolveReferenceOutputPath(mCurrentPath, aLinkPath)) {
      pOutErrorMessage = "failed reserving a visible output path for reference.";
      return false;
    }
    if (!IsOutputPathInsideDestination(aLinkPath)) {
      pOutErrorMessage = "decoded reference path escapes destination directory.";
      return false;
    }

    const std::string aParentPath = mFileSystem.ParentPath(aLinkPath);
    if (!aParentPath.empty() && !mFileSystem.EnsureDirectory(aParentPath)) {
      pOutErrorMessage = "failed creating parent directory for output reference.";
      return false;
    }

    mCurrentFinalPath = aLinkPath;
    switch (static_cast<memory_layout::ReferenceRecordKindV2>(mCurrentReferenceKind)) {
      case memory_layout::ReferenceRecordKindV2::kSymlink: {
        const std::string aTargetDestinationPath =
            mFileSystem.JoinPath(mDestinationDirectory, mCurrentReferenceTargetPath);
        if (!IsOutputPathInsideDestination(aTargetDestinationPath)) {
          pOutErrorMessage = "reference target escapes destination directory.";
          return false;
        }
        std::error_code aRelativeError;
        std::filesystem::path aRelativeTargetPath = std::filesystem::relative(
            std::filesystem::path(aTargetDestinationPath),
            std::filesystem::path(aParentPath.empty() ? mDestinationDirectory : aParentPath),
            aRelativeError);
        std::string aRelativeTarget =
            aRelativeError
                ? mCurrentReferenceTargetPath
                : aRelativeTargetPath.lexically_normal().generic_string();
        if (aRelativeTarget.empty()) {
          aRelativeTarget = mCurrentReferenceTargetPath;
        }
        if (!IsSafeSymlinkTargetPathV2(aRelativeTarget)) {
          pOutErrorMessage = "reference target path failed safety validation.";
          return false;
        }
        if (!mFileSystem.CreateSymlink(mCurrentFinalPath, aRelativeTarget, false)) {
          pOutErrorMessage = "failed creating decoded symbolic link.";
          return false;
        }
        break;
      }
      case memory_layout::ReferenceRecordKindV2::kAlias: {
        std::string aResolvedAliasTargetPath;
        std::string aResolveError;
        if (!TryResolveAliasTargetDescriptorV2(
                mCurrentReferenceTargetPath,
                mDestinationDirectory,
                aResolvedAliasTargetPath,
                aResolveError)) {
          pOutErrorMessage = aResolveError.empty()
                                 ? "failed resolving alias target descriptor."
                                 : aResolveError;
          return false;
        }
        if (!mFileSystem.CreateAlias(mCurrentFinalPath, aResolvedAliasTargetPath, false)) {
          // Some Apple APIs refuse creating alias files when the target does not exist.
          // Preserve reference intent with a symlink fallback instead of failing decode.
          if (!mFileSystem.CreateSymlink(
                  mCurrentFinalPath, aResolvedAliasTargetPath, false)) {
            pOutErrorMessage =
                "failed creating decoded alias file and symlink fallback.";
            return false;
          }
        }
        break;
      }
      case memory_layout::ReferenceRecordKindV2::kReparsePoint:
      case memory_layout::ReferenceRecordKindV2::kHardlink:
        pOutErrorMessage = "reference kind is unsupported for materialization.";
        return false;
    }
    ++mFilesWritten;
  }

  pOutFinishedReference = mCurrentPath;
  pOutShouldContinue = EmitRecordFinishEvent();
  ResetRecordState();
  return true;
}

bool DecodeLogicalRecordDecoderV2::IsAtIllegalPartialScalarBoundary() const {
  switch (mStage) {
    case Stage::kPathLength:
      return mPathLengthBytesUsed != 0u;
    case Stage::kReferenceKind:
      return false;
    case Stage::kReferenceTargetLength:
      return mReferenceTargetLengthBytesUsed != 0u;
    case Stage::kReferenceTargetBytes:
      return false;
    case Stage::kFileSize:
      return mFileSizeBytesUsed != 0u;
    case Stage::kTypeFlag:
    case Stage::kPreviewPlaceholder:
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
  const std::string aPreferredLeaf = aStem + aExtension;
  return ResolveNoOverwritePathTripletV2(mFileSystem,
                                         aParentPath,
                                         aPreferredLeaf,
                                         "$WRITING_",
                                         "$PARTIAL_",
                                         pOutWritingPath,
                                         pOutFinalPath,
                                         pOutPartialPath);
}

bool DecodeLogicalRecordDecoderV2::IsOutputPathInsideDestination(
    const std::string& pOutputPath) const {
  if (pOutputPath.empty() || !EnsureResolvedDestinationDirectory()) {
    return false;
  }
  const std::string aParentPath = mFileSystem.ParentPath(pOutputPath);
  const std::string aCheckPath = aParentPath.empty() ? pOutputPath : aParentPath;
  std::string aResolvedPath;
  if (!ResolveCanonicalOrAbsolutePath(aCheckPath, aResolvedPath)) {
    return false;
  }
  return IsResolvedPathInsideResolvedDestination(aResolvedPath);
}

bool DecodeLogicalRecordDecoderV2::ResolveCanonicalOrAbsolutePath(
    const std::string& pPath,
    std::string& pOutResolvedPath) const {
  pOutResolvedPath.clear();
  std::error_code aError;
  const std::filesystem::path aCanonical =
      std::filesystem::weakly_canonical(std::filesystem::path(pPath), aError);
  if (!aError) {
    pOutResolvedPath = aCanonical.lexically_normal().generic_string();
    return !pOutResolvedPath.empty();
  }

  aError.clear();
  const std::filesystem::path aAbsolute =
      std::filesystem::absolute(std::filesystem::path(pPath), aError);
  if (!aError) {
    pOutResolvedPath = aAbsolute.lexically_normal().generic_string();
    return !pOutResolvedPath.empty();
  }
  return false;
}

bool DecodeLogicalRecordDecoderV2::EnsureResolvedDestinationDirectory() const {
  if (mResolvedDestinationDirectoryInitialized) {
    return mResolvedDestinationDirectoryValid;
  }
  mResolvedDestinationDirectoryInitialized = true;
  mResolvedDestinationDirectoryValid = ResolveCanonicalOrAbsolutePath(
      mDestinationDirectory, mResolvedDestinationDirectory);
  if (!mResolvedDestinationDirectoryValid) {
    return false;
  }
  if (mResolvedDestinationDirectory.size() > 1u &&
      mResolvedDestinationDirectory.back() == '/') {
    mResolvedDestinationDirectory.pop_back();
  }
  return !mResolvedDestinationDirectory.empty();
}

bool DecodeLogicalRecordDecoderV2::IsResolvedPathInsideResolvedDestination(
    const std::string& pResolvedPath) const {
  if (!mResolvedDestinationDirectoryValid || pResolvedPath.empty()) {
    return false;
  }
  std::string aPath = pResolvedPath;
  if (aPath.size() > 1u && aPath.back() == '/') {
    aPath.pop_back();
  }
  if (mResolvedDestinationDirectory == "/") {
    return !aPath.empty() && aPath[0] == '/';
  }
  if (aPath == mResolvedDestinationDirectory) {
    return true;
  }
  return aPath.size() > mResolvedDestinationDirectory.size() &&
         aPath.compare(
             0,
             mResolvedDestinationDirectory.size(),
             mResolvedDestinationDirectory) == 0 &&
         aPath[mResolvedDestinationDirectory.size()] == '/';
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

void DecodeLogicalRecordDecoderV2::EmitRecordStartEvent() const {
  if (!mObserver || mCurrentPath.empty()) {
    return;
  }

  RuntimeEventV2 aEvent;
  aEvent.mKind = mStartEventKind;
  aEvent.mStage = mRuntimeStage;
  aEvent.SetInfo("relative_path", mCurrentPath);
  aEvent.SetInfo("file_name", mCurrentPath);
  aEvent.SetInfo("output_path", mCurrentFinalPath.empty() ? mCurrentOutputPath
                                                          : mCurrentFinalPath);
  aEvent.SetInfo("is_directory", memory_layout::TypedRecordTypeIsFolderV2(mCurrentTypeFlag));
  aEvent.SetInfo("content_length", mCurrentFileSize);

  switch (mStartEventKind) {
    case RuntimeEventKindV2::kDecodeFolderStarted:
      aEvent.mLabel = "Decode started folder " + mCurrentPath;
      break;
    case RuntimeEventKindV2::kDecodeManifestItemStarted:
      aEvent.mLabel = "Decode started manifest item " + mCurrentPath;
      break;
    case RuntimeEventKindV2::kDecodeFileStarted:
      aEvent.mLabel = "Decode started file " + mCurrentPath;
      break;
    default:
      aEvent.mLabel = RuntimeEventKindLabelV2(mStartEventKind);
      break;
  }

  (void)mObserver(aEvent);
}

bool DecodeLogicalRecordDecoderV2::EmitRecordFinishEvent() const {
  if (!mObserver || mCurrentPath.empty()) {
    return true;
  }

  RuntimeEventV2 aEvent;
  aEvent.mKind = mFinishEventKind;
  aEvent.mStage = mRuntimeStage;
  aEvent.SetInfo("relative_path", mCurrentPath);
  aEvent.SetInfo("file_name", mCurrentPath);
  aEvent.SetInfo("output_path", mCurrentFinalPath.empty() ? mCurrentOutputPath
                                                          : mCurrentFinalPath);
  aEvent.SetInfo("is_directory", memory_layout::TypedRecordTypeIsFolderV2(mCurrentTypeFlag));
  aEvent.SetInfo("content_length", mCurrentFileSize);
  aEvent.SetInfo("bytes_written", mCurrentFileBytesWritten);

  switch (mFinishEventKind) {
    case RuntimeEventKindV2::kDecodeFolderFinished:
      aEvent.mLabel = "Decode finished folder " + mCurrentPath;
      break;
    case RuntimeEventKindV2::kDecodeManifestItemFinished:
      aEvent.mLabel = "Decode finished manifest item " + mCurrentPath;
      break;
    case RuntimeEventKindV2::kDecodeFileFinished:
      aEvent.mLabel = "Decode finished file " + mCurrentPath;
      break;
    default:
      aEvent.mLabel = RuntimeEventKindLabelV2(mFinishEventKind);
      break;
  }

  return mObserver(aEvent);
}

}  // namespace peanutbutter
