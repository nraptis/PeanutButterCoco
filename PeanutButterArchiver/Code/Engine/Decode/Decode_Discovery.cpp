#include "Decode_Discovery.hpp"

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

void SwitchToPessimistic(DecodeStageContextV2& pContext,
                         const std::string& pReason) {
  if (pContext.State().mDiscovery.mMode == DecodeModeV2::kPessimistic) {
    return;
  }
  pContext.State().mDiscovery.mMode = DecodeModeV2::kPessimistic;
  pContext.EmitLog(LogLevelV2::kWarning,
                   LogPessimisticSwitchV2(
                       LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                       pReason));
}

}  // namespace

bool DecodeDiscoveryV2::Run(DecodeStageContextV2& pContext) {
  DecodeDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  aDiscovery = DecodeDiscoveryStateV2{};

  const std::vector<DirectoryEntryV2> aFiles =
      pContext.FileSystem().ListFiles(pContext.State().mBootstrap.mSourceDirectory);
  std::size_t aProcessedFiles = 0u;
  for (const DirectoryEntryV2& aFile : aFiles) {
    if (!HasArchiveSuffix(aFile.mPath)) {
      continue;
    }

    memory_layout::ArchiveHeaderV2 aHeader;
    std::uint64_t aFileLength = 0u;
    if (!ReadArchiveHeaderFromPath(pContext.FileSystem(), aFile.mPath, aHeader, aFileLength)) {
      ++aProcessedFiles;
      continue;
    }
    if (aHeader.mArchiveFamilyId !=
        pContext.State().mBootstrap.mFirstHeader.mArchiveFamilyId) {
      ++aProcessedFiles;
      continue;
    }

    DiscoveredArchiveFileV2 aDiscovered;
    aDiscovered.mPath = aFile.mPath;
    aDiscovered.mFileLength = aFileLength;
    aDiscovered.mHeader = aHeader;
    if (aFileLength >= memory_layout::kArchiveHeaderBytesV2) {
      aDiscovered.mReadableBlockCount =
          (aFileLength - memory_layout::kArchiveHeaderBytesV2) /
          memory_layout::kArchiveBlockBytesV2;
    }

    aDiscovery.mArchives.push_back(std::move(aDiscovered));
    ++aProcessedFiles;
    pContext.EmitPhaseProgress(
        aFiles.empty()
            ? 1.0
            : static_cast<double>(aProcessedFiles) /
                  static_cast<double>(aFiles.size()),
        "Discovering archive family");
    if (pContext.IsCancelRequested()) {
      return false;
    }
  }

  if (aDiscovery.mArchives.empty()) {
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
              const std::uint64_t aLeftIndex =
                  memory_layout::PackedUint48ToUInt64(pLeft.mHeader.mArchiveIndex);
              const std::uint64_t aRightIndex =
                  memory_layout::PackedUint48ToUInt64(pRight.mHeader.mArchiveIndex);
              if (aLeftIndex != aRightIndex) {
                return aLeftIndex < aRightIndex;
              }
              return pLeft.mPath < pRight.mPath;
            });

  const memory_layout::ArchiveHeaderV2& aFirstHeader =
      pContext.State().mBootstrap.mFirstHeader;
  std::uint64_t aExpectedIndex = 0u;
  std::uint64_t aLastSeenIndex = static_cast<std::uint64_t>(-1);
  for (const DiscoveredArchiveFileV2& aArchive : aDiscovery.mArchives) {
    aDiscovery.mTotalReadableBlocks += aArchive.mReadableBlockCount;

    if (memory_layout::PackedUint48ToUInt64(aArchive.mHeader.mArchiveCount) !=
            pContext.State().mBootstrap.mExpectedArchiveCount ||
        memory_layout::PackedUint48ToUInt64(aArchive.mHeader.mEmptyFolderBlockCount) !=
            pContext.State().mBootstrap.mExpectedEmptyFolderBlockCount ||
        memory_layout::PackedUint48ToUInt64(aArchive.mHeader.mPreviewManifestBlockCount) !=
            pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount ||
        memory_layout::PackedUint48ToUInt64(aArchive.mHeader.mArchiveDataBlockCount) !=
            pContext.State().mBootstrap.mExpectedArchiveDataBlockCount ||
        memory_layout::PackedUint48ToUInt64(aArchive.mHeader.mRepairSectorBlockCount) !=
            pContext.State().mBootstrap.mExpectedRepairBlockCount ||
        aArchive.mHeader.mIsEncrypted != aFirstHeader.mIsEncrypted) {
      SwitchToPessimistic(pContext, "a later archive header disagreed with the bootstrap counts.");
    }

    const std::uint64_t aArchiveIndex =
        memory_layout::PackedUint48ToUInt64(aArchive.mHeader.mArchiveIndex);
    if (aArchiveIndex == aLastSeenIndex) {
      SwitchToPessimistic(pContext, "duplicate archive indexes were discovered.");
    }
    if (aArchiveIndex != aExpectedIndex) {
      SwitchToPessimistic(pContext, "archive indexes were not contiguous.");
      aExpectedIndex = aArchiveIndex;
    }
    aLastSeenIndex = aArchiveIndex;
    ++aExpectedIndex;
  }

  if (aDiscovery.mArchives.size() !=
      static_cast<std::size_t>(pContext.State().mBootstrap.mExpectedArchiveCount)) {
    SwitchToPessimistic(pContext, "observed archive count did not match the header.");
  }

  pContext.EmitLog(
      LogLevelV2::kInfo,
      LogDecodeDiscoverySummaryV2(aDiscovery.mArchives.size(),
                                  aDiscovery.mTotalReadableBlocks));
  pContext.EmitPhaseProgress(1.0, "Discovery complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
