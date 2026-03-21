#include "Sanity_Compare.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "../../Common/LogCatalog.hpp"

namespace peanutbutter {
namespace {

inline constexpr std::size_t kCompareChunkBytesV2 = 64u * 1024u;
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

bool CompareFiles(FileSystemV2& pFileSystem,
                  const SanityEntryV2& pLeft,
                  const SanityEntryV2& pRight,
                  ByteBufferV2& pLeftBuffer,
                  ByteBufferV2& pRightBuffer,
                  LoggingStatV2& pStat,
                  SanityStageContextV2& pContext) {
  std::unique_ptr<FileReadStreamV2> aLeftRead = pFileSystem.OpenReadStream(pLeft.mPath);
  std::unique_ptr<FileReadStreamV2> aRightRead = pFileSystem.OpenReadStream(pRight.mPath);
  if (aLeftRead == nullptr || aRightRead == nullptr ||
      !aLeftRead->IsReady() || !aRightRead->IsReady()) {
    return false;
  }
  if (aLeftRead->GetLength() != aRightRead->GetLength()) {
    return false;
  }

  const std::size_t aLength = aLeftRead->GetLength();
  std::size_t aOffset = 0u;
  while (aOffset < aLength) {
    if (pContext.IsCancelRequested()) {
      return false;
    }
    const std::size_t aChunk =
        std::min<std::size_t>(kCompareChunkBytesV2, aLength - aOffset);
    if (!aLeftRead->Read(aOffset, pLeftBuffer.Data(), aChunk) ||
        !aRightRead->Read(aOffset, pRightBuffer.Data(), aChunk)) {
      return false;
    }
    const bool aEqual =
        std::memcmp(pLeftBuffer.Data(), pRightBuffer.Data(), aChunk) == 0;
    if (!aEqual) {
      return false;
    }
    aOffset += aChunk;
    pStat.mBytesCompleted += static_cast<std::uint64_t>(aChunk);
    pContext.EmitLog(LogLevelV2::kInfo, LogSanityCompareSliceV2(pStat));
    if (pStat.mBytesTotal > 0u) {
      pContext.EmitPhaseProgress(
          static_cast<double>(pStat.mBytesCompleted) /
              static_cast<double>(pStat.mBytesTotal),
          ProgressStageLabelV2(ProgressStageV2::kCompare));
    }
  }
  return true;
}

}  // namespace

bool SanityCompareV2::Run(SanityStageContextV2& pContext) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseStartedV2(LogActionV2::kSanity, ProgressStageV2::kCompare));
  pContext.EmitLog(LogLevelV2::kInfo, LogSanityCompareStartV2());

  auto& aState = pContext.State().mCompare;
  aState = SanityCompareStateV2{};
  const std::string& aLeftRoot = pContext.Request().mLeftDirectory;
  const std::string& aRightRoot = pContext.Request().mRightDirectory;

  std::unordered_map<std::string, SanityEntryV2> aRightByRelative;
  for (const SanityEntryV2& aEntry : pContext.State().mDiscovery.mRightFiles) {
    aRightByRelative[aEntry.mRelativePath] = aEntry;
  }
  for (const SanityEntryV2& aEntry : pContext.State().mDiscovery.mLeftFiles) {
    aState.mStat.mFilesTotal += 1u;
    aState.mStat.mBytesTotal += aEntry.mLength;
  }
  aState.mStat.mFoldersTotal =
      static_cast<std::uint64_t>(pContext.State().mDiscovery.mLeftFolders.size());

  ByteBufferV2 aLeftBuffer(kCompareChunkBytesV2);
  ByteBufferV2 aRightBuffer(kCompareChunkBytesV2);
  if (aLeftBuffer.Empty() || aRightBuffer.Empty()) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kSanity,
                                      ProgressStageV2::kCompare,
                                      "could not allocate compare buffers"));
    return false;
  }

  for (const SanityEntryV2& aEntry : pContext.State().mDiscovery.mLeftFolders) {
    (void)aEntry;
    ++aState.mStat.mFoldersCompleted;
  }
  for (const SanityEntryV2& aEntry : pContext.State().mDiscovery.mRightFolders) {
    (void)aEntry;
  }

  for (const SanityEntryV2& aLeftEntry : pContext.State().mDiscovery.mLeftFiles) {
    if (pContext.IsCancelRequested()) {
      return false;
    }
    auto aRightIt = aRightByRelative.find(aLeftEntry.mRelativePath);
    if (aRightIt == aRightByRelative.end()) {
      if (IsHiddenRelativePath(aLeftEntry.mRelativePath)) {
        aState.mHiddenFilesMissingFromRight.push_back(aLeftEntry.mRelativePath);
        ++aState.mHiddenMismatchCount;
      } else {
        aState.mNormalFilesMissingFromRight.push_back(aLeftEntry.mRelativePath);
        ++aState.mNormalMismatchCount;
      }
      ++aState.mMismatchCount;
      ++aState.mStat.mFilesCompleted;
      pContext.EmitLog(LogLevelV2::kWarning, LogSanityCompareSliceV2(aState.mStat));
      continue;
    }
    if (!CompareFiles(pContext.FileSystem(),
                      aLeftEntry,
                      aRightIt->second,
                      aLeftBuffer,
                      aRightBuffer,
                      aState.mStat,
                      pContext)) {
      if (IsHiddenRelativePath(aLeftEntry.mRelativePath)) {
        aState.mHiddenInequalFiles.push_back(aLeftEntry.mRelativePath);
        ++aState.mHiddenMismatchCount;
      } else {
        aState.mNormalInequalFiles.push_back(aLeftEntry.mRelativePath);
        ++aState.mNormalMismatchCount;
      }
      ++aState.mMismatchCount;
    }
    ++aState.mStat.mFilesCompleted;
  }

  std::unordered_map<std::string, SanityEntryV2> aLeftByRelative;
  for (const SanityEntryV2& aEntry : pContext.State().mDiscovery.mLeftFiles) {
    aLeftByRelative[aEntry.mRelativePath] = aEntry;
  }
  for (const SanityEntryV2& aRightEntry : pContext.State().mDiscovery.mRightFiles) {
    if (aLeftByRelative.find(aRightEntry.mRelativePath) != aLeftByRelative.end()) {
      continue;
    }
    if (IsHiddenRelativePath(aRightEntry.mRelativePath)) {
      aState.mHiddenFilesMissingFromLeft.push_back(aRightEntry.mRelativePath);
      ++aState.mHiddenMismatchCount;
    } else {
      aState.mNormalFilesMissingFromLeft.push_back(aRightEntry.mRelativePath);
      ++aState.mNormalMismatchCount;
    }
    ++aState.mMismatchCount;
  }

  std::unordered_map<std::string, SanityEntryV2> aRightFoldersByRelative;
  for (const SanityEntryV2& aEntry : pContext.State().mDiscovery.mRightFolders) {
    aRightFoldersByRelative[aEntry.mRelativePath] = aEntry;
  }
  for (const SanityEntryV2& aLeftFolder : pContext.State().mDiscovery.mLeftFolders) {
    ++aState.mStat.mFoldersCompleted;
    if (aRightFoldersByRelative.find(aLeftFolder.mRelativePath) != aRightFoldersByRelative.end()) {
      continue;
    }
    if (IsHiddenRelativePath(aLeftFolder.mRelativePath)) {
      aState.mHiddenFoldersMissingFromRight.push_back(aLeftFolder.mRelativePath);
      ++aState.mHiddenMismatchCount;
    } else {
      aState.mNormalFoldersMissingFromRight.push_back(aLeftFolder.mRelativePath);
      ++aState.mNormalMismatchCount;
    }
    ++aState.mMismatchCount;
  }

  std::unordered_map<std::string, SanityEntryV2> aLeftFoldersByRelative;
  for (const SanityEntryV2& aEntry : pContext.State().mDiscovery.mLeftFolders) {
    aLeftFoldersByRelative[aEntry.mRelativePath] = aEntry;
  }
  for (const SanityEntryV2& aRightFolder : pContext.State().mDiscovery.mRightFolders) {
    if (aLeftFoldersByRelative.find(aRightFolder.mRelativePath) != aLeftFoldersByRelative.end()) {
      continue;
    }
    if (IsHiddenRelativePath(aRightFolder.mRelativePath)) {
      aState.mHiddenFoldersMissingFromLeft.push_back(aRightFolder.mRelativePath);
      ++aState.mHiddenMismatchCount;
    } else {
      aState.mNormalFoldersMissingFromLeft.push_back(aRightFolder.mRelativePath);
      ++aState.mNormalMismatchCount;
    }
    ++aState.mMismatchCount;
  }

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

}  // namespace peanutbutter
