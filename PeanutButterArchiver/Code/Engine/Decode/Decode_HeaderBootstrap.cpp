#include "Decode_HeaderBootstrap.hpp"

#include <algorithm>
#include <array>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {
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

}  // namespace

bool DecodeHeaderBootstrapV2::Run(DecodeStageContextV2& pContext) {
  DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  aBootstrap = DecodeBootstrapStateV2{};

  const bool aSourceIsDirectory =
      pContext.FileSystem().IsDirectory(pContext.Request().mSourcePath);
  std::string aBootstrapPath;
  if (aSourceIsDirectory) {
    aBootstrap.mSourceDirectory = pContext.Request().mSourcePath;
    const std::vector<DirectoryEntryV2> aFiles =
        pContext.FileSystem().ListFiles(aBootstrap.mSourceDirectory);
    for (const DirectoryEntryV2& aFile : aFiles) {
      if (!HasArchiveSuffix(aFile.mPath)) {
        continue;
      }
      aBootstrapPath = aFile.mPath;
      break;
    }
  } else {
    aBootstrapPath = pContext.Request().mSourcePath;
    aBootstrap.mSourceDirectory =
        pContext.FileSystem().ParentPath(pContext.Request().mSourcePath);
  }

  if (aBootstrapPath.empty()) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kHeaderBootstrap,
                                      "no archive file was found"));
    return false;
  }

  std::uint64_t aFileLength = 0u;
  if (!ReadArchiveHeaderFromPath(pContext.FileSystem(),
                                 aBootstrapPath,
                                 aBootstrap.mFirstHeader,
                                 aFileLength)) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                      ProgressStageV2::kHeaderBootstrap,
                                      "first archive header could not be read"));
    return false;
  }

  aBootstrap.mBootstrapArchivePath = aBootstrapPath;
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
