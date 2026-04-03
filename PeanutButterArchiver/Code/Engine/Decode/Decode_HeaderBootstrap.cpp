#include "Decode_HeaderBootstrap.hpp"

#include <algorithm>
#include <array>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

class DecodeHeaderBootstrapCursorV2 {
 public:
  bool mSourceIsDirectory = false;
  std::vector<DirectoryEntryV2> mEntries;
  std::size_t mEntryIndex = 0u;
  bool mBootstrapPathResolved = false;
  std::string mBootstrapPath;
};

namespace {

bool HasArchiveSuffix(const std::string& pName) {
  const std::string aSuffix = memory_layout::kArchiveFileSuffixV2;
  if (pName.size() < aSuffix.size()) {
    return false;
  }
  return pName.compare(pName.size() - aSuffix.size(), aSuffix.size(), aSuffix) == 0;
}

bool ReadArchiveHeaderFromPath(FileSystemV2& pFileSystem,
                               const std::string& pPath,
                               memory_layout::ArchiveHeaderV2& pOutHeader,
                               std::uint64_t& pOutFileLength) {
  std::unique_ptr<FileReadStreamV2> aRead = pFileSystem.OpenReadStream(pPath);
  if (aRead == nullptr || !aRead->IsReady()) {
    return false;
  }
  if (aRead->GetLength() < memory_layout::kArchiveHeaderBytesV2) {
    return false;
  }

  std::array<unsigned char, memory_layout::kArchiveHeaderBytesV2> aHeaderBytes{};
  if (!aRead->Read(0u, aHeaderBytes.data(), aHeaderBytes.size())) {
    return false;
  }
  if (!memory_layout::ReadArchiveHeader(aHeaderBytes.data(),
                                        aHeaderBytes.size(),
                                        pOutHeader,
                                        nullptr)) {
    return false;
  }

  pOutFileLength = static_cast<std::uint64_t>(aRead->GetLength());
  return true;
}

void EmitDecodeArchiveHeaderEvent(DecodeStageContextV2& pContext,
                                  const std::string& pArchivePath,
                                  const memory_layout::ArchiveHeaderV2& pHeader,
                                  std::uint64_t pFileLength) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeArchiveHeaderRead;
  aEvent.mStage = ProgressStageV2::kHeaderBootstrap;
  aEvent.mLabel = "Decode bootstrap read archive header " + pArchivePath;
  aEvent.SetInfo("archive_path", pArchivePath);
  aEvent.SetInfo("archive_index", memory_layout::PackedUint48ToUInt64(pHeader.mArchiveIndex));
  aEvent.SetInfo("archive_count", memory_layout::PackedUint48ToUInt64(pHeader.mArchiveCount));
  aEvent.SetInfo("archive_data_block_count",
                 memory_layout::PackedUint48ToUInt64(pHeader.mArchiveDataBlockCount));
  const std::uint64_t aLegacyEmptyFolderBlocks =
      memory_layout::PackedUint48ToUInt64(pHeader.mEmptyFolderBlockCount);
  if (aLegacyEmptyFolderBlocks > 0u) {
    aEvent.SetInfo("legacy_empty_folder_block_count", aLegacyEmptyFolderBlocks);
  }
  aEvent.SetInfo("preview_manifest_block_count",
                 memory_layout::PackedUint48ToUInt64(pHeader.mPreviewManifestBlockCount));
  aEvent.SetInfo("repair_block_count",
                 memory_layout::PackedUint48ToUInt64(pHeader.mRepairSectorBlockCount));
  aEvent.SetInfo("archive_family_id", pHeader.mArchiveFamilyId);
  aEvent.SetInfo("file_length", pFileLength);
  aEvent.SetInfo("is_encrypted", pHeader.mIsEncrypted != 0u);
  pContext.EmitRuntimeEvent(aEvent);
}

void SortDirectoryEntries(std::vector<DirectoryEntryV2>& pEntries) {
  std::sort(pEntries.begin(),
            pEntries.end(),
            [](const DirectoryEntryV2& pLeft, const DirectoryEntryV2& pRight) {
              return pLeft.mPath < pRight.mPath;
            });
}

void EmitBootstrapProgress(DecodeStageContextV2& pContext,
                           std::size_t pScannedEntries,
                           std::size_t pTotalEntries,
                           const std::string& pLabel) {
  const double aFraction =
      pTotalEntries == 0u ? 0.0
                          : static_cast<double>(pScannedEntries) /
                                static_cast<double>(pTotalEntries);
  pContext.EmitPhaseProgress(std::max(0.0, std::min(1.0, aFraction)), pLabel);
}

bool FailHeaderBootstrap(DecodeStageContextV2& pContext,
                         const std::string& pReason) {
  pContext.State().mCursor.mHeaderBootstrap.reset();
  pContext.EmitLog(LogLevelV2::kError,
                   LogPhaseFailedV2(
                       LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                       ProgressStageV2::kHeaderBootstrap,
                       pReason));
  return false;
}

}  // namespace

bool DecodeHeaderBootstrapV2::Run(DecodeStageContextV2& pContext) {
  DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  std::shared_ptr<DecodeHeaderBootstrapCursorV2>& aCursorPtr =
      pContext.State().mCursor.mHeaderBootstrap;
  if (!aCursorPtr) {
    aBootstrap = DecodeBootstrapStateV2{};
    aCursorPtr = std::make_shared<DecodeHeaderBootstrapCursorV2>();
    aCursorPtr->mSourceIsDirectory =
        pContext.FileSystem().IsDirectory(pContext.Request().mSourcePath);
    if (aCursorPtr->mSourceIsDirectory) {
      aBootstrap.mSourceDirectory = pContext.Request().mSourcePath;
      aCursorPtr->mEntries =
          pContext.FileSystem().ListDirectoryEntries(aBootstrap.mSourceDirectory);
      SortDirectoryEntries(aCursorPtr->mEntries);
      if (!aCursorPtr->mEntries.empty()) {
        EmitBootstrapProgress(pContext,
                              0u,
                              aCursorPtr->mEntries.size(),
                              "Scanning source folder for archives");
      }
    } else {
      aCursorPtr->mBootstrapPath = pContext.Request().mSourcePath;
      aCursorPtr->mBootstrapPathResolved = true;
      aBootstrap.mSourceDirectory =
          pContext.FileSystem().ParentPath(pContext.Request().mSourcePath);
    }
  }

  DecodeHeaderBootstrapCursorV2& aCursor = *aCursorPtr;
  if (!aCursor.mBootstrapPathResolved && aCursor.mSourceIsDirectory) {
    if (aCursor.mEntryIndex >= aCursor.mEntries.size()) {
      return FailHeaderBootstrap(pContext, "no archive file was found");
    }

    const DirectoryEntryV2& aEntry = aCursor.mEntries[aCursor.mEntryIndex];
    ++aCursor.mEntryIndex;
    if (!aEntry.mIsDirectory && HasArchiveSuffix(aEntry.mPath)) {
      aCursor.mBootstrapPath = aEntry.mPath;
      aCursor.mBootstrapPathResolved = true;
    }

    EmitBootstrapProgress(
        pContext,
        aCursor.mEntryIndex,
        aCursor.mEntries.size(),
        aCursor.mBootstrapPathResolved
            ? "Bootstrap archive selected"
            : "Scanning source folder for archives");
    if (!aCursor.mBootstrapPathResolved &&
        aCursor.mEntryIndex >= aCursor.mEntries.size()) {
      return FailHeaderBootstrap(pContext, "no archive file was found");
    }
    pContext.ContinuePhaseOnNextHeartbeat();
    return true;
  }

  if (aCursor.mBootstrapPath.empty()) {
    return FailHeaderBootstrap(pContext, "no archive file was found");
  }

  std::uint64_t aFileLength = 0u;
  if (!ReadArchiveHeaderFromPath(pContext.FileSystem(),
                                 aCursor.mBootstrapPath,
                                 aBootstrap.mFirstHeader,
                                 aFileLength)) {
    return FailHeaderBootstrap(pContext,
                               "first archive header could not be read");
  }
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeArchiveHeaderRead)) {
    EmitDecodeArchiveHeaderEvent(
        pContext, aCursor.mBootstrapPath, aBootstrap.mFirstHeader, aFileLength);
  }

  aBootstrap.mBootstrapArchivePath = aCursor.mBootstrapPath;
  aBootstrap.mExpectedArchiveCount =
      memory_layout::PackedUint48ToUInt64(aBootstrap.mFirstHeader.mArchiveCount);
  aBootstrap.mExpectedEmptyFolderBlockCount =
      memory_layout::PackedUint48ToUInt64(
          aBootstrap.mFirstHeader.mEmptyFolderBlockCount);
  aBootstrap.mExpectedPreviewManifestBlockCount =
      memory_layout::PackedUint48ToUInt64(
          aBootstrap.mFirstHeader.mPreviewManifestBlockCount);
  aBootstrap.mExpectedArchiveDataBlockCount =
      memory_layout::PackedUint48ToUInt64(
          aBootstrap.mFirstHeader.mArchiveDataBlockCount);
  aBootstrap.mExpectedRepairBlockCount =
      memory_layout::PackedUint48ToUInt64(
          aBootstrap.mFirstHeader.mRepairSectorBlockCount);
  aBootstrap.mHeaderRead = true;
  const std::string aBootstrapPath = aCursor.mBootstrapPath;
  pContext.State().mCursor.mHeaderBootstrap.reset();

  pContext.EmitLog(
      LogLevelV2::kInfo,
      LogDecodeBootstrapSummaryV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                  aBootstrap.mFirstHeader.mArchiveFamilyId,
                                  aBootstrap.mSourceDirectory,
                                  aBootstrapPath));
  pContext.EmitPhaseProgress(1.0, "Header bootstrap complete");
  (void)aFileLength;
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
