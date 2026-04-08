#include "Decode_Discovery.hpp"

#include <algorithm>
#include <array>
#include <memory>

#include "../../Common/LogCatalog.hpp"
#include "../../Knobs.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

class DecodeDiscoveryCursorV2 {
 public:
  std::vector<DirectoryEntryV2> mFiles;
  std::size_t mFileIndex = 0u;
  std::uint64_t mArchiveFilesScanned = 0u;
  std::uint64_t mNonArchiveFilesScanned = 0u;
  std::uint64_t mNextProgressLogAt =
      static_cast<std::uint64_t>(knobs::kDecodeDiscoveryProgressItemIntervalV2);
};

namespace {

bool HasArchiveSuffix(const std::string& pName) {
  const std::string aSuffix = memory_layout::kArchiveFileSuffixV2;
  if (pName.size() < aSuffix.size()) {
    return false;
  }
  return pName.compare(pName.size() - aSuffix.size(), aSuffix.size(), aSuffix) == 0;
}

bool MatchesBootstrapTemplate(const std::string& pBootstrapPath,
                              FileSystemV2& pFileSystem,
                              const std::string& pCandidatePath,
                              std::uint32_t& pOutFilenameIndex) {
  pOutFilenameIndex = 0u;

  std::string aBootstrapPrefix;
  std::string aBootstrapSuffix;
  std::size_t aBootstrapDigits = 0u;
  std::uint32_t aBootstrapIndex = 0u;
  if (!memory_layout::ParseArchiveFileTemplateV2(
          pFileSystem.FileName(pBootstrapPath),
          aBootstrapPrefix,
          aBootstrapIndex,
          aBootstrapSuffix,
          aBootstrapDigits)) {
    return false;
  }

  std::string aCandidatePrefix;
  std::string aCandidateSuffix;
  std::size_t aCandidateDigits = 0u;
  if (!memory_layout::ParseArchiveFileTemplateV2(
          pFileSystem.FileName(pCandidatePath),
          aCandidatePrefix,
          pOutFilenameIndex,
          aCandidateSuffix,
          aCandidateDigits)) {
    return false;
  }

  return aBootstrapPrefix == aCandidatePrefix &&
         aBootstrapSuffix == aCandidateSuffix &&
         aBootstrapDigits == aCandidateDigits;
}

bool ReadArchiveHeaderFromPath(FileSystemV2& pFileSystem,
                               const std::string& pPath,
                               memory_layout::ArchiveHeaderV2& pOutHeader,
                               std::uint64_t& pOutFileLength) {
  pOutFileLength = 0u;
  std::unique_ptr<FileReadStreamV2> aRead = pFileSystem.OpenReadStream(pPath);
  if (aRead == nullptr || !aRead->IsReady()) {
    return false;
  }
  pOutFileLength = static_cast<std::uint64_t>(aRead->GetLength());
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
  return true;
}

void EmitDecodeDiscoveryArchiveEvent(DecodeStageContextV2& pContext,
                                     const std::string& pArchivePath,
                                     std::uint32_t pFilenameIndex,
                                     bool pMatchesTemplate,
                                     bool pHeaderReadable,
                                     const memory_layout::ArchiveHeaderV2& pHeader,
                                     std::uint64_t pFileLength,
                                     std::size_t pProcessedFiles,
                                     std::size_t pTotalFiles) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeDiscoveryArchiveScanned;
  aEvent.mStage = ProgressStageV2::kDiscovery;
  aEvent.mLabel = "Decode discovery scanned archive " + pArchivePath;
  aEvent.SetInfo("archive_path", pArchivePath);
  aEvent.SetInfo("filename_index", static_cast<std::uint64_t>(pFilenameIndex));
  aEvent.SetInfo("matched_template", pMatchesTemplate);
  aEvent.SetInfo("header_readable", pHeaderReadable);
  aEvent.SetInfo("file_length", pFileLength);
  aEvent.SetInfo("scan_index", static_cast<std::uint64_t>(pProcessedFiles));
  aEvent.SetInfo("scan_total", static_cast<std::uint64_t>(pTotalFiles));
  if (pHeaderReadable) {
    aEvent.SetInfo("archive_index", memory_layout::PackedUint48ToUInt64(pHeader.mArchiveIndex));
    aEvent.SetInfo("archive_count", memory_layout::PackedUint48ToUInt64(pHeader.mArchiveCount));
    aEvent.SetInfo("archive_family_id", pHeader.mArchiveFamilyId);
    aEvent.SetInfo("is_encrypted", pHeader.mIsEncrypted != 0u);
  }
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitDecodeArchiveHeaderEvent(DecodeStageContextV2& pContext,
                                  const std::string& pArchivePath,
                                  const memory_layout::ArchiveHeaderV2& pHeader,
                                  std::uint64_t pFileLength) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeArchiveHeaderRead;
  aEvent.mStage = ProgressStageV2::kDiscovery;
  aEvent.mLabel = "Decode read archive header " + pArchivePath;
  aEvent.SetInfo("archive_path", pArchivePath);
  aEvent.SetInfo("archive_index", memory_layout::PackedUint48ToUInt64(pHeader.mArchiveIndex));
  aEvent.SetInfo("archive_count", memory_layout::PackedUint48ToUInt64(pHeader.mArchiveCount));
  aEvent.SetInfo("archive_data_block_count",
                 memory_layout::PackedUint48ToUInt64(pHeader.mBlockCountMain));
  const std::uint64_t aReservedCount0 =
      memory_layout::PackedUint48ToUInt64(pHeader.mReservedCount0);
  if (aReservedCount0 > 0u) {
    aEvent.SetInfo("reserved_count0", aReservedCount0);
  }
  aEvent.SetInfo("preview_manifest_block_count",
                 memory_layout::PackedUint48ToUInt64(pHeader.mBlockCountPreview));
  aEvent.SetInfo("repair_block_count",
                 memory_layout::PackedUint48ToUInt64(pHeader.mBlockCountRepair));
  aEvent.SetInfo("archive_family_id", pHeader.mArchiveFamilyId);
  aEvent.SetInfo("file_length", pFileLength);
  aEvent.SetInfo("is_encrypted", pHeader.mIsEncrypted != 0u);
  pContext.EmitRuntimeEvent(aEvent);
}

bool FinalizeDecodeDiscovery(DecodeStageContextV2& pContext) {
  DecodeDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  if (aDiscovery.mArchives.empty()) {
    pContext.State().mCursor.mDiscovery.reset();
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kDiscovery,
                                      "no readable archives matched the family"));
    return false;
  }

  std::sort(aDiscovery.mArchives.begin(),
            aDiscovery.mArchives.end(),
            [](const DiscoveredArchiveFileV2& pLeft,
               const DiscoveredArchiveFileV2& pRight) {
              if (pLeft.mArchiveIndex != pRight.mArchiveIndex) {
                return pLeft.mArchiveIndex < pRight.mArchiveIndex;
              }
              return pLeft.mPath < pRight.mPath;
            });

  aDiscovery.mTotalReadableBlocks = 0u;
  for (const DiscoveredArchiveFileV2& aArchive : aDiscovery.mArchives) {
    aDiscovery.mTotalReadableBlocks += aArchive.mReadableBlockCount;
  }

  pContext.State().mCursor.mDiscovery.reset();
  pContext.EmitLog(
      LogLevelV2::kInfo,
      LogDecodeDiscoverySummaryV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                  aDiscovery.mArchives.size(),
                                  aDiscovery.mTotalReadableBlocks));
  pContext.EmitPhaseProgress(1.0, "Discovery complete");
  return !pContext.IsCancelRequested();
}

}  // namespace

bool DecodeDiscoveryV2::Run(DecodeStageContextV2& pContext) {
  DecodeDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  std::shared_ptr<DecodeDiscoveryCursorV2>& aCursor =
      pContext.State().mCursor.mDiscovery;
  const std::uint64_t aArchiveBlockBytes =
      static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes);

  if (!aCursor) {
    aDiscovery = DecodeDiscoveryStateV2{};
    aCursor = std::make_shared<DecodeDiscoveryCursorV2>();
    aCursor->mFiles =
        pContext.FileSystem().ListFiles(pContext.State().mBootstrap.mSourceDirectory);
    if (aCursor->mFiles.empty()) {
      return FinalizeDecodeDiscovery(pContext);
    }
  }

  const DirectoryEntryV2& aFile = aCursor->mFiles[aCursor->mFileIndex];
  const bool aIsArchiveFile = HasArchiveSuffix(aFile.mPath);
  std::uint32_t aFilenameIndex = 0u;
  const bool aMatchesTemplate = aIsArchiveFile &&
                                MatchesBootstrapTemplate(
                                    pContext.State().mBootstrap.mBootstrapArchivePath,
                                    pContext.FileSystem(),
                                    aFile.mPath,
                                    aFilenameIndex);

  memory_layout::ArchiveHeaderV2 aHeader{};
  std::uint64_t aFileLength = 0u;
  const bool aHeaderReadable =
      aIsArchiveFile &&
      ReadArchiveHeaderFromPath(pContext.FileSystem(), aFile.mPath, aHeader, aFileLength);
  if (aHeaderReadable &&
      pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeArchiveHeaderRead)) {
    EmitDecodeArchiveHeaderEvent(pContext, aFile.mPath, aHeader, aFileLength);
  }

  if (aIsArchiveFile &&
      (aHeaderReadable || aMatchesTemplate) &&
      (!aHeaderReadable ||
       aHeader.mArchiveFamilyId ==
           pContext.State().mBootstrap.mFirstHeader.mArchiveFamilyId)) {
    DiscoveredArchiveFileV2 aDiscovered;
    aDiscovered.mPath = aFile.mPath;
    aDiscovered.mFileLength = aFileLength;
    aDiscovered.mFilenameIndex = static_cast<std::uint64_t>(aFilenameIndex);
    aDiscovered.mHasReadableHeader = aHeaderReadable;
    aDiscovered.mHeader = aHeader;
    if (aFileLength >= memory_layout::kArchiveHeaderBytesV2) {
      aDiscovered.mReadableBlockCount =
          (aFileLength - memory_layout::kArchiveHeaderBytesV2) /
          aArchiveBlockBytes;
    }
    if (aHeaderReadable && aFileLength >= memory_layout::kArchiveHeaderBytesV2) {
      aDiscovered.mHeaderIndex =
          memory_layout::PackedUint48ToUInt64(aHeader.mArchiveIndex);
    }
    aDiscovered.mArchiveIndex =
        aMatchesTemplate ? static_cast<std::uint64_t>(aFilenameIndex)
                         : aDiscovered.mHeaderIndex;
    aDiscovered.mArchiveBlockCount = aDiscovered.mReadableBlockCount;
    aDiscovery.mArchives.push_back(std::move(aDiscovered));
  }

  if (aIsArchiveFile) {
    ++aCursor->mArchiveFilesScanned;
  } else {
    ++aCursor->mNonArchiveFilesScanned;
  }
  ++aCursor->mFileIndex;
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeDiscoveryArchiveScanned) &&
      aIsArchiveFile) {
    EmitDecodeDiscoveryArchiveEvent(pContext,
                                    aFile.mPath,
                                    aFilenameIndex,
                                    aMatchesTemplate,
                                    aHeaderReadable,
                                    aHeader,
                                    aFileLength,
                                    aCursor->mFileIndex,
                                    aCursor->mFiles.size());
  }
  if (aCursor->mFileIndex >= aCursor->mNextProgressLogAt) {
    pContext.EmitLog(
        LogLevelV2::kInfo,
        LogDecodeDiscoverySliceV2(
            LogActionFromDecodeIntentV2(pContext.Request().mIntent),
            aCursor->mArchiveFilesScanned,
            aCursor->mNonArchiveFilesScanned));
    const std::uint64_t aInterval =
        std::max<std::uint64_t>(1u, knobs::kDecodeDiscoveryProgressItemIntervalV2);
    aCursor->mNextProgressLogAt += aInterval;
  }
  pContext.EmitPhaseProgress(
      static_cast<double>(aCursor->mFileIndex) /
          static_cast<double>(std::max<std::size_t>(1u, aCursor->mFiles.size())),
      "Discovering archive family");
  if (pContext.IsCancelRequested()) {
    return false;
  }

  if (aCursor->mFileIndex < aCursor->mFiles.size()) {
    pContext.ContinuePhaseOnNextHeartbeat();
    return true;
  }

  return FinalizeDecodeDiscovery(pContext);
}

}  // namespace peanutbutter
