#include "Sanity_Discovery.hpp"

#include <algorithm>
#include <filesystem>

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {
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

std::uint64_t VisibleEntryCount(const std::vector<DirectoryEntryV2>& pEntries,
                                bool pIgnoreHidden) {
  std::uint64_t aCount = 0u;
  for (const DirectoryEntryV2& aEntry : pEntries) {
    if (pIgnoreHidden && RelativePathContainsHiddenSegment(aEntry.mRelativePath)) {
      continue;
    }
    ++aCount;
  }
  return aCount;
}

SanityEntryV2 ToSanityEntry(const DirectoryEntryV2& pEntry,
                            const FileSystemV2& pFileSystem) {
  SanityEntryV2 aEntry;
  aEntry.mPath = pEntry.mPath;
  aEntry.mRelativePath = pEntry.mRelativePath;
  aEntry.mIsDirectory = pEntry.mIsDirectory;
  if (!pEntry.mIsDirectory) {
    std::unique_ptr<FileReadStreamV2> aRead = pFileSystem.OpenReadStream(pEntry.mPath);
    if (aRead != nullptr && aRead->IsReady()) {
      aEntry.mLength = static_cast<std::uint64_t>(aRead->GetLength());
    }
  }
  return aEntry;
}

}  // namespace

bool SanityDiscoveryV2::Run(SanityStageContextV2& pContext) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionV2::kSanity, ProgressStageV2::kDiscovery));

  auto& aDiscovery = pContext.State().mDiscovery;
  aDiscovery = SanityDiscoveryStateV2{};

  const std::vector<DirectoryEntryV2> aLeftFiles =
      pContext.FileSystem().ListFilesRecursive(pContext.Request().mLeftDirectory);
  const std::vector<DirectoryEntryV2> aRightFiles =
      pContext.FileSystem().ListFilesRecursive(pContext.Request().mRightDirectory);
  const std::vector<DirectoryEntryV2> aLeftFolders =
      pContext.FileSystem().ListDirectoriesRecursive(pContext.Request().mLeftDirectory);
  const std::vector<DirectoryEntryV2> aRightFolders =
      pContext.FileSystem().ListDirectoriesRecursive(pContext.Request().mRightDirectory);

  aDiscovery.mLeftFiles.reserve(aLeftFiles.size());
  aDiscovery.mRightFiles.reserve(aRightFiles.size());
  aDiscovery.mLeftFolders.reserve(aLeftFolders.size());
  aDiscovery.mRightFolders.reserve(aRightFolders.size());

  LoggingStatV2 aStat;
  aStat.mFilesTotal =
      VisibleEntryCount(aLeftFiles, pContext.Request().mIgnoreHidden) +
      VisibleEntryCount(aRightFiles, pContext.Request().mIgnoreHidden);
  aStat.mFoldersTotal =
      VisibleEntryCount(aLeftFolders, pContext.Request().mIgnoreHidden) +
      VisibleEntryCount(aRightFolders, pContext.Request().mIgnoreHidden);

  for (const DirectoryEntryV2& aEntry : aLeftFiles) {
    if (pContext.IsCancelRequested()) {
      return false;
    }
    const bool aIsHidden = RelativePathContainsHiddenSegment(aEntry.mRelativePath);
    if (aIsHidden) {
      ++aDiscovery.mHiddenLeftFileCount;
    }
    if (pContext.Request().mIgnoreHidden && aIsHidden) {
      ++aDiscovery.mSkippedHiddenLeftFileCount;
      continue;
    }
    aDiscovery.mLeftFiles.push_back(ToSanityEntry(aEntry, pContext.FileSystem()));
    ++aStat.mFilesCompleted;
    pContext.EmitLog(LogLevelV2::kInfo, LogSanityDiscoverySliceV2(aStat));
  }
  for (const DirectoryEntryV2& aEntry : aRightFiles) {
    if (pContext.IsCancelRequested()) {
      return false;
    }
    const bool aIsHidden = RelativePathContainsHiddenSegment(aEntry.mRelativePath);
    if (aIsHidden) {
      ++aDiscovery.mHiddenRightFileCount;
    }
    if (pContext.Request().mIgnoreHidden && aIsHidden) {
      ++aDiscovery.mSkippedHiddenRightFileCount;
      continue;
    }
    aDiscovery.mRightFiles.push_back(ToSanityEntry(aEntry, pContext.FileSystem()));
    ++aStat.mFilesCompleted;
    pContext.EmitLog(LogLevelV2::kInfo, LogSanityDiscoverySliceV2(aStat));
  }
  for (const DirectoryEntryV2& aEntry : aLeftFolders) {
    const bool aIsHidden = RelativePathContainsHiddenSegment(aEntry.mRelativePath);
    if (aIsHidden) {
      ++aDiscovery.mHiddenLeftFolderCount;
    }
    if (pContext.Request().mIgnoreHidden && aIsHidden) {
      ++aDiscovery.mSkippedHiddenLeftFolderCount;
      continue;
    }
    aDiscovery.mLeftFolders.push_back(ToSanityEntry(aEntry, pContext.FileSystem()));
    ++aStat.mFoldersCompleted;
  }
  for (const DirectoryEntryV2& aEntry : aRightFolders) {
    const bool aIsHidden = RelativePathContainsHiddenSegment(aEntry.mRelativePath);
    if (aIsHidden) {
      ++aDiscovery.mHiddenRightFolderCount;
    }
    if (pContext.Request().mIgnoreHidden && aIsHidden) {
      ++aDiscovery.mSkippedHiddenRightFolderCount;
      continue;
    }
    aDiscovery.mRightFolders.push_back(ToSanityEntry(aEntry, pContext.FileSystem()));
    ++aStat.mFoldersCompleted;
  }

  if (pContext.Request().mIgnoreHidden) {
    const std::uint64_t aSkippedHiddenCount =
        aDiscovery.mSkippedHiddenLeftFileCount +
        aDiscovery.mSkippedHiddenRightFileCount +
        aDiscovery.mSkippedHiddenLeftFolderCount +
        aDiscovery.mSkippedHiddenRightFolderCount;
    if (aSkippedHiddenCount > 0u) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Folder Compare][Discovery] Ignored " +
              std::to_string(aSkippedHiddenCount) +
              " hidden entries. left_files=" +
              std::to_string(aDiscovery.mSkippedHiddenLeftFileCount) +
              ", right_files=" +
              std::to_string(aDiscovery.mSkippedHiddenRightFileCount) +
              ", left_folders=" +
              std::to_string(aDiscovery.mSkippedHiddenLeftFolderCount) +
              ", right_folders=" +
              std::to_string(aDiscovery.mSkippedHiddenRightFolderCount) + ".");
    }
  } else {
    const std::uint64_t aVisibleHiddenCount =
        aDiscovery.mHiddenLeftFileCount +
        aDiscovery.mHiddenRightFileCount +
        aDiscovery.mHiddenLeftFolderCount +
        aDiscovery.mHiddenRightFolderCount;
    if (aVisibleHiddenCount > 0u) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Folder Compare][Discovery] Hidden entries are included in this comparison. left_files=" +
              std::to_string(aDiscovery.mHiddenLeftFileCount) +
              ", right_files=" +
              std::to_string(aDiscovery.mHiddenRightFileCount) +
              ", left_folders=" +
              std::to_string(aDiscovery.mHiddenLeftFolderCount) +
              ", right_folders=" +
              std::to_string(aDiscovery.mHiddenRightFolderCount) + ".");
    }
  }

  pContext.EmitPhaseProgress(1.0, ProgressStageLabelV2(ProgressStageV2::kDiscovery));
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kSanity, ProgressStageV2::kDiscovery));
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
