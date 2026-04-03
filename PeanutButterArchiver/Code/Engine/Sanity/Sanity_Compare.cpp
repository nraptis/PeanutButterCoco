#include "Sanity_Compare.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>

#include "../../Knobs.hpp"
#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

class SanityCompareCursorV2 {
 public:
  enum class StageV2 {
    kCompareLeftFiles = 0,
    kScanRightFiles = 1,
    kCompareLeftFolders = 2,
    kScanRightFolders = 3,
    kFinalize = 4,
  };

  StageV2 mStage = StageV2::kCompareLeftFiles;
  std::unordered_map<std::string, std::size_t> mRightFileIndexByRelative;
  std::unordered_map<std::string, std::size_t> mLeftFileIndexByRelative;
  std::unordered_map<std::string, std::size_t> mRightFolderIndexByRelative;
  std::unordered_map<std::string, std::size_t> mLeftFolderIndexByRelative;
  FixedBlockBufferV2 mLeftBuffer;
  FixedBlockBufferV2 mRightBuffer;
  std::size_t mLeftFileIndex = 0u;
  std::size_t mRightFileIndex = 0u;
  std::size_t mLeftFolderIndex = 0u;
  std::size_t mRightFolderIndex = 0u;
  bool mActiveFileCompare = false;
  std::size_t mActiveLeftFileIndex = 0u;
  std::size_t mActiveRightFileIndex = 0u;
  std::uint64_t mActiveOffset = 0u;
  std::uint64_t mActiveLength = 0u;
  std::unique_ptr<FileReadStreamV2> mActiveLeftRead;
  std::unique_ptr<FileReadStreamV2> mActiveRightRead;
  std::uint64_t mResolvedFileBytes = 0u;
  std::uint64_t mResolvedStructuralEntries = 0u;
  std::uint64_t mTotalStructuralEntries = 0u;
  std::uint64_t mNextFileLog = knobs::kSanityCompareFileLogIntervalV2;
  std::uint64_t mNextFolderLog = knobs::kSanityCompareFolderLogIntervalV2;
  std::uint64_t mNextByteLog = knobs::kSanityCompareByteLogIntervalV2;
};

namespace {

inline constexpr std::size_t kCompareChunkBytesV2 = FixedBlockBufferV2::kCapacity;
inline constexpr std::size_t kSanityExampleCapV2 = 5u;

bool IsHiddenRelativePath(const std::string& pRelativePath) {
  if (pRelativePath.empty()) {
    return false;
  }
  std::size_t aStart = 0u;
  while (aStart < pRelativePath.size()) {
    std::size_t aEnd = pRelativePath.find('/', aStart);
    if (aEnd == std::string::npos) {
      aEnd = pRelativePath.size();
    }
    if (aEnd > aStart && pRelativePath[aStart] == '.') {
      return true;
    }
    aStart = aEnd + 1u;
  }
  return false;
}

void SortStrings(std::vector<std::string>& pPaths) {
  std::sort(pPaths.begin(), pPaths.end());
}

void LogExampleList(SanityStageContextV2& pContext,
                    LogLevelV2 pLevel,
                    const std::vector<std::string>& pRelativePaths,
                    bool pIsDirectory,
                    const std::string& pLabel,
                    const std::string& pFromRoot,
                    const std::string& pToRoot) {
  const std::size_t aLimit = std::min<std::size_t>(pRelativePaths.size(), kSanityExampleCapV2);
  for (std::size_t aIndex = 0u; aIndex < aLimit; ++aIndex) {
    const std::string aSuffix = pIsDirectory ? "/" : "";
    const std::string aRelativeItem = pRelativePaths[aIndex] + aSuffix;
    const std::string aSourceDisplay =
        FormatPathRelativeToRootV2(pFromRoot, pFromRoot + "/" + aRelativeItem);
    const std::string aTargetDisplay =
        FormatPathRelativeToRootV2(pToRoot, pToRoot + "/" + aRelativeItem);
    pContext.EmitLog(pLevel,
                     "[Folder Compare][Summary] " + pLabel + ".");
    pContext.EmitLog(pLevel,
                     "[Folder Compare][Summary]   source: " + aSourceDisplay);
    pContext.EmitLog(pLevel,
                     "[Folder Compare][Summary]   target: " + aTargetDisplay);
  }
}

void ResetActiveFileCompare(SanityCompareCursorV2& pCursor) {
  pCursor.mActiveFileCompare = false;
  pCursor.mActiveLeftFileIndex = 0u;
  pCursor.mActiveRightFileIndex = 0u;
  pCursor.mActiveOffset = 0u;
  pCursor.mActiveLength = 0u;
  pCursor.mActiveLeftRead.reset();
  pCursor.mActiveRightRead.reset();
}

void RecordMissingPath(SanityCompareStateV2& pState,
                       const std::string& pRelativePath,
                       bool pIsHidden,
                       bool pMissingFromRight,
                       bool pIsDirectory) {
  std::vector<std::string>* aTarget = nullptr;
  if (pMissingFromRight) {
    if (pIsDirectory) {
      aTarget = pIsHidden ? &pState.mHiddenFoldersMissingFromRight
                          : &pState.mNormalFoldersMissingFromRight;
    } else {
      aTarget = pIsHidden ? &pState.mHiddenFilesMissingFromRight
                          : &pState.mNormalFilesMissingFromRight;
    }
  } else if (pIsDirectory) {
    aTarget = pIsHidden ? &pState.mHiddenFoldersMissingFromLeft
                        : &pState.mNormalFoldersMissingFromLeft;
  } else {
    aTarget = pIsHidden ? &pState.mHiddenFilesMissingFromLeft
                        : &pState.mNormalFilesMissingFromLeft;
  }

  if (aTarget != nullptr) {
    aTarget->push_back(pRelativePath);
  }
  ++pState.mMismatchCount;
  if (pIsHidden) {
    ++pState.mHiddenMismatchCount;
  } else {
    ++pState.mNormalMismatchCount;
  }
}

void RecordInequalFile(SanityCompareStateV2& pState,
                       const std::string& pRelativePath,
                       bool pIsHidden) {
  if (pIsHidden) {
    pState.mHiddenInequalFiles.push_back(pRelativePath);
    ++pState.mHiddenMismatchCount;
  } else {
    pState.mNormalInequalFiles.push_back(pRelativePath);
    ++pState.mNormalMismatchCount;
  }
  ++pState.mMismatchCount;
}

void EmitCompareProgress(SanityStageContextV2& pContext,
                         const SanityCompareStateV2& pState,
                         const SanityCompareCursorV2& pCursor) {
  double aLocalFraction = 1.0;
  if (pState.mStat.mBytesTotal > 0u) {
    aLocalFraction =
        static_cast<double>(pCursor.mResolvedFileBytes) /
        static_cast<double>(pState.mStat.mBytesTotal);
  } else if (pCursor.mTotalStructuralEntries > 0u) {
    aLocalFraction =
        static_cast<double>(pCursor.mResolvedStructuralEntries) /
        static_cast<double>(pCursor.mTotalStructuralEntries);
  }
  pContext.EmitPhaseProgress(
      std::max(0.0, std::min(1.0, aLocalFraction)),
      ProgressStageLabelV2(ProgressStageV2::kCompare));
}

bool ShouldEmitCompareSlice(SanityCompareCursorV2& pCursor,
                            const SanityCompareStateV2& pState) {
  bool aShouldEmit = false;
  while (pState.mStat.mFilesCompleted >= pCursor.mNextFileLog) {
    aShouldEmit = true;
    pCursor.mNextFileLog += knobs::kSanityCompareFileLogIntervalV2;
  }
  while (pState.mStat.mFoldersCompleted >= pCursor.mNextFolderLog) {
    aShouldEmit = true;
    pCursor.mNextFolderLog += knobs::kSanityCompareFolderLogIntervalV2;
  }
  while (pState.mStat.mBytesCompleted >= pCursor.mNextByteLog) {
    aShouldEmit = true;
    pCursor.mNextByteLog += knobs::kSanityCompareByteLogIntervalV2;
  }
  return aShouldEmit;
}

void MaybeEmitCompareSlice(SanityStageContextV2& pContext,
                           SanityCompareCursorV2& pCursor,
                           const SanityCompareStateV2& pState) {
  if (!ShouldEmitCompareSlice(pCursor, pState)) {
    return;
  }
  pContext.EmitLog(LogLevelV2::kInfo, LogSanityCompareSliceV2(pState.mStat));
}

bool FinalizeSanityCompare(SanityStageContextV2& pContext) {
  SanityCompareStateV2& aState = pContext.State().mCompare;
  pContext.State().mCursor.mCompare.reset();

  SortStrings(aState.mHiddenFoldersMissingFromRight);
  SortStrings(aState.mHiddenFilesMissingFromRight);
  SortStrings(aState.mNormalFoldersMissingFromRight);
  SortStrings(aState.mNormalFilesMissingFromRight);
  SortStrings(aState.mHiddenFoldersMissingFromLeft);
  SortStrings(aState.mHiddenFilesMissingFromLeft);
  SortStrings(aState.mNormalFoldersMissingFromLeft);
  SortStrings(aState.mNormalFilesMissingFromLeft);
  SortStrings(aState.mHiddenInequalFiles);
  SortStrings(aState.mNormalInequalFiles);

  const std::string& aLeftRoot = pContext.Request().mLeftDirectory;
  const std::string& aRightRoot = pContext.Request().mRightDirectory;
  LogExampleList(pContext, LogLevelV2::kWarning,
                 aState.mHiddenFoldersMissingFromRight, true,
                 "Warn: hidden folder missing from destination",
                 aLeftRoot, aRightRoot);
  LogExampleList(pContext, LogLevelV2::kWarning,
                 aState.mHiddenFoldersMissingFromLeft, true,
                 "Warn: hidden folder missing from source",
                 aRightRoot, aLeftRoot);
  LogExampleList(pContext, LogLevelV2::kWarning,
                 aState.mHiddenFilesMissingFromRight, false,
                 "Warn: hidden file missing from destination",
                 aLeftRoot, aRightRoot);
  LogExampleList(pContext, LogLevelV2::kWarning,
                 aState.mHiddenFilesMissingFromLeft, false,
                 "Warn: hidden file missing from source",
                 aRightRoot, aLeftRoot);
  LogExampleList(pContext, LogLevelV2::kError,
                 aState.mNormalFoldersMissingFromRight, true,
                 "Fail: folder missing from destination",
                 aLeftRoot, aRightRoot);
  LogExampleList(pContext, LogLevelV2::kError,
                 aState.mNormalFoldersMissingFromLeft, true,
                 "Fail: folder missing from source",
                 aRightRoot, aLeftRoot);
  LogExampleList(pContext, LogLevelV2::kError,
                 aState.mNormalFilesMissingFromRight, false,
                 "Fail: file missing from destination",
                 aLeftRoot, aRightRoot);
  LogExampleList(pContext, LogLevelV2::kError,
                 aState.mNormalFilesMissingFromLeft, false,
                 "Fail: file missing from source",
                 aRightRoot, aLeftRoot);
  LogExampleList(pContext, LogLevelV2::kWarning,
                 aState.mHiddenInequalFiles, false,
                 "Warn: hidden file contents differ",
                 aLeftRoot, aRightRoot);
  LogExampleList(pContext, LogLevelV2::kWarning,
                 aState.mNormalInequalFiles, false,
                 "Warn: file contents differ",
                 aLeftRoot, aRightRoot);

  pContext.EmitLog(LogLevelV2::kInfo, LogSanityCompareEndV2(aState.mStat));
  if (aState.mHiddenMismatchCount > 0u) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        "[Folder Compare][Summary] Warn: Found " +
            std::to_string(aState.mHiddenMismatchCount) +
            " mismatches in hidden files and folders.");
  }
  if (aState.mNormalMismatchCount > 0u) {
    pContext.EmitLog(
        LogLevelV2::kError,
        "[Folder Compare][Summary] Fail: Found " +
            std::to_string(aState.mNormalMismatchCount) +
            " mismatches in normal files and folders.");
  }

  if (aState.mNormalMismatchCount == 0u && aState.mHiddenMismatchCount == 0u) {
    pContext.EmitLog(LogLevelV2::kInfo, LogSanitySummaryHealthyV2(aState.mStat));
  } else {
    pContext.EmitLog(aState.mNormalMismatchCount == 0u ? LogLevelV2::kWarning
                                                       : LogLevelV2::kError,
                     LogSanitySummaryMismatchV2(aState.mStat, aState.mMismatchCount));
  }
  pContext.EmitPhaseProgress(1.0, ProgressStageLabelV2(ProgressStageV2::kCompare));
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kSanity, ProgressStageV2::kCompare));
  return !pContext.IsCancelRequested() && aState.mNormalMismatchCount == 0u;
}

}  // namespace

bool SanityCompareV2::Run(SanityStageContextV2& pContext) {
  auto& aState = pContext.State().mCompare;
  std::shared_ptr<SanityCompareCursorV2>& aCursorPtr =
      pContext.State().mCursor.mCompare;
  if (!aCursorPtr) {
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseStartedV2(LogActionV2::kSanity, ProgressStageV2::kCompare));
    pContext.EmitLog(LogLevelV2::kInfo, LogSanityCompareStartV2());
    aState = SanityCompareStateV2{};

    aCursorPtr = std::make_shared<SanityCompareCursorV2>();
    if (!aCursorPtr->mLeftBuffer.Resize(kCompareChunkBytesV2) ||
        !aCursorPtr->mRightBuffer.Resize(kCompareChunkBytesV2)) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kSanity,
                                        ProgressStageV2::kCompare,
                                        "could not allocate compare buffers"));
      aCursorPtr.reset();
      return false;
    }

    for (std::size_t aIndex = 0u;
         aIndex < pContext.State().mDiscovery.mRightFiles.size();
         ++aIndex) {
      aCursorPtr->mRightFileIndexByRelative.emplace(
          pContext.State().mDiscovery.mRightFiles[aIndex].mRelativePath, aIndex);
    }
    for (std::size_t aIndex = 0u;
         aIndex < pContext.State().mDiscovery.mLeftFiles.size();
         ++aIndex) {
      const SanityEntryV2& aEntry = pContext.State().mDiscovery.mLeftFiles[aIndex];
      aCursorPtr->mLeftFileIndexByRelative.emplace(aEntry.mRelativePath, aIndex);
      ++aState.mStat.mFilesTotal;
      aState.mStat.mBytesTotal += aEntry.mLength;
    }
    for (std::size_t aIndex = 0u;
         aIndex < pContext.State().mDiscovery.mRightFolders.size();
         ++aIndex) {
      aCursorPtr->mRightFolderIndexByRelative.emplace(
          pContext.State().mDiscovery.mRightFolders[aIndex].mRelativePath, aIndex);
    }
    for (std::size_t aIndex = 0u;
         aIndex < pContext.State().mDiscovery.mLeftFolders.size();
         ++aIndex) {
      aCursorPtr->mLeftFolderIndexByRelative.emplace(
          pContext.State().mDiscovery.mLeftFolders[aIndex].mRelativePath, aIndex);
    }
    aState.mStat.mFoldersTotal =
        static_cast<std::uint64_t>(pContext.State().mDiscovery.mLeftFolders.size());
    aCursorPtr->mTotalStructuralEntries =
        static_cast<std::uint64_t>(pContext.State().mDiscovery.mLeftFiles.size()) +
        static_cast<std::uint64_t>(pContext.State().mDiscovery.mRightFiles.size()) +
        static_cast<std::uint64_t>(pContext.State().mDiscovery.mLeftFolders.size()) +
        static_cast<std::uint64_t>(pContext.State().mDiscovery.mRightFolders.size());
  }

  SanityCompareCursorV2& aCursor = *aCursorPtr;
  std::uint32_t aWorkBudget =
      std::max<std::uint32_t>(1u, knobs::kBatchSizeSanityCompareV2);

  const auto ConsumeWorkUnit = [&]() {
    MaybeEmitCompareSlice(pContext, aCursor, aState);
    EmitCompareProgress(pContext, aState, aCursor);
    if (aWorkBudget > 0u) {
      --aWorkBudget;
    }
  };

  while (true) {
    if (pContext.IsCancelRequested()) {
      return false;
    }
    if (aCursor.mStage == SanityCompareCursorV2::StageV2::kFinalize) {
      break;
    }
    if (aWorkBudget == 0u) {
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    if (aCursor.mStage == SanityCompareCursorV2::StageV2::kCompareLeftFiles) {
      if (aCursor.mActiveFileCompare) {
        const SanityEntryV2& aLeftEntry =
            pContext.State().mDiscovery.mLeftFiles[aCursor.mActiveLeftFileIndex];
        const std::size_t aChunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(kCompareChunkBytesV2,
                                    aCursor.mActiveLength - aCursor.mActiveOffset));
        if (!aCursor.mActiveLeftRead->Read(
                static_cast<std::size_t>(aCursor.mActiveOffset),
                aCursor.mLeftBuffer.Data(),
                aChunk) ||
            !aCursor.mActiveRightRead->Read(
                static_cast<std::size_t>(aCursor.mActiveOffset),
                aCursor.mRightBuffer.Data(),
                aChunk)) {
          RecordInequalFile(aState,
                            aLeftEntry.mRelativePath,
                            IsHiddenRelativePath(aLeftEntry.mRelativePath));
          ++aState.mStat.mFilesCompleted;
          ++aCursor.mResolvedStructuralEntries;
          aCursor.mResolvedFileBytes += (aCursor.mActiveLength - aCursor.mActiveOffset);
          ResetActiveFileCompare(aCursor);
          ++aCursor.mLeftFileIndex;
          ConsumeWorkUnit();
          continue;
        }

        const bool aEqual =
            std::memcmp(aCursor.mLeftBuffer.Data(),
                        aCursor.mRightBuffer.Data(),
                        aChunk) == 0;
        if (!aEqual) {
          RecordInequalFile(aState,
                            aLeftEntry.mRelativePath,
                            IsHiddenRelativePath(aLeftEntry.mRelativePath));
          ++aState.mStat.mFilesCompleted;
          ++aCursor.mResolvedStructuralEntries;
          aCursor.mResolvedFileBytes += (aCursor.mActiveLength - aCursor.mActiveOffset);
          ResetActiveFileCompare(aCursor);
          ++aCursor.mLeftFileIndex;
          ConsumeWorkUnit();
          continue;
        }

        aCursor.mActiveOffset += static_cast<std::uint64_t>(aChunk);
        aState.mStat.mBytesCompleted += static_cast<std::uint64_t>(aChunk);
        aCursor.mResolvedFileBytes += static_cast<std::uint64_t>(aChunk);
        if (aCursor.mActiveOffset >= aCursor.mActiveLength) {
          ++aState.mStat.mFilesCompleted;
          ++aCursor.mResolvedStructuralEntries;
          ResetActiveFileCompare(aCursor);
          ++aCursor.mLeftFileIndex;
        }
        ConsumeWorkUnit();
        continue;
      }

      if (aCursor.mLeftFileIndex >= pContext.State().mDiscovery.mLeftFiles.size()) {
        aCursor.mStage = SanityCompareCursorV2::StageV2::kScanRightFiles;
        continue;
      }

      const SanityEntryV2& aLeftEntry =
          pContext.State().mDiscovery.mLeftFiles[aCursor.mLeftFileIndex];
      const bool aIsHidden = IsHiddenRelativePath(aLeftEntry.mRelativePath);
      const auto aRightIt =
          aCursor.mRightFileIndexByRelative.find(aLeftEntry.mRelativePath);
      if (aRightIt == aCursor.mRightFileIndexByRelative.end()) {
        RecordMissingPath(aState, aLeftEntry.mRelativePath, aIsHidden, true, false);
        ++aState.mStat.mFilesCompleted;
        ++aCursor.mResolvedStructuralEntries;
        aCursor.mResolvedFileBytes += aLeftEntry.mLength;
        ++aCursor.mLeftFileIndex;
        ConsumeWorkUnit();
        continue;
      }

      const SanityEntryV2& aRightEntry =
          pContext.State().mDiscovery.mRightFiles[aRightIt->second];
      std::unique_ptr<FileReadStreamV2> aLeftRead =
          pContext.FileSystem().OpenReadStream(aLeftEntry.mPath);
      std::unique_ptr<FileReadStreamV2> aRightRead =
          pContext.FileSystem().OpenReadStream(aRightEntry.mPath);
      const bool aReady = aLeftRead != nullptr && aRightRead != nullptr &&
                          aLeftRead->IsReady() && aRightRead->IsReady();
      const std::uint64_t aLeftLength =
          aReady ? static_cast<std::uint64_t>(aLeftRead->GetLength()) : aLeftEntry.mLength;
      const std::uint64_t aRightLength =
          aReady ? static_cast<std::uint64_t>(aRightRead->GetLength()) : aRightEntry.mLength;
      if (!aReady || aLeftLength != aRightLength) {
        RecordInequalFile(aState, aLeftEntry.mRelativePath, aIsHidden);
        ++aState.mStat.mFilesCompleted;
        ++aCursor.mResolvedStructuralEntries;
        aCursor.mResolvedFileBytes += aLeftEntry.mLength;
        ++aCursor.mLeftFileIndex;
        ConsumeWorkUnit();
        continue;
      }

      if (aLeftLength == 0u) {
        ++aState.mStat.mFilesCompleted;
        ++aCursor.mResolvedStructuralEntries;
        ++aCursor.mLeftFileIndex;
        ConsumeWorkUnit();
        continue;
      }

      aCursor.mActiveFileCompare = true;
      aCursor.mActiveLeftFileIndex = aCursor.mLeftFileIndex;
      aCursor.mActiveRightFileIndex = aRightIt->second;
      aCursor.mActiveOffset = 0u;
      aCursor.mActiveLength = aLeftLength;
      aCursor.mActiveLeftRead = std::move(aLeftRead);
      aCursor.mActiveRightRead = std::move(aRightRead);
      continue;
    }

    if (aCursor.mStage == SanityCompareCursorV2::StageV2::kScanRightFiles) {
      if (aCursor.mRightFileIndex >= pContext.State().mDiscovery.mRightFiles.size()) {
        aCursor.mStage = SanityCompareCursorV2::StageV2::kCompareLeftFolders;
        continue;
      }

      const SanityEntryV2& aRightEntry =
          pContext.State().mDiscovery.mRightFiles[aCursor.mRightFileIndex];
      if (aCursor.mLeftFileIndexByRelative.find(aRightEntry.mRelativePath) ==
          aCursor.mLeftFileIndexByRelative.end()) {
        RecordMissingPath(aState,
                          aRightEntry.mRelativePath,
                          IsHiddenRelativePath(aRightEntry.mRelativePath),
                          false,
                          false);
      }
      ++aCursor.mResolvedStructuralEntries;
      ++aCursor.mRightFileIndex;
      ConsumeWorkUnit();
      continue;
    }

    if (aCursor.mStage == SanityCompareCursorV2::StageV2::kCompareLeftFolders) {
      if (aCursor.mLeftFolderIndex >= pContext.State().mDiscovery.mLeftFolders.size()) {
        aCursor.mStage = SanityCompareCursorV2::StageV2::kScanRightFolders;
        continue;
      }

      const SanityEntryV2& aLeftFolder =
          pContext.State().mDiscovery.mLeftFolders[aCursor.mLeftFolderIndex];
      if (aCursor.mRightFolderIndexByRelative.find(aLeftFolder.mRelativePath) ==
          aCursor.mRightFolderIndexByRelative.end()) {
        RecordMissingPath(aState,
                          aLeftFolder.mRelativePath,
                          IsHiddenRelativePath(aLeftFolder.mRelativePath),
                          true,
                          true);
      }
      ++aState.mStat.mFoldersCompleted;
      ++aCursor.mResolvedStructuralEntries;
      ++aCursor.mLeftFolderIndex;
      ConsumeWorkUnit();
      continue;
    }

    if (aCursor.mStage == SanityCompareCursorV2::StageV2::kScanRightFolders) {
      if (aCursor.mRightFolderIndex >= pContext.State().mDiscovery.mRightFolders.size()) {
        aCursor.mStage = SanityCompareCursorV2::StageV2::kFinalize;
        continue;
      }

      const SanityEntryV2& aRightFolder =
          pContext.State().mDiscovery.mRightFolders[aCursor.mRightFolderIndex];
      if (aCursor.mLeftFolderIndexByRelative.find(aRightFolder.mRelativePath) ==
          aCursor.mLeftFolderIndexByRelative.end()) {
        RecordMissingPath(aState,
                          aRightFolder.mRelativePath,
                          IsHiddenRelativePath(aRightFolder.mRelativePath),
                          false,
                          true);
      }
      ++aCursor.mResolvedStructuralEntries;
      ++aCursor.mRightFolderIndex;
      ConsumeWorkUnit();
      continue;
    }
  }

  return FinalizeSanityCompare(pContext);
}

}  // namespace peanutbutter
