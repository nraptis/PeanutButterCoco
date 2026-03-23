#include "Decode_ManifestDiscovery.hpp"

#include <array>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "../MemoryLayout/Primatives.hpp"

namespace peanutbutter {
namespace {

using namespace memory_layout;

bool TryReadValidatedInspectionHeader(DecodeStageContextV2& pContext,
                                      const DiscoveredArchiveFileV2& pArchive,
                                      std::uint64_t pBlockIndex,
                                      SectionHeaderV2& pOutHeader) {
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (!pArchive.mIsPresent || pBlockIndex >= pArchive.mReadableBlockCount) {
    return false;
  }

  ByteBufferV2 aEncryptedBlock(aArchiveBlockBytes);
  ByteBufferV2 aPlainBlock(aArchiveBlockBytes);
  if (aEncryptedBlock.Empty() || aPlainBlock.Empty()) {
    return false;
  }

  const std::size_t aOffset = static_cast<std::size_t>(
      kArchiveHeaderBytesV2 + (pBlockIndex * static_cast<std::uint64_t>(aArchiveBlockBytes)));
  std::unique_ptr<FileReadStreamV2> aRead =
      pContext.FileSystem().OpenReadStream(pArchive.mPath);
  if (aRead == nullptr || !aRead->IsReady() ||
      !aRead->Read(aOffset, aEncryptedBlock.Data(), aArchiveBlockBytes)) {
    return false;
  }

  if (pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u) {
    std::memcpy(aPlainBlock.Data(), aEncryptedBlock.Data(), aArchiveBlockBytes);
    if (ReadSectionHeader(aPlainBlock.Data(), kSectionHeaderBytesV2, pOutHeader, nullptr) &&
        ValidateSectionCheckSum(
            pOutHeader, aPlainBlock.Data() + kSectionHeaderBytesV2, aSectionPayloadBytes) &&
        pOutHeader.mSectionType ==
            static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest)) {
      return true;
    }

    std::string aUnsealError;
    if (!pContext.State().mCipher.mCipher.Unseal(aEncryptedBlock.Data(),
                                                 aPlainBlock.Data(),
                                                 aArchiveBlockBytes,
                                                 &aUnsealError)) {
      return false;
    }
  } else {
    std::memcpy(aPlainBlock.Data(), aEncryptedBlock.Data(), aArchiveBlockBytes);
  }

  return ReadSectionHeader(aPlainBlock.Data(), kSectionHeaderBytesV2, pOutHeader, nullptr) &&
         ValidateSectionCheckSum(
             pOutHeader, aPlainBlock.Data() + kSectionHeaderBytesV2, aSectionPayloadBytes);
}

void RefineArchiveWindowFromInspection(DecodeStageContextV2& pContext,
                                       const SectionHeaderV2& pSectionHeader) {
  DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  DecodeDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;

  if (pSectionHeader.mArchiveFileCount != 0u) {
    aBootstrap.mExpectedArchiveCount = pSectionHeader.mArchiveFileCount;
  }
  aBootstrap.mExpectedEmptyFolderBlockCount = pSectionHeader.mFolderManifestBlockCount;
  aBootstrap.mExpectedPreviewManifestBlockCount = pSectionHeader.mPreviewManifestBlockCount;
  aBootstrap.mExpectedArchiveDataBlockCount = pSectionHeader.mArchiveDataBlockCount;
  aBootstrap.mExpectedRepairBlockCount = pSectionHeader.mRepairDataBlockCount;

  const std::uint64_t aExpectedArchiveCount =
      std::max<std::uint64_t>(aBootstrap.mExpectedArchiveCount,
                              static_cast<std::uint64_t>(aDiscovery.mArchives.size()));
  if (aExpectedArchiveCount == 0u) {
    return;
  }

  std::vector<DiscoveredArchiveFileV2> aRefined(
      static_cast<std::size_t>(aExpectedArchiveCount));
  for (std::uint64_t aIndex = 0u; aIndex < aExpectedArchiveCount; ++aIndex) {
    aRefined[static_cast<std::size_t>(aIndex)].mArchiveIndex = aIndex;
    aRefined[static_cast<std::size_t>(aIndex)].mArchiveBlockCount =
        pSectionHeader.mArchiveBlockCount;
    aRefined[static_cast<std::size_t>(aIndex)].mIsPresent = false;
  }

  for (const DiscoveredArchiveFileV2& aArchive : aDiscovery.mArchives) {
    if (aArchive.mArchiveIndex >= aExpectedArchiveCount) {
      continue;
    }
    DiscoveredArchiveFileV2& aSlot =
        aRefined[static_cast<std::size_t>(aArchive.mArchiveIndex)];
    if (!aSlot.mIsPresent ||
        (!aSlot.mHasReadableHeader && aArchive.mHasReadableHeader) ||
        (aSlot.mPath.empty() && !aArchive.mPath.empty())) {
      aSlot = aArchive;
      aSlot.mIsPresent = true;
    }
  }

  aDiscovery.mArchives = std::move(aRefined);
  aDiscovery.mTotalReadableBlocks = 0u;
  for (const DiscoveredArchiveFileV2& aArchive : aDiscovery.mArchives) {
    if (aArchive.mIsPresent) {
      aDiscovery.mTotalReadableBlocks += aArchive.mReadableBlockCount;
    }
  }
}

std::uint64_t CeilingDivideU64(std::uint64_t pValue,
                               std::uint64_t pDivisor) {
  return pDivisor == 0u ? 0u : ((pValue + pDivisor - 1u) / pDivisor);
}

std::uint64_t MultiplyClampedU64(std::uint64_t pLeft,
                                 std::uint64_t pRight) {
  if (pLeft == 0u || pRight == 0u) {
    return 0u;
  }
  if (pLeft > (std::numeric_limits<std::uint64_t>::max() / pRight)) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return pLeft * pRight;
}

struct ArchiveNameTemplateV2 {
  std::string mPrefix;
  std::string mSuffix;
  std::size_t mDigits = 1u;
};

bool BuildArchiveNameTemplate(const DecodeStageContextV2& pContext,
                              ArchiveNameTemplateV2& pOutTemplate) {
  pOutTemplate = ArchiveNameTemplateV2{};

  std::uint32_t aIgnoredIndex = 0u;
  if (!ParseArchiveFileTemplateV2(
          pContext.FileSystem().FileName(
              pContext.State().mBootstrap.mBootstrapArchivePath),
          pOutTemplate.mPrefix,
          aIgnoredIndex,
          pOutTemplate.mSuffix,
          pOutTemplate.mDigits)) {
    return false;
  }
  return true;
}

std::string MakeArchiveNameFromTemplate(const ArchiveNameTemplateV2& pTemplate,
                                        std::uint64_t pArchiveIndex) {
  std::ostringstream aOut;
  aOut << pTemplate.mPrefix << std::setw(static_cast<int>(pTemplate.mDigits))
       << std::setfill('0') << pArchiveIndex << pTemplate.mSuffix;
  return aOut.str();
}

bool EnsureArchiveSectionTruth(DecodeStageContextV2& pContext,
                               std::size_t pArchiveSlot) {
  if (pArchiveSlot >= pContext.State().mDiscovery.mArchives.size()) {
    return false;
  }

  DiscoveredArchiveFileV2& aArchive =
      pContext.State().mDiscovery.mArchives[pArchiveSlot];
  if (!aArchive.mIsPresent || aArchive.mPath.empty()) {
    return false;
  }
  if (aArchive.mHasReadableSection) {
    return true;
  }

  for (std::uint64_t aBlockIndex = 0u;
       aBlockIndex < aArchive.mReadableBlockCount;
       ++aBlockIndex) {
    SectionHeaderV2 aHeader;
    if (!TryReadValidatedInspectionHeader(pContext, aArchive, aBlockIndex, aHeader)) {
      continue;
    }

    aArchive.mHasReadableSection = true;
    aArchive.mFirstSectionHeader = aHeader;
    aArchive.mArchiveIndex = aHeader.mArchiveIndex;
    aArchive.mArchiveBlockCount = aHeader.mArchiveBlockCount;
    return true;
  }

  return false;
}

std::uint64_t ComputeTotalFamilyBlocks(const DecodeStageContextV2& pContext) {
  const DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  const std::uint64_t aAdvertised =
      aBootstrap.mExpectedEmptyFolderBlockCount +
      aBootstrap.mExpectedPreviewManifestBlockCount +
      aBootstrap.mExpectedArchiveDataBlockCount +
      aBootstrap.mExpectedRepairBlockCount;
  if (aAdvertised > 0u) {
    return aAdvertised;
  }
  return pContext.State().mDiscovery.mTotalReadableBlocks;
}

std::uint64_t DetermineNominalBlocksPerArchive(const DecodeStageContextV2& pContext,
                                               std::uint64_t pExpectedArchiveCount,
                                               std::uint64_t pTotalFamilyBlocks) {
  std::uint64_t aMaxNonLast = 0u;
  std::uint64_t aMaxAny = 0u;

  for (const DiscoveredArchiveFileV2& aArchive : pContext.State().mDiscovery.mArchives) {
    const std::uint64_t aBlockCount =
        aArchive.mHasReadableSection ? aArchive.mArchiveBlockCount
                                     : aArchive.mReadableBlockCount;
    if (aBlockCount == 0u) {
      continue;
    }

    aMaxAny = std::max(aMaxAny, aBlockCount);
    if (pExpectedArchiveCount > 0u &&
        aArchive.mArchiveIndex + 1u < pExpectedArchiveCount) {
      aMaxNonLast = std::max(aMaxNonLast, aBlockCount);
    }
  }

  if (pContext.State().mInspection.mHasValidSection) {
    const DecodeInspectionStateV2& aInspection = pContext.State().mInspection;
    const std::uint64_t aInspectionBlocks = aInspection.mSectionHeader.mArchiveBlockCount;
    aMaxAny = std::max(aMaxAny, aInspectionBlocks);
    if (pExpectedArchiveCount > 0u &&
        aInspection.mArchiveIndex + 1u < pExpectedArchiveCount) {
      aMaxNonLast = std::max(aMaxNonLast, aInspectionBlocks);
    }
  }

  std::uint64_t aNominal = aMaxNonLast != 0u ? aMaxNonLast : aMaxAny;
  if (aNominal == 0u && pExpectedArchiveCount > 0u && pTotalFamilyBlocks > 0u) {
    aNominal = CeilingDivideU64(pTotalFamilyBlocks, pExpectedArchiveCount);
  }
  if (aNominal != 0u &&
      pExpectedArchiveCount > 0u &&
      MultiplyClampedU64(aNominal, pExpectedArchiveCount) < pTotalFamilyBlocks) {
    aNominal = CeilingDivideU64(pTotalFamilyBlocks, pExpectedArchiveCount);
  }

  return aNominal;
}

std::uint64_t DetermineExpectedBlocksForArchive(const DiscoveredArchiveFileV2* pArchive,
                                                std::uint64_t pArchiveSlot,
                                                std::uint64_t pExpectedArchiveCount,
                                                std::uint64_t pNominalBlocksPerArchive,
                                                std::uint64_t pTotalFamilyBlocks) {
  if (pArchive != nullptr &&
      pArchive->mHasReadableSection &&
      pArchive->mArchiveBlockCount != 0u) {
    return pArchive->mArchiveBlockCount;
  }

  if (pExpectedArchiveCount == 0u) {
    return pArchive != nullptr ? pArchive->mReadableBlockCount : 0u;
  }

  if (pTotalFamilyBlocks == 0u) {
    return pNominalBlocksPerArchive;
  }

  const std::uint64_t aConsumedBefore =
      std::min(pTotalFamilyBlocks,
               MultiplyClampedU64(pArchiveSlot, pNominalBlocksPerArchive));
  if (aConsumedBefore >= pTotalFamilyBlocks) {
    return 0u;
  }
  return std::min(pNominalBlocksPerArchive, pTotalFamilyBlocks - aConsumedBefore);
}

bool CopyFileStreamed(FileSystemV2& pFileSystem,
                      const std::string& pSourcePath,
                      const std::string& pDestinationPath,
                      std::uint64_t& pOutBytesCopied,
                      std::string& pOutError) {
  pOutBytesCopied = 0u;
  pOutError.clear();

  std::unique_ptr<FileReadStreamV2> aRead =
      pFileSystem.OpenReadStream(pSourcePath);
  if (aRead == nullptr || !aRead->IsReady()) {
    pOutError = "failed opening source archive for read";
    return false;
  }

  std::unique_ptr<FileWriteStreamV2> aWrite =
      pFileSystem.OpenWriteStream(pDestinationPath);
  if (aWrite == nullptr || !aWrite->IsReady()) {
    pOutError = "failed opening destination archive for write";
    return false;
  }

  constexpr std::size_t kCopyChunkBytes = 1024u * 1024u;
  ByteBufferV2 aBuffer(kCopyChunkBytes);
  if (aBuffer.Empty()) {
    pOutError = "failed allocating copy buffer";
    return false;
  }

  const std::uint64_t aSourceLength =
      static_cast<std::uint64_t>(aRead->GetLength());
  while (pOutBytesCopied < aSourceLength) {
    const std::size_t aChunkBytes = static_cast<std::size_t>(
        std::min<std::uint64_t>(kCopyChunkBytes, aSourceLength - pOutBytesCopied));
    if (!aRead->Read(static_cast<std::size_t>(pOutBytesCopied),
                     aBuffer.Data(),
                     aChunkBytes)) {
      pOutError = "failed reading source archive bytes";
      return false;
    }
    if (!aWrite->Write(aBuffer.Data(), aChunkBytes)) {
      pOutError = "failed writing destination archive bytes: " +
                  aWrite->LastErrorMessage();
      return false;
    }
    pOutBytesCopied += static_cast<std::uint64_t>(aChunkBytes);
  }

  if (!aWrite->Close()) {
    pOutError = "failed closing destination archive: " +
                aWrite->LastErrorMessage();
    return false;
  }
  return true;
}

bool AppendZeroBytes(FileSystemV2& pFileSystem,
                     const std::string& pPath,
                     std::uint64_t pByteCount,
                     std::string& pOutError) {
  pOutError.clear();
  if (pByteCount == 0u) {
    return true;
  }

  constexpr std::size_t kZeroChunkBytes = 1024u * 1024u;
  ByteBufferV2 aZeroBuffer(kZeroChunkBytes);
  if (aZeroBuffer.Empty()) {
    pOutError = "failed allocating zero-fill buffer";
    return false;
  }
  std::memset(aZeroBuffer.Data(), 0, aZeroBuffer.Size());

  std::uint64_t aRemaining = pByteCount;
  while (aRemaining > 0u) {
    const std::size_t aChunkBytes = static_cast<std::size_t>(
        std::min<std::uint64_t>(aRemaining, kZeroChunkBytes));
    if (!pFileSystem.AppendFile(pPath, aZeroBuffer.Data(), aChunkBytes)) {
      pOutError = "failed appending zero bytes";
      return false;
    }
    aRemaining -= static_cast<std::uint64_t>(aChunkBytes);
  }
  return true;
}

bool BuildSyntheticArchiveHeader(const DecodeStageContextV2& pContext,
                                 std::uint64_t pArchiveIndex,
                                 ArchiveHeaderV2& pOutHeader,
                                 std::string& pOutError) {
  pOutError.clear();
  pOutHeader = pContext.State().mBootstrap.mFirstHeader;
  pOutHeader.mDirtyState =
      static_cast<std::uint8_t>(ArchiveDirtyStateV2::kFinishedWithError);

  if (!TrySetPackedUint48(pOutHeader.mArchiveIndex,
                          pArchiveIndex,
                          nullptr,
                          "ArchiveIndex") ||
      !TrySetPackedUint48(pOutHeader.mArchiveCount,
                          pContext.State().mBootstrap.mExpectedArchiveCount,
                          nullptr,
                          "ArchiveCount") ||
      !TrySetPackedUint48(pOutHeader.mArchiveDataBlockCount,
                          pContext.State().mBootstrap.mExpectedArchiveDataBlockCount,
                          nullptr,
                          "ArchiveDataBlockCount") ||
      !TrySetPackedUint48(pOutHeader.mEmptyFolderBlockCount,
                          pContext.State().mBootstrap.mExpectedEmptyFolderBlockCount,
                          nullptr,
                          "EmptyFolderBlockCount") ||
      !TrySetPackedUint48(pOutHeader.mPreviewManifestBlockCount,
                          pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount,
                          nullptr,
                          "PreviewManifestBlockCount") ||
      !TrySetPackedUint48(pOutHeader.mRepairSectorBlockCount,
                          pContext.State().mBootstrap.mExpectedRepairBlockCount,
                          nullptr,
                          "RepairSectorBlockCount")) {
    pOutError = "synthetic archive header values were out of range";
    return false;
  }
  return true;
}

bool WriteOrPatchArchiveHeader(FileSystemV2& pFileSystem,
                               const std::string& pDestinationPath,
                               const ArchiveHeaderV2& pHeader,
                               bool pWriteFreshFile,
                               std::string& pOutError) {
  pOutError.clear();
  std::array<unsigned char, kArchiveHeaderBytesV2> aHeaderBytes{};
  if (!WriteArchiveHeader(pHeader,
                          aHeaderBytes.data(),
                          aHeaderBytes.size(),
                          nullptr)) {
    pOutError = "failed serializing synthetic archive header";
    return false;
  }

  if (pWriteFreshFile) {
    if (!pFileSystem.WriteFile(
            pDestinationPath, aHeaderBytes.data(), aHeaderBytes.size())) {
      pOutError = "failed writing synthetic archive header";
      return false;
    }
    return true;
  }

  if (!pFileSystem.OverwriteFileRegion(
          pDestinationPath, 0u, aHeaderBytes.data(), aHeaderBytes.size())) {
    pOutError = "failed patching synthetic archive header";
    return false;
  }
  return true;
}

std::string BuildRepairProgressLabel(const DecodeRepairStateV2& pRepairState) {
  std::ostringstream aOut;
  aOut << "Repairing archive family: "
       << pRepairState.mPatchedBlocks << "/" << pRepairState.mRepairableBlocks
       << " blocks, " << FormatHumanBytesV2(pRepairState.mPatchedBytes) << " / "
       << FormatHumanBytesV2(pRepairState.mRepairableBytes);
  if (pRepairState.mArchivesTotal > 0u) {
    aOut << ", archives " << pRepairState.mArchivesCompleted << "/"
         << pRepairState.mArchivesTotal;
  }
  return aOut.str();
}

}  // namespace

bool DecodeInspectionV2::Run(DecodeStageContextV2& pContext) {
  DecodeInspectionStateV2& aInspection = pContext.State().mInspection;
  aInspection = DecodeInspectionStateV2{};

  auto aAccept = [&](std::size_t pArchiveSlot,
                     std::uint64_t pBlockIndex,
                     const SectionHeaderV2& pSectionHeader) {
    aInspection.mHasValidSection = true;
    aInspection.mArchiveIndex = pSectionHeader.mArchiveIndex;
    aInspection.mBlockIndex = pBlockIndex;
    aInspection.mSectionHeader = pSectionHeader;

    DiscoveredArchiveFileV2& aArchive =
        pContext.State().mDiscovery.mArchives[pArchiveSlot];
    aArchive.mHasReadableSection = true;
    aArchive.mFirstSectionHeader = pSectionHeader;
    aArchive.mArchiveIndex = pSectionHeader.mArchiveIndex;
    aArchive.mArchiveBlockCount = pSectionHeader.mArchiveBlockCount;

    RefineArchiveWindowFromInspection(pContext, pSectionHeader);
    return true;
  };

  if (pContext.Request().mIntent == DecodeIntentV2::kUnbundle) {
    if (pContext.State().mDiscovery.mArchives.empty() ||
        !pContext.State().mDiscovery.mArchives.front().mIsPresent) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                        ProgressStageV2::kInspection,
                                        "the first archive box is missing"));
      return false;
    }

    SectionHeaderV2 aHeader;
    if (!TryReadValidatedInspectionHeader(
            pContext, pContext.State().mDiscovery.mArchives.front(), 0u, aHeader)) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                        ProgressStageV2::kInspection,
                                        "the first block in the first archive did not validate"));
      return false;
    }
    (void)aAccept(0u, 0u, aHeader);
  } else {
    for (std::size_t aArchiveSlot = 0u;
         aArchiveSlot < pContext.State().mDiscovery.mArchives.size();
         ++aArchiveSlot) {
      const DiscoveredArchiveFileV2& aArchive =
          pContext.State().mDiscovery.mArchives[aArchiveSlot];
      if (!aArchive.mIsPresent) {
        continue;
      }
      for (std::uint64_t aBlockIndex = 0u;
           aBlockIndex < aArchive.mReadableBlockCount;
           ++aBlockIndex) {
        SectionHeaderV2 aHeader;
        if (TryReadValidatedInspectionHeader(pContext, aArchive, aBlockIndex, aHeader)) {
          (void)aAccept(aArchiveSlot, aBlockIndex, aHeader);
          aArchiveSlot = pContext.State().mDiscovery.mArchives.size();
          break;
        }
      }
    }

    if (!aInspection.mHasValidSection) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                                        ProgressStageV2::kInspection,
                                        "no valid block was found during inspection"));
      return false;
    }
  }

  pContext.EmitLog(
      LogLevelV2::kInfo,
      "[" + LogActionLabelV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent)) +
          "][Inspection] Accepted archive " +
          std::to_string(aInspection.mArchiveIndex) + " block " +
          std::to_string(aInspection.mBlockIndex) + " as the first valid inspected block.");
  pContext.EmitPhaseProgress(1.0, "Inspection complete");
  return !pContext.IsCancelRequested();
}

bool DecodeManifestDiscoveryV2::Run(DecodeStageContextV2& pContext) {
  if (pContext.Request().mIntent == DecodeIntentV2::kManifest) {
    const DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
    const DecodeDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;

    std::ostringstream aOut;
    aOut << "[Read Manifest][Report] Family "
         << aBootstrap.mFirstHeader.mArchiveFamilyId
         << ", archives=" << aDiscovery.mArchives.size()
         << ", readable_blocks=" << aDiscovery.mTotalReadableBlocks
         << ", advertised_empty_folder_blocks="
         << aBootstrap.mExpectedEmptyFolderBlockCount
         << ", advertised_preview_manifest_blocks="
         << aBootstrap.mExpectedPreviewManifestBlockCount
         << ", advertised_archive_data_blocks="
         << aBootstrap.mExpectedArchiveDataBlockCount
         << ", advertised_repair_blocks="
         << aBootstrap.mExpectedRepairBlockCount
         << ", encrypted="
         << (aBootstrap.mFirstHeader.mIsEncrypted != 0u ? "true" : "false")
         << ", discovery_mode="
         << (aDiscovery.mMode == DecodeModeV2::kOptimistic ? "optimistic"
                                                           : "pessimistic")
         << ".";
    pContext.EmitLog(LogLevelV2::kInfo, aOut.str());

    if (aBootstrap.mExpectedPreviewManifestBlockCount > 0u) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Read Manifest][Report] Preview-manifest block count is only an "
          "advertised header value here; this flow does not decode preview "
          "manifest payloads.");
    }
  } else {
    pContext.EmitLog(
        LogLevelV2::kInfo,
        LogDecodeManifestSummaryV2(
            LogActionFromDecodeIntentV2(pContext.Request().mIntent),
            pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount,
            pContext.State().mBootstrap.mExpectedRepairBlockCount));
  }
  pContext.EmitPhaseProgress(1.0, "Manifest discovery complete");
  return !pContext.IsCancelRequested();
}

bool DecodeRepairApplyV2::Run(DecodeStageContextV2& pContext) {
  DecodeRepairStateV2& aRepair = pContext.State().mRepair;
  aRepair = DecodeRepairStateV2{};

  const std::uint64_t aExpectedArchiveCount =
      std::max<std::uint64_t>(pContext.State().mBootstrap.mExpectedArchiveCount,
                              static_cast<std::uint64_t>(
                                  pContext.State().mDiscovery.mArchives.size()));
  if (aExpectedArchiveCount == 0u) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kRepair,
                                      ProgressStageV2::kRepairApply,
                                      "no archive boxes were available for repair"));
    return false;
  }

  ArchiveNameTemplateV2 aTemplate;
  if (!BuildArchiveNameTemplate(pContext, aTemplate)) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kRepair,
                                      ProgressStageV2::kRepairApply,
                                      "bootstrap archive filename did not match the archive template"));
    return false;
  }

  const std::size_t aArchiveHeaderBytes = kArchiveHeaderBytesV2;
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::uint64_t aTotalFamilyBlocks = ComputeTotalFamilyBlocks(pContext);

  for (std::size_t aArchiveSlot = 0u;
       aArchiveSlot < pContext.State().mDiscovery.mArchives.size();
       ++aArchiveSlot) {
    (void)EnsureArchiveSectionTruth(pContext, aArchiveSlot);
  }

  const std::uint64_t aNominalBlocksPerArchive =
      DetermineNominalBlocksPerArchive(
          pContext, aExpectedArchiveCount, aTotalFamilyBlocks);
  if (aNominalBlocksPerArchive == 0u &&
      aTotalFamilyBlocks != 0u) {
    pContext.EmitLog(LogLevelV2::kError,
                     LogPhaseFailedV2(LogActionV2::kRepair,
                                      ProgressStageV2::kRepairApply,
                                      "could not infer the per-archive block count for repair sizing"));
    return false;
  }

  aRepair.mArchivesTotal = aExpectedArchiveCount;
  pContext.EmitLog(
      LogLevelV2::kInfo,
      "[Repair][Repair Apply] Planning " + std::to_string(aExpectedArchiveCount) +
          " archive boxes, nominal blocks/archive=" +
          std::to_string(aNominalBlocksPerArchive) +
          ", family blocks=" + std::to_string(aTotalFamilyBlocks) + ".");

  for (std::uint64_t aArchiveIndex = 0u;
       aArchiveIndex < aExpectedArchiveCount;
       ++aArchiveIndex) {
    if (pContext.IsCancelRequested()) {
      return false;
    }

    DiscoveredArchiveFileV2* aArchive = nullptr;
    if (aArchiveIndex < pContext.State().mDiscovery.mArchives.size()) {
      aArchive = &pContext.State().mDiscovery.mArchives[static_cast<std::size_t>(aArchiveIndex)];
    }

    const std::uint64_t aExpectedBlocks =
        DetermineExpectedBlocksForArchive(aArchive,
                                          aArchiveIndex,
                                          aExpectedArchiveCount,
                                          aNominalBlocksPerArchive,
                                          aTotalFamilyBlocks);
    const std::uint64_t aExpectedFileBytes =
        static_cast<std::uint64_t>(aArchiveHeaderBytes) +
        (aExpectedBlocks * static_cast<std::uint64_t>(aArchiveBlockBytes));

    const bool aHasSourceFile =
        aArchive != nullptr && aArchive->mIsPresent && !aArchive->mPath.empty();
    const std::string aOutputName =
        aHasSourceFile ? pContext.FileSystem().FileName(aArchive->mPath)
                       : MakeArchiveNameFromTemplate(aTemplate, aArchiveIndex);
    const std::string aOutputPath = pContext.FileSystem().JoinPath(
        pContext.Request().mDestinationDirectory, aOutputName);

    if (aHasSourceFile && aArchive->mPath == aOutputPath) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kRepair,
                                        ProgressStageV2::kRepairApply,
                                        "source and destination archive paths would collide for " +
                                            aOutputName));
      return false;
    }

    std::uint64_t aBytesCopied = 0u;
    std::string aIoError;
    if (aHasSourceFile) {
      if (!CopyFileStreamed(
              pContext.FileSystem(), aArchive->mPath, aOutputPath, aBytesCopied, aIoError)) {
        pContext.EmitLog(LogLevelV2::kError,
                         LogPhaseFailedV2(LogActionV2::kRepair,
                                          ProgressStageV2::kRepairApply,
                                          aIoError + " for " + aOutputName));
        return false;
      }
    }

    ArchiveHeaderV2 aSyntheticHeader;
    std::string aHeaderError;
    if (!BuildSyntheticArchiveHeader(pContext, aArchiveIndex, aSyntheticHeader, aHeaderError)) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kRepair,
                                        ProgressStageV2::kRepairApply,
                                        aHeaderError));
      return false;
    }

    const bool aNeedsSyntheticHeader =
        !aHasSourceFile ||
        (aArchive != nullptr && (!aArchive->mHasReadableHeader ||
                                 aArchive->mFileLength < aArchiveHeaderBytes));
    if (aNeedsSyntheticHeader) {
      if (!WriteOrPatchArchiveHeader(
              pContext.FileSystem(),
              aOutputPath,
              aSyntheticHeader,
              !aHasSourceFile,
              aHeaderError)) {
        pContext.EmitLog(LogLevelV2::kError,
                         LogPhaseFailedV2(LogActionV2::kRepair,
                                          ProgressStageV2::kRepairApply,
                                          aHeaderError + " for " + aOutputName));
        return false;
      }
      if (!aHasSourceFile) {
        aBytesCopied = static_cast<std::uint64_t>(aArchiveHeaderBytes);
      } else {
        aBytesCopied =
            std::max<std::uint64_t>(aBytesCopied, static_cast<std::uint64_t>(aArchiveHeaderBytes));
      }
    }

    const std::uint64_t aSourceFileBytes =
        aHasSourceFile ? aArchive->mFileLength : 0u;
    const std::uint64_t aCoveredFileBytes =
        std::max<std::uint64_t>(aSourceFileBytes,
                                static_cast<std::uint64_t>(aArchiveHeaderBytes));
    const std::uint64_t aRepairableBytes =
        aExpectedFileBytes > aCoveredFileBytes
            ? (aExpectedFileBytes - aCoveredFileBytes)
            : 0u;
    const std::uint64_t aFullReadableBlocks =
        aSourceFileBytes > static_cast<std::uint64_t>(aArchiveHeaderBytes)
            ? ((aSourceFileBytes - static_cast<std::uint64_t>(aArchiveHeaderBytes)) /
               static_cast<std::uint64_t>(aArchiveBlockBytes))
            : 0u;
    const std::uint64_t aRepairableBlocks =
        aExpectedBlocks > aFullReadableBlocks
            ? (aExpectedBlocks - aFullReadableBlocks)
            : 0u;

    if (aExpectedFileBytes > aBytesCopied) {
      if (!AppendZeroBytes(pContext.FileSystem(),
                           aOutputPath,
                           aExpectedFileBytes - aBytesCopied,
                           aIoError)) {
        pContext.EmitLog(LogLevelV2::kError,
                         LogPhaseFailedV2(LogActionV2::kRepair,
                                          ProgressStageV2::kRepairApply,
                                          aIoError + " for " + aOutputName));
        return false;
      }
      aBytesCopied = aExpectedFileBytes;
    }

    aRepair.mRepairableBlocks += aRepairableBlocks;
    aRepair.mPatchedBlocks += aRepairableBlocks;
    aRepair.mRepairableBytes += aRepairableBytes;
    aRepair.mPatchedBytes += aRepairableBytes;
    if (!aHasSourceFile) {
      ++aRepair.mArchivesSynthesized;
    } else if (aRepairableBlocks > 0u || aRepairableBytes > 0u) {
      ++aRepair.mArchivesExpanded;
    }
    ++aRepair.mArchivesCompleted;

    ++pContext.State().mOutput.mFilesWritten;
    pContext.State().mOutput.mBytesWritten += aBytesCopied;

    if (!aHasSourceFile) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Repair][Repair Apply] Synthesized missing archive " + aOutputName +
              " with " + std::to_string(aExpectedBlocks) + " blocks (" +
              FormatHumanBytesV2(aRepairableBytes) + ").");
    } else if (aRepairableBlocks > 0u || aRepairableBytes > 0u) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Repair][Repair Apply] Expanded " + aOutputName + " to exact size " +
              FormatHumanBytesV2(aExpectedFileBytes) + " by patching " +
              std::to_string(aRepairableBlocks) + " blocks (" +
              FormatHumanBytesV2(aRepairableBytes) + ").");
    } else if (aNeedsSyntheticHeader) {
      pContext.EmitLog(
          LogLevelV2::kInfo,
          "[Repair][Repair Apply] Rebuilt archive header for " + aOutputName + ".");
    }

    pContext.EmitPhaseProgress(
        static_cast<double>(aRepair.mArchivesCompleted) /
            static_cast<double>(std::max<std::uint64_t>(1u, aRepair.mArchivesTotal)),
        BuildRepairProgressLabel(aRepair));
  }

  std::ostringstream aSummary;
  aSummary << "[Repair][Repair Apply] Successfully patched "
           << aRepair.mPatchedBlocks << "/" << aRepair.mRepairableBlocks
           << " repairable blocks (" << FormatHumanBytesV2(aRepair.mPatchedBytes)
           << " / " << FormatHumanBytesV2(aRepair.mRepairableBytes)
           << ") across " << aRepair.mArchivesCompleted << "/"
           << aRepair.mArchivesTotal << " archives.";
  if (aRepair.mArchivesSynthesized > 0u || aRepair.mArchivesExpanded > 0u) {
    aSummary << " Synthesized " << aRepair.mArchivesSynthesized
             << ", expanded " << aRepair.mArchivesExpanded << ".";
  }
  pContext.EmitLog(LogLevelV2::kInfo, aSummary.str());
  pContext.EmitPhaseProgress(1.0, "Repair apply complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
