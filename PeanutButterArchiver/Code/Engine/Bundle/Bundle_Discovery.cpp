#include "Bundle_Discovery.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <unordered_set>

#include "../../Knobs.hpp"
#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

struct BundleDiscoveryFrameV2 {
  std::string mDirectoryPath;
  std::string mRelativePath;
  std::vector<DirectoryEntryV2> mChildren;
  std::size_t mChildIndex = 0u;
  bool mLoaded = false;
  bool mHasResolvedDirectoryIdentity = false;
  bool mTracksResolvedIdentityInActiveSet = false;
  std::string mResolvedDirectoryIdentity;
};

class BundleDiscoveryCursorV2 {
 public:
  bool mSingleFilePending = false;
  DirectoryEntryV2 mSingleFile;
  std::vector<BundleDiscoveryFrameV2> mFrames;
  std::string mResolvedSourceRoot;
  bool mHasResolvedSourceRoot = false;
  std::size_t mProcessedItems = 0u;
  std::size_t mBatchItemsProcessed = 0u;
  std::size_t mPendingItems = 0u;
  std::size_t mMaxObservedItems = 1u;
  std::size_t mNextProgressItem = 1u;
  std::unordered_set<std::string> mActiveResolvedDirectoryIdentities;
};

namespace {

const char* DiscoveryItemTypeLabel(bool pIsDirectory) {
  return pIsDirectory ? "folder" : "file";
}

std::size_t BundleDiscoveryBatchSize() {
  return std::max<std::size_t>(
      1u, static_cast<std::size_t>(knobs::kBatchSizeBundleDiscoveryV2));
}

void EmitBundleDiscoveryBatchLog(BundleStageContextV2& pContext,
                                 const BundleDiscoveryCursorV2& pCursor) {
  pContext.EmitLog(
      LogLevelV2::kInfo,
      LogBundleDiscoverySliceV2(
          pContext.State().mDiscovery.mFileCount,
          pContext.State().mDiscovery.mEmptyFolderCount,
          static_cast<std::uint64_t>(pCursor.mProcessedItems)));
}

bool ShouldEmitBundleDiscoveryProgress(BundleDiscoveryCursorV2& pCursor) {
  const std::size_t aInterval = std::max<std::size_t>(
      1u, knobs::kBundleDiscoveryProgressItemIntervalV2);
  if (pCursor.mProcessedItems < pCursor.mNextProgressItem) {
    return false;
  }
  do {
    pCursor.mNextProgressItem += aInterval;
  } while (pCursor.mProcessedItems >= pCursor.mNextProgressItem);
  return true;
}

bool DiscoveryEntryLess(const DirectoryEntryV2& pLeft,
                        const DirectoryEntryV2& pRight) {
  if (pLeft.mRelativePath != pRight.mRelativePath) {
    return pLeft.mRelativePath < pRight.mRelativePath;
  }
  return pLeft.mPath < pRight.mPath;
}

std::string JoinRelativePathFast(const std::string& pParent,
                                 const std::string& pChild) {
  if (pParent.empty()) {
    return pChild;
  }
  if (pChild.empty()) {
    return pParent;
  }

  std::string aJoined;
  aJoined.reserve(pParent.size() + 1u + pChild.size());
  aJoined.append(pParent);
  if (aJoined.back() != '/') {
    aJoined.push_back('/');
  }
  aJoined.append(pChild);
  return aJoined;
}

bool ResolveDirectoryIdentity(const std::string& pPath,
                              std::string& pOutResolvedIdentity) {
  pOutResolvedIdentity.clear();
  std::error_code aError;
  const std::filesystem::path aCanonical =
      std::filesystem::weakly_canonical(std::filesystem::path(pPath), aError);
  if (!aError) {
    pOutResolvedIdentity = aCanonical.lexically_normal().generic_string();
    return !pOutResolvedIdentity.empty();
  }

  aError.clear();
  const std::filesystem::path aAbsolute =
      std::filesystem::absolute(std::filesystem::path(pPath), aError);
  if (!aError) {
    pOutResolvedIdentity = aAbsolute.lexically_normal().generic_string();
    return !pOutResolvedIdentity.empty();
  }
  return false;
}

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

bool IsSafeRelativeReferencePath(const std::string& pPath,
                                 std::size_t pMaxPathLength) {
  if (pPath.empty() || pPath.size() > pMaxPathLength) {
    return false;
  }
  if (IsAbsolutePathLikeV2(pPath)) {
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

bool IsResolvedPathInsideResolvedRoot(const std::string& pResolvedRoot,
                                      const std::string& pResolvedPath) {
  if (pResolvedRoot.empty() || pResolvedPath.empty()) {
    return false;
  }

  std::string aRoot = pResolvedRoot;
  std::string aPath = pResolvedPath;
  if (aRoot.size() > 1u && aRoot.back() == '/') {
    aRoot.pop_back();
  }
  if (aPath.size() > 1u && aPath.back() == '/') {
    aPath.pop_back();
  }
  if (aRoot == "/") {
    return !aPath.empty() && aPath[0] == '/';
  }
  if (aPath == aRoot) {
    return true;
  }
  return aPath.size() > aRoot.size() &&
         aPath.compare(0, aRoot.size(), aRoot) == 0 &&
         aPath[aRoot.size()] == '/';
}

bool TryEncodeAliasTargetHomeRelative(const std::string& pAbsolutePath,
                                      std::string& pOutHomeRelative) {
  pOutHomeRelative.clear();
  constexpr char kUsersPrefix[] = "/Users/";
  if (pAbsolutePath.rfind(kUsersPrefix, 0u) != 0u) {
    return false;
  }
  const std::size_t aUserStart = sizeof(kUsersPrefix) - 1u;
  const std::size_t aUserEnd = pAbsolutePath.find('/', aUserStart);
  if (aUserEnd == std::string::npos) {
    return true;
  }
  if (aUserEnd + 1u >= pAbsolutePath.size()) {
    return true;
  }
  pOutHomeRelative = pAbsolutePath.substr(aUserEnd + 1u);
  return true;
}

bool BuildAliasTargetDescriptor(const std::string& pResolvedSourceRoot,
                                const std::string& pResolvedTargetPath,
                                std::size_t pMaxPathLength,
                                std::string& pOutTargetDescriptor,
                                std::string& pOutFailureReason) {
  pOutTargetDescriptor.clear();
  pOutFailureReason.clear();

  if (pResolvedSourceRoot.empty() || pResolvedTargetPath.empty()) {
    pOutFailureReason = "alias target path could not be resolved";
    return false;
  }

  if (IsResolvedPathInsideResolvedRoot(pResolvedSourceRoot, pResolvedTargetPath)) {
    std::error_code aRelativeError;
    const std::filesystem::path aRelativePath = std::filesystem::relative(
        std::filesystem::path(pResolvedTargetPath),
        std::filesystem::path(pResolvedSourceRoot),
        aRelativeError);
    if (aRelativeError) {
      pOutFailureReason = "alias target relative path could not be derived";
      return false;
    }
    const std::string aRelativeText = aRelativePath.lexically_normal().generic_string();
    pOutTargetDescriptor = aRelativeText.empty() ? "r" : "r/" + aRelativeText;
  } else {
    std::string aHomeRelative;
    if (TryEncodeAliasTargetHomeRelative(pResolvedTargetPath, aHomeRelative)) {
      pOutTargetDescriptor = aHomeRelative.empty() ? "h" : "h/" + aHomeRelative;
    } else if (IsAbsolutePathLikeV2(pResolvedTargetPath)) {
      std::size_t aTrimStart = 0u;
      while (aTrimStart < pResolvedTargetPath.size() &&
             (pResolvedTargetPath[aTrimStart] == '/' ||
              pResolvedTargetPath[aTrimStart] == '\\')) {
        ++aTrimStart;
      }
      const std::string aAbsoluteSansRoot = pResolvedTargetPath.substr(aTrimStart);
      pOutTargetDescriptor =
          aAbsoluteSansRoot.empty() ? "a" : "a/" + aAbsoluteSansRoot;
    } else {
      pOutFailureReason = "alias target path format was not supported";
      return false;
    }
  }

  if (!IsSafeRelativeReferencePath(pOutTargetDescriptor, pMaxPathLength)) {
    pOutFailureReason = "alias target descriptor exceeded safety limits";
    pOutTargetDescriptor.clear();
    return false;
  }
  return true;
}

const char* ReferenceKindLabel(std::uint8_t pReferenceKind) {
  switch (static_cast<memory_layout::ReferenceRecordKindV2>(pReferenceKind)) {
    case memory_layout::ReferenceRecordKindV2::kSymlink:
      return "symlink";
    case memory_layout::ReferenceRecordKindV2::kAlias:
      return "alias";
    case memory_layout::ReferenceRecordKindV2::kReparsePoint:
      return "reparse point";
    case memory_layout::ReferenceRecordKindV2::kHardlink:
      return "hardlink";
  }
  return "reference";
}

bool TryBuildInternalReferenceTargetRelativePath(
    BundleStageContextV2& pContext,
    const BundleDiscoveryCursorV2& pCursor,
    const DirectoryEntryV2& pEntry,
    std::uint8_t pReferenceKind,
    std::size_t pMaxPathLength,
    std::string& pOutTargetRelativePath,
    std::string& pOutFailureReason) {
  pOutTargetRelativePath.clear();
  pOutFailureReason.clear();

  if (!pCursor.mHasResolvedSourceRoot) {
    pOutFailureReason = "source root identity could not be resolved";
    return false;
  }

  std::string aRawTargetPath;
  const bool aTargetRead =
      pReferenceKind ==
              static_cast<std::uint8_t>(memory_layout::ReferenceRecordKindV2::kAlias)
          ? pContext.FileSystem().TryReadAliasTarget(pEntry.mPath, aRawTargetPath)
          : pContext.FileSystem().TryReadSymlinkTarget(pEntry.mPath, aRawTargetPath);
  if (!aTargetRead || aRawTargetPath.empty()) {
    pOutFailureReason = std::string(ReferenceKindLabel(pReferenceKind)) +
                        " target could not be read";
    return false;
  }

  const std::string aTargetProbePath =
      IsAbsolutePathLikeV2(aRawTargetPath)
          ? aRawTargetPath
          : pContext.FileSystem().JoinPath(
                pContext.FileSystem().ParentPath(pEntry.mPath),
                aRawTargetPath);

  std::string aResolvedTargetPath;
  if (!ResolveDirectoryIdentity(aTargetProbePath, aResolvedTargetPath)) {
    pOutFailureReason = std::string(ReferenceKindLabel(pReferenceKind)) +
                        " target path could not be resolved";
    return false;
  }

  if (pReferenceKind ==
      static_cast<std::uint8_t>(memory_layout::ReferenceRecordKindV2::kAlias)) {
    return BuildAliasTargetDescriptor(pCursor.mResolvedSourceRoot,
                                      aResolvedTargetPath,
                                      pMaxPathLength,
                                      pOutTargetRelativePath,
                                      pOutFailureReason);
  }

  if (!IsResolvedPathInsideResolvedRoot(
          pCursor.mResolvedSourceRoot, aResolvedTargetPath)) {
    pOutFailureReason = std::string(ReferenceKindLabel(pReferenceKind)) +
                        " target escaped source root and was omitted for privacy";
    return false;
  }

  std::error_code aRelativeError;
  const std::filesystem::path aRelativePath = std::filesystem::relative(
      std::filesystem::path(aResolvedTargetPath),
      std::filesystem::path(pCursor.mResolvedSourceRoot),
      aRelativeError);
  if (aRelativeError) {
    pOutFailureReason = std::string(ReferenceKindLabel(pReferenceKind)) +
                        " target relative path could not be derived";
    return false;
  }

  const std::string aTargetRelativePath =
      aRelativePath.lexically_normal().generic_string();
  if (!IsSafeRelativeReferencePath(aTargetRelativePath, pMaxPathLength)) {
    pOutFailureReason = std::string(ReferenceKindLabel(pReferenceKind)) +
                        " target relative path was not safe";
    return false;
  }

  pOutTargetRelativePath = aTargetRelativePath;
  return true;
}

bool DirectoryWouldCreateCycle(const BundleDiscoveryCursorV2& pCursor,
                               const std::string& pResolvedDirectoryIdentity) {
  return !pResolvedDirectoryIdentity.empty() &&
         pCursor.mActiveResolvedDirectoryIdentities.find(
             pResolvedDirectoryIdentity) !=
             pCursor.mActiveResolvedDirectoryIdentities.end();
}

void EmitBundleDiscoveryItemEvent(BundleStageContextV2& pContext,
                                  const std::string& pSourcePath,
                                  const std::string& pRelativePath,
                                  bool pIsDirectory,
                                  std::uint64_t pContentLength,
                                  std::size_t pItemIndex,
                                  std::size_t pItemsTotal) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kBundleDiscoveryItemScanned;
  aEvent.mStage = ProgressStageV2::kDiscovery;
  aEvent.mLabel = std::string("Bundle discovery scanned ") +
                  DiscoveryItemTypeLabel(pIsDirectory) + " " + pRelativePath;
  aEvent.SetInfo("source_path", pSourcePath);
  aEvent.SetInfo("relative_path", pRelativePath);
  aEvent.SetInfo("item_type", DiscoveryItemTypeLabel(pIsDirectory));
  aEvent.SetInfo("content_length", pContentLength);
  aEvent.SetInfo("item_index", static_cast<std::uint64_t>(pItemIndex));
  aEvent.SetInfo("items_total", static_cast<std::uint64_t>(pItemsTotal));
  pContext.EmitRuntimeEvent(aEvent);
}

bool FinalizeBundleDiscovery(BundleStageContextV2& pContext) {
  BundleDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  std::sort(aDiscovery.mFileRecords.begin(),
            aDiscovery.mFileRecords.end(),
            [](const BundleRecordEntryV2& pLeft,
               const BundleRecordEntryV2& pRight) {
              return pLeft.mRelativePath < pRight.mRelativePath;
            });
  std::sort(aDiscovery.mEmptyFolderRecords.begin(),
            aDiscovery.mEmptyFolderRecords.end(),
            [](const BundleRecordEntryV2& pLeft,
               const BundleRecordEntryV2& pRight) {
              return pLeft.mRelativePath < pRight.mRelativePath;
            });

  pContext.State().mCursor.mDiscovery.reset();
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogBundleDiscoverySummaryV2(aDiscovery.mFileCount,
                                               aDiscovery.mEmptyFolderCount));
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kDiscovery));
  pContext.EmitPhaseProgress(1.0, "Discovery complete");
  return !pContext.IsCancelRequested();
}

void UpdateObservedItemCount(BundleDiscoveryCursorV2& pCursor) {
  const std::size_t aObserved =
      pCursor.mProcessedItems + std::max<std::size_t>(1u, pCursor.mPendingItems);
  pCursor.mMaxObservedItems = std::max(pCursor.mMaxObservedItems, aObserved);
}

double DiscoveryProgressFraction(const BundleDiscoveryCursorV2& pCursor) {
  if (!pCursor.mSingleFilePending && pCursor.mFrames.empty()) {
    return 1.0;
  }
  return static_cast<double>(pCursor.mProcessedItems) /
         static_cast<double>(std::max<std::size_t>(1u, pCursor.mMaxObservedItems));
}

void EmitDiscoveryProgress(BundleStageContextV2& pContext,
                           const BundleDiscoveryCursorV2& pCursor,
                           bool pIsDirectory) {
  pContext.EmitPhaseProgress(
      DiscoveryProgressFraction(pCursor),
      pIsDirectory ? "Discovering source folders" : "Discovering source files");
}

void NoteEmptyDirectory(BundleStageContextV2& pContext,
                        const BundleDiscoveryFrameV2& pFrame) {
  if (pFrame.mRelativePath.empty()) {
    return;
  }

  BundleRecordEntryV2 aFolderRecord;
  aFolderRecord.mSourcePath = pFrame.mDirectoryPath;
  aFolderRecord.mRelativePath = pFrame.mRelativePath;
  aFolderRecord.mContentLength = 0u;
  aFolderRecord.mIsDirectory = true;
  pContext.State().mDiscovery.mEmptyFolderRecords.push_back(std::move(aFolderRecord));
  ++pContext.State().mDiscovery.mEmptyFolderCount;
}

void LoadDirectoryChildren(BundleStageContextV2& pContext,
                           BundleDiscoveryCursorV2& pCursor,
                           BundleDiscoveryFrameV2& pFrame) {
  pFrame.mChildren = pContext.FileSystem().ListDirectoryEntries(pFrame.mDirectoryPath);
  if (!std::is_sorted(
          pFrame.mChildren.begin(), pFrame.mChildren.end(), &DiscoveryEntryLess)) {
    std::sort(pFrame.mChildren.begin(), pFrame.mChildren.end(), &DiscoveryEntryLess);
  }
  pFrame.mChildIndex = 0u;
  pFrame.mLoaded = true;
  if (pCursor.mPendingItems > 0u) {
    --pCursor.mPendingItems;
  }
  pCursor.mPendingItems += pFrame.mChildren.size();
  if (pFrame.mChildren.empty()) {
    NoteEmptyDirectory(pContext, pFrame);
  }
}

bool ProcessDiscoveredFile(BundleStageContextV2& pContext,
                           BundleDiscoveryCursorV2& pCursor,
                           const DirectoryEntryV2& pEntry,
                           const std::string& pRelativePath,
                           std::size_t pMaxPathLength) {
  if (pRelativePath.empty() || pRelativePath.size() > pMaxPathLength) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Bundle discovery failed: source file path exceeds supported length.");
    pContext.State().mCursor.mDiscovery.reset();
    return false;
  }

  std::unique_ptr<FileReadStreamV2> aRead =
      pContext.FileSystem().OpenReadStream(pEntry.mPath);
  if (aRead == nullptr || !aRead->IsReady()) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Bundle discovery failed: source file stream could not be opened.");
    pContext.State().mCursor.mDiscovery.reset();
    return false;
  }
  const std::uint64_t aFileLength = static_cast<std::uint64_t>(aRead->GetLength());

  BundleRecordEntryV2 aRecord;
  aRecord.mSourcePath = pEntry.mPath;
  aRecord.mRelativePath = pRelativePath;
  aRecord.mContentLength = aFileLength;
  aRecord.mIsDirectory = false;
  pContext.State().mDiscovery.mFileRecords.push_back(std::move(aRecord));
  ++pContext.State().mDiscovery.mFileCount;
  pContext.State().mDiscovery.mTotalSourceBytes += aFileLength;

  ++pCursor.mProcessedItems;
  ++pCursor.mBatchItemsProcessed;
  UpdateObservedItemCount(pCursor);
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleDiscoveryItemScanned)) {
    EmitBundleDiscoveryItemEvent(pContext,
                                 pEntry.mPath,
                                 pRelativePath,
                                 false,
                                 aFileLength,
                                 pCursor.mProcessedItems,
                                 pCursor.mMaxObservedItems);
  }
  if (ShouldEmitBundleDiscoveryProgress(pCursor)) {
    EmitDiscoveryProgress(pContext, pCursor, false);
  }
  return !pContext.IsCancelRequested();
}

bool ProcessDiscoveredReference(BundleStageContextV2& pContext,
                                BundleDiscoveryCursorV2& pCursor,
                                const DirectoryEntryV2& pEntry,
                                const std::string& pRelativePath,
                                std::uint8_t pReferenceKind,
                                std::size_t pMaxPathLength) {
  if (pRelativePath.empty() || pRelativePath.size() > pMaxPathLength) {
    pContext.EmitLog(
        LogLevelV2::kError,
        "Bundle discovery failed: source link path exceeds supported length.");
    pContext.State().mCursor.mDiscovery.reset();
    return false;
  }

  std::string aTargetRelativePath;
  std::string aFailureReason;
  if (!TryBuildInternalReferenceTargetRelativePath(pContext,
                                                   pCursor,
                                                   pEntry,
                                                   pReferenceKind,
                                                   pMaxPathLength,
                                                   aTargetRelativePath,
                                                   aFailureReason)) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        "[Bundle][Discovery] Skipped link '" + pRelativePath + "': " +
            aFailureReason + ".");
    ++pCursor.mProcessedItems;
    ++pCursor.mBatchItemsProcessed;
    UpdateObservedItemCount(pCursor);
    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleDiscoveryItemScanned)) {
      EmitBundleDiscoveryItemEvent(pContext,
                                   pEntry.mPath,
                                   pRelativePath,
                                   false,
                                   0u,
                                   pCursor.mProcessedItems,
                                   pCursor.mMaxObservedItems);
    }
    if (ShouldEmitBundleDiscoveryProgress(pCursor)) {
      EmitDiscoveryProgress(pContext, pCursor, false);
    }
    return !pContext.IsCancelRequested();
  }

  BundleRecordEntryV2 aRecord;
  aRecord.mSourcePath = pEntry.mPath;
  aRecord.mRelativePath = pRelativePath;
  aRecord.mContentLength = 0u;
  aRecord.mIsDirectory = false;
  aRecord.mIsReference = true;
  aRecord.mReferenceKind = pReferenceKind;
  aRecord.mReferenceTargetRelativePath = aTargetRelativePath;
  pContext.State().mDiscovery.mFileRecords.push_back(std::move(aRecord));
  ++pContext.State().mDiscovery.mFileCount;

  ++pCursor.mProcessedItems;
  ++pCursor.mBatchItemsProcessed;
  UpdateObservedItemCount(pCursor);
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleDiscoveryItemScanned)) {
    EmitBundleDiscoveryItemEvent(pContext,
                                 pEntry.mPath,
                                 pRelativePath,
                                 false,
                                 0u,
                                 pCursor.mProcessedItems,
                                 pCursor.mMaxObservedItems);
  }
  if (ShouldEmitBundleDiscoveryProgress(pCursor)) {
    EmitDiscoveryProgress(pContext, pCursor, false);
  }
  return !pContext.IsCancelRequested();
}

bool ProcessDiscoveredDirectory(BundleStageContextV2& pContext,
                                BundleDiscoveryCursorV2& pCursor,
                                const DirectoryEntryV2& pEntry,
                                const std::string& pRelativePath,
                                std::size_t pMaxPathLength) {
  if (pRelativePath.empty() || pRelativePath.size() > pMaxPathLength) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Bundle discovery failed: source folder path exceeds supported length.");
    pContext.State().mCursor.mDiscovery.reset();
    return false;
  }

  ++pCursor.mProcessedItems;
  ++pCursor.mBatchItemsProcessed;
  BundleDiscoveryFrameV2 aNextFrame;
  aNextFrame.mDirectoryPath = pEntry.mPath;
  aNextFrame.mRelativePath = pRelativePath;
  aNextFrame.mChildren.clear();
  aNextFrame.mChildIndex = 0u;
  aNextFrame.mLoaded = false;
  aNextFrame.mHasResolvedDirectoryIdentity =
      ResolveDirectoryIdentity(
          pEntry.mPath, aNextFrame.mResolvedDirectoryIdentity);
  if (aNextFrame.mHasResolvedDirectoryIdentity &&
      DirectoryWouldCreateCycle(
          pCursor, aNextFrame.mResolvedDirectoryIdentity)) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        "[Bundle][Discovery] Skipping recursive directory link to avoid a cycle: '" +
            pRelativePath + "'.");
  } else {
    if (aNextFrame.mHasResolvedDirectoryIdentity) {
      const auto aInserted = pCursor.mActiveResolvedDirectoryIdentities.insert(
          aNextFrame.mResolvedDirectoryIdentity);
      aNextFrame.mTracksResolvedIdentityInActiveSet = aInserted.second;
    }
    pCursor.mFrames.push_back(std::move(aNextFrame));
    ++pCursor.mPendingItems;
  }
  UpdateObservedItemCount(pCursor);
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kBundleDiscoveryItemScanned)) {
    EmitBundleDiscoveryItemEvent(pContext,
                                 pEntry.mPath,
                                 pRelativePath,
                                 true,
                                 0u,
                                 pCursor.mProcessedItems,
                                 pCursor.mMaxObservedItems);
  }
  if (ShouldEmitBundleDiscoveryProgress(pCursor)) {
    EmitDiscoveryProgress(pContext, pCursor, true);
  }
  return !pContext.IsCancelRequested();
}

bool AdvanceDirectoryTree(BundleStageContextV2& pContext,
                          BundleDiscoveryCursorV2& pCursor,
                          std::size_t pMaxPathLength) {
  while (!pCursor.mFrames.empty()) {
    BundleDiscoveryFrameV2& aFrame = pCursor.mFrames.back();
    if (!aFrame.mLoaded) {
      LoadDirectoryChildren(pContext, pCursor, aFrame);
      UpdateObservedItemCount(pCursor);
    }

    if (aFrame.mChildIndex >= aFrame.mChildren.size()) {
      if (aFrame.mTracksResolvedIdentityInActiveSet &&
          !aFrame.mResolvedDirectoryIdentity.empty()) {
        pCursor.mActiveResolvedDirectoryIdentities.erase(
            aFrame.mResolvedDirectoryIdentity);
      }
      pCursor.mFrames.pop_back();
      continue;
    }

    const DirectoryEntryV2& aChild = aFrame.mChildren[aFrame.mChildIndex];
    ++aFrame.mChildIndex;
    if (pCursor.mPendingItems > 0u) {
      --pCursor.mPendingItems;
    }

    const std::string aRelativePath =
        JoinRelativePathFast(aFrame.mRelativePath, aChild.mRelativePath);

    const bool aIsSymlink = pContext.FileSystem().IsSymlink(aChild.mPath);
    const bool aIsAlias = !aIsSymlink && pContext.FileSystem().IsAlias(aChild.mPath);
    if (aIsSymlink || aIsAlias) {
      const std::uint8_t aReferenceKind =
          static_cast<std::uint8_t>(aIsAlias
                                        ? memory_layout::ReferenceRecordKindV2::kAlias
                                        : memory_layout::ReferenceRecordKindV2::kSymlink);
      return ProcessDiscoveredReference(
          pContext, pCursor, aChild, aRelativePath, aReferenceKind, pMaxPathLength);
    }

    if (aChild.mIsDirectory) {
      return ProcessDiscoveredDirectory(
          pContext, pCursor, aChild, aRelativePath, pMaxPathLength);
    }
    return ProcessDiscoveredFile(
        pContext, pCursor, aChild, aRelativePath, pMaxPathLength);
  }

  return FinalizeBundleDiscovery(pContext);
}

}  // namespace

bool BundleDiscoveryV2::Run(BundleStageContextV2& pContext) {
  BundleDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  std::shared_ptr<BundleDiscoveryCursorV2>& aCursor =
      pContext.State().mCursor.mDiscovery;
  const std::size_t aMaxPathLength = pContext.Layout().mMaxPathLength;
  const std::string& aSourcePath = pContext.Request().mSourceDirectory;

  if (!aCursor) {
    aDiscovery = BundleDiscoveryStateV2{};
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseStartedV2(LogActionV2::kBundle,
                                       ProgressStageV2::kDiscovery));
    const bool aSourceExists = pContext.FileSystem().Exists(aSourcePath);
    const bool aSourceIsDirectory = pContext.FileSystem().IsDirectory(aSourcePath);
    const bool aSourceIsFile = pContext.FileSystem().IsFile(aSourcePath);
    aDiscovery.mSourceExists = aSourceExists;

    if (!aDiscovery.mSourceExists) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kDiscovery,
                                        "source path does not exist"));
      return false;
    }

    aDiscovery.mSourceStem = pContext.FileSystem().StemName(aSourcePath);
    if (aDiscovery.mSourceStem.empty()) {
      aDiscovery.mSourceStem = "archive_data";
    }

    aCursor = std::make_shared<BundleDiscoveryCursorV2>();
    if (aSourceIsFile) {
      const std::string aRelativePath = pContext.FileSystem().FileName(aSourcePath);
      aCursor->mSingleFilePending = true;
      aCursor->mPendingItems = 1u;
      aCursor->mSingleFile = {aSourcePath, aRelativePath, false};
      const std::string aSourceParent = pContext.FileSystem().ParentPath(aSourcePath);
      if (!aSourceParent.empty()) {
        aCursor->mHasResolvedSourceRoot =
            ResolveDirectoryIdentity(aSourceParent, aCursor->mResolvedSourceRoot);
      }
      aCursor->mMaxObservedItems = 1u;
    } else if (aSourceIsDirectory) {
      aCursor->mHasResolvedSourceRoot =
          ResolveDirectoryIdentity(aSourcePath, aCursor->mResolvedSourceRoot);
      BundleDiscoveryFrameV2 aRootFrame;
      aRootFrame.mDirectoryPath = aSourcePath;
      aRootFrame.mRelativePath.clear();
      aRootFrame.mChildren.clear();
      aRootFrame.mChildIndex = 0u;
      aRootFrame.mLoaded = false;
      aRootFrame.mHasResolvedDirectoryIdentity =
          ResolveDirectoryIdentity(
              aSourcePath, aRootFrame.mResolvedDirectoryIdentity);
      if (aRootFrame.mHasResolvedDirectoryIdentity) {
        const auto aInserted =
            aCursor->mActiveResolvedDirectoryIdentities.insert(
                aRootFrame.mResolvedDirectoryIdentity);
        aRootFrame.mTracksResolvedIdentityInActiveSet = aInserted.second;
      }
      aCursor->mFrames.push_back(std::move(aRootFrame));
      aCursor->mPendingItems += 1u;
      UpdateObservedItemCount(*aCursor);
    } else {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kDiscovery,
                                        "source path is not a readable file or folder"));
      aCursor.reset();
      return false;
    }
  }

  if (aCursor->mSingleFilePending) {
    aCursor->mSingleFilePending = false;
    if (aCursor->mPendingItems > 0u) {
      --aCursor->mPendingItems;
    }
    const bool aIsSymlink = pContext.FileSystem().IsSymlink(aCursor->mSingleFile.mPath) &&
                            aCursor->mHasResolvedSourceRoot;
    const bool aIsAlias = !aIsSymlink &&
                          pContext.FileSystem().IsAlias(aCursor->mSingleFile.mPath) &&
                          aCursor->mHasResolvedSourceRoot;
    const bool aSucceeded =
        (aIsSymlink || aIsAlias)
            ? ProcessDiscoveredReference(pContext,
                                         *aCursor,
                                         aCursor->mSingleFile,
                                         aCursor->mSingleFile.mRelativePath,
                                         static_cast<std::uint8_t>(
                                             aIsAlias
                                                 ? memory_layout::ReferenceRecordKindV2::kAlias
                                                 : memory_layout::ReferenceRecordKindV2::kSymlink),
                                         aMaxPathLength)
            : ProcessDiscoveredFile(pContext,
                                    *aCursor,
                                    aCursor->mSingleFile,
                                    aCursor->mSingleFile.mRelativePath,
                                    aMaxPathLength);
    if (!aSucceeded) {
      return false;
    }
    return FinalizeBundleDiscovery(pContext);
  }

  const std::size_t aBatchSize = BundleDiscoveryBatchSize();
  while (pContext.State().mCursor.mDiscovery != nullptr &&
         aCursor->mBatchItemsProcessed < aBatchSize) {
    const bool aSucceeded = AdvanceDirectoryTree(pContext, *aCursor, aMaxPathLength);
    if (!aSucceeded) {
      return false;
    }
  }

  if (pContext.State().mCursor.mDiscovery != nullptr &&
      aCursor->mBatchItemsProcessed >= aBatchSize) {
    EmitBundleDiscoveryBatchLog(pContext, *aCursor);
    aCursor->mBatchItemsProcessed = 0u;
    pContext.ContinuePhaseOnNextHeartbeat();
  }
  return true;
}

}  // namespace peanutbutter
