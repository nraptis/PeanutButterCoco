#include "Sanity_Discovery.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>

#include "../../Knobs.hpp"
#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {

struct SanityDiscoveryFrameV2 {
  struct ChildEntryV2 {
    std::string mPath;
    std::string mRelativePath;
    bool mIsDirectory = false;
    std::uint64_t mLength = 0u;
    bool mHasLength = false;
  };

  std::string mDirectoryPath;
  std::string mRelativePath;
  std::vector<ChildEntryV2> mChildren;
  std::size_t mChildIndex = 0u;
  bool mLoaded = false;
};

struct SanityDiscoverySideCursorV2 {
  std::string mRootPath;
  std::vector<SanityDiscoveryFrameV2> mFrames;
  std::uint64_t mProcessedEntries = 0u;
  std::uint64_t mMaxObservedEntries = 1u;
  bool mFinished = false;
};

class SanityDiscoveryCursorV2 {
 public:
  enum class SideV2 {
    kLeft = 0,
    kRight = 1,
  };

  SanityDiscoverySideCursorV2 mLeft;
  SanityDiscoverySideCursorV2 mRight;
  SideV2 mActiveSide = SideV2::kLeft;
  LoggingStatV2 mStat{};
  std::uint64_t mNextFileLog = knobs::kSanityDiscoveryFileLogIntervalV2;
  std::uint64_t mNextFolderLog = knobs::kSanityDiscoveryFolderLogIntervalV2;
};

namespace {

bool RelativePathContainsHiddenSegment(const std::string& pRelativePath) {
  if (pRelativePath.empty()) {
    return false;
  }

  const std::filesystem::path aPath(pRelativePath);
  for (const std::filesystem::path& aPart : aPath) {
    const std::string aName = aPart.generic_string();
    if (aName.empty() || aName == "." || aName == "..") {
      continue;
    }
    if (aName[0] == '.') {
      return true;
    }
  }
  return false;
}

bool DiscoveryEntryLess(const SanityDiscoveryFrameV2::ChildEntryV2& pLeft,
                        const SanityDiscoveryFrameV2::ChildEntryV2& pRight) {
  if (pLeft.mRelativePath != pRight.mRelativePath) {
    return pLeft.mRelativePath < pRight.mRelativePath;
  }
  return pLeft.mPath < pRight.mPath;
}

SanityEntryV2 ToSanityEntry(const SanityDiscoveryFrameV2::ChildEntryV2& pEntry) {
  SanityEntryV2 aEntry;
  aEntry.mPath = pEntry.mPath;
  aEntry.mRelativePath = pEntry.mRelativePath;
  aEntry.mIsDirectory = pEntry.mIsDirectory;
  if (!pEntry.mIsDirectory && pEntry.mHasLength) {
    aEntry.mLength = pEntry.mLength;
  }
  return aEntry;
}

bool LoadDirectoryChildren(const std::string& pRootPath,
                           SanityDiscoveryFrameV2& pFrame) {
  pFrame.mChildren.clear();
  pFrame.mChildIndex = 0u;
  pFrame.mLoaded = true;

  std::error_code aIteratorError;
  std::filesystem::directory_iterator aIterator(
      std::filesystem::path(pFrame.mDirectoryPath),
      std::filesystem::directory_options::skip_permission_denied,
      aIteratorError);
  std::filesystem::directory_iterator aEnd;
  while (!aIteratorError && aIterator != aEnd) {
    const std::filesystem::directory_entry aEntry = *aIterator;
    std::error_code aTypeError;
    const bool aIsDirectory = aEntry.is_directory(aTypeError);
    const bool aIsRegularFile = !aTypeError && aEntry.is_regular_file(aTypeError);
    if (!aTypeError && (aIsDirectory || aIsRegularFile)) {
      std::error_code aRelativeError;
      const std::filesystem::path aRelativePath =
          std::filesystem::relative(aEntry.path(),
                                    std::filesystem::path(pRootPath),
                                    aRelativeError);
      if (!aRelativeError) {
        SanityDiscoveryFrameV2::ChildEntryV2 aChild;
        aChild.mPath = aEntry.path().lexically_normal().generic_string();
        aChild.mRelativePath = aRelativePath.generic_string();
        aChild.mIsDirectory = aIsDirectory;
        if (!aIsDirectory) {
          std::error_code aSizeError;
          const std::uintmax_t aRawSize = aEntry.file_size(aSizeError);
          if (!aSizeError) {
            aChild.mLength = static_cast<std::uint64_t>(aRawSize);
            aChild.mHasLength = true;
          }
        }
        pFrame.mChildren.push_back(std::move(aChild));
      }
    }
    aIterator.increment(aIteratorError);
  }

  std::sort(pFrame.mChildren.begin(), pFrame.mChildren.end(), &DiscoveryEntryLess);
  return true;
}

std::uint64_t PendingEntryCount(const SanityDiscoverySideCursorV2& pSide) {
  std::uint64_t aPending = 0u;
  for (const SanityDiscoveryFrameV2& aFrame : pSide.mFrames) {
    if (!aFrame.mLoaded) {
      ++aPending;
      continue;
    }
    if (aFrame.mChildIndex < aFrame.mChildren.size()) {
      aPending += static_cast<std::uint64_t>(aFrame.mChildren.size() - aFrame.mChildIndex);
    }
  }
  return aPending;
}

void UpdateObservedEntryCount(SanityDiscoverySideCursorV2& pSide) {
  const std::uint64_t aObserved =
      pSide.mProcessedEntries + std::max<std::uint64_t>(1u, PendingEntryCount(pSide));
  pSide.mMaxObservedEntries = std::max(pSide.mMaxObservedEntries, aObserved);
}

double SideProgressFraction(const SanityDiscoverySideCursorV2& pSide) {
  if (pSide.mFinished) {
    return 1.0;
  }
  if (pSide.mMaxObservedEntries == 0u) {
    return 0.0;
  }
  return static_cast<double>(pSide.mProcessedEntries) /
         static_cast<double>(pSide.mMaxObservedEntries);
}

std::string DiscoveryProgressLabel(const SanityDiscoveryCursorV2& pCursor) {
  if (!pCursor.mLeft.mFinished) {
    return "Discovering left tree";
  }
  if (!pCursor.mRight.mFinished) {
    return "Discovering right tree";
  }
  return ProgressStageLabelV2(ProgressStageV2::kDiscovery);
}

void EmitDiscoveryProgress(SanityStageContextV2& pContext,
                           const SanityDiscoveryCursorV2& pCursor) {
  const double aLocalFraction =
      (0.5 * SideProgressFraction(pCursor.mLeft)) +
      (0.5 * SideProgressFraction(pCursor.mRight));
  pContext.EmitPhaseProgress(aLocalFraction, DiscoveryProgressLabel(pCursor));
}

bool ShouldEmitDiscoverySlice(SanityDiscoveryCursorV2& pCursor) {
  bool aShouldEmit = false;
  while (pCursor.mStat.mFilesCompleted >= pCursor.mNextFileLog) {
    aShouldEmit = true;
    pCursor.mNextFileLog += knobs::kSanityDiscoveryFileLogIntervalV2;
  }
  while (pCursor.mStat.mFoldersCompleted >= pCursor.mNextFolderLog) {
    aShouldEmit = true;
    pCursor.mNextFolderLog += knobs::kSanityDiscoveryFolderLogIntervalV2;
  }
  return aShouldEmit;
}

void MaybeEmitDiscoverySlice(SanityStageContextV2& pContext,
                             SanityDiscoveryCursorV2& pCursor) {
  if (!ShouldEmitDiscoverySlice(pCursor)) {
    return;
  }
  pContext.EmitLog(
      LogLevelV2::kInfo,
      LogSanityDiscoverySliceV2(pCursor.mStat.mFilesCompleted +
                                pCursor.mStat.mFoldersCompleted));
}

void SortDiscoveryEntries(std::vector<SanityEntryV2>& pEntries) {
  std::sort(pEntries.begin(),
            pEntries.end(),
            [](const SanityEntryV2& pLeft, const SanityEntryV2& pRight) {
              if (pLeft.mRelativePath != pRight.mRelativePath) {
                return pLeft.mRelativePath < pRight.mRelativePath;
              }
              return pLeft.mPath < pRight.mPath;
            });
}

void EmitHiddenDiscoverySummary(SanityStageContextV2& pContext,
                                const SanityDiscoveryStateV2& pDiscovery) {
  if (pContext.Request().mIgnoreHidden) {
    const std::uint64_t aSkippedHiddenCount =
        pDiscovery.mSkippedHiddenLeftFileCount +
        pDiscovery.mSkippedHiddenRightFileCount +
        pDiscovery.mSkippedHiddenLeftFolderCount +
        pDiscovery.mSkippedHiddenRightFolderCount;
    if (aSkippedHiddenCount > 0u) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Folder Compare][Discovery] Ignored " +
              std::to_string(aSkippedHiddenCount) +
              " hidden entries. left_files=" +
              std::to_string(pDiscovery.mSkippedHiddenLeftFileCount) +
              ", right_files=" +
              std::to_string(pDiscovery.mSkippedHiddenRightFileCount) +
              ", left_folders=" +
              std::to_string(pDiscovery.mSkippedHiddenLeftFolderCount) +
              ", right_folders=" +
              std::to_string(pDiscovery.mSkippedHiddenRightFolderCount) + ".");
    }
    return;
  }

  const std::uint64_t aVisibleHiddenCount =
      pDiscovery.mHiddenLeftFileCount +
      pDiscovery.mHiddenRightFileCount +
      pDiscovery.mHiddenLeftFolderCount +
      pDiscovery.mHiddenRightFolderCount;
  if (aVisibleHiddenCount > 0u) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        "[Folder Compare][Discovery] Hidden entries are included in this comparison. left_files=" +
            std::to_string(pDiscovery.mHiddenLeftFileCount) +
            ", right_files=" +
            std::to_string(pDiscovery.mHiddenRightFileCount) +
            ", left_folders=" +
            std::to_string(pDiscovery.mHiddenLeftFolderCount) +
            ", right_folders=" +
            std::to_string(pDiscovery.mHiddenRightFolderCount) + ".");
  }
}

bool FinalizeSanityDiscovery(SanityStageContextV2& pContext) {
  SanityDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  SortDiscoveryEntries(aDiscovery.mLeftFiles);
  SortDiscoveryEntries(aDiscovery.mRightFiles);
  SortDiscoveryEntries(aDiscovery.mLeftFolders);
  SortDiscoveryEntries(aDiscovery.mRightFolders);
  EmitHiddenDiscoverySummary(pContext, aDiscovery);
  pContext.State().mCursor.mDiscovery.reset();
  pContext.EmitPhaseProgress(1.0, ProgressStageLabelV2(ProgressStageV2::kDiscovery));
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kSanity, ProgressStageV2::kDiscovery));
  return !pContext.IsCancelRequested();
}

void ProcessFileEntry(SanityStageContextV2& pContext,
                      SanityDiscoveryStateV2& pDiscovery,
                      SanityDiscoveryCursorV2& pCursor,
                      const SanityDiscoveryFrameV2::ChildEntryV2& pEntry,
                      bool pIsLeft) {
  const bool aIsHidden = RelativePathContainsHiddenSegment(pEntry.mRelativePath);
  if (pIsLeft) {
    if (aIsHidden) {
      ++pDiscovery.mHiddenLeftFileCount;
    }
  } else if (aIsHidden) {
    ++pDiscovery.mHiddenRightFileCount;
  }

  if (pContext.Request().mIgnoreHidden && aIsHidden) {
    if (pIsLeft) {
      ++pDiscovery.mSkippedHiddenLeftFileCount;
    } else {
      ++pDiscovery.mSkippedHiddenRightFileCount;
    }
    return;
  }

  SanityEntryV2 aSanityEntry = ToSanityEntry(pEntry);
  if (pIsLeft) {
    pDiscovery.mLeftFiles.push_back(std::move(aSanityEntry));
  } else {
    pDiscovery.mRightFiles.push_back(std::move(aSanityEntry));
  }
  ++pCursor.mStat.mFilesTotal;
  ++pCursor.mStat.mFilesCompleted;
  MaybeEmitDiscoverySlice(pContext, pCursor);
}

void ProcessFolderEntry(SanityStageContextV2& pContext,
                        SanityDiscoveryStateV2& pDiscovery,
                        SanityDiscoveryCursorV2& pCursor,
                        const SanityDiscoveryFrameV2::ChildEntryV2& pEntry,
                        bool pIsLeft) {
  const bool aIsHidden = RelativePathContainsHiddenSegment(pEntry.mRelativePath);
  if (pIsLeft) {
    if (aIsHidden) {
      ++pDiscovery.mHiddenLeftFolderCount;
    }
  } else if (aIsHidden) {
    ++pDiscovery.mHiddenRightFolderCount;
  }

  if (pContext.Request().mIgnoreHidden && aIsHidden) {
    if (pIsLeft) {
      ++pDiscovery.mSkippedHiddenLeftFolderCount;
    } else {
      ++pDiscovery.mSkippedHiddenRightFolderCount;
    }
    return;
  }

  SanityEntryV2 aSanityEntry = ToSanityEntry(pEntry);
  if (pIsLeft) {
    pDiscovery.mLeftFolders.push_back(std::move(aSanityEntry));
  } else {
    pDiscovery.mRightFolders.push_back(std::move(aSanityEntry));
  }
  ++pCursor.mStat.mFoldersTotal;
  ++pCursor.mStat.mFoldersCompleted;
  MaybeEmitDiscoverySlice(pContext, pCursor);
}

enum class SideAdvanceResultV2 {
  kProcessed = 0,
  kFinished = 1,
  kError = 2,
};

SideAdvanceResultV2 AdvanceDiscoverySide(SanityStageContextV2& pContext,
                                         SanityDiscoveryStateV2& pDiscovery,
                                         SanityDiscoveryCursorV2& pCursor,
                                         SanityDiscoverySideCursorV2& pSide,
                                         bool pIsLeft) {
  while (!pSide.mFrames.empty()) {
    SanityDiscoveryFrameV2& aFrame = pSide.mFrames.back();
    if (!aFrame.mLoaded) {
      (void)LoadDirectoryChildren(pSide.mRootPath, aFrame);
      UpdateObservedEntryCount(pSide);
      if (aFrame.mChildren.empty()) {
        pSide.mFrames.pop_back();
        continue;
      }
    }

    if (aFrame.mChildIndex >= aFrame.mChildren.size()) {
      pSide.mFrames.pop_back();
      UpdateObservedEntryCount(pSide);
      continue;
    }

    const SanityDiscoveryFrameV2::ChildEntryV2 aEntry =
        aFrame.mChildren[aFrame.mChildIndex];
    ++aFrame.mChildIndex;
    ++pSide.mProcessedEntries;

    if (aEntry.mIsDirectory) {
      ProcessFolderEntry(pContext, pDiscovery, pCursor, aEntry, pIsLeft);
      SanityDiscoveryFrameV2 aChildFrame;
      aChildFrame.mDirectoryPath = aEntry.mPath;
      aChildFrame.mRelativePath = aEntry.mRelativePath;
      (void)LoadDirectoryChildren(pSide.mRootPath, aChildFrame);
      if (!aChildFrame.mChildren.empty()) {
        pSide.mFrames.push_back(std::move(aChildFrame));
      }
    } else {
      ProcessFileEntry(pContext, pDiscovery, pCursor, aEntry, pIsLeft);
    }

    UpdateObservedEntryCount(pSide);
    return SideAdvanceResultV2::kProcessed;
  }

  pSide.mFinished = true;
  pSide.mMaxObservedEntries = std::max(pSide.mMaxObservedEntries, pSide.mProcessedEntries);
  return SideAdvanceResultV2::kFinished;
}

}  // namespace

bool SanityDiscoveryV2::Run(SanityStageContextV2& pContext) {
  auto& aDiscovery = pContext.State().mDiscovery;
  std::shared_ptr<SanityDiscoveryCursorV2>& aCursorPtr =
      pContext.State().mCursor.mDiscovery;
  if (!aCursorPtr) {
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseStartedV2(LogActionV2::kSanity, ProgressStageV2::kDiscovery));
    aDiscovery = SanityDiscoveryStateV2{};

    aCursorPtr = std::make_shared<SanityDiscoveryCursorV2>();
    aCursorPtr->mLeft.mRootPath = pContext.Request().mLeftDirectory;
    aCursorPtr->mRight.mRootPath = pContext.Request().mRightDirectory;
    aCursorPtr->mLeft.mFrames.push_back({pContext.Request().mLeftDirectory, std::string()});
    aCursorPtr->mRight.mFrames.push_back({pContext.Request().mRightDirectory, std::string()});
    UpdateObservedEntryCount(aCursorPtr->mLeft);
    UpdateObservedEntryCount(aCursorPtr->mRight);
  }

  SanityDiscoveryCursorV2& aCursor = *aCursorPtr;
  std::uint32_t aWorkBudget =
      std::max<std::uint32_t>(1u, knobs::kBatchSizeSanityDiscoveryV2);

  while (aWorkBudget > 0u) {
    if (pContext.IsCancelRequested()) {
      return false;
    }
    if (aCursor.mLeft.mFinished && aCursor.mRight.mFinished) {
      break;
    }

    SideAdvanceResultV2 aAdvance = SideAdvanceResultV2::kFinished;
    if (!aCursor.mLeft.mFinished) {
      aCursor.mActiveSide = SanityDiscoveryCursorV2::SideV2::kLeft;
      aAdvance = AdvanceDiscoverySide(
          pContext, aDiscovery, aCursor, aCursor.mLeft, true);
    } else if (!aCursor.mRight.mFinished) {
      aCursor.mActiveSide = SanityDiscoveryCursorV2::SideV2::kRight;
      aAdvance = AdvanceDiscoverySide(
          pContext, aDiscovery, aCursor, aCursor.mRight, false);
    }

    if (aAdvance == SideAdvanceResultV2::kError) {
      aCursorPtr.reset();
      return false;
    }
    if (aAdvance == SideAdvanceResultV2::kProcessed) {
      --aWorkBudget;
    }
  }

  EmitDiscoveryProgress(pContext, aCursor);

  if (pContext.IsCancelRequested()) {
    return false;
  }
  if (!aCursor.mLeft.mFinished || !aCursor.mRight.mFinished) {
    pContext.ContinuePhaseOnNextHeartbeat();
    return true;
  }

  return FinalizeSanityDiscovery(pContext);
}

}  // namespace peanutbutter
