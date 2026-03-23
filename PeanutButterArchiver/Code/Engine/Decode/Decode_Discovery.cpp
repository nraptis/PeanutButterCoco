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

bool DecodeDiscoveryV2::Run(DecodeStageContextV2& pContext) {
  DecodeDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;
  aDiscovery = DecodeDiscoveryStateV2{};
  const std::uint64_t aArchiveBlockBytes =
      static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes);

  const std::vector<DirectoryEntryV2> aFiles =
      pContext.FileSystem().ListFiles(pContext.State().mBootstrap.mSourceDirectory);
  std::size_t aProcessedFiles = 0u;
  for (const DirectoryEntryV2& aFile : aFiles) {
    if (!HasArchiveSuffix(aFile.mPath)) {
      continue;
    }

    std::uint32_t aFilenameIndex = 0u;
    const bool aMatchesTemplate = MatchesBootstrapTemplate(
        pContext.State().mBootstrap.mBootstrapArchivePath,
        pContext.FileSystem(),
        aFile.mPath,
        aFilenameIndex);

    memory_layout::ArchiveHeaderV2 aHeader;
    std::uint64_t aFileLength = 0u;
    const bool aHeaderReadable =
        ReadArchiveHeaderFromPath(pContext.FileSystem(), aFile.mPath, aHeader, aFileLength);
    if (!aHeaderReadable && !aMatchesTemplate) {
      ++aProcessedFiles;
      continue;
    }
    if (aHeaderReadable &&
        aHeader.mArchiveFamilyId !=
            pContext.State().mBootstrap.mFirstHeader.mArchiveFamilyId) {
      ++aProcessedFiles;
      continue;
    }

    DiscoveredArchiveFileV2 aDiscovered;
    aDiscovered.mPath = aFile.mPath;
    aDiscovered.mFileLength = aFileLength;
    aDiscovered.mFilenameIndex = static_cast<std::uint64_t>(aFilenameIndex);
    aDiscovered.mHasReadableHeader = aHeaderReadable;
    aDiscovered.mHeader = aHeader;
    if (aHeaderReadable && aFileLength >= memory_layout::kArchiveHeaderBytesV2) {
      aDiscovered.mReadableBlockCount =
          (aFileLength - memory_layout::kArchiveHeaderBytesV2) /
          aArchiveBlockBytes;
      aDiscovered.mHeaderIndex =
          memory_layout::PackedUint48ToUInt64(aHeader.mArchiveIndex);
    }
    aDiscovered.mArchiveIndex =
        aMatchesTemplate ? static_cast<std::uint64_t>(aFilenameIndex)
                         : aDiscovered.mHeaderIndex;
    aDiscovered.mArchiveBlockCount = aDiscovered.mReadableBlockCount;

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
              if (pLeft.mArchiveIndex != pRight.mArchiveIndex) {
                return pLeft.mArchiveIndex < pRight.mArchiveIndex;
              }
              return pLeft.mPath < pRight.mPath;
            });

  for (const DiscoveredArchiveFileV2& aArchive : aDiscovery.mArchives) {
    aDiscovery.mTotalReadableBlocks += aArchive.mReadableBlockCount;
  }

  pContext.EmitLog(
      LogLevelV2::kInfo,
      LogDecodeDiscoverySummaryV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                  aDiscovery.mArchives.size(),
                                  aDiscovery.mTotalReadableBlocks));
  pContext.EmitPhaseProgress(1.0, "Discovery complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
