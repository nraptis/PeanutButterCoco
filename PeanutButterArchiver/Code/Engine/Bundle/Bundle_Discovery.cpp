#include "Bundle_Discovery.hpp"

#include <algorithm>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

bool BundleDiscoveryV2::Run(BundleStageContextV2& pContext) {
  BundleDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  aDiscovery = BundleDiscoveryStateV2{};
  const std::string& aSourcePath = pContext.Request().mSourceDirectory;
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

  std::vector<DirectoryEntryV2> aFiles;
  std::vector<DirectoryEntryV2> aDirectories;

  if (aSourceIsFile) {
    const std::string aRelativePath = pContext.FileSystem().FileName(aSourcePath);
    if (aRelativePath.empty() ||
        aRelativePath.size() > memory_layout::kMaxPathLengthV2) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Bundle discovery failed: source file path exceeds supported length.");
      return false;
    }

    aFiles.push_back({aSourcePath, aRelativePath, false});
  } else if (aSourceIsDirectory) {
    aFiles = pContext.FileSystem().ListFilesRecursive(aSourcePath);
    aDirectories = pContext.FileSystem().ListDirectoriesRecursive(aSourcePath);
  } else {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kDiscovery,
                                      "source path is not a readable file or folder"));
    return false;
  }

  const std::size_t aTotalItems = aFiles.size() + aDirectories.size();
  std::size_t aProcessedItems = 0u;

  for (const DirectoryEntryV2& aFile : aFiles) {
    if (aFile.mRelativePath.empty() ||
        aFile.mRelativePath.size() > memory_layout::kMaxPathLengthV2) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Bundle discovery failed: source file path exceeds supported length.");
      return false;
    }
    std::unique_ptr<FileReadStreamV2> aRead =
        pContext.FileSystem().OpenReadStream(aFile.mPath);
    if (aRead == nullptr || !aRead->IsReady()) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Bundle discovery failed: source file stream could not be opened.");
      return false;
    }

    BundleRecordEntryV2 aRecord;
    aRecord.mSourcePath = aFile.mPath;
    aRecord.mRelativePath = aFile.mRelativePath;
    aRecord.mContentLength = static_cast<std::uint64_t>(aRead->GetLength());
    aRecord.mIsDirectory = false;
    aDiscovery.mFileRecords.push_back(std::move(aRecord));
    ++aDiscovery.mFileCount;
    aDiscovery.mTotalSourceBytes +=
        static_cast<std::uint64_t>(aRead->GetLength());

    ++aProcessedItems;
    pContext.EmitPhaseProgress(
        aTotalItems == 0u ? 1.0
                          : static_cast<double>(aProcessedItems) /
                                static_cast<double>(aTotalItems),
        "Discovering source files");
    if (pContext.IsCancelRequested()) {
      return false;
    }
  }

  for (const DirectoryEntryV2& aDirectory : aDirectories) {
    if (aDirectory.mRelativePath.empty() ||
        aDirectory.mRelativePath.size() > memory_layout::kMaxPathLengthV2) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Bundle discovery failed: source folder path exceeds supported length.");
      return false;
    }
    if (!pContext.FileSystem().DirectoryHasEntries(aDirectory.mPath)) {
      BundleRecordEntryV2 aFolderRecord;
      aFolderRecord.mSourcePath = aDirectory.mPath;
      aFolderRecord.mRelativePath = aDirectory.mRelativePath;
      aFolderRecord.mContentLength = 0u;
      aFolderRecord.mIsDirectory = true;
      aDiscovery.mEmptyFolderRecords.push_back(std::move(aFolderRecord));
      ++aDiscovery.mEmptyFolderCount;
    }

    ++aProcessedItems;
    pContext.EmitPhaseProgress(
        aTotalItems == 0u ? 1.0
                          : static_cast<double>(aProcessedItems) /
                                static_cast<double>(aTotalItems),
        "Discovering source folders");
    if (pContext.IsCancelRequested()) {
      return false;
    }
  }

  std::sort(aDiscovery.mFileRecords.begin(),
            aDiscovery.mFileRecords.end(),
            [](const BundleRecordEntryV2& pLeft, const BundleRecordEntryV2& pRight) {
              return pLeft.mRelativePath < pRight.mRelativePath;
            });
  std::sort(aDiscovery.mEmptyFolderRecords.begin(),
            aDiscovery.mEmptyFolderRecords.end(),
            [](const BundleRecordEntryV2& pLeft, const BundleRecordEntryV2& pRight) {
              return pLeft.mRelativePath < pRight.mRelativePath;
            });

  pContext.EmitLog(LogLevelV2::kInfo,
                   LogBundleDiscoverySummaryV2(aDiscovery.mFileCount,
                                               aDiscovery.mEmptyFolderCount));
  pContext.EmitPhaseProgress(1.0, "Discovery complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
