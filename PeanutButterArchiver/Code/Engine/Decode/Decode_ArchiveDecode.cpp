#include "Decode_ArchiveDecode.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

#include "../../Knobs.hpp"
#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"
#include "Decode_LogicalRecordDecoder.hpp"

namespace peanutbutter {
namespace {

using namespace memory_layout;

constexpr std::uint64_t kDecodeProgressArchiveLogIntervalV2 =
    knobs::kDecodeArchiveProgressArchiveLogIntervalV2;
constexpr std::uint64_t kDecodeProgressFileLogIntervalV2 =
    knobs::kDecodeArchiveProgressFileLogIntervalV2;
constexpr std::uint64_t kDecodeProgressFolderLogIntervalV2 =
    knobs::kDecodeArchiveProgressFolderLogIntervalV2;
constexpr std::uint64_t kDecodeProgressByteLogIntervalV2 =
    knobs::kDecodeArchiveProgressByteLogIntervalV2;
constexpr std::uint64_t kHealingProgressByteLogIntervalV2 =
    knobs::kDecodeArchiveProgressByteLogIntervalV2;
constexpr std::size_t kRecoverLedgerDigitsV2 = 5u;
constexpr const char* kRecoverPreserveDirectoryNameV2 = "$PRESERVE";
std::string ZeroPadNumber(std::uint64_t pValue, std::size_t pDigits) {
  std::ostringstream aOut;
  aOut << std::setw(static_cast<int>(pDigits)) << std::setfill('0') << pValue;
  return aOut.str();
}

const char* SectionTypeLabel(SectionTypeV2 pSectionType) {
  switch (pSectionType) {
    case SectionTypeV2::kPreviewManifest:
      return "preview_manifest";
    case SectionTypeV2::kArchiveData:
      return "archive_data";
    case SectionTypeV2::kRepairData:
      return "repair_data";
  }
  return "unknown";
}

std::string DecodeStagePrefix(DecodeIntentV2 pIntent,
                              ProgressStageV2 pStage) {
  return "[" + LogActionLabelV2(LogActionFromDecodeIntentV2(pIntent)) + "][" +
         ProgressStageLabelV2(pStage) + "]";
}

LoggingStatV2 BuildArchiveDecodeStat(const DecodeStageContextV2& pContext,
                                     const DecodeLogicalRecordDecoderV2& pFolderDecoder,
                                     const DecodeLogicalRecordDecoderV2& pFileDecoder,
                                     std::uint64_t pArchivesCompleted) {
  LoggingStatV2 aStat;
  aStat.mArchivesCompleted = pArchivesCompleted;
  aStat.mArchivesTotal = static_cast<std::uint64_t>(
      pContext.State().mDiscovery.mArchives.size());
  aStat.mFilesCompleted = pFileDecoder.FilesWritten();
  aStat.mFoldersCompleted =
      pFolderDecoder.FoldersCreated() + pFileDecoder.FoldersCreated();
  aStat.mBytesCompleted = pFileDecoder.BytesWritten();
  return aStat;
}

std::string BuildArchiveDecodeSummary(const LoggingStatV2& pStat) {
  std::vector<std::string> aParts;
  if (pStat.mArchivesTotal > 0u) {
    aParts.push_back(std::to_string(pStat.mArchivesCompleted) + " of " +
                     std::to_string(pStat.mArchivesTotal) + " archives");
  }
  aParts.push_back(std::to_string(pStat.mFilesCompleted) + " files");
  aParts.push_back(std::to_string(pStat.mFoldersCompleted) + " folders");
  if (pStat.mBytesCompleted > 0u) {
    aParts.push_back(FormatHumanBytesV2(pStat.mBytesCompleted));
  } else {
    aParts.push_back("0B");
  }

  std::string aSummary;
  for (std::size_t aIndex = 0u; aIndex < aParts.size(); ++aIndex) {
    if (aIndex != 0u) {
      aSummary += ", ";
    }
    aSummary += aParts[aIndex];
  }
  return aSummary;
}

std::string BuildArchiveDecodeProgressLabel(const LoggingStatV2& pStat) {
  return "Decoding archive blocks: " + BuildArchiveDecodeSummary(pStat);
}

void EmitDecodeArchiveEvent(DecodeStageContextV2& pContext,
                            const DiscoveredArchiveFileV2& pArchive,
                            std::size_t pArchiveSlot) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeArchiveStarted;
  aEvent.mStage = ProgressStageV2::kArchiveDecode;
  aEvent.mLabel = "Decode started archive " + std::to_string(pArchiveSlot);
  aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("present", pArchive.mIsPresent);
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitDecodeArchiveFinishedEvent(DecodeStageContextV2& pContext,
                                    const DiscoveredArchiveFileV2& pArchive,
                                    std::size_t pArchiveSlot,
                                    std::uint64_t pBlocksRead) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeArchiveFinished;
  aEvent.mStage = ProgressStageV2::kArchiveDecode;
  aEvent.mLabel = "Decode finished archive " + std::to_string(pArchiveSlot);
  aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("present", pArchive.mIsPresent);
  aEvent.SetInfo("blocks_read", pBlocksRead);
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitDecodeArchiveHeaderEvent(DecodeStageContextV2& pContext,
                                  const DiscoveredArchiveFileV2& pArchive,
                                  std::size_t pArchiveSlot) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeArchiveHeaderRead;
  aEvent.mStage = ProgressStageV2::kArchiveDecode;
  aEvent.mLabel = "Decode read archive header for archive " +
                  std::to_string(pArchiveSlot);
  aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("archive_count",
                 PackedUint48ToUInt64(pArchive.mHeader.mArchiveCount));
  aEvent.SetInfo("archive_data_block_count",
                 PackedUint48ToUInt64(pArchive.mHeader.mBlockCountMain));
  const std::uint64_t aReservedCount0 =
      PackedUint48ToUInt64(pArchive.mHeader.mReservedCount0);
  if (aReservedCount0 > 0u) {
    aEvent.SetInfo("reserved_count0", aReservedCount0);
  }
  aEvent.SetInfo("preview_manifest_block_count",
                 PackedUint48ToUInt64(pArchive.mHeader.mBlockCountPreview));
  aEvent.SetInfo("repair_block_count",
                 PackedUint48ToUInt64(pArchive.mHeader.mBlockCountRepair));
  aEvent.SetInfo("archive_family_id", pArchive.mHeader.mArchiveFamilyId);
  aEvent.SetInfo("is_encrypted", pArchive.mHeader.mIsEncrypted != 0u);
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitDecodeBlockEvent(DecodeStageContextV2& pContext,
                          RuntimeEventKindV2 pKind,
                          const DiscoveredArchiveFileV2& pArchive,
                          std::size_t pArchiveSlot,
                          std::uint64_t pBlockIndex,
                          const char* pSectionTypeLabel,
                          const std::string& pFileReference) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = pKind;
  aEvent.mStage = ProgressStageV2::kArchiveDecode;
  aEvent.mLabel =
      std::string(pKind == RuntimeEventKindV2::kDecodeBlockStarted
                      ? "Decode started block "
                      : "Decode cleared block ") +
      std::to_string(pBlockIndex) + " in archive " +
      std::to_string(pArchiveSlot);
  aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("block_index", pBlockIndex);
  if (pSectionTypeLabel != nullptr && *pSectionTypeLabel != '\0') {
    aEvent.SetInfo("section_type", pSectionTypeLabel);
  }
  if (!pFileReference.empty()) {
    aEvent.SetInfo("file_name", pFileReference);
    aEvent.SetInfo("relative_path", pFileReference);
  }
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitDecodeBlockHeaderEvent(DecodeStageContextV2& pContext,
                                const DiscoveredArchiveFileV2& pArchive,
                                std::size_t pArchiveSlot,
                                std::uint64_t pBlockIndex,
                                const SectionHeaderV2& pSectionHeader,
                                const std::string& pFileReference) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeBlockHeaderRead;
  aEvent.mStage = ProgressStageV2::kArchiveDecode;
  aEvent.mLabel =
      "Decode read block header " + std::to_string(pBlockIndex) +
      " in archive " + std::to_string(pArchiveSlot);
  aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("block_index", pBlockIndex);
  aEvent.SetInfo("section_type",
                 SectionTypeLabel(
                     static_cast<SectionTypeV2>(pSectionHeader.mSectionType)));
  aEvent.SetInfo("payload_bytes_used",
                 static_cast<std::uint64_t>(pSectionHeader.mPayloadBytesUsed));
  aEvent.SetInfo("archive_block_count",
                 static_cast<std::uint64_t>(pSectionHeader.mArchiveBlockCount));
  if (!pFileReference.empty()) {
    aEvent.SetInfo("file_name", pFileReference);
    aEvent.SetInfo("relative_path", pFileReference);
  }
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitDecodeDecryptionStartedEvent(DecodeStageContextV2& pContext,
                                      const DiscoveredArchiveFileV2& pArchive,
                                      std::size_t pArchiveSlot,
                                      std::uint64_t pBlockIndex) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeDecryptionStarted;
  aEvent.mStage = ProgressStageV2::kArchiveDecode;
  aEvent.mLabel =
      "Decode started decryption for block " + std::to_string(pBlockIndex) +
      " in archive " + std::to_string(pArchiveSlot);
  aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("block_index", pBlockIndex);
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitDecodeDecryptionEvent(DecodeStageContextV2& pContext,
                               const DiscoveredArchiveFileV2& pArchive,
                               std::size_t pArchiveSlot,
                               std::uint64_t pBlockIndex) {
  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeDecryptionFinished;
  aEvent.mStage = ProgressStageV2::kArchiveDecode;
  aEvent.mLabel =
      "Decode finished decryption for block " + std::to_string(pBlockIndex) +
      " in archive " + std::to_string(pArchiveSlot);
  aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("block_index", pBlockIndex);
  pContext.EmitRuntimeEvent(aEvent);
}

void EmitDecodeErrorMarkerEvent(DecodeStageContextV2& pContext,
                                const std::string& pErrorKind,
                                const std::string& pLabel,
                                const DiscoveredArchiveFileV2& pArchive,
                                std::size_t pArchiveSlot,
                                std::uint64_t pBlockIndex,
                                const char* pSectionTypeLabel,
                                const std::string& pFileReference) {
  if (!pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeError)) {
    return;
  }

  RuntimeEventV2 aEvent;
  aEvent.mKind = RuntimeEventKindV2::kDecodeError;
  aEvent.mStage = ProgressStageV2::kArchiveDecode;
  aEvent.mLabel = pLabel;
  aEvent.SetInfo("decode_error_kind", pErrorKind);
  aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
  aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
  aEvent.SetInfo("archive_path", pArchive.mPath);
  aEvent.SetInfo("block_index", pBlockIndex);
  if (pSectionTypeLabel != nullptr && *pSectionTypeLabel != '\0') {
    aEvent.SetInfo("section_type", pSectionTypeLabel);
  }
  if (!pFileReference.empty()) {
    aEvent.SetInfo("file_name", pFileReference);
    aEvent.SetInfo("relative_path", pFileReference);
  }
  pContext.EmitRuntimeEvent(aEvent);
}

bool ShouldEmitArchiveDecodeSlice(const LoggingStatV2& pStat,
                                  std::uint64_t& pNextArchiveLog,
                                  std::uint64_t& pNextFileLog,
                                  std::uint64_t& pNextFolderLog,
                                  std::uint64_t& pNextByteLog) {
  bool aShouldEmit = false;
  while (pStat.mArchivesCompleted >= pNextArchiveLog) {
    aShouldEmit = true;
    pNextArchiveLog += kDecodeProgressArchiveLogIntervalV2;
  }
  while (pStat.mFilesCompleted >= pNextFileLog) {
    aShouldEmit = true;
    pNextFileLog += kDecodeProgressFileLogIntervalV2;
  }
  while (pStat.mFoldersCompleted >= pNextFolderLog) {
    aShouldEmit = true;
    pNextFolderLog += kDecodeProgressFolderLogIntervalV2;
  }
  while (pStat.mBytesCompleted >= pNextByteLog) {
    aShouldEmit = true;
    pNextByteLog += kDecodeProgressByteLogIntervalV2;
  }
  return aShouldEmit;
}

bool ShouldContinuePastDamagedBlock(const DecodeStageContextV2& pContext) {
  return DecodeIntentAllowsSalvageV2(pContext.Request().mIntent);
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

bool TryAcceptMissingPreviewManifest(DecodeStageContextV2& pContext,
                                     const SectionHeaderV2& pSectionHeader) {
  DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  DecodeManifestStateV2& aManifest = pContext.State().mManifest;
  if (aBootstrap.mExpectedPreviewManifestBlockCount == 0u ||
      aManifest.mPreviewManifestBlocksProcessed != 0u ||
      pSectionHeader.mSectionType !=
          static_cast<std::uint8_t>(SectionTypeV2::kArchiveData)) {
    return false;
  }

  aBootstrap.mExpectedPreviewManifestBlockCount = 0u;
  pContext.EmitLog(LogLevelV2::kWarning,
                   DecodeStagePrefix(pContext.Request().mIntent,
                                     ProgressStageV2::kManifestDiscovery) +
                       " Header advertised preview manifest blocks, but archive "
                       "data started immediately. Treating preview manifest "
                       "count as zero.");
  return true;
}

bool TryReadValidatedSectionHeader(const unsigned char* pBlockBytes,
                                   std::size_t pPayloadBytes,
                                   SectionHeaderV2& pOutHeader) {
  if (!ReadSectionHeader(pBlockBytes, kSectionHeaderBytesV2, pOutHeader, nullptr)) {
    return false;
  }
  return ValidateSectionCheckSum(
      pOutHeader,
      pBlockBytes + kSectionHeaderBytesV2,
      pPayloadBytes);
}

bool HeaderIndexMatchesCursor(const DiscoveredArchiveFileV2& pArchive,
                              const SectionHeaderV2& pSectionHeader,
                              std::uint64_t pExpectedBlockIndex) {
  return pSectionHeader.mArchiveIndex ==
             static_cast<std::uint32_t>(pArchive.mArchiveIndex) &&
         pSectionHeader.mBlockIndex ==
             static_cast<std::uint32_t>(pExpectedBlockIndex);
}

bool ShouldTryPlaintextPreviewManifestBlock(const DecodeStageContextV2& pContext) {
  const DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  const DecodeManifestStateV2& aManifest = pContext.State().mManifest;
  return aBootstrap.mFirstHeader.mIsEncrypted != 0u &&
         aManifest.mEmptyFolderBlocksProcessed >=
             aBootstrap.mExpectedEmptyFolderBlockCount &&
         aManifest.mPreviewManifestBlocksProcessed <
             aBootstrap.mExpectedPreviewManifestBlockCount;
}

std::uint8_t ExpectedSectionType(const DecodeStageContextV2& pContext) {
  const DecodeManifestStateV2& aManifest = pContext.State().mManifest;
  const DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
  if (aManifest.mPreviewManifestBlocksProcessed <
      aBootstrap.mExpectedPreviewManifestBlockCount) {
    return static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest);
  }
  if (aManifest.mArchiveDataBlocksProcessed <
      aBootstrap.mExpectedArchiveDataBlockCount) {
    return static_cast<std::uint8_t>(SectionTypeV2::kArchiveData);
  }
  if (aManifest.mRepairBlocksProcessed <
      aBootstrap.mExpectedRepairBlockCount) {
    return static_cast<std::uint8_t>(SectionTypeV2::kRepairData);
  }
  return static_cast<std::uint8_t>(SectionTypeV2::kRepairData);
}

bool ReadBlock(FileReadStreamV2& pRead,
               std::uint64_t pBlockIndex,
               std::size_t pArchiveBlockBytes,
               FixedBlockBufferV2& pOutBlockBytes) {
  const std::size_t aOffset =
      static_cast<std::size_t>(kArchiveHeaderBytesV2 +
                               (pBlockIndex * pArchiveBlockBytes));
  return pRead.Read(aOffset, pOutBlockBytes.Data(), pArchiveBlockBytes);
}

bool IsExpectedContinuationBlock(const std::vector<DiscoveredArchiveFileV2>& pArchives,
                                 std::size_t pPreviousArchiveSlot,
                                 std::uint64_t pPreviousBlockIndex,
                                 std::size_t pCurrentArchiveSlot,
                                 std::uint64_t pCurrentBlockIndex) {
  if (pPreviousArchiveSlot >= pArchives.size() || pCurrentArchiveSlot >= pArchives.size()) {
    return false;
  }
  if (pCurrentArchiveSlot == pPreviousArchiveSlot) {
    return pCurrentBlockIndex == (pPreviousBlockIndex + 1u);
  }
  if (pCurrentArchiveSlot != (pPreviousArchiveSlot + 1u) || pCurrentBlockIndex != 0u) {
    return false;
  }

  const DiscoveredArchiveFileV2& aPreviousArchive = pArchives[pPreviousArchiveSlot];
  const std::uint64_t aPreviousDeclaredBlockCount =
      aPreviousArchive.mArchiveBlockCount != 0u
          ? aPreviousArchive.mArchiveBlockCount
          : aPreviousArchive.mReadableBlockCount;
  const std::uint64_t aPreviousArchiveBlockCount = aPreviousDeclaredBlockCount;
  if (aPreviousArchive.mReadableBlockCount < aPreviousDeclaredBlockCount) {
    // Tail truncation cannot continue into the next archive because trailing
    // bytes are missing. But if the observed logical index reached the declared
    // end-of-archive boundary, the missing block(s) were inferred earlier in
    // the archive (drift), and cross-archive continuation is still valid.
    return aPreviousArchiveBlockCount != 0u &&
           (pPreviousBlockIndex + 1u) >= aPreviousArchiveBlockCount;
  }
  return aPreviousArchiveBlockCount != 0u &&
         (pPreviousBlockIndex + 1u) >= aPreviousArchiveBlockCount;
}

bool SkipRecordIsZero(const SkipRecordV2& pSkipRecord) {
  return GetSkipRecordArchiveIndex(pSkipRecord) == 0u &&
         pSkipRecord.mBlockIndex == 0u &&
         GetSkipRecordByteDistance(pSkipRecord) == 0u;
}

bool IsSafeRelativePathForSingleByteTypeGap(const std::string& pPath,
                                            std::size_t pMaxPathLength) {
  if (pPath.empty() || pPath.size() > pMaxPathLength) {
    return false;
  }
  if (pPath[0] == '/' || pPath[0] == '\\') {
    return false;
  }
  if (pPath.size() > 2u &&
      std::isalpha(static_cast<unsigned char>(pPath[0])) != 0 &&
      pPath[1] == ':') {
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

bool IsSafeReferenceTargetForSingleByteTypeGap(const std::string& pPath,
                                               std::size_t pMaxPathLength) {
  if (pPath.empty() || pPath.size() > pMaxPathLength) {
    return false;
  }
  if (pPath[0] == '/' || pPath[0] == '\\') {
    return false;
  }
  if (pPath.size() > 1u &&
      std::isalpha(static_cast<unsigned char>(pPath[0])) != 0 &&
      pPath[1] == ':') {
    return false;
  }
  for (char aChar : pPath) {
    const unsigned char aByte = static_cast<unsigned char>(aChar);
    if (aByte < 32u || aByte == 127u || aByte == 0u) {
      return false;
    }
  }
  return true;
}

bool ParseDataPrefixWithInjectedTypeForSingleByteGap(
    const std::string& pFirstPath,
    std::uint8_t pInjectedType,
    const std::vector<unsigned char>& pBufferedBytes,
    const memory_layout::ArchiveLayoutConfigV2& pLayout) {
  if (!IsSafeRelativePathForSingleByteTypeGap(
          pFirstPath, pLayout.mMaxPathLength)) {
    return false;
  }

  auto ParseRecordBody = [&](const std::string& pPath,
                             std::uint8_t pType,
                             std::size_t& pCursor) -> bool {
    auto ConsumeRemainingBufferedBytes = [&](void) -> bool {
      pCursor = pBufferedBytes.size();
      return true;
    };
    if (!IsSafeRelativePathForSingleByteTypeGap(pPath, pLayout.mMaxPathLength)) {
      return false;
    }
    if (pType == static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFolder)) {
      return true;
    }
    if (pType == static_cast<std::uint8_t>(TypedRecordTypeV2::kDataReference)) {
      if (pCursor >= pBufferedBytes.size()) {
        return true;
      }
      const std::uint8_t aReferenceKind = pBufferedBytes[pCursor++];
      if (!IsKnownReferenceRecordKindV2(aReferenceKind)) {
        return false;
      }
      if ((pBufferedBytes.size() - pCursor) < 2u) {
        return ConsumeRemainingBufferedBytes();
      }
      const std::uint16_t aTargetLength = static_cast<std::uint16_t>(
          static_cast<std::uint16_t>(pBufferedBytes[pCursor]) |
          (static_cast<std::uint16_t>(pBufferedBytes[pCursor + 1u]) << 8u));
      pCursor += 2u;
      if (aTargetLength == 0u || aTargetLength > pLayout.mMaxPathLength) {
        return false;
      }
      if ((pBufferedBytes.size() - pCursor) < aTargetLength) {
        return ConsumeRemainingBufferedBytes();
      }
      const std::string aTargetPath(
          reinterpret_cast<const char*>(pBufferedBytes.data() + pCursor),
          static_cast<std::size_t>(aTargetLength));
      pCursor += static_cast<std::size_t>(aTargetLength);
      return IsSafeReferenceTargetForSingleByteTypeGap(
          aTargetPath, pLayout.mMaxPathLength);
    }
    if (pType != static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFile)) {
      return false;
    }
    if ((pBufferedBytes.size() - pCursor) < 8u) {
      return ConsumeRemainingBufferedBytes();
    }
    std::uint64_t aContentLength = 0u;
    for (std::size_t aByte = 0u; aByte < 8u; ++aByte) {
      aContentLength |=
          static_cast<std::uint64_t>(pBufferedBytes[pCursor + aByte]) << (8u * aByte);
    }
    pCursor += 8u;
    if ((pBufferedBytes.size() - pCursor) < aContentLength) {
      return ConsumeRemainingBufferedBytes();
    }
    pCursor += static_cast<std::size_t>(aContentLength);
    return true;
  };

  std::size_t aCursor = 0u;
  if (!ParseRecordBody(pFirstPath, pInjectedType, aCursor)) {
    return false;
  }

  while (aCursor < pBufferedBytes.size()) {
    if ((pBufferedBytes.size() - aCursor) < 2u) {
      return true;
    }
    const std::uint16_t aPathLength = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(pBufferedBytes[aCursor]) |
        (static_cast<std::uint16_t>(pBufferedBytes[aCursor + 1u]) << 8u));
    aCursor += 2u;
    if (aPathLength == 0u || aPathLength > pLayout.mMaxPathLength) {
      return false;
    }
    if ((pBufferedBytes.size() - aCursor) < aPathLength) {
      return true;
    }
    const std::string aPath(
        reinterpret_cast<const char*>(pBufferedBytes.data() + aCursor),
        static_cast<std::size_t>(aPathLength));
    aCursor += static_cast<std::size_t>(aPathLength);
    if (!IsSafeRelativePathForSingleByteTypeGap(aPath, pLayout.mMaxPathLength)) {
      return false;
    }
    if (aCursor >= pBufferedBytes.size()) {
      return true;
    }
    const std::uint8_t aType = pBufferedBytes[aCursor++];
    if (aType != static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFile) &&
        aType != static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFolder) &&
        aType != static_cast<std::uint8_t>(TypedRecordTypeV2::kDataReference)) {
      return false;
    }
    if (!ParseRecordBody(aPath, aType, aCursor)) {
      return false;
    }
  }
  return true;
}

enum class SingleByteTypeGapResolutionV2 {
  kNeedMore = 0,
  kResolvedFile = 1,
  kResolvedUnsupported = 2,
  kImpossible = 3,
};

SingleByteTypeGapResolutionV2 ResolveSingleByteTypeGap(
    const std::string& pCurrentPath,
    const std::vector<unsigned char>& pBufferedBytes,
    const memory_layout::ArchiveLayoutConfigV2& pLayout) {
  const bool aFileValid = ParseDataPrefixWithInjectedTypeForSingleByteGap(
      pCurrentPath,
      static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFile),
      pBufferedBytes,
      pLayout);
  const bool aFolderValid = ParseDataPrefixWithInjectedTypeForSingleByteGap(
      pCurrentPath,
      static_cast<std::uint8_t>(TypedRecordTypeV2::kDataFolder),
      pBufferedBytes,
      pLayout);
  const bool aReferenceValid = ParseDataPrefixWithInjectedTypeForSingleByteGap(
      pCurrentPath,
      static_cast<std::uint8_t>(TypedRecordTypeV2::kDataReference),
      pBufferedBytes,
      pLayout);

  const int aValidCount = (aFileValid ? 1 : 0) + (aFolderValid ? 1 : 0) +
                          (aReferenceValid ? 1 : 0);
  if (aValidCount == 0) {
    return SingleByteTypeGapResolutionV2::kImpossible;
  }
  if (aValidCount != 1) {
    return SingleByteTypeGapResolutionV2::kNeedMore;
  }
  if (aFileValid) {
    return SingleByteTypeGapResolutionV2::kResolvedFile;
  }
  return SingleByteTypeGapResolutionV2::kResolvedUnsupported;
}

bool TryApplyRecoverSkipRecord(DecodeStageContextV2& pContext,
                               DecodeArchiveDecodeCursorV2& pCursor,
                               const SectionHeaderV2& pSectionHeader,
                               std::size_t pCurrentArchiveSlot,
                               std::uint64_t pCurrentPhysicalBlockIndex,
                               std::uint64_t pCurrentLogicalBlockIndex,
                               std::size_t pCurrentPayloadStart,
                               std::size_t pCurrentPayloadEnd);

bool TryApplyRecoverLocalSkipAnchor(DecodeStageContextV2& pContext,
                                    DecodeArchiveDecodeCursorV2& pCursor,
                                    const SectionHeaderV2& pSectionHeader,
                                    std::size_t pCurrentArchiveSlot,
                                    std::uint64_t pCurrentLogicalBlockIndex,
                                    std::size_t pCurrentPayloadStart,
                                    std::size_t pCurrentPayloadEnd);

bool HandleDamagedBlock(DecodeStageContextV2& pContext,
                        const std::string& pReason) {
  if (pContext.Request().mIntent == DecodeIntentV2::kUnbundle) {
    DecodeCancelStateV2& aCancel = pContext.State().mCancel;
    if (!aCancel.mObserved) {
      aCancel = DecodeCancelStateV2{};
      aCancel.mObserved = true;
      aCancel.mShouldFinalizeAfterCancel = true;
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Encountered damaged data; stopping unbundle at first error: " +
              pReason);
    }
    pContext.RequestBatchYield();
    return true;
  }

  if (!ShouldContinuePastDamagedBlock(pContext)) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                         ProgressStageV2::kArchiveDecode,
                         pReason));
    return false;
  }

  pContext.RequestBatchYield();
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeSkipJump)) {
    RuntimeEventV2 aEvent;
    aEvent.mKind = RuntimeEventKindV2::kDecodeSkipJump;
    aEvent.mStage = ProgressStageV2::kArchiveDecode;
    aEvent.mLabel = "Decode skip-jump: " + pReason;
    aEvent.SetInfo("reason", pReason);
    pContext.EmitRuntimeEvent(aEvent);
  }
  return true;
}

bool ShouldStopAfterFirstDamagedBlock(const DecodeStageContextV2& pContext) {
  return pContext.Request().mIntent == DecodeIntentV2::kUnbundle;
}

bool HandleArchiveDecodeCancel(DecodeStageContextV2& pContext,
                               DecodeLogicalRecordDecoderV2& pFileDecoder,
                               const std::string& pFileReferenceAtBlockStart,
                               const std::string& pFileReferenceAfterBlock) {
  (void)pFileReferenceAfterBlock;
  if (!pContext.IsCancelRequested()) {
    return false;
  }

  DecodeCancelStateV2& aCancel = pContext.State().mCancel;
  if (!aCancel.mObserved) {
    aCancel = DecodeCancelStateV2{};
    aCancel.mObserved = true;
    aCancel.mCancelFileReference = pFileReferenceAtBlockStart;
    if (aCancel.mCancelFileReference.empty()) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel requested at a block boundary.");
    } else {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel requested while reading '" +
              aCancel.mCancelFileReference +
              "'; stopping after this block.");
    }
  }

  if (pFileDecoder.IsInsideFile() && !pFileDecoder.AbortCurrentFile()) {
    if (!aCancel.mCancelFileReference.empty()) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel could not tag the partial file '" +
              aCancel.mCancelFileReference + "'.");
    } else {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel could not tag the partial output file.");
    }
  }

  aCancel.mShouldFinalizeAfterCancel = true;
  if (!aCancel.mCancelFileReference.empty()) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Cancel will stop after this block while reading '" +
            aCancel.mCancelFileReference + "'.");
  }
  return true;
}

}  // namespace

inline constexpr std::uint16_t kDeepRecoverPackedBlocksPerArchiveV2 = 1000u;

struct DeepRecoverPackedBlockRefV2 {
  static constexpr std::uint32_t kInvalidTempArchiveIndex =
      std::numeric_limits<std::uint32_t>::max();
  static constexpr std::uint16_t kInvalidTempBlockIndex =
      std::numeric_limits<std::uint16_t>::max();

  std::uint32_t mTempArchiveIndex = kInvalidTempArchiveIndex;
  std::uint16_t mTempBlockIndex = kInvalidTempBlockIndex;
  std::uint16_t mReserved = 0u;

  bool IsValid() const {
    return mTempArchiveIndex != kInvalidTempArchiveIndex &&
           mTempBlockIndex != kInvalidTempBlockIndex;
  }

  void Clear() {
    mTempArchiveIndex = kInvalidTempArchiveIndex;
    mTempBlockIndex = kInvalidTempBlockIndex;
    mReserved = 0u;
  }
};

struct DeepRecoverPackedArchiveV2 {
  std::string mPath;
  std::uint16_t mBlockCount = 0u;
  std::uint16_t mReserved = 0u;
  std::uint32_t mLiveCount = 0u;
  bool mSealed = false;
};

class DecodeArchiveDecodeCursorV2 {
 public:
  static constexpr std::size_t kInvalidArchiveSlot =
      std::numeric_limits<std::size_t>::max();

  explicit DecodeArchiveDecodeCursorV2(DecodeStageContextV2& pContext)
      : mFolderDecoder(
            pContext.Request().mDestinationDirectory,
            pContext.FileSystem(),
            pContext.Layout(),
            DecodeLogicalZoneV2::kFolderManifest,
            ProgressStageV2::kArchiveDecode,
            RuntimeEventKindV2::kDecodeFolderStarted,
            RuntimeEventKindV2::kDecodeFolderFinished,
            [&pContext](const RuntimeEventV2& pEvent) {
              return pContext.EmitRuntimeEvent(pEvent);
            }),
        mFileDecoder(
            pContext.Request().mDestinationDirectory,
            pContext.FileSystem(),
            pContext.Layout(),
            DecodeLogicalZoneV2::kData,
            ProgressStageV2::kArchiveDecode,
            RuntimeEventKindV2::kDecodeFileStarted,
            RuntimeEventKindV2::kDecodeFileFinished,
            [&pContext](const RuntimeEventV2& pEvent) {
              return pContext.EmitRuntimeEvent(pEvent);
            }),
        mPreviewDecoder(
            pContext.Request().mDestinationDirectory,
            pContext.FileSystem(),
            pContext.Layout(),
            DecodeLogicalZoneV2::kPreviewManifest,
            ProgressStageV2::kArchiveDecode,
            RuntimeEventKindV2::kDecodeManifestItemStarted,
            RuntimeEventKindV2::kDecodeManifestItemFinished,
            [&pContext](const RuntimeEventV2& pEvent) {
              return pContext.EmitRuntimeEvent(pEvent);
            }),
        mBlockBytes(pContext.Layout().mArchiveBlockBytes),
        mDeepDecodeBlockBytes(pContext.Layout().mArchiveBlockBytes),
        mDeepResumeProbeRawBytes(pContext.Layout().mArchiveBlockBytes),
        mDeepResumeProbeDecodeBytes(pContext.Layout().mArchiveBlockBytes) {
    const std::vector<DiscoveredArchiveFileV2>& aArchives =
        pContext.State().mDiscovery.mArchives;
    mArchiveLostBlocksBySlot.assign(
        aArchives.size(), 0u);
    mDeepArchiveNonRepairPrefixBySlot.assign(aArchives.size(), 0u);
    mDeepArchiveNonRepairCountBySlot.assign(aArchives.size(), 0u);
    std::uint64_t aMaxArchiveIndex = 0u;
    for (const DiscoveredArchiveFileV2& aArchive : aArchives) {
      aMaxArchiveIndex = std::max(aMaxArchiveIndex, aArchive.mArchiveIndex);
    }
    if (!aArchives.empty() &&
        aMaxArchiveIndex < static_cast<std::uint64_t>(16u * 1024u * 1024u)) {
      mArchiveSlotByArchiveIndex.assign(
          static_cast<std::size_t>(aMaxArchiveIndex + 1u), kInvalidArchiveSlot);
      for (std::size_t aSlot = 0u; aSlot < aArchives.size(); ++aSlot) {
        const std::uint64_t aArchiveIndex = aArchives[aSlot].mArchiveIndex;
        if (aArchiveIndex >= mArchiveSlotByArchiveIndex.size()) {
          continue;
        }
        if (mArchiveSlotByArchiveIndex[static_cast<std::size_t>(aArchiveIndex)] ==
            kInvalidArchiveSlot) {
          mArchiveSlotByArchiveIndex[static_cast<std::size_t>(aArchiveIndex)] = aSlot;
        }
      }
    }
    std::uint64_t aRemainingNonRepair =
        pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount +
        pContext.State().mBootstrap.mExpectedArchiveDataBlockCount;
    std::uint64_t aNonRepairPrefix = 0u;
    for (std::size_t aSlot = 0u; aSlot < aArchives.size(); ++aSlot) {
      const DiscoveredArchiveFileV2& aArchive = aArchives[aSlot];
      const std::uint64_t aExpectedBlocks =
          aArchive.mArchiveBlockCount != 0u ? aArchive.mArchiveBlockCount
                                            : aArchive.mReadableBlockCount;
      const std::uint64_t aNonRepairHere =
          std::min(aExpectedBlocks, aRemainingNonRepair);
      mDeepArchiveNonRepairPrefixBySlot[aSlot] = aNonRepairPrefix;
      mDeepArchiveNonRepairCountBySlot[aSlot] = aNonRepairHere;
      aNonRepairPrefix += aNonRepairHere;
      aRemainingNonRepair -= aNonRepairHere;
    }
    mDeepRegularSlotFilledByArchive.resize(aArchives.size());
    mDeepRegularFilledCountByArchive.assign(aArchives.size(), 0u);
    mDeepArchiveSealedBySlot.assign(aArchives.size(), 0u);
    mDeepArchiveSealFloorBySlot.assign(
        aArchives.size(), std::numeric_limits<std::uint64_t>::max());
    for (std::size_t aSlot = 0u; aSlot < aArchives.size(); ++aSlot) {
      const std::uint64_t aRegularSlotCount =
          mDeepArchiveNonRepairCountBySlot[aSlot];
      if (aRegularSlotCount == 0u) {
        continue;
      }
      const std::size_t aTrackCount = static_cast<std::size_t>(
          std::min<std::uint64_t>(
              aRegularSlotCount,
              static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())));
      mDeepRegularSlotFilledByArchive[aSlot].assign(aTrackCount, 0u);
    }
    mNextArchiveLog = kDecodeProgressArchiveLogIntervalV2;
    mNextFileLog = kDecodeProgressFileLogIntervalV2;
    mNextFolderLog = kDecodeProgressFolderLogIntervalV2;
    mNextByteLog = kDecodeProgressByteLogIntervalV2;
  }

  DecodeLogicalRecordDecoderV2 mFolderDecoder;
  DecodeLogicalRecordDecoderV2 mFileDecoder;
  DecodeLogicalRecordDecoderV2 mPreviewDecoder;
  FixedBlockBufferV2 mBlockBytes;
  FixedBlockBufferV2 mDeepDecodeBlockBytes;
  FixedBlockBufferV2 mDeepResumeProbeRawBytes;
  FixedBlockBufferV2 mDeepResumeProbeDecodeBytes;
  std::unique_ptr<FileReadStreamV2> mRead;
  std::unique_ptr<FileReadStreamV2> mDeepMappedRead;
  std::unique_ptr<FileReadStreamV2> mDeepScanRead;
  std::size_t mArchiveSlot = 0u;
  std::uint64_t mBlockIndex = 0u;
  std::size_t mDeepScanArchiveSlot = 0u;
  std::uint64_t mDeepScanBlockIndex = 0u;
  std::uint64_t mArchiveBlocksRead = 0u;
  std::uint64_t mProcessedBlocks = 0u;
  std::uint64_t mArchivesCompleted = 0u;
  std::uint64_t mNextArchiveLog = 0u;
  std::uint64_t mNextFileLog = 0u;
  std::uint64_t mNextFolderLog = 0u;
  std::uint64_t mNextByteLog = 0u;
  bool mHasOpenFileContinuation = false;
  std::size_t mContinuationArchiveSlot = 0u;
  std::uint64_t mContinuationBlockIndex = 0u;
  bool mArchiveAnnounced = false;
  bool mHasPausedBlockBoundary = false;
  SectionHeaderV2 mPausedSectionHeader{};
  std::size_t mPausedBlockPayloadOffset = 0u;
  std::size_t mPausedBlockPayloadEnd = 0u;
  std::string mPausedBoundaryRecordReference;
  bool mHasForcedBlockPayloadStart = false;
  std::size_t mForcedBlockPayloadStart = 0u;
  bool mPendingRecoverResync = false;
  bool mPendingSingleByteTypeGap = false;
  bool mHasPendingSkipLandingValidation = false;
  std::size_t mPendingSkipSourceArchiveSlot = 0u;
  std::uint64_t mPendingSkipSourceBlockIndex = 0u;
  std::size_t mPendingSkipTargetArchiveSlot = 0u;
  std::uint64_t mPendingSkipTargetBlockIndex = 0u;
  std::uint64_t mPendingSkipTargetLogicalBlockIndex = 0u;
  std::size_t mPendingSkipTargetPayloadOffset = 0u;
  std::size_t mLastSkipFallbackSourceArchiveSlot = kInvalidArchiveSlot;
  std::uint64_t mLastSkipFallbackSourceBlockIndex =
      std::numeric_limits<std::uint64_t>::max();
  std::vector<unsigned char> mPendingSingleByteTypePayload;
  std::vector<std::uint64_t> mArchiveLostBlocksBySlot;
  std::vector<std::size_t> mArchiveSlotByArchiveIndex;
  std::vector<std::uint64_t> mDeepArchiveNonRepairPrefixBySlot;
  std::vector<std::uint64_t> mDeepArchiveNonRepairCountBySlot;
  std::vector<std::uint64_t> mDeepRegularFilledCountByArchive;
  std::vector<std::vector<std::uint8_t>> mDeepRegularSlotFilledByArchive;
  std::vector<std::vector<std::uint8_t>> mDeepPhysicalSlotFilledByArchive;
  std::vector<std::vector<DeepRecoverPackedBlockRefV2>> mDeepPackedBlockRefsByArchive;
  std::vector<DeepRecoverPackedArchiveV2> mDeepPackedArchives;
  std::vector<std::uint8_t> mDeepArchiveSealedBySlot;
  std::vector<std::uint64_t> mDeepArchiveSealFloorBySlot;
  std::uint32_t mDeepMappedReadArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  std::string mDeepTempRoot;
  bool mDeepScanActive = false;
  bool mDeepTriggered = false;
  bool mDeepScanExhausted = false;
  bool mDeepDecodeFromTemp = false;
  bool mDeepOutputReplayActive = false;
  bool mReadFromDeepTemp = false;
  std::uint64_t mDeepStagedBlockCount = 0u;
  std::uint64_t mDeepStagedByteCount = 0u;
  std::uint64_t mNextHealingProgressByteLog = 0u;
  bool mDeepCancelFinalizeRequested = false;
};

namespace {

std::string MakeUniquePath(FileSystemV2& pFileSystem,
                           const std::string& pDirectory,
                           const std::string& pBaseName,
                           const std::string& pExtension) {
  std::string aCandidateName = pBaseName + pExtension;
  std::string aCandidatePath = pFileSystem.JoinPath(pDirectory, aCandidateName);
  if (!pFileSystem.Exists(aCandidatePath)) {
    return aCandidatePath;
  }

  for (std::uint64_t aSuffix = 1u; aSuffix < 1000000u; ++aSuffix) {
    aCandidateName = pBaseName + "_" + std::to_string(aSuffix) + pExtension;
    aCandidatePath = pFileSystem.JoinPath(pDirectory, aCandidateName);
    if (!pFileSystem.Exists(aCandidatePath)) {
      return aCandidatePath;
    }
  }

  return pFileSystem.JoinPath(
      pDirectory, pBaseName + "_" + std::to_string(std::numeric_limits<std::uint64_t>::max()) +
                      pExtension);
}

void ResetPendingSkipLandingValidation(DecodeArchiveDecodeCursorV2& pCursor) {
  pCursor.mHasPendingSkipLandingValidation = false;
  pCursor.mPendingSkipSourceArchiveSlot = 0u;
  pCursor.mPendingSkipSourceBlockIndex = 0u;
  pCursor.mPendingSkipTargetArchiveSlot = 0u;
  pCursor.mPendingSkipTargetBlockIndex = 0u;
  pCursor.mPendingSkipTargetLogicalBlockIndex = 0u;
  pCursor.mPendingSkipTargetPayloadOffset = 0u;
}

void EmitAppliedRecoverSkipRecordLog(DecodeStageContextV2& pContext,
                                     std::size_t pTargetArchiveSlot,
                                     std::uint64_t pTargetLogicalBlockIndex,
                                     std::uint64_t pTargetPhysicalBlockIndex,
                                     std::size_t pTargetPayloadOffset) {
  pContext.EmitLog(
      LogLevelV2::kInfo,
      DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
          " Applied skip record jump to archive slot " +
          std::to_string(pTargetArchiveSlot) + ", block " +
          std::to_string(pTargetLogicalBlockIndex) + " (physical " +
          std::to_string(pTargetPhysicalBlockIndex) + "), payload offset " +
          std::to_string(pTargetPayloadOffset) + ".");
}

bool ShouldLogSkipLandingFallback(DecodeArchiveDecodeCursorV2& pCursor,
                                  std::size_t pSourceArchiveSlot,
                                  std::uint64_t pSourceBlockIndex) {
  if (pCursor.mLastSkipFallbackSourceArchiveSlot == pSourceArchiveSlot &&
      pCursor.mLastSkipFallbackSourceBlockIndex == pSourceBlockIndex) {
    return false;
  }
  pCursor.mLastSkipFallbackSourceArchiveSlot = pSourceArchiveSlot;
  pCursor.mLastSkipFallbackSourceBlockIndex = pSourceBlockIndex;
  return true;
}

void ResetHealingProgress(DecodeArchiveDecodeCursorV2& pCursor) {
  pCursor.mDeepStagedBlockCount = 0u;
  pCursor.mDeepStagedByteCount = 0u;
  pCursor.mNextHealingProgressByteLog = kHealingProgressByteLogIntervalV2;
}

void EmitHealingProgressLog(DecodeStageContextV2& pContext,
                            const DecodeArchiveDecodeCursorV2& pCursor) {
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogHealingScanProgressV2(
                       pCursor.mDeepStagedBlockCount, pCursor.mDeepStagedByteCount));
}

void MaybeEmitHealingProgressLog(DecodeStageContextV2& pContext,
                                 DecodeArchiveDecodeCursorV2& pCursor,
                                 bool pForce = false) {
  if (!pForce && pCursor.mDeepStagedByteCount < pCursor.mNextHealingProgressByteLog) {
    return;
  }
  EmitHealingProgressLog(pContext, pCursor);
  while (pCursor.mNextHealingProgressByteLog <= pCursor.mDeepStagedByteCount) {
    pCursor.mNextHealingProgressByteLog += kHealingProgressByteLogIntervalV2;
  }
}

void NoteDeepRecoverStagedPhysicalBlock(DecodeStageContextV2& pContext,
                                        DecodeArchiveDecodeCursorV2& pCursor,
                                        bool pWasAlreadyFilled) {
  if (pWasAlreadyFilled) {
    return;
  }
  ++pCursor.mDeepStagedBlockCount;
  pCursor.mDeepStagedByteCount +=
      static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes);
  MaybeEmitHealingProgressLog(pContext, pCursor);
}

void EmitDecodeHealingModeEnteredEvent(DecodeStageContextV2& pContext,
                                       const DecodeArchiveDecodeCursorV2& pCursor,
                                       const std::string& pReason) {
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeHealingModeEntered)) {
    RuntimeEventV2 aEvent;
    aEvent.mKind = RuntimeEventKindV2::kDecodeHealingModeEntered;
    aEvent.mStage = ProgressStageV2::kArchiveDecode;
    aEvent.mLabel = "Decode switched to healing mode.";
    aEvent.SetInfo("reason", pReason);
    aEvent.SetInfo("waiting_archive_slot",
                   static_cast<std::uint64_t>(pCursor.mArchiveSlot));
    aEvent.SetInfo("waiting_block_index", pCursor.mBlockIndex);
    aEvent.SetInfo("temp_root", pCursor.mDeepTempRoot);
    pContext.EmitRuntimeEvent(aEvent);
  }
  pContext.RequestBatchYield();
}

void EmitDecodeHealingArchiveSealedEvent(DecodeStageContextV2& pContext,
                                         const DecodeArchiveDecodeCursorV2& pCursor,
                                         const DiscoveredArchiveFileV2& pArchive,
                                         std::size_t pArchiveSlot) {
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeHealingArchiveSealed)) {
    RuntimeEventV2 aEvent;
    aEvent.mKind = RuntimeEventKindV2::kDecodeHealingArchiveSealed;
    aEvent.mStage = ProgressStageV2::kArchiveDecode;
    aEvent.mLabel = "Decode sealed healing archive " + std::to_string(pArchiveSlot) + ".";
    aEvent.SetInfo("archive_slot", static_cast<std::uint64_t>(pArchiveSlot));
    aEvent.SetInfo("archive_index", pArchive.mArchiveIndex);
    if (pArchiveSlot < pCursor.mDeepArchiveNonRepairCountBySlot.size()) {
      aEvent.SetInfo("regular_block_count",
                     pCursor.mDeepArchiveNonRepairCountBySlot[pArchiveSlot]);
    }
    if (pArchiveSlot < pCursor.mDeepRegularFilledCountByArchive.size()) {
      aEvent.SetInfo("regular_blocks_filled",
                     pCursor.mDeepRegularFilledCountByArchive[pArchiveSlot]);
    }
    pContext.EmitRuntimeEvent(aEvent);
  }
  pContext.RequestBatchYield();
}

void EmitDecodeHealingModeExitedEvent(DecodeStageContextV2& pContext,
                                      const DecodeArchiveDecodeCursorV2& pCursor,
                                      const char* pResumeMode) {
  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeHealingModeExited)) {
    RuntimeEventV2 aEvent;
    aEvent.mKind = RuntimeEventKindV2::kDecodeHealingModeExited;
    aEvent.mStage = ProgressStageV2::kArchiveDecode;
    aEvent.mLabel = "Decode switched from healing mode.";
    aEvent.SetInfo("resume_archive_slot",
                   static_cast<std::uint64_t>(pCursor.mArchiveSlot));
    aEvent.SetInfo("resume_block_index", pCursor.mBlockIndex);
    aEvent.SetInfo("resume_mode", pResumeMode);
    pContext.EmitRuntimeEvent(aEvent);
  }
  pContext.RequestBatchYield();
}

bool CanActivateDeepRecoverHealing(const DecodeStageContextV2& pContext,
                                   const DecodeArchiveDecodeCursorV2& pCursor) {
  if (pContext.Request().mIntent == DecodeIntentV2::kUnbundle ||
      !DecodeIntentAllowsSalvageV2(pContext.Request().mIntent)) {
    return false;
  }
  if (pCursor.mDeepOutputReplayActive || pCursor.mDeepScanExhausted ||
      pCursor.mDeepScanActive) {
    return false;
  }
  return !pCursor.mDeepTriggered || pCursor.mDeepDecodeFromTemp;
}

std::uint64_t KnownLostBlocksForArchive(
    const DecodeArchiveDecodeCursorV2& pCursor,
    std::size_t pArchiveSlot) {
  if (pArchiveSlot >= pCursor.mArchiveLostBlocksBySlot.size()) {
    return 0u;
  }
  return pCursor.mArchiveLostBlocksBySlot[pArchiveSlot];
}

std::uint64_t ExpectedLogicalBlockIndexForPhysical(
    const DecodeArchiveDecodeCursorV2& pCursor,
    std::size_t pArchiveSlot,
    std::uint64_t pPhysicalBlockIndex) {
  return pPhysicalBlockIndex + KnownLostBlocksForArchive(pCursor, pArchiveSlot);
}

std::uint64_t LogicalToPhysicalBlockIndexWithKnownLoss(
    const DecodeArchiveDecodeCursorV2& pCursor,
    std::size_t pArchiveSlot,
    std::uint64_t pLogicalBlockIndex) {
  const std::uint64_t aKnownLost = KnownLostBlocksForArchive(pCursor, pArchiveSlot);
  if (pLogicalBlockIndex <= aKnownLost) {
    return 0u;
  }
  return pLogicalBlockIndex - aKnownLost;
}

std::size_t ResolveArchiveSlotForArchiveIndex(
    const DecodeArchiveDecodeCursorV2& pCursor,
    std::uint64_t pArchiveIndex) {
  if (pArchiveIndex >= pCursor.mArchiveSlotByArchiveIndex.size()) {
    return DecodeArchiveDecodeCursorV2::kInvalidArchiveSlot;
  }
  return pCursor.mArchiveSlotByArchiveIndex[static_cast<std::size_t>(pArchiveIndex)];
}

std::uint64_t ArchivePhysicalBlockCapacityFromFileLength(
    const DecodeStageContextV2& pContext,
    const DiscoveredArchiveFileV2& pArchive) {
  if (pArchive.mFileLength <= static_cast<std::uint64_t>(kArchiveHeaderBytesV2)) {
    return 0u;
  }
  const std::uint64_t aPayloadBytes =
      pArchive.mFileLength - static_cast<std::uint64_t>(kArchiveHeaderBytesV2);
  return aPayloadBytes / static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes);
}

std::uint64_t DeepRecoverTempBlockCapacity(
    const DecodeStageContextV2& pContext,
    const DiscoveredArchiveFileV2& pArchive) {
  const std::uint64_t aReadableFromFile =
      ArchivePhysicalBlockCapacityFromFileLength(pContext, pArchive);
  const std::uint64_t aDeclared =
      pArchive.mArchiveBlockCount != 0u ? pArchive.mArchiveBlockCount
                                        : pArchive.mReadableBlockCount;
  std::uint64_t aCapacity = std::max(aReadableFromFile, aDeclared);
  const std::uint64_t aMaxPerArchive =
      static_cast<std::uint64_t>(pContext.Layout().mMaxBlocksPerArchive);
  if (aMaxPerArchive > 0u && aCapacity > aMaxPerArchive) {
    aCapacity = aMaxPerArchive;
  }
  return aCapacity;
}

bool BuildSyntheticArchiveHeaderForDeepRecover(
    const DecodeStageContextV2& pContext,
    std::uint64_t pArchiveIndex,
    ArchiveHeaderV2& pOutHeader,
    std::string& pOutError) {
  pOutError.clear();
  pOutHeader = pContext.State().mBootstrap.mFirstHeader;
  pOutHeader.mDirtyState =
      static_cast<std::uint8_t>(ArchiveDirtyStateV2::kFinishedWithError);
  pOutHeader.mIsEncrypted = 0u;

  if (!TrySetPackedUint48(pOutHeader.mArchiveIndex,
                          pArchiveIndex,
                          nullptr,
                          "ArchiveIndex") ||
      !TrySetPackedUint48(pOutHeader.mArchiveCount,
                          pContext.State().mBootstrap.mExpectedArchiveCount,
                          nullptr,
                          "ArchiveCount") ||
      !TrySetPackedUint48(pOutHeader.mBlockCountMain,
                          pContext.State().mBootstrap.mExpectedArchiveDataBlockCount,
                          nullptr,
                          "BlockCountMain") ||
      !TrySetPackedUint48(pOutHeader.mReservedCount0,
                          0u,
                          nullptr,
                          "ReservedCount0") ||
      !TrySetPackedUint48(pOutHeader.mBlockCountPreview,
                          pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount,
                          nullptr,
                          "BlockCountPreview") ||
      !TrySetPackedUint48(pOutHeader.mBlockCountRepair,
                          pContext.State().mBootstrap.mExpectedRepairBlockCount,
                          nullptr,
                          "BlockCountRepair")) {
    pOutError = "synthetic archive header values were out of range";
    return false;
  }
  return true;
}

bool DecodeValidatedSectionHeaderFromRawBlockForDeepRecover(
    DecodeStageContextV2& pContext,
    const unsigned char* pRawBlockBytes,
    FixedBlockBufferV2& pDecodeBuffer,
    SectionHeaderV2& pOutHeader,
    std::string& pOutError) {
  pOutError.clear();
  if (pRawBlockBytes == nullptr || pDecodeBuffer.Empty()) {
    pOutError = "block buffers were unavailable for deep recover validation";
    return false;
  }

  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (pDecodeBuffer.Size() < aArchiveBlockBytes) {
    pOutError = "decode buffer was too small for archive block bytes";
    return false;
  }

  // Preview-manifest blocks can be plaintext even for encrypted jobs.
  if (TryReadValidatedSectionHeader(
          pRawBlockBytes, aSectionPayloadBytes, pOutHeader) &&
      pOutHeader.mSectionType ==
          static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest)) {
    return true;
  }

  const bool aEncrypted = pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u;
  const unsigned char* aParseBytes = pRawBlockBytes;
  if (aEncrypted) {
    std::memcpy(pDecodeBuffer.Data(), pRawBlockBytes, aArchiveBlockBytes);
    std::string aUnsealError;
    if (pContext.State().mCipher.mWorkerBuffer.Size() < aArchiveBlockBytes ||
        !pContext.State().mCipher.mCipher.Unseal(
            pDecodeBuffer.Data(),
            pContext.State().mCipher.mWorkerBuffer.Data(),
            pDecodeBuffer.Data(),
            aArchiveBlockBytes,
            &aUnsealError)) {
      pOutError = aUnsealError.empty()
                      ? "block failed decryption/checksum validation"
                      : "block failed decryption/checksum validation: " +
                            aUnsealError;
      return false;
    }
    aParseBytes = pDecodeBuffer.Data();
  }

  SectionHeaderV2 aReadableHeader;
  if (!ReadSectionHeader(aParseBytes, kSectionHeaderBytesV2, aReadableHeader, nullptr)) {
    pOutError = "block header structure/validation failed";
    return false;
  }
  if (!ValidateSectionCheckSum(
          aReadableHeader, aParseBytes + kSectionHeaderBytesV2, aSectionPayloadBytes)) {
    pOutError = "block header checksum/validation failed";
    return false;
  }
  pOutHeader = aReadableHeader;
  return true;
}

bool RepairRecordLooksUnset(const RepairRecordV2& pRepairRecord) {
  return pRepairRecord.mArchiveIndex == std::numeric_limits<std::uint16_t>::max() &&
         pRepairRecord.mBlockIndex == std::numeric_limits<std::uint16_t>::max();
}

bool ResolvePatchedSectionTypeForDeepRecoverTarget(
    const DecodeStageContextV2& pContext,
    const DecodeArchiveDecodeCursorV2& pCursor,
    std::size_t pTargetArchiveSlot,
    std::uint64_t pTargetBlockIndex,
    std::uint8_t& pOutSectionType) {
  pOutSectionType = static_cast<std::uint8_t>(SectionTypeV2::kArchiveData);
  if (pTargetArchiveSlot >= pCursor.mDeepArchiveNonRepairCountBySlot.size() ||
      pTargetArchiveSlot >= pCursor.mDeepArchiveNonRepairPrefixBySlot.size()) {
    return false;
  }

  const std::uint64_t aNonRepairInArchive =
      pCursor.mDeepArchiveNonRepairCountBySlot[pTargetArchiveSlot];
  if (pTargetBlockIndex >= aNonRepairInArchive) {
    return false;
  }

  const std::uint64_t aGlobalNonRepairIndex =
      pCursor.mDeepArchiveNonRepairPrefixBySlot[pTargetArchiveSlot] + pTargetBlockIndex;
  if (aGlobalNonRepairIndex <
      pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount) {
    pOutSectionType = static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest);
  } else {
    pOutSectionType = static_cast<std::uint8_t>(SectionTypeV2::kArchiveData);
  }
  return true;
}

bool IsDeepRecoverRegularSlot(const DecodeArchiveDecodeCursorV2& pCursor,
                              std::size_t pArchiveSlot,
                              std::uint64_t pBlockIndex) {
  if (pArchiveSlot >= pCursor.mDeepArchiveNonRepairCountBySlot.size()) {
    return false;
  }
  return pBlockIndex < pCursor.mDeepArchiveNonRepairCountBySlot[pArchiveSlot];
}

bool IsDeepRecoverRegularSlotFilled(const DecodeArchiveDecodeCursorV2& pCursor,
                                    std::size_t pArchiveSlot,
                                    std::uint64_t pBlockIndex) {
  if (!IsDeepRecoverRegularSlot(pCursor, pArchiveSlot, pBlockIndex)) {
    return false;
  }
  if (pArchiveSlot >= pCursor.mDeepRegularSlotFilledByArchive.size()) {
    return false;
  }
  const std::vector<std::uint8_t>& aFlags =
      pCursor.mDeepRegularSlotFilledByArchive[pArchiveSlot];
  if (pBlockIndex >= static_cast<std::uint64_t>(aFlags.size())) {
    return false;
  }
  return aFlags[static_cast<std::size_t>(pBlockIndex)] != 0u;
}

bool IsDeepRecoverArchiveSealed(const DecodeArchiveDecodeCursorV2& pCursor,
                                std::size_t pArchiveSlot) {
  if (pArchiveSlot >= pCursor.mDeepArchiveSealedBySlot.size() ||
      pArchiveSlot >= pCursor.mDeepArchiveSealFloorBySlot.size()) {
    return false;
  }
  return pCursor.mDeepArchiveSealedBySlot[pArchiveSlot] != 0u &&
         pCursor.mDeepArchiveSealFloorBySlot[pArchiveSlot] !=
             std::numeric_limits<std::uint64_t>::max();
}

bool IsDeepRecoverPhysicalSlotFilled(const DecodeArchiveDecodeCursorV2& pCursor,
                                     std::size_t pArchiveSlot,
                                     std::uint64_t pBlockIndex) {
  if (pArchiveSlot >= pCursor.mDeepPhysicalSlotFilledByArchive.size()) {
    return false;
  }
  const std::vector<std::uint8_t>& aFlags =
      pCursor.mDeepPhysicalSlotFilledByArchive[pArchiveSlot];
  if (pBlockIndex >= static_cast<std::uint64_t>(aFlags.size())) {
    return false;
  }
  return aFlags[static_cast<std::size_t>(pBlockIndex)] != 0u;
}

bool TryResolveDeepRecoverPackedBlock(const DecodeArchiveDecodeCursorV2& pCursor,
                                      std::size_t pArchiveSlot,
                                      std::uint64_t pBlockIndex,
                                      DeepRecoverPackedBlockRefV2& pOutRef) {
  pOutRef.Clear();
  if (pArchiveSlot >= pCursor.mDeepPackedBlockRefsByArchive.size()) {
    return false;
  }
  const std::vector<DeepRecoverPackedBlockRefV2>& aRefs =
      pCursor.mDeepPackedBlockRefsByArchive[pArchiveSlot];
  if (pBlockIndex >= static_cast<std::uint64_t>(aRefs.size())) {
    return false;
  }
  const DeepRecoverPackedBlockRefV2& aRef = aRefs[static_cast<std::size_t>(pBlockIndex)];
  if (!aRef.IsValid() || aRef.mTempArchiveIndex >= pCursor.mDeepPackedArchives.size()) {
    return false;
  }
  if (pCursor.mDeepPackedArchives[aRef.mTempArchiveIndex].mPath.empty()) {
    return false;
  }
  pOutRef = aRef;
  return true;
}

bool MarkDeepRecoverRegularSlotFilled(DecodeArchiveDecodeCursorV2& pCursor,
                                      std::size_t pArchiveSlot,
                                      std::uint64_t pBlockIndex) {
  if (!IsDeepRecoverRegularSlot(pCursor, pArchiveSlot, pBlockIndex)) {
    return false;
  }
  if (pArchiveSlot >= pCursor.mDeepRegularSlotFilledByArchive.size()) {
    return false;
  }
  std::vector<std::uint8_t>& aFlags =
      pCursor.mDeepRegularSlotFilledByArchive[pArchiveSlot];
  if (pBlockIndex >= static_cast<std::uint64_t>(aFlags.size())) {
    return false;
  }
  if (aFlags[static_cast<std::size_t>(pBlockIndex)] != 0u) {
    return false;
  }
  aFlags[static_cast<std::size_t>(pBlockIndex)] = 1u;
  if (pArchiveSlot < pCursor.mDeepRegularFilledCountByArchive.size()) {
    ++pCursor.mDeepRegularFilledCountByArchive[pArchiveSlot];
  }
  return true;
}

void MarkDeepRecoverPhysicalSlotFilled(DecodeArchiveDecodeCursorV2& pCursor,
                                       std::size_t pArchiveSlot,
                                       std::uint64_t pBlockIndex) {
  if (pArchiveSlot >= pCursor.mDeepPhysicalSlotFilledByArchive.size()) {
    return;
  }
  std::vector<std::uint8_t>& aFlags =
      pCursor.mDeepPhysicalSlotFilledByArchive[pArchiveSlot];
  if (pBlockIndex >= static_cast<std::uint64_t>(aFlags.size())) {
    return;
  }
  aFlags[static_cast<std::size_t>(pBlockIndex)] = 1u;
}

bool IsDeepRecoverArchiveFullyPacked(const DecodeArchiveDecodeCursorV2& pCursor,
                                     std::size_t pArchiveSlot) {
  if (pArchiveSlot >= pCursor.mDeepArchiveNonRepairCountBySlot.size() ||
      pArchiveSlot >= pCursor.mDeepRegularFilledCountByArchive.size()) {
    return false;
  }
  const std::uint64_t aRegularSlotCount =
      pCursor.mDeepArchiveNonRepairCountBySlot[pArchiveSlot];
  return aRegularSlotCount > 0u &&
         pCursor.mDeepRegularFilledCountByArchive[pArchiveSlot] >= aRegularSlotCount;
}

bool IsDeepRecoverArchiveFilledFromBlock(const DecodeArchiveDecodeCursorV2& pCursor,
                                         std::size_t pArchiveSlot,
                                         std::uint64_t pStartBlockIndex) {
  if (pArchiveSlot >= pCursor.mDeepRegularSlotFilledByArchive.size()) {
    return false;
  }
  const std::vector<std::uint8_t>& aFlags =
      pCursor.mDeepRegularSlotFilledByArchive[pArchiveSlot];
  if (pStartBlockIndex >= static_cast<std::uint64_t>(aFlags.size())) {
    return false;
  }
  for (std::uint64_t aBlockIndex = pStartBlockIndex;
       aBlockIndex < static_cast<std::uint64_t>(aFlags.size());
       ++aBlockIndex) {
    if (aFlags[static_cast<std::size_t>(aBlockIndex)] == 0u) {
      return false;
    }
  }
  return true;
}

bool TrySealDeepRecoverArchiveIfReady(DecodeStageContextV2& pContext,
                                      DecodeArchiveDecodeCursorV2& pCursor,
                                      std::size_t pArchiveSlot) {
  const std::vector<DiscoveredArchiveFileV2>& aArchives =
      pContext.State().mDiscovery.mArchives;
  if (pArchiveSlot >= aArchives.size()) {
    return false;
  }
  if (IsDeepRecoverArchiveSealed(pCursor, pArchiveSlot) ||
      !IsDeepRecoverArchiveFullyPacked(pCursor, pArchiveSlot)) {
    return false;
  }
  if (!pCursor.mDeepScanExhausted && pCursor.mDeepScanArchiveSlot <= pArchiveSlot) {
    return false;
  }
  pCursor.mDeepArchiveSealedBySlot[pArchiveSlot] = 1u;
  pCursor.mDeepArchiveSealFloorBySlot[pArchiveSlot] = 0u;
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogHealingArchiveSealedV2(static_cast<std::uint64_t>(pArchiveSlot)));
  EmitDecodeHealingArchiveSealedEvent(
      pContext, pCursor, aArchives[pArchiveSlot], pArchiveSlot);
  return true;
}

bool TrySealDeepRecoverArchiveForResumeIfReady(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor,
    std::size_t pArchiveSlot,
    std::uint64_t pStartBlockIndex) {
  const std::vector<DiscoveredArchiveFileV2>& aArchives =
      pContext.State().mDiscovery.mArchives;
  if (pArchiveSlot >= aArchives.size()) {
    return false;
  }
  if (IsDeepRecoverArchiveSealed(pCursor, pArchiveSlot) ||
      !IsDeepRecoverRegularSlot(pCursor, pArchiveSlot, pStartBlockIndex) ||
      !IsDeepRecoverArchiveFilledFromBlock(pCursor, pArchiveSlot, pStartBlockIndex)) {
    return false;
  }
  if (!pCursor.mDeepScanExhausted && pCursor.mDeepScanArchiveSlot <= pArchiveSlot) {
    return false;
  }
  pCursor.mDeepArchiveSealedBySlot[pArchiveSlot] = 1u;
  pCursor.mDeepArchiveSealFloorBySlot[pArchiveSlot] = pStartBlockIndex;
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogHealingArchiveSealedV2(static_cast<std::uint64_t>(pArchiveSlot)));
  EmitDecodeHealingArchiveSealedEvent(
      pContext, pCursor, aArchives[pArchiveSlot], pArchiveSlot);
  return true;
}

bool ShouldDecodeBlockFromDeepTemp(const DecodeArchiveDecodeCursorV2& pCursor,
                                   std::size_t pArchiveSlot,
                                   std::uint64_t pBlockIndex) {
  if (!pCursor.mDeepDecodeFromTemp) {
    return false;
  }
  DeepRecoverPackedBlockRefV2 aPackedRef;
  if (pCursor.mDeepOutputReplayActive) {
    return TryResolveDeepRecoverPackedBlock(
        pCursor, pArchiveSlot, pBlockIndex, aPackedRef);
  }
  return IsDeepRecoverArchiveSealed(pCursor, pArchiveSlot) &&
         pArchiveSlot < pCursor.mDeepArchiveSealFloorBySlot.size() &&
         pBlockIndex >= pCursor.mDeepArchiveSealFloorBySlot[pArchiveSlot] &&
         TryResolveDeepRecoverPackedBlock(
             pCursor, pArchiveSlot, pBlockIndex, aPackedRef);
}

bool CanResumeDecodeFromSealedHealingArchive(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor,
    std::size_t pArchiveSlot,
    std::uint64_t pBlockIndex);

bool ResolveDeepRecoverResumePoint(const DecodeStageContextV2& pContext,
                                   const DecodeArchiveDecodeCursorV2& pCursor,
                                   std::size_t& pOutArchiveSlot,
                                   std::uint64_t& pOutBlockIndex) {
  pOutArchiveSlot = pCursor.mArchiveSlot;
  pOutBlockIndex = pCursor.mBlockIndex;
  if (!IsDeepRecoverRegularSlot(pCursor, pOutArchiveSlot, pOutBlockIndex)) {
    return false;
  }

  const std::uint64_t aPreviewBlockCount =
      pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount;
  if (aPreviewBlockCount == 0u ||
      pOutArchiveSlot >= pCursor.mDeepArchiveNonRepairPrefixBySlot.size()) {
    return true;
  }

  const std::uint64_t aCurrentGlobalBlockIndex =
      pCursor.mDeepArchiveNonRepairPrefixBySlot[pOutArchiveSlot] + pOutBlockIndex;
  if (aCurrentGlobalBlockIndex >= aPreviewBlockCount) {
    return true;
  }

  std::uint64_t aRemainingPreviewBlocks = aPreviewBlockCount;
  for (std::size_t aSlot = 0u;
       aSlot < pCursor.mDeepArchiveNonRepairCountBySlot.size();
       ++aSlot) {
    const std::uint64_t aRegularCount =
        pCursor.mDeepArchiveNonRepairCountBySlot[aSlot];
    if (aRegularCount == 0u) {
      continue;
    }
    if (aRemainingPreviewBlocks >= aRegularCount) {
      aRemainingPreviewBlocks -= aRegularCount;
      continue;
    }
    pOutArchiveSlot = aSlot;
    pOutBlockIndex = aRemainingPreviewBlocks;
    return true;
  }

  return false;
}

bool TrySealDeepRecoverArchiveForActiveResumeIfReady(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor) {
  std::size_t aResumeArchiveSlot = 0u;
  std::uint64_t aResumeBlockIndex = 0u;
  if (!ResolveDeepRecoverResumePoint(
          pContext, pCursor, aResumeArchiveSlot, aResumeBlockIndex)) {
    return false;
  }
  return TrySealDeepRecoverArchiveForResumeIfReady(
      pContext, pCursor, aResumeArchiveSlot, aResumeBlockIndex);
}

bool TryMoveDecodeCursorToSealedHealingResumePoint(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor) {
  // The packed-temp healing path is designed around a deterministic full replay
  // once staging is complete. Resuming mid-scan from partially sealed temp
  // archives can skip over recoverable partial outputs when later temp runs are
  // still unresolved.
  if (pCursor.mDeepDecodeFromTemp && !pCursor.mDeepOutputReplayActive) {
    return false;
  }

  std::size_t aResumeArchiveSlot = 0u;
  std::uint64_t aResumeBlockIndex = 0u;
  if (!ResolveDeepRecoverResumePoint(
          pContext, pCursor, aResumeArchiveSlot, aResumeBlockIndex) ||
      !CanResumeDecodeFromSealedHealingArchive(
          pContext, pCursor, aResumeArchiveSlot, aResumeBlockIndex)) {
    return false;
  }

  if (aResumeArchiveSlot < pCursor.mArchiveLostBlocksBySlot.size()) {
    pCursor.mArchiveLostBlocksBySlot[aResumeArchiveSlot] = 0u;
  }

  // Resuming from staged healing blocks can re-enter at a recovered
  // mid-record boundary, so the landing block needs the same skip-record
  // resync treatment as a full temp replay.
  pCursor.mPendingRecoverResync = true;

  if (aResumeArchiveSlot == pCursor.mArchiveSlot &&
      aResumeBlockIndex == pCursor.mBlockIndex) {
    return true;
  }

  pCursor.mRead.reset();
  pCursor.mDeepMappedRead.reset();
  pCursor.mDeepMappedReadArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  pCursor.mReadFromDeepTemp = false;
  pCursor.mArchiveSlot = aResumeArchiveSlot;
  pCursor.mBlockIndex = aResumeBlockIndex;
  pCursor.mArchiveBlocksRead = 0u;
  pCursor.mArchiveAnnounced = false;
  pCursor.mHasPausedBlockBoundary = false;
  pCursor.mPausedSectionHeader = SectionHeaderV2{};
  pCursor.mPausedBlockPayloadOffset = 0u;
  pCursor.mPausedBlockPayloadEnd = 0u;
  pCursor.mPausedBoundaryRecordReference.clear();
  pCursor.mHasForcedBlockPayloadStart = false;
  pCursor.mForcedBlockPayloadStart = 0u;
  pCursor.mPreviewDecoder.ResetAfterParseError();
  pCursor.mFolderDecoder.ResetAfterParseError();
  if (!pCursor.mFileDecoder.IsInsideFile()) {
    pCursor.mFileDecoder.ResetAfterParseError();
  }
  pCursor.mHasOpenFileContinuation = false;
  pCursor.mContinuationArchiveSlot = 0u;
  pCursor.mContinuationBlockIndex = 0u;
  pCursor.mPendingSingleByteTypeGap = false;
  pCursor.mPendingSingleByteTypePayload.clear();
  ResetPendingSkipLandingValidation(pCursor);
  return true;
}

bool BuildPatchedPlainTargetBlockFromRepairForDeepRecover(
    DecodeStageContextV2& pContext,
    const DiscoveredArchiveFileV2& pTargetArchive,
    const unsigned char* pRawRepairBlockBytes,
    const FixedBlockBufferV2& pDecodedRepairBlockBytes,
    const SectionHeaderV2& pRepairHeader,
    std::uint64_t pTargetArchiveIndex,
    std::uint64_t pTargetBlockIndex,
    std::uint8_t pPatchedSectionType,
    FixedBlockBufferV2& pOutPatchedRawBlock,
    std::string& pOutError) {
  pOutError.clear();

  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  if (pRawRepairBlockBytes == nullptr || pOutPatchedRawBlock.Empty() ||
      pOutPatchedRawBlock.Size() < aArchiveBlockBytes) {
    pOutError = "repair patch buffers were unavailable";
    return false;
  }

  const bool aEncrypted = pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u;
  const unsigned char* aRepairPlainBytes = pRawRepairBlockBytes;
  if (aEncrypted) {
    if (pDecodedRepairBlockBytes.Empty() ||
        pDecodedRepairBlockBytes.Size() < aArchiveBlockBytes) {
      pOutError = "decoded repair block buffer was too small";
      return false;
    }
    aRepairPlainBytes = pDecodedRepairBlockBytes.Data();
  }
  if (pOutPatchedRawBlock.Data() != aRepairPlainBytes) {
    std::memcpy(pOutPatchedRawBlock.Data(), aRepairPlainBytes, aArchiveBlockBytes);
  }

  SectionHeaderV2 aPatchedHeader = pRepairHeader;
  aPatchedHeader.mSectionType = pPatchedSectionType;
  aPatchedHeader.mArchiveIndex = static_cast<std::uint32_t>(pTargetArchiveIndex);
  aPatchedHeader.mBlockIndex = static_cast<std::uint16_t>(pTargetBlockIndex);
  aPatchedHeader.mArchiveBlockCount = static_cast<std::uint32_t>(
      pTargetArchive.mArchiveBlockCount != 0u ? pTargetArchive.mArchiveBlockCount
                                              : pTargetArchive.mReadableBlockCount);
  aPatchedHeader.mRepairRecord.mArchiveIndex = std::numeric_limits<std::uint16_t>::max();
  aPatchedHeader.mRepairRecord.mBlockIndex = std::numeric_limits<std::uint16_t>::max();
  aPatchedHeader.mCheckSum = ComputeSectionCheckSum(
      pOutPatchedRawBlock.Data() + kSectionHeaderBytesV2,
      aSectionPayloadBytes,
      aPatchedHeader);

  if (!WriteSectionHeader(aPatchedHeader,
                          pOutPatchedRawBlock.Data(),
                          kSectionHeaderBytesV2,
                          nullptr)) {
    pOutError = "failed writing patched section header";
    return false;
  }

  return true;
}

void ResetDecodeCursorForArchiveWalk(DecodeArchiveDecodeCursorV2& pCursor) {
  pCursor.mRead.reset();
  pCursor.mDeepMappedRead.reset();
  pCursor.mDeepMappedReadArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  pCursor.mReadFromDeepTemp = false;
  pCursor.mArchiveSlot = 0u;
  pCursor.mBlockIndex = 0u;
  pCursor.mDeepScanRead.reset();
  pCursor.mDeepScanArchiveSlot = 0u;
  pCursor.mDeepScanBlockIndex = 0u;
  pCursor.mArchiveBlocksRead = 0u;
  pCursor.mProcessedBlocks = 0u;
  pCursor.mArchivesCompleted = 0u;
  pCursor.mArchiveAnnounced = false;
  pCursor.mHasPausedBlockBoundary = false;
  pCursor.mPausedSectionHeader = SectionHeaderV2{};
  pCursor.mPausedBlockPayloadOffset = 0u;
  pCursor.mPausedBlockPayloadEnd = 0u;
  pCursor.mPausedBoundaryRecordReference.clear();
  pCursor.mHasForcedBlockPayloadStart = false;
  pCursor.mForcedBlockPayloadStart = 0u;
  pCursor.mPendingRecoverResync = false;
  pCursor.mPendingSingleByteTypeGap = false;
  pCursor.mPendingSingleByteTypePayload.clear();
  ResetPendingSkipLandingValidation(pCursor);
  pCursor.mHasOpenFileContinuation = false;
  pCursor.mContinuationArchiveSlot = 0u;
  pCursor.mContinuationBlockIndex = 0u;
  pCursor.mLastSkipFallbackSourceArchiveSlot =
      DecodeArchiveDecodeCursorV2::kInvalidArchiveSlot;
  pCursor.mLastSkipFallbackSourceBlockIndex =
      std::numeric_limits<std::uint64_t>::max();
  pCursor.mDeepScanExhausted = false;
  pCursor.mDeepDecodeFromTemp = false;
  pCursor.mDeepOutputReplayActive = false;
  pCursor.mDeepStagedBlockCount = 0u;
  pCursor.mDeepStagedByteCount = 0u;
  pCursor.mNextHealingProgressByteLog = 0u;
  pCursor.mDeepCancelFinalizeRequested = false;
  pCursor.mLastSkipFallbackSourceArchiveSlot =
      DecodeArchiveDecodeCursorV2::kInvalidArchiveSlot;
  pCursor.mLastSkipFallbackSourceBlockIndex =
      std::numeric_limits<std::uint64_t>::max();
  std::fill(pCursor.mDeepArchiveSealedBySlot.begin(),
            pCursor.mDeepArchiveSealedBySlot.end(),
            0u);
  std::fill(pCursor.mDeepArchiveSealFloorBySlot.begin(),
            pCursor.mDeepArchiveSealFloorBySlot.end(),
            std::numeric_limits<std::uint64_t>::max());
  std::fill(pCursor.mDeepRegularFilledCountByArchive.begin(),
            pCursor.mDeepRegularFilledCountByArchive.end(),
            0u);
  std::fill(
      pCursor.mArchiveLostBlocksBySlot.begin(), pCursor.mArchiveLostBlocksBySlot.end(), 0u);
}

void StartUnbundleArchiveWalkAtFirstMainBlock(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor) {
  const std::uint64_t aExpectedPreviewBlocks =
      pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount;
  const std::uint64_t aBlocksPerArchive =
      std::max<std::uint64_t>(
          1u, static_cast<std::uint64_t>(pContext.Layout().mMaxBlocksPerArchive));
  const std::size_t aFirstMainArchiveSlot = static_cast<std::size_t>(
      aExpectedPreviewBlocks / aBlocksPerArchive);
  const std::uint64_t aFirstMainBlockIndex =
      aExpectedPreviewBlocks % aBlocksPerArchive;

  pContext.State().mManifest.mPreviewManifestBlocksProcessed = aExpectedPreviewBlocks;
  pCursor.mPreviewDecoder.ResetAfterParseError();
  pCursor.mRead.reset();
  pCursor.mDeepMappedRead.reset();
  pCursor.mDeepMappedReadArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  pCursor.mReadFromDeepTemp = false;
  if (aFirstMainArchiveSlot >= pContext.State().mDiscovery.mArchives.size()) {
    pCursor.mArchiveSlot = pContext.State().mDiscovery.mArchives.size();
    pCursor.mBlockIndex = 0u;
    return;
  }
  pCursor.mArchiveSlot = aFirstMainArchiveSlot;
  pCursor.mBlockIndex = aFirstMainBlockIndex;
}

std::string BuildDeepRecoverTempRootPath(DecodeStageContextV2& pContext) {
  return MakeUniquePath(
      pContext.FileSystem(), pContext.Request().mDestinationDirectory, "$RECOVER", std::string());
}

std::string BuildDeepRecoverLedgerPath(DecodeStageContextV2& pContext,
                                       const DecodeArchiveDecodeCursorV2& pCursor,
                                       std::size_t pArchiveSlot) {
  const std::string aOrdinal =
      ZeroPadNumber(static_cast<std::uint64_t>(pArchiveSlot) + 1u, kRecoverLedgerDigitsV2);
  return MakeUniquePath(pContext.FileSystem(),
                        pCursor.mDeepTempRoot,
                        "$RECOVER_data_" + aOrdinal,
                        ".dat");
}

std::string BuildDeepRecoverPreserveRootPath(
    const DecodeStageContextV2& pContext,
    const DecodeArchiveDecodeCursorV2& pCursor) {
  if (pCursor.mDeepTempRoot.empty()) {
    return std::string();
  }
  return pContext.FileSystem().JoinPath(
      pCursor.mDeepTempRoot, kRecoverPreserveDirectoryNameV2);
}

bool IsPathInsidePreservedRoot(const std::string& pPath,
                               const std::string& pPreservedRoot) {
  if (pPreservedRoot.empty()) {
    return false;
  }
  if (pPath == pPreservedRoot) {
    return true;
  }
  return pPath.size() > pPreservedRoot.size() &&
         pPath.compare(0u, pPreservedRoot.size(), pPreservedRoot) == 0 &&
         pPath[pPreservedRoot.size()] == '/';
}

bool IsTransientDecodeOutputPath(DecodeStageContextV2& pContext,
                                 const std::string& pPath) {
  const std::string aLeafName = pContext.FileSystem().FileName(pPath);
  return aLeafName.rfind("$WRITING_", 0u) == 0u;
}

bool PruneTransientDecodeOutputs(DecodeStageContextV2& pContext,
                                 const std::string& pRootPath,
                                 std::string& pOutError) {
  pOutError.clear();
  if (pRootPath.empty() || !pContext.FileSystem().Exists(pRootPath)) {
    return true;
  }

  const auto aFiles = pContext.FileSystem().ListFilesRecursive(pRootPath);
  for (const DirectoryEntryV2& aEntry : aFiles) {
    if (!IsTransientDecodeOutputPath(pContext, aEntry.mPath)) {
      continue;
    }
    if (!pContext.FileSystem().RemovePath(aEntry.mPath)) {
      pOutError = "failed removing transient decode output: " + aEntry.mPath;
      return false;
    }
  }

  return true;
}

bool StashRecoverReplayOutputs(DecodeStageContextV2& pContext,
                               DecodeArchiveDecodeCursorV2& pCursor,
                               std::string& pOutError) {
  pOutError.clear();
  const std::string aPreserveRoot =
      BuildDeepRecoverPreserveRootPath(pContext, pCursor);
  if (aPreserveRoot.empty()) {
    return true;
  }

  const auto aEntries =
      pContext.FileSystem().ListDirectoryEntries(pContext.Request().mDestinationDirectory);
  std::vector<DirectoryEntryV2> aEntriesToStash;
  for (const DirectoryEntryV2& aEntry : aEntries) {
    if (aEntry.mPath == pCursor.mDeepTempRoot ||
        IsPathInsidePreservedRoot(aEntry.mPath, pCursor.mDeepTempRoot)) {
      continue;
    }
    aEntriesToStash.push_back(aEntry);
  }
  if (aEntriesToStash.empty()) {
    return true;
  }

  if (pContext.FileSystem().Exists(aPreserveRoot) &&
      !pContext.FileSystem().ClearDirectory(aPreserveRoot)) {
    pOutError = "failed clearing deep-recover preserve directory";
    return false;
  }
  if (!pContext.FileSystem().EnsureDirectory(aPreserveRoot)) {
    pOutError = "failed creating deep-recover preserve directory";
    return false;
  }

  for (const DirectoryEntryV2& aEntry : aEntriesToStash) {
    const std::string aStashPath =
        pContext.FileSystem().JoinPath(aPreserveRoot, aEntry.mRelativePath);
    if (pContext.FileSystem().Exists(aStashPath) &&
        !pContext.FileSystem().RemovePath(aStashPath)) {
      pOutError = "failed clearing stale stashed recover output: " + aStashPath;
      return false;
    }
    if (!pContext.FileSystem().RenamePath(aEntry.mPath, aStashPath)) {
      pOutError = "failed stashing recover output before deep replay: " +
                  aEntry.mPath;
      return false;
    }
  }

  if (!PruneTransientDecodeOutputs(pContext, aPreserveRoot, pOutError)) {
    return false;
  }

  return true;
}

bool RestoreStashedRecoverOutputs(DecodeStageContextV2& pContext,
                                  DecodeArchiveDecodeCursorV2& pCursor,
                                  std::string& pOutError) {
  pOutError.clear();
  const std::string aPreserveRoot =
      BuildDeepRecoverPreserveRootPath(pContext, pCursor);
  if (aPreserveRoot.empty() || !pContext.FileSystem().Exists(aPreserveRoot)) {
    return true;
  }

  if (!PruneTransientDecodeOutputs(pContext, aPreserveRoot, pOutError)) {
    return false;
  }

  const auto aFiles = pContext.FileSystem().ListFilesRecursive(aPreserveRoot);
  for (const DirectoryEntryV2& aRoot : aFiles) {
    const std::string aRestorePath = pContext.FileSystem().JoinPath(
        pContext.Request().mDestinationDirectory, aRoot.mRelativePath);
    if (pContext.FileSystem().Exists(aRestorePath)) {
      continue;
    }
    if (!pContext.FileSystem().RenamePath(aRoot.mPath, aRestorePath)) {
      pOutError = "failed restoring preserved partial output: " + aRestorePath;
      return false;
    }
  }

  auto aDirectories = pContext.FileSystem().ListDirectoriesRecursive(aPreserveRoot);
  std::sort(aDirectories.begin(),
            aDirectories.end(),
            [](const DirectoryEntryV2& pLeft, const DirectoryEntryV2& pRight) {
              return pLeft.mPath.size() > pRight.mPath.size();
            });
  for (const DirectoryEntryV2& aRoot : aDirectories) {
    const std::string aRestorePath = pContext.FileSystem().JoinPath(
        pContext.Request().mDestinationDirectory, aRoot.mRelativePath);
    if (pContext.FileSystem().Exists(aRestorePath)) {
      continue;
    }
    if (!pContext.FileSystem().RenamePath(aRoot.mPath, aRestorePath)) {
      pOutError = "failed restoring preserved recover directory: " + aRestorePath;
      return false;
    }
  }

  return true;
}

bool ClearDirectoryPreservingPath(
    DecodeStageContextV2& pContext,
    const std::string& pDirectoryPath,
    const std::string& pPreservePath,
    std::string& pOutError) {
  pOutError.clear();

  if (pPreservePath.empty()) {
    if (!pContext.FileSystem().ClearDirectory(pDirectoryPath)) {
      pOutError = "failed clearing directory";
      return false;
    }
    return true;
  }

  if (!pContext.FileSystem().EnsureDirectory(pDirectoryPath)) {
    pOutError = "failed ensuring directory";
    return false;
  }

  const auto aFiles = pContext.FileSystem().ListFilesRecursive(pDirectoryPath);
  for (const DirectoryEntryV2& aEntry : aFiles) {
    if (IsPathInsidePreservedRoot(aEntry.mPath, pPreservePath)) {
      continue;
    }
    if (!pContext.FileSystem().RemovePath(aEntry.mPath)) {
      pOutError = "failed removing file while clearing destination: " + aEntry.mPath;
      return false;
    }
  }

  const auto aDirectories =
      pContext.FileSystem().ListDirectoriesRecursive(pDirectoryPath);
  std::vector<DirectoryEntryV2> aDirectoriesReversed = aDirectories;
  std::sort(aDirectoriesReversed.begin(),
            aDirectoriesReversed.end(),
            [](const DirectoryEntryV2& pLeft, const DirectoryEntryV2& pRight) {
              return pLeft.mPath.size() > pRight.mPath.size();
            });
  for (const DirectoryEntryV2& aEntry : aDirectoriesReversed) {
    if (IsPathInsidePreservedRoot(aEntry.mPath, pPreservePath)) {
      continue;
    }
    if (!pContext.FileSystem().RemovePath(aEntry.mPath)) {
      pOutError = "failed removing directory while clearing destination: " +
                  aEntry.mPath;
      return false;
    }
  }

  return true;
}

bool CreateDeepRecoverPackedArchive(DecodeStageContextV2& pContext,
                                    DecodeArchiveDecodeCursorV2& pCursor,
                                    std::uint32_t pTempArchiveIndex,
                                    std::string& pOutError) {
  pOutError.clear();
  if (!pCursor.mDeepTempRoot.empty() &&
      !pContext.FileSystem().EnsureDirectory(pCursor.mDeepTempRoot)) {
    pOutError = "failed ensuring deep-recover temp directory";
    return false;
  }

  ArchiveHeaderV2 aSyntheticHeader;
  std::string aHeaderError;
  if (!BuildSyntheticArchiveHeaderForDeepRecover(
          pContext, static_cast<std::uint64_t>(pTempArchiveIndex), aSyntheticHeader, aHeaderError)) {
    pOutError = "failed building synthetic temp archive header: " + aHeaderError;
    return false;
  }
  std::array<unsigned char, kArchiveHeaderBytesV2> aHeaderBytes{};
  if (!WriteArchiveHeader(aSyntheticHeader,
                          aHeaderBytes.data(),
                          aHeaderBytes.size(),
                          nullptr)) {
    pOutError = "failed serializing synthetic temp archive header";
    return false;
  }

  const std::string aTempPath =
      BuildDeepRecoverLedgerPath(pContext, pCursor, static_cast<std::size_t>(pTempArchiveIndex));
  if (!pContext.FileSystem().WriteFile(
          aTempPath, aHeaderBytes.data(), aHeaderBytes.size())) {
    pOutError = "failed creating packed deep-recover temp archive: " + aTempPath;
    return false;
  }

  DeepRecoverPackedArchiveV2 aTempArchive;
  aTempArchive.mPath = aTempPath;
  pCursor.mDeepPackedArchives.push_back(std::move(aTempArchive));
  return true;
}

bool EnsureDeepRecoverAppendLocation(DecodeStageContextV2& pContext,
                                     DecodeArchiveDecodeCursorV2& pCursor,
                                     std::uint32_t& pOutTempArchiveIndex,
                                     std::uint16_t& pOutTempBlockIndex,
                                     std::string& pOutError) {
  pOutError.clear();
  if (!pCursor.mDeepPackedArchives.empty()) {
    DeepRecoverPackedArchiveV2& aTail = pCursor.mDeepPackedArchives.back();
    if (!aTail.mPath.empty() &&
        aTail.mBlockCount < kDeepRecoverPackedBlocksPerArchiveV2) {
      pOutTempArchiveIndex = static_cast<std::uint32_t>(pCursor.mDeepPackedArchives.size() - 1u);
      pOutTempBlockIndex = aTail.mBlockCount;
      return true;
    }
    aTail.mSealed = true;
  }

  const std::uint32_t aTempArchiveIndex =
      static_cast<std::uint32_t>(pCursor.mDeepPackedArchives.size());
  if (!CreateDeepRecoverPackedArchive(
          pContext, pCursor, aTempArchiveIndex, pOutError)) {
    return false;
  }
  pOutTempArchiveIndex = aTempArchiveIndex;
  pOutTempBlockIndex = 0u;
  return true;
}

void SealActiveDeepRecoverPackedArchive(DecodeArchiveDecodeCursorV2& pCursor) {
  if (pCursor.mDeepPackedArchives.empty()) {
    return;
  }
  DeepRecoverPackedArchiveV2& aTail = pCursor.mDeepPackedArchives.back();
  if (!aTail.mPath.empty()) {
    aTail.mSealed = true;
  }
}

void MaybeDeleteDeepRecoverPackedArchiveIfUnused(DecodeStageContextV2& pContext,
                                                 DecodeArchiveDecodeCursorV2& pCursor,
                                                 std::uint32_t pTempArchiveIndex) {
  if (pTempArchiveIndex >= pCursor.mDeepPackedArchives.size()) {
    return;
  }
  DeepRecoverPackedArchiveV2& aArchive = pCursor.mDeepPackedArchives[pTempArchiveIndex];
  if (!aArchive.mSealed || aArchive.mLiveCount != 0u || aArchive.mPath.empty()) {
    return;
  }
  if (pCursor.mDeepMappedReadArchiveIndex == pTempArchiveIndex) {
    pCursor.mDeepMappedRead.reset();
    pCursor.mDeepMappedReadArchiveIndex =
        DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  }
  if (pContext.FileSystem().RemovePath(aArchive.mPath) ||
      !pContext.FileSystem().Exists(aArchive.mPath)) {
    aArchive.mPath.clear();
  }
}

void ConsumeCurrentDeepRecoverPackedBlock(DecodeStageContextV2& pContext,
                                          DecodeArchiveDecodeCursorV2& pCursor) {
  // Sealed-healing resume is speculative: if later blocks still fail, we may
  // need to restart from a full temp-backed replay. Only retire temp blocks
  // once the decoder is in the final replay walk.
  if (!pCursor.mDeepOutputReplayActive) {
    return;
  }
  if (!ShouldDecodeBlockFromDeepTemp(
          pCursor, pCursor.mArchiveSlot, pCursor.mBlockIndex)) {
    return;
  }
  DeepRecoverPackedBlockRefV2 aPackedRef;
  if (!TryResolveDeepRecoverPackedBlock(
          pCursor, pCursor.mArchiveSlot, pCursor.mBlockIndex, aPackedRef)) {
    return;
  }
  if (pCursor.mArchiveSlot < pCursor.mDeepPackedBlockRefsByArchive.size()) {
    std::vector<DeepRecoverPackedBlockRefV2>& aRefs =
        pCursor.mDeepPackedBlockRefsByArchive[pCursor.mArchiveSlot];
    if (pCursor.mBlockIndex < static_cast<std::uint64_t>(aRefs.size())) {
      aRefs[static_cast<std::size_t>(pCursor.mBlockIndex)].Clear();
    }
  }
  if (aPackedRef.mTempArchiveIndex < pCursor.mDeepPackedArchives.size()) {
    DeepRecoverPackedArchiveV2& aPackedArchive =
        pCursor.mDeepPackedArchives[aPackedRef.mTempArchiveIndex];
    if (aPackedArchive.mLiveCount > 0u) {
      --aPackedArchive.mLiveCount;
    }
    MaybeDeleteDeepRecoverPackedArchiveIfUnused(
        pContext, pCursor, aPackedRef.mTempArchiveIndex);
  }
}

bool PrepareDeepRecoverTempArchives(DecodeStageContextV2& pContext,
                                    DecodeArchiveDecodeCursorV2& pCursor,
                                    std::string& pOutError) {
  pOutError.clear();
  const auto aCleanupTempRoot = [&]() {
    if (!pCursor.mDeepTempRoot.empty()) {
      if (!pContext.FileSystem().RemovePath(pCursor.mDeepTempRoot)) {
        (void)pContext.FileSystem().ClearDirectory(pCursor.mDeepTempRoot);
      }
    }
    pCursor.mDeepTempRoot.clear();
    pCursor.mDeepPackedArchives.clear();
    pCursor.mDeepMappedRead.reset();
    pCursor.mDeepMappedReadArchiveIndex =
        DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
    pCursor.mDeepPackedBlockRefsByArchive.clear();
    pCursor.mDeepPhysicalSlotFilledByArchive.clear();
    std::fill(pCursor.mDeepArchiveSealedBySlot.begin(),
              pCursor.mDeepArchiveSealedBySlot.end(),
              0u);
    std::fill(pCursor.mDeepArchiveSealFloorBySlot.begin(),
              pCursor.mDeepArchiveSealFloorBySlot.end(),
              std::numeric_limits<std::uint64_t>::max());
    std::fill(pCursor.mDeepRegularFilledCountByArchive.begin(),
              pCursor.mDeepRegularFilledCountByArchive.end(),
              0u);
  };
  const auto aFailPrep = [&](const std::string& pMessage) {
    pOutError = pMessage;
    aCleanupTempRoot();
    return false;
  };

  pCursor.mDeepTempRoot = BuildDeepRecoverTempRootPath(pContext);
  if (!pContext.FileSystem().EnsureDirectory(pCursor.mDeepTempRoot) ||
      !pContext.FileSystem().ClearDirectory(pCursor.mDeepTempRoot)) {
    return aFailPrep("failed preparing deep-recover temp directory: " +
                     pCursor.mDeepTempRoot);
  }

  const std::vector<DiscoveredArchiveFileV2>& aArchives =
      pContext.State().mDiscovery.mArchives;
  pCursor.mDeepPackedArchives.clear();
  pCursor.mDeepMappedRead.reset();
  pCursor.mDeepMappedReadArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  pCursor.mDeepPackedBlockRefsByArchive.clear();
  pCursor.mDeepPackedBlockRefsByArchive.resize(aArchives.size());
  pCursor.mDeepPhysicalSlotFilledByArchive.clear();
  pCursor.mDeepPhysicalSlotFilledByArchive.resize(aArchives.size());
  std::fill(pCursor.mDeepArchiveSealedBySlot.begin(),
            pCursor.mDeepArchiveSealedBySlot.end(),
            0u);
  std::fill(pCursor.mDeepArchiveSealFloorBySlot.begin(),
            pCursor.mDeepArchiveSealFloorBySlot.end(),
            std::numeric_limits<std::uint64_t>::max());
  std::fill(pCursor.mDeepRegularFilledCountByArchive.begin(),
            pCursor.mDeepRegularFilledCountByArchive.end(),
            0u);
  for (std::size_t aSlot = 0u; aSlot < pCursor.mDeepRegularSlotFilledByArchive.size();
       ++aSlot) {
    std::fill(pCursor.mDeepRegularSlotFilledByArchive[aSlot].begin(),
              pCursor.mDeepRegularSlotFilledByArchive[aSlot].end(),
              0u);
  }
  for (std::size_t aSlot = 0u; aSlot < aArchives.size(); ++aSlot) {
    const DiscoveredArchiveFileV2& aArchive = aArchives[aSlot];
    const std::uint64_t aTempBlockCapacity =
        DeepRecoverTempBlockCapacity(pContext, aArchive);
    pCursor.mDeepPhysicalSlotFilledByArchive[aSlot].assign(
        static_cast<std::size_t>(std::min<std::uint64_t>(
            aTempBlockCapacity,
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))),
        0u);
    pCursor.mDeepPackedBlockRefsByArchive[aSlot].assign(
        static_cast<std::size_t>(std::min<std::uint64_t>(
            aTempBlockCapacity,
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))),
        DeepRecoverPackedBlockRefV2{});
  }
  return true;
}

bool StageRawBlockIntoDeepRecoverTemp(DecodeStageContextV2& pContext,
                                      DecodeArchiveDecodeCursorV2& pCursor,
                                      std::size_t pTargetArchiveSlot,
                                      std::uint64_t pTargetBlockIndex,
                                      const unsigned char* pRawBlockBytes,
                                      std::string& pOutError) {
  pOutError.clear();
  if (pRawBlockBytes == nullptr) {
    pOutError = "null block pointer for deep temp write";
    return false;
  }
  const std::vector<DiscoveredArchiveFileV2>& aArchives =
      pContext.State().mDiscovery.mArchives;
  if (pTargetArchiveSlot >= aArchives.size() ||
      pTargetArchiveSlot >= pCursor.mDeepPhysicalSlotFilledByArchive.size() ||
      pTargetArchiveSlot >= pCursor.mDeepPackedBlockRefsByArchive.size()) {
    return true;
  }
  const DiscoveredArchiveFileV2& aTargetArchive = aArchives[pTargetArchiveSlot];
  if (IsDeepRecoverArchiveSealed(pCursor, pTargetArchiveSlot)) {
    return true;
  }

  const std::uint64_t aPhysicalCapacity =
      DeepRecoverTempBlockCapacity(pContext, aTargetArchive);
  if (pTargetBlockIndex >= aPhysicalCapacity) {
    return true;
  }
  const bool aWasAlreadyFilled =
      IsDeepRecoverPhysicalSlotFilled(pCursor, pTargetArchiveSlot, pTargetBlockIndex);
  if (aWasAlreadyFilled) {
    return true;
  }

  std::uint32_t aTempArchiveIndex = DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  std::uint16_t aTempBlockIndex = DeepRecoverPackedBlockRefV2::kInvalidTempBlockIndex;
  if (!EnsureDeepRecoverAppendLocation(
          pContext, pCursor, aTempArchiveIndex, aTempBlockIndex, pOutError)) {
    return false;
  }
  if (aTempArchiveIndex >= pCursor.mDeepPackedArchives.size()) {
    pOutError = "deep-recover append location referenced an invalid temp archive";
    return false;
  }
  const std::string& aTempPath = pCursor.mDeepPackedArchives[aTempArchiveIndex].mPath;

  const std::size_t aOffset = static_cast<std::size_t>(
      kArchiveHeaderBytesV2 +
      (static_cast<std::uint64_t>(aTempBlockIndex) *
       static_cast<std::uint64_t>(pContext.Layout().mArchiveBlockBytes)));
  if (!pContext.FileSystem().OverwriteFileRegion(aTempPath,
                                                 aOffset,
                                                 pRawBlockBytes,
                                                 pContext.Layout().mArchiveBlockBytes)) {
    pOutError = "overwrite failed for temp archive block";
    return false;
  }
  std::vector<DeepRecoverPackedBlockRefV2>& aRefs =
      pCursor.mDeepPackedBlockRefsByArchive[pTargetArchiveSlot];
  if (pTargetBlockIndex >= static_cast<std::uint64_t>(aRefs.size())) {
    pOutError = "deep-recover mapping exceeded temp block reference capacity";
    return false;
  }
  aRefs[static_cast<std::size_t>(pTargetBlockIndex)].mTempArchiveIndex =
      aTempArchiveIndex;
  aRefs[static_cast<std::size_t>(pTargetBlockIndex)].mTempBlockIndex =
      aTempBlockIndex;
  MarkDeepRecoverPhysicalSlotFilled(pCursor, pTargetArchiveSlot, pTargetBlockIndex);
  DeepRecoverPackedArchiveV2& aPackedArchive =
      pCursor.mDeepPackedArchives[aTempArchiveIndex];
  ++aPackedArchive.mBlockCount;
  ++aPackedArchive.mLiveCount;
  if (aPackedArchive.mBlockCount >= kDeepRecoverPackedBlocksPerArchiveV2) {
    aPackedArchive.mSealed = true;
  }
  NoteDeepRecoverStagedPhysicalBlock(pContext, pCursor, false);
  return true;
}

void AdvanceDeepRecoverScanCursor(DecodeArchiveDecodeCursorV2& pCursor,
                                  const DiscoveredArchiveFileV2& pArchive) {
  ++pCursor.mDeepScanBlockIndex;
  if (pCursor.mDeepScanBlockIndex >= pArchive.mReadableBlockCount) {
    ++pCursor.mDeepScanArchiveSlot;
    pCursor.mDeepScanBlockIndex = 0u;
    pCursor.mDeepScanRead.reset();
  }
}

std::uint64_t DecodeReadableBlockCountForArchiveSlot(
    const DecodeStageContextV2& pContext,
    const DecodeArchiveDecodeCursorV2& pCursor,
    std::size_t pArchiveSlot,
    const DiscoveredArchiveFileV2& pArchive) {
  if (!pCursor.mDeepDecodeFromTemp) {
    return pArchive.mReadableBlockCount;
  }
  if (pCursor.mDeepOutputReplayActive ||
      IsDeepRecoverArchiveSealed(pCursor, pArchiveSlot)) {
    return DeepRecoverTempBlockCapacity(pContext, pArchive);
  }
  return pArchive.mReadableBlockCount;
}

std::uint64_t CurrentDecodeReadableBlockCountForArchive(
    const DecodeStageContextV2& pContext,
    const DecodeArchiveDecodeCursorV2& pCursor,
    const DiscoveredArchiveFileV2& pArchive) {
  return DecodeReadableBlockCountForArchiveSlot(
      pContext, pCursor, pCursor.mArchiveSlot, pArchive);
}

bool ShouldDecodeArchiveFromDeepTemp(const DecodeArchiveDecodeCursorV2& pCursor,
                                     std::size_t pArchiveSlot,
                                     std::uint64_t pBlockIndex) {
  return ShouldDecodeBlockFromDeepTemp(pCursor, pArchiveSlot, pBlockIndex);
}

bool IsFirstMainArchiveDataBlock(const DecodeStageContextV2& pContext,
                                 const SectionHeaderV2& pSectionHeader) {
  if (static_cast<SectionTypeV2>(pSectionHeader.mSectionType) !=
      SectionTypeV2::kArchiveData) {
    return false;
  }
  const std::uint64_t aBlocksPerArchive =
      std::max<std::uint64_t>(
          1u, static_cast<std::uint64_t>(pContext.Layout().mMaxBlocksPerArchive));
  const std::uint64_t aFlatBlockIndex =
      (static_cast<std::uint64_t>(pSectionHeader.mArchiveIndex) * aBlocksPerArchive) +
      static_cast<std::uint64_t>(pSectionHeader.mBlockIndex);
  return aFlatBlockIndex ==
         pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount;
}

bool CanUseRecoverLocalSkipAnchorAtCurrentBlock(
    const DecodeStageContextV2& pContext,
    const SectionHeaderV2& pSectionHeader,
    std::size_t pCurrentPayloadStart,
    std::size_t pCurrentPayloadEnd) {
  if (!DecodeIntentAllowsSalvageV2(pContext.Request().mIntent) ||
      SkipRecordIsZero(pSectionHeader.mSkipRecord)) {
    return false;
  }

  const std::uint64_t aCurrentArchiveIndex =
      static_cast<std::uint64_t>(pSectionHeader.mArchiveIndex);
  const std::uint64_t aCurrentBlockIndex =
      static_cast<std::uint64_t>(pSectionHeader.mBlockIndex);
  const std::uint64_t aTargetArchiveIndex =
      static_cast<std::uint64_t>(GetSkipRecordArchiveIndex(pSectionHeader.mSkipRecord));
  const std::uint64_t aTargetBlockIndex =
      static_cast<std::uint64_t>(pSectionHeader.mSkipRecord.mBlockIndex);
  const std::size_t aTargetPayloadOffset =
      static_cast<std::size_t>(GetSkipRecordByteDistance(pSectionHeader.mSkipRecord));
  const std::uint64_t aArchiveCount =
      pContext.State().mBootstrap.mExpectedArchiveCount;
  const std::uint64_t aBlocksPerArchive =
      std::max<std::uint64_t>(
          1u, static_cast<std::uint64_t>(pContext.Layout().mMaxBlocksPerArchive));
  if (aTargetArchiveIndex >= aArchiveCount || aTargetBlockIndex >= aBlocksPerArchive ||
      aTargetPayloadOffset >= pCurrentPayloadEnd || aTargetPayloadOffset < pCurrentPayloadStart) {
    return false;
  }

  return (aCurrentArchiveIndex == aTargetArchiveIndex &&
          aCurrentBlockIndex == aTargetBlockIndex) ||
         (aCurrentArchiveIndex > aTargetArchiveIndex);
}

bool CanFindRecoverResyncPointInSealedTemp(DecodeStageContextV2& pContext,
                                           DecodeArchiveDecodeCursorV2& pCursor,
                                           std::size_t pArchiveSlot,
                                           std::uint64_t pBlockIndex) {
  const std::vector<DiscoveredArchiveFileV2>& aArchives =
      pContext.State().mDiscovery.mArchives;
  if (pArchiveSlot >= aArchives.size()) {
    return false;
  }

  if (pCursor.mDeepResumeProbeRawBytes.Empty() ||
      pCursor.mDeepResumeProbeDecodeBytes.Empty()) {
    return false;
  }

  std::unique_ptr<FileReadStreamV2> aProbeRead;
  std::uint32_t aProbeTempArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  std::size_t aProbeArchiveSlot = pArchiveSlot;
  std::uint64_t aProbeBlockIndex = pBlockIndex;
  while (aProbeArchiveSlot < aArchives.size()) {
    if (!ShouldDecodeBlockFromDeepTemp(pCursor, aProbeArchiveSlot, aProbeBlockIndex)) {
      return false;
    }

    DeepRecoverPackedBlockRefV2 aPackedRef;
    if (!TryResolveDeepRecoverPackedBlock(
            pCursor, aProbeArchiveSlot, aProbeBlockIndex, aPackedRef) ||
        aPackedRef.mTempArchiveIndex >= pCursor.mDeepPackedArchives.size()) {
      return false;
    }
    const DeepRecoverPackedArchiveV2& aPackedArchive =
        pCursor.mDeepPackedArchives[aPackedRef.mTempArchiveIndex];
    if (aPackedArchive.mPath.empty()) {
      return false;
    }
    if (aProbeRead == nullptr || aProbeTempArchiveIndex != aPackedRef.mTempArchiveIndex) {
      aProbeRead = pContext.FileSystem().OpenReadStream(aPackedArchive.mPath);
      if (aProbeRead == nullptr || !aProbeRead->IsReady()) {
        return false;
      }
      aProbeTempArchiveIndex = aPackedRef.mTempArchiveIndex;
    }
    if (!ReadBlock(*aProbeRead,
                   static_cast<std::uint64_t>(aPackedRef.mTempBlockIndex),
                   pContext.Layout().mArchiveBlockBytes,
                   pCursor.mDeepResumeProbeRawBytes)) {
      return false;
    }

    SectionHeaderV2 aSectionHeader;
    std::string aValidationError;
    if (!DecodeValidatedSectionHeaderFromRawBlockForDeepRecover(
            pContext,
            pCursor.mDeepResumeProbeRawBytes.Data(),
            pCursor.mDeepResumeProbeDecodeBytes,
            aSectionHeader,
            aValidationError)) {
      return false;
    }

    if (static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
        SectionTypeV2::kArchiveData) {
      const std::size_t aBlockPayloadEnd = std::min<std::size_t>(
          pContext.Layout().SectionPayloadBytes(),
          static_cast<std::size_t>(aSectionHeader.mPayloadBytesUsed));
      if (IsFirstMainArchiveDataBlock(pContext, aSectionHeader) ||
          CanUseRecoverLocalSkipAnchorAtCurrentBlock(
              pContext, aSectionHeader, 0u, aBlockPayloadEnd)) {
        return true;
      }
    }

    ++aProbeBlockIndex;
    const std::uint64_t aReadableBlockCount = DecodeReadableBlockCountForArchiveSlot(
        pContext, pCursor, aProbeArchiveSlot, aArchives[aProbeArchiveSlot]);
    if (aProbeBlockIndex >= aReadableBlockCount) {
      ++aProbeArchiveSlot;
      aProbeBlockIndex = 0u;
      aProbeRead.reset();
      aProbeTempArchiveIndex =
          DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
    }
  }

  return false;
}

bool CanResumeDecodeFromSealedHealingArchive(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor,
    std::size_t pArchiveSlot,
    std::uint64_t pBlockIndex) {
  return ShouldDecodeBlockFromDeepTemp(pCursor, pArchiveSlot, pBlockIndex) &&
         CanFindRecoverResyncPointInSealedTemp(
             pContext, pCursor, pArchiveSlot, pBlockIndex);
}

bool StartDeepRecoverOutputPass(DecodeStageContextV2& pContext,
                                DecodeArchiveDecodeCursorV2& pCursor,
                                std::string& pOutError) {
  pOutError.clear();
  if (!StashRecoverReplayOutputs(pContext, pCursor, pOutError)) {
    pOutError =
        "failed preserving recover outputs before deep recover output pass: " +
        pOutError;
    return false;
  }
  if (!ClearDirectoryPreservingPath(
          pContext,
          pContext.Request().mDestinationDirectory,
          pCursor.mDeepTempRoot,
          pOutError)) {
    pOutError = "failed clearing destination before deep recover output pass begins: " +
                pOutError;
    return false;
  }

  pContext.State().mManifest = DecodeManifestStateV2{};
  pContext.State().mOutput = DecodeOutputStateV2{};
  pContext.State().mDiscovery.mMode = DecodeModeV2::kPessimistic;
  pCursor.mFolderDecoder.ResetAfterParseError();
  pCursor.mFileDecoder.ResetAfterParseError();
  pCursor.mPreviewDecoder.ResetAfterParseError();
  ResetDecodeCursorForArchiveWalk(pCursor);
  StartUnbundleArchiveWalkAtFirstMainBlock(pContext, pCursor);
  pCursor.mDeepDecodeFromTemp = true;
  pCursor.mDeepOutputReplayActive = true;
  // Full temp replay can restart on a repaired run boundary rather than a
  // pristine record boundary. Arm skip-record resync immediately so the first
  // staged archive-data block can re-enter mid-record when needed.
  pCursor.mPendingRecoverResync = true;
  pCursor.mReadFromDeepTemp = false;
  pCursor.mDeepScanActive = false;
  pCursor.mDeepScanExhausted = true;
  MaybeEmitHealingProgressLog(pContext, pCursor, true);
  pContext.EmitLog(LogLevelV2::kInfo, LogHealingScanCompletedV2());
  EmitDecodeHealingModeExitedEvent(pContext, pCursor, "full_replay");
  pContext.EmitLog(
      LogLevelV2::kWarning,
      DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
          " Healing scan exhausted reachable blocks. Starting full replay from "
          "staged temp archives.");
  return true;
}

bool RunDeepRecoverScanHeartbeat(DecodeStageContextV2& pContext,
                                 DecodeArchiveDecodeCursorV2& pCursor,
                                 std::string& pOutError) {
  pOutError.clear();
  const std::vector<DiscoveredArchiveFileV2>& aArchives =
      pContext.State().mDiscovery.mArchives;

  while (pCursor.mDeepScanArchiveSlot < aArchives.size()) {
    if (pContext.IsCancelRequested()) {
      DecodeCancelStateV2& aCancel = pContext.State().mCancel;
      if (!aCancel.mObserved) {
        aCancel = DecodeCancelStateV2{};
        aCancel.mObserved = true;
        aCancel.mShouldFinalizeAfterCancel = true;
        aCancel.mCancelFileReference = pCursor.mFileDecoder.CurrentFileReference();
        if (aCancel.mCancelFileReference.empty()) {
          pContext.EmitLog(
              LogLevelV2::kWarning,
              DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
                  " Cancel requested during healing scan; stopping before more staged blocks are written.");
        } else {
          pContext.EmitLog(
              LogLevelV2::kWarning,
              DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
                  " Cancel requested during healing scan while waiting on '" +
                  aCancel.mCancelFileReference +
                  "'; stopping before more staged blocks are written.");
        }
      }
      pCursor.mDeepCancelFinalizeRequested = true;
      pCursor.mDeepScanRead.reset();
      return true;
    }

    const DiscoveredArchiveFileV2& aArchive = aArchives[pCursor.mDeepScanArchiveSlot];
    if (!aArchive.mIsPresent || aArchive.mPath.empty() ||
        pCursor.mDeepScanBlockIndex >= aArchive.mReadableBlockCount) {
      const std::size_t aFinishedArchiveSlot = pCursor.mDeepScanArchiveSlot;
      ++pCursor.mDeepScanArchiveSlot;
      pCursor.mDeepScanBlockIndex = 0u;
      pCursor.mDeepScanRead.reset();
      const bool aSealedForResume =
          TrySealDeepRecoverArchiveForActiveResumeIfReady(pContext, pCursor);
      const bool aSealedWholeArchive =
          TrySealDeepRecoverArchiveIfReady(
              pContext, pCursor, aFinishedArchiveSlot);
      if (aSealedForResume || aSealedWholeArchive) {
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }
      continue;
    }

    if (pCursor.mDeepScanRead == nullptr) {
      pCursor.mDeepScanRead = pContext.FileSystem().OpenReadStream(aArchive.mPath);
      if (pCursor.mDeepScanRead == nullptr || !pCursor.mDeepScanRead->IsReady()) {
        pOutError = "failed opening source archive during deep scan: " + aArchive.mPath;
        return false;
      }
    }

    if (!ReadBlock(*pCursor.mDeepScanRead,
                   pCursor.mDeepScanBlockIndex,
                   pContext.Layout().mArchiveBlockBytes,
                   pCursor.mBlockBytes)) {
      const std::size_t aScannedArchiveSlot = pCursor.mDeepScanArchiveSlot;
      AdvanceDeepRecoverScanCursor(pCursor, aArchive);
      if (pCursor.mDeepScanArchiveSlot != aScannedArchiveSlot) {
        const bool aSealedForResume =
            TrySealDeepRecoverArchiveForActiveResumeIfReady(pContext, pCursor);
        const bool aSealedWholeArchive =
            TrySealDeepRecoverArchiveIfReady(
                pContext, pCursor, aScannedArchiveSlot);
        if (aSealedForResume || aSealedWholeArchive) {
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
      }
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    SectionHeaderV2 aSectionHeader;
    std::string aValidationError;
    if (!DecodeValidatedSectionHeaderFromRawBlockForDeepRecover(
            pContext,
            pCursor.mBlockBytes.Data(),
            pCursor.mDeepDecodeBlockBytes,
            aSectionHeader,
            aValidationError)) {
      const std::size_t aScannedArchiveSlot = pCursor.mDeepScanArchiveSlot;
      AdvanceDeepRecoverScanCursor(pCursor, aArchive);
      if (pCursor.mDeepScanArchiveSlot != aScannedArchiveSlot) {
        const bool aSealedForResume =
            TrySealDeepRecoverArchiveForActiveResumeIfReady(pContext, pCursor);
        const bool aSealedWholeArchive =
            TrySealDeepRecoverArchiveIfReady(
                pContext, pCursor, aScannedArchiveSlot);
        if (aSealedForResume || aSealedWholeArchive) {
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
      }
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    const SectionTypeV2 aSectionType =
        static_cast<SectionTypeV2>(aSectionHeader.mSectionType);
    const std::size_t aHeaderArchiveSlot = ResolveArchiveSlotForArchiveIndex(
        pCursor, static_cast<std::uint64_t>(aSectionHeader.mArchiveIndex));
    const unsigned char* aValidatedBlockBytes =
        (pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u &&
         aSectionType != SectionTypeV2::kPreviewManifest)
            ? pCursor.mDeepDecodeBlockBytes.Data()
            : pCursor.mBlockBytes.Data();
    if (aHeaderArchiveSlot != DecodeArchiveDecodeCursorV2::kInvalidArchiveSlot) {
      const std::uint64_t aHeaderBlockIndex =
          static_cast<std::uint64_t>(aSectionHeader.mBlockIndex);
      const bool aHeaderTargetsRegular =
          IsDeepRecoverRegularSlot(pCursor, aHeaderArchiveSlot, aHeaderBlockIndex);
      const bool aHeaderWritable =
          aHeaderBlockIndex <
          DeepRecoverTempBlockCapacity(pContext, aArchives[aHeaderArchiveSlot]);
      const bool aHeaderIsRepair = aSectionType == SectionTypeV2::kRepairData;

      // Never stage a raw repair block into a regular-zone slot. Repair content
      // can only materialize regular slots via the explicit patched-target path.
      if (!aHeaderIsRepair || !aHeaderTargetsRegular) {
        std::string aStageError;
        if (!StageRawBlockIntoDeepRecoverTemp(pContext,
                                              pCursor,
                                              aHeaderArchiveSlot,
                                              aHeaderBlockIndex,
                                              aValidatedBlockBytes,
                                              aStageError)) {
          pOutError = "deep recover temp write failed for archive slot " +
                      std::to_string(aHeaderArchiveSlot) + ", block " +
                      std::to_string(aHeaderBlockIndex) + ": " + aStageError;
          return false;
        }
      }
      if (!aHeaderIsRepair && aHeaderTargetsRegular && aHeaderWritable) {
        if (MarkDeepRecoverRegularSlotFilled(
                pCursor, aHeaderArchiveSlot, aHeaderBlockIndex)) {
          const bool aSealedForResume =
              TrySealDeepRecoverArchiveForActiveResumeIfReady(pContext, pCursor);
          const bool aSealedWholeArchive =
              TrySealDeepRecoverArchiveIfReady(pContext, pCursor, aHeaderArchiveSlot);
          if (aSealedForResume || aSealedWholeArchive) {
            pContext.ContinuePhaseOnNextHeartbeat();
            return true;
          }
        }
      }
    }

    if (aSectionType == SectionTypeV2::kRepairData &&
        !RepairRecordLooksUnset(aSectionHeader.mRepairRecord)) {
      const std::uint64_t aTargetArchiveIndex =
          static_cast<std::uint64_t>(aSectionHeader.mRepairRecord.mArchiveIndex);
      const std::uint64_t aTargetBlockIndex =
          static_cast<std::uint64_t>(aSectionHeader.mRepairRecord.mBlockIndex);
      const std::size_t aTargetArchiveSlot =
          ResolveArchiveSlotForArchiveIndex(pCursor, aTargetArchiveIndex);
      if (aTargetArchiveSlot != DecodeArchiveDecodeCursorV2::kInvalidArchiveSlot) {
        std::uint8_t aPatchedSectionType =
            static_cast<std::uint8_t>(SectionTypeV2::kArchiveData);
        if (ResolvePatchedSectionTypeForDeepRecoverTarget(pContext,
                                                          pCursor,
                                                          aTargetArchiveSlot,
                                                          aTargetBlockIndex,
                                                          aPatchedSectionType)) {
          const bool aTargetRegularSlot =
              IsDeepRecoverRegularSlot(
                  pCursor, aTargetArchiveSlot, aTargetBlockIndex);
          const bool aTargetAlreadyFilled =
              IsDeepRecoverRegularSlotFilled(
                  pCursor, aTargetArchiveSlot, aTargetBlockIndex);
          if (aTargetRegularSlot && !aTargetAlreadyFilled) {
            const bool aTargetWritable =
                aTargetBlockIndex <
                DeepRecoverTempBlockCapacity(pContext, aArchives[aTargetArchiveSlot]);
            if (aTargetWritable) {
              std::string aPatchError;
              if (!BuildPatchedPlainTargetBlockFromRepairForDeepRecover(
                      pContext,
                      aArchives[aTargetArchiveSlot],
                      pCursor.mBlockBytes.Data(),
                      pCursor.mDeepDecodeBlockBytes,
                      aSectionHeader,
                      aTargetArchiveIndex,
                      aTargetBlockIndex,
                      aPatchedSectionType,
                      pCursor.mDeepDecodeBlockBytes,
                      aPatchError)) {
                pOutError = "deep recover failed building patched repair target: " + aPatchError;
                return false;
              }
              std::string aStageError;
              if (!StageRawBlockIntoDeepRecoverTemp(pContext,
                                                    pCursor,
                                                    aTargetArchiveSlot,
                                                    aTargetBlockIndex,
                                                    pCursor.mDeepDecodeBlockBytes.Data(),
                                                    aStageError)) {
                pOutError = "deep recover temp write failed for repair target archive slot " +
                            std::to_string(aTargetArchiveSlot) + ", block " +
                            std::to_string(aTargetBlockIndex) + ": " + aStageError;
                return false;
              }
              if (MarkDeepRecoverRegularSlotFilled(
                      pCursor, aTargetArchiveSlot, aTargetBlockIndex)) {
                const bool aSealedForResume =
                    TrySealDeepRecoverArchiveForActiveResumeIfReady(
                        pContext, pCursor);
                const bool aSealedWholeArchive =
                    TrySealDeepRecoverArchiveIfReady(
                        pContext, pCursor, aTargetArchiveSlot);
                if (aSealedForResume || aSealedWholeArchive) {
                  pContext.ContinuePhaseOnNextHeartbeat();
                  return true;
                }
              }
            }
          }
        }
      }
    }

    const std::size_t aScannedArchiveSlot = pCursor.mDeepScanArchiveSlot;
    AdvanceDeepRecoverScanCursor(pCursor, aArchive);
    if (pCursor.mDeepScanArchiveSlot != aScannedArchiveSlot) {
      const bool aSealedForResume =
          TrySealDeepRecoverArchiveForActiveResumeIfReady(pContext, pCursor);
      const bool aSealedWholeArchive =
          TrySealDeepRecoverArchiveIfReady(
              pContext, pCursor, aScannedArchiveSlot);
      if (aSealedForResume || aSealedWholeArchive) {
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }
    }
    pContext.ContinuePhaseOnNextHeartbeat();
    return true;
  }

  pCursor.mDeepScanActive = false;
  pCursor.mDeepScanExhausted = true;
  pCursor.mDeepScanRead.reset();
  SealActiveDeepRecoverPackedArchive(pCursor);
  (void)TrySealDeepRecoverArchiveForActiveResumeIfReady(pContext, pCursor);
  for (std::size_t aSlot = 0u; aSlot < aArchives.size(); ++aSlot) {
    (void)TrySealDeepRecoverArchiveIfReady(pContext, pCursor, aSlot);
  }
  return true;
}

bool ActivateDeepRecoverHealing(DecodeStageContextV2& pContext,
                                DecodeArchiveDecodeCursorV2& pCursor,
                                std::string& pOutError) {
  pOutError.clear();
  if (!DecodeIntentAllowsSalvageV2(pContext.Request().mIntent)) {
    return true;
  }

  if (!pCursor.mDeepTriggered) {
    if (!PrepareDeepRecoverTempArchives(pContext, pCursor, pOutError)) {
      return false;
    }
    pCursor.mDeepTriggered = true;
    pCursor.mDeepDecodeFromTemp = true;
    pCursor.mDeepScanArchiveSlot = 0u;
    pCursor.mDeepScanBlockIndex = 0u;
    pCursor.mDeepScanRead.reset();
    ResetHealingProgress(pCursor);
    pContext.EmitLog(LogLevelV2::kWarning, LogHealingScanStartedV2());
    EmitHealingProgressLog(pContext, pCursor);
  }

  pCursor.mDeepScanActive = true;
  pCursor.mDeepScanExhausted = false;
  pCursor.mDeepDecodeFromTemp = true;
  pCursor.mDeepOutputReplayActive = false;
  pCursor.mReadFromDeepTemp = false;
  pCursor.mRead.reset();
  pCursor.mDeepMappedRead.reset();
  pCursor.mDeepMappedReadArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  return true;
}

enum class HealingActivationResultV2 {
  kUnavailable = 0,
  kActivatedAndYielding = 1,
  kError = 2,
};

HealingActivationResultV2 TryActivateDeepRecoverHealingAndYield(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor,
    const std::string& pWaitMessage,
    std::string& pOutError) {
  pOutError.clear();
  const std::string aReason =
      pWaitMessage.empty() ? "Found corrupt data, switching to healing scan."
                           : pWaitMessage;
  if (!CanActivateDeepRecoverHealing(pContext, pCursor)) {
    return HealingActivationResultV2::kUnavailable;
  }
  if (!ActivateDeepRecoverHealing(pContext, pCursor, pOutError)) {
    return HealingActivationResultV2::kError;
  }
  pContext.EmitLog(
      LogLevelV2::kWarning,
      LogHealingSwitchReasonV2(
          LogActionFromDecodeIntentV2(pContext.Request().mIntent), aReason));
  EmitDecodeHealingModeEnteredEvent(pContext, pCursor, aReason);
  pContext.ContinuePhaseOnNextHeartbeat();
  return HealingActivationResultV2::kActivatedAndYielding;
}

void CleanupDeepRecoverTempArchives(DecodeStageContextV2& pContext,
                                    DecodeArchiveDecodeCursorV2& pCursor) {
  std::string aRestoreError;
  if (!RestoreStashedRecoverOutputs(pContext, pCursor, aRestoreError) &&
      !aRestoreError.empty()) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Failed restoring preserved partial outputs: " + aRestoreError);
  }
  if (pCursor.mDeepTempRoot.empty()) {
    return;
  }
  if (!pContext.FileSystem().RemovePath(pCursor.mDeepTempRoot)) {
    (void)pContext.FileSystem().ClearDirectory(pCursor.mDeepTempRoot);
  }
  pCursor.mDeepTempRoot.clear();
  pCursor.mDeepPackedArchives.clear();
  pCursor.mDeepPackedBlockRefsByArchive.clear();
  pCursor.mDeepPhysicalSlotFilledByArchive.clear();
  pCursor.mDeepScanActive = false;
  pCursor.mDeepScanExhausted = false;
  pCursor.mDeepDecodeFromTemp = false;
  pCursor.mDeepOutputReplayActive = false;
  pCursor.mDeepTriggered = false;
  pCursor.mReadFromDeepTemp = false;
  pCursor.mDeepStagedBlockCount = 0u;
  pCursor.mDeepStagedByteCount = 0u;
  pCursor.mNextHealingProgressByteLog = 0u;
  pCursor.mDeepCancelFinalizeRequested = false;
  std::fill(pCursor.mDeepArchiveSealedBySlot.begin(),
            pCursor.mDeepArchiveSealedBySlot.end(),
            0u);
  std::fill(pCursor.mDeepArchiveSealFloorBySlot.begin(),
            pCursor.mDeepArchiveSealFloorBySlot.end(),
            std::numeric_limits<std::uint64_t>::max());
  std::fill(pCursor.mDeepRegularFilledCountByArchive.begin(),
            pCursor.mDeepRegularFilledCountByArchive.end(),
            0u);
  pCursor.mRead.reset();
  pCursor.mDeepMappedRead.reset();
  pCursor.mDeepMappedReadArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  pCursor.mDeepScanRead.reset();
}

void ResetPendingSingleByteTypeGap(DecodeArchiveDecodeCursorV2& pCursor) {
  pCursor.mPendingSingleByteTypeGap = false;
  pCursor.mPendingSingleByteTypePayload.clear();
}

void ResetDecodeRecordStateAfterDamagedBlock(
    DecodeArchiveDecodeCursorV2& pCursor) {
  pCursor.mPreviewDecoder.ResetAfterParseError();
  pCursor.mFolderDecoder.ResetAfterParseError();
  if (!pCursor.mFileDecoder.IsInsideFile()) {
    pCursor.mFileDecoder.ResetAfterParseError();
  }
  pCursor.mHasOpenFileContinuation = false;
  pCursor.mContinuationArchiveSlot = 0u;
  pCursor.mContinuationBlockIndex = 0u;
  ResetPendingSingleByteTypeGap(pCursor);
}

struct PartialCloseResultV2 {
  bool mHadOpenFile = false;
  bool mPromotedPartial = false;
  std::string mReference;
};

PartialCloseResultV2 CloseCurrentRecordAsPartial(
    DecodeArchiveDecodeCursorV2& pCursor) {
  PartialCloseResultV2 aResult;
  aResult.mHadOpenFile = pCursor.mFileDecoder.IsInsideFile();
  aResult.mReference = pCursor.mFileDecoder.CurrentFileReference();
  aResult.mPromotedPartial =
      aResult.mHadOpenFile ? pCursor.mFileDecoder.AbortCurrentFile()
                           : pCursor.mFileDecoder.AbortCurrentRecordAsPartial();
  if (!aResult.mHadOpenFile && !aResult.mPromotedPartial) {
    pCursor.mFileDecoder.ResetAfterParseError();
  }
  return aResult;
}

bool ReplayPendingSingleByteTypePayload(DecodeStageContextV2& pContext,
                                        DecodeArchiveDecodeCursorV2& pCursor,
                                        std::string& pOutError) {
  (void)pContext;
  pOutError.clear();
  std::string aAssumeError;
  if (!pCursor.mFileDecoder.AssumeFileTypeFlag(aAssumeError)) {
    pOutError = aAssumeError.empty()
                    ? "failed assuming a deferred file type flag"
                    : aAssumeError;
    return false;
  }

  for (unsigned char aByte : pCursor.mPendingSingleByteTypePayload) {
    bool aTerminated = false;
    bool aStoppedAtPadding = false;
    bool aParseError = false;
    std::string aParseErrorMessage;
    std::uint64_t aDataBytesWritten = 0u;
    bool aPausedAtBoundary = false;
    std::size_t aResumeOffset = 0u;
    std::string aPausedRecordReference;
    if (!pCursor.mFileDecoder.Consume(&aByte,
                                      0u,
                                      1u,
                                      false,
                                      aTerminated,
                                      aStoppedAtPadding,
                                      aParseError,
                                      aParseErrorMessage,
                                      aDataBytesWritten,
                                      aPausedAtBoundary,
                                      aResumeOffset,
                                      aPausedRecordReference)) {
      pOutError = aParseErrorMessage.empty()
                      ? "deferred type replay failed"
                      : aParseErrorMessage;
      return false;
    }
    if (aParseError) {
      pOutError = aParseErrorMessage.empty()
                      ? "deferred type replay parse failed"
                      : aParseErrorMessage;
      return false;
    }
    if (aPausedAtBoundary) {
      pOutError =
          "deferred type replay encountered an unexpected paused record boundary.";
      return false;
    }
    (void)aTerminated;
    (void)aStoppedAtPadding;
    (void)aDataBytesWritten;
    (void)aResumeOffset;
    (void)aPausedRecordReference;
  }

  ResetPendingSingleByteTypeGap(pCursor);
  return true;
}

bool TryApplyRecoverSkipRecord(DecodeStageContextV2& pContext,
                               DecodeArchiveDecodeCursorV2& pCursor,
                               const SectionHeaderV2& pSectionHeader,
                               std::size_t pCurrentArchiveSlot,
                               std::uint64_t pCurrentPhysicalBlockIndex,
                               std::uint64_t pCurrentLogicalBlockIndex,
                               std::size_t pCurrentPayloadStart,
                               std::size_t pCurrentPayloadEnd) {
  if (!DecodeIntentAllowsSalvageV2(pContext.Request().mIntent) ||
      SkipRecordIsZero(pSectionHeader.mSkipRecord)) {
    return false;
  }

  const std::vector<DiscoveredArchiveFileV2>& aArchives =
      pContext.State().mDiscovery.mArchives;
  const std::size_t aTargetArchiveSlot =
      static_cast<std::size_t>(GetSkipRecordArchiveIndex(pSectionHeader.mSkipRecord));
  const std::uint64_t aTargetBlockIndex =
      static_cast<std::uint64_t>(pSectionHeader.mSkipRecord.mBlockIndex);
  const std::size_t aTargetPayloadOffset = static_cast<std::size_t>(
      GetSkipRecordByteDistance(pSectionHeader.mSkipRecord));
  const std::size_t aPayloadBytesPerBlock = pContext.Layout().SectionPayloadBytes();
  if (aTargetArchiveSlot >= aArchives.size()) {
    return false;
  }
  const bool aTargetUsesDeepTemp = ShouldDecodeBlockFromDeepTemp(
      pCursor, aTargetArchiveSlot, aTargetBlockIndex);
  const std::uint64_t aTargetPhysicalBlockIndex =
      aTargetUsesDeepTemp
          ? aTargetBlockIndex
          : LogicalToPhysicalBlockIndexWithKnownLoss(
                pCursor, aTargetArchiveSlot, aTargetBlockIndex);
  if (aTargetPayloadOffset >= aPayloadBytesPerBlock) {
    return false;
  }

  const DiscoveredArchiveFileV2& aTargetArchive = aArchives[aTargetArchiveSlot];
  if (pCursor.mDeepDecodeFromTemp && !aTargetUsesDeepTemp && !aTargetArchive.mIsPresent) {
    return false;
  }
  const std::uint64_t aTargetReadableBlockCount =
      DecodeReadableBlockCountForArchiveSlot(
          pContext, pCursor, aTargetArchiveSlot, aTargetArchive);
  if (aTargetPhysicalBlockIndex >= aTargetReadableBlockCount) {
    return false;
  }

  const bool aForward =
      (aTargetArchiveSlot > pCurrentArchiveSlot) ||
      (aTargetArchiveSlot == pCurrentArchiveSlot &&
       (aTargetBlockIndex > pCurrentLogicalBlockIndex ||
        (aTargetBlockIndex == pCurrentLogicalBlockIndex &&
         aTargetPayloadOffset > pCurrentPayloadStart)));
  const bool aCurrentBlockResyncAnchor =
      aTargetArchiveSlot == pCurrentArchiveSlot &&
      aTargetBlockIndex == pCurrentLogicalBlockIndex &&
      pCurrentPayloadStart == 0u &&
      aTargetPayloadOffset == 0u;
  if (!aForward && !aCurrentBlockResyncAnchor) {
    return false;
  }

  const bool aTargetsCurrentBlock =
      aTargetArchiveSlot == pCurrentArchiveSlot &&
      aTargetBlockIndex == pCurrentLogicalBlockIndex;

  if (aTargetsCurrentBlock) {
    ResetPendingSkipLandingValidation(pCursor);
    if (aTargetPayloadOffset >= pCurrentPayloadEnd) {
      return false;
    }
    pCursor.mHasPausedBlockBoundary = true;
    pCursor.mPausedSectionHeader = pSectionHeader;
    pCursor.mPausedBlockPayloadOffset = aTargetPayloadOffset;
    pCursor.mPausedBlockPayloadEnd = pCurrentPayloadEnd;
    pCursor.mPausedBoundaryRecordReference.clear();
    pCursor.mHasForcedBlockPayloadStart = false;
    pCursor.mForcedBlockPayloadStart = 0u;
    EmitAppliedRecoverSkipRecordLog(pContext,
                                    aTargetArchiveSlot,
                                    aTargetBlockIndex,
                                    aTargetPhysicalBlockIndex,
                                    aTargetPayloadOffset);
  } else {
    // Forward inter-block jumps are valid in both optimistic and pessimistic
    // salvage walks. Landing validation guards against bad destinations and
    // falls back to source+1 when the landing block cannot be consumed.
    pCursor.mHasPendingSkipLandingValidation = true;
    pCursor.mPendingSkipSourceArchiveSlot = pCurrentArchiveSlot;
    pCursor.mPendingSkipSourceBlockIndex = pCurrentPhysicalBlockIndex;
    pCursor.mPendingSkipTargetArchiveSlot = aTargetArchiveSlot;
    pCursor.mPendingSkipTargetBlockIndex = aTargetPhysicalBlockIndex;
    pCursor.mPendingSkipTargetLogicalBlockIndex = aTargetBlockIndex;
    pCursor.mPendingSkipTargetPayloadOffset = aTargetPayloadOffset;
    pCursor.mRead.reset();
    pCursor.mDeepMappedRead.reset();
    pCursor.mDeepMappedReadArchiveIndex =
        DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
    pCursor.mArchiveSlot = aTargetArchiveSlot;
    pCursor.mBlockIndex = aTargetPhysicalBlockIndex;
    pCursor.mArchiveBlocksRead = 0u;
    pCursor.mArchiveAnnounced = false;
    pCursor.mHasPausedBlockBoundary = false;
    pCursor.mPausedSectionHeader = SectionHeaderV2{};
    pCursor.mPausedBlockPayloadOffset = 0u;
    pCursor.mPausedBlockPayloadEnd = 0u;
    pCursor.mPausedBoundaryRecordReference.clear();
    pCursor.mHasForcedBlockPayloadStart = aTargetPayloadOffset > 0u;
    pCursor.mForcedBlockPayloadStart = aTargetPayloadOffset;
  }

  pCursor.mHasOpenFileContinuation = false;
  pCursor.mContinuationArchiveSlot = 0u;
  pCursor.mContinuationBlockIndex = 0u;
  return true;
}

bool TryApplyRecoverLocalSkipAnchor(DecodeStageContextV2& pContext,
                                    DecodeArchiveDecodeCursorV2& pCursor,
                                    const SectionHeaderV2& pSectionHeader,
                                    std::size_t pCurrentArchiveSlot,
                                    std::uint64_t pCurrentLogicalBlockIndex,
                                    std::size_t pCurrentPayloadStart,
                                    std::size_t pCurrentPayloadEnd) {
  if (!DecodeIntentAllowsSalvageV2(pContext.Request().mIntent) ||
      SkipRecordIsZero(pSectionHeader.mSkipRecord)) {
    return false;
  }

  const std::uint64_t aCurrentArchiveIndex =
      static_cast<std::uint64_t>(pSectionHeader.mArchiveIndex);
  const std::uint64_t aCurrentBlockIndex =
      static_cast<std::uint64_t>(pSectionHeader.mBlockIndex);
  const std::uint64_t aTargetArchiveIndex =
      static_cast<std::uint64_t>(GetSkipRecordArchiveIndex(pSectionHeader.mSkipRecord));
  const std::uint64_t aTargetBlockIndex =
      static_cast<std::uint64_t>(pSectionHeader.mSkipRecord.mBlockIndex);
  const std::size_t aTargetPayloadOffset =
      static_cast<std::size_t>(GetSkipRecordByteDistance(pSectionHeader.mSkipRecord));
  const std::uint64_t aArchiveCount =
      pContext.State().mBootstrap.mExpectedArchiveCount;
  const std::uint64_t aBlocksPerArchive =
      std::max<std::uint64_t>(
          1u, static_cast<std::uint64_t>(pContext.Layout().mMaxBlocksPerArchive));
  if (aTargetArchiveIndex >= aArchiveCount || aTargetBlockIndex >= aBlocksPerArchive ||
      aTargetPayloadOffset >= pCurrentPayloadEnd || aTargetPayloadOffset < pCurrentPayloadStart) {
    return false;
  }

  const bool aCurrentBlockAnchor =
      aCurrentArchiveIndex == aTargetArchiveIndex && aCurrentBlockIndex == aTargetBlockIndex;
  const bool aLaterArchiveAnchor = aCurrentArchiveIndex > aTargetArchiveIndex;
  if (!aCurrentBlockAnchor && !aLaterArchiveAnchor) {
    return false;
  }

  ResetPendingSkipLandingValidation(pCursor);
  pCursor.mHasPausedBlockBoundary = true;
  pCursor.mPausedSectionHeader = pSectionHeader;
  pCursor.mPausedBlockPayloadOffset = aTargetPayloadOffset;
  pCursor.mPausedBlockPayloadEnd = pCurrentPayloadEnd;
  pCursor.mPausedBoundaryRecordReference.clear();
  pCursor.mHasForcedBlockPayloadStart = false;
  pCursor.mForcedBlockPayloadStart = 0u;
  pCursor.mHasOpenFileContinuation = false;
  pCursor.mContinuationArchiveSlot = 0u;
  pCursor.mContinuationBlockIndex = 0u;
  pContext.EmitLog(
      LogLevelV2::kInfo,
      DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
          " Applied local recover resync anchor at archive slot " +
          std::to_string(pCurrentArchiveSlot) + ", block " +
          std::to_string(pCurrentLogicalBlockIndex) + ", payload offset " +
          std::to_string(aTargetPayloadOffset) + ".");
  return true;
}

void SnapshotDecodeOutput(DecodeStageContextV2& pContext,
                          const DecodeArchiveDecodeCursorV2& pCursor) {
  pContext.State().mOutput.mFilesWritten = pCursor.mFileDecoder.FilesWritten();
  pContext.State().mOutput.mFoldersCreated =
      pCursor.mFolderDecoder.FoldersCreated() +
      pCursor.mFileDecoder.FoldersCreated();
  pContext.State().mOutput.mBytesWritten = pCursor.mFileDecoder.BytesWritten();
}

void AdvanceDecodeArchiveCursor(DecodeArchiveDecodeCursorV2& pCursor) {
  ++pCursor.mArchiveSlot;
  pCursor.mBlockIndex = 0u;
  pCursor.mArchiveBlocksRead = 0u;
  pCursor.mRead.reset();
  pCursor.mDeepMappedRead.reset();
  pCursor.mDeepMappedReadArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  pCursor.mReadFromDeepTemp = false;
  pCursor.mArchiveAnnounced = false;
  pCursor.mHasPausedBlockBoundary = false;
  pCursor.mPausedSectionHeader = SectionHeaderV2{};
  pCursor.mPausedBlockPayloadOffset = 0u;
  pCursor.mPausedBlockPayloadEnd = 0u;
  pCursor.mPausedBoundaryRecordReference.clear();
  pCursor.mHasForcedBlockPayloadStart = false;
  pCursor.mForcedBlockPayloadStart = 0u;
  ResetPendingSkipLandingValidation(pCursor);
}

bool CloseDecodeArchiveForCursor(DecodeStageContextV2& pContext,
                                 DecodeArchiveDecodeCursorV2& pCursor,
                                 const DiscoveredArchiveFileV2& pArchive) {
  const std::uint64_t aReadableBlockCount =
      CurrentDecodeReadableBlockCountForArchive(pContext, pCursor, pArchive);
  const std::uint64_t aDeclaredBlockCount =
      pArchive.mArchiveBlockCount != 0u ? pArchive.mArchiveBlockCount
                                        : aReadableBlockCount;
  const bool aMissingTailBlocks =
      !pCursor.mDeepDecodeFromTemp && pArchive.mReadableBlockCount < aDeclaredBlockCount;
  bool aStopAfterArchiveClose = false;
  if (aMissingTailBlocks) {
    const std::uint64_t aLostTailBlocks =
        aDeclaredBlockCount - pArchive.mReadableBlockCount;
    if (pContext.Request().mIntent == DecodeIntentV2::kUnbundle) {
      const bool aInsideFile = pCursor.mFileDecoder.IsInsideFile();
      const std::string aPartialReference =
          pCursor.mFileDecoder.CurrentFileReference();
      const bool aPromotedPartial = aInsideFile
                                        ? pCursor.mFileDecoder.AbortCurrentFile()
                                        : pCursor.mFileDecoder.AbortCurrentRecordAsPartial();
      if (!aInsideFile && !aPromotedPartial) {
        pCursor.mFileDecoder.ResetAfterParseError();
      }
      EmitDecodeErrorMarkerEvent(
          pContext,
            "file_data_error",
            "Decode file encountered missing archive-tail block(s).",
            pArchive,
            pCursor.mArchiveSlot,
            aReadableBlockCount,
            SectionTypeLabel(SectionTypeV2::kArchiveData),
            aPartialReference);
      if (aPromotedPartial) {
        EmitDecodeErrorMarkerEvent(
            pContext,
              "file_closed_partial",
              "Decode closed file as partial after missing archive-tail block(s).",
              pArchive,
              pCursor.mArchiveSlot,
              aReadableBlockCount,
              SectionTypeLabel(SectionTypeV2::kArchiveData),
              aPartialReference);
      }
      ResetDecodeRecordStateAfterDamagedBlock(pCursor);
      if (!HandleDamagedBlock(
              pContext,
              "archive ended before declared block count; inferred deleted tail block(s).")) {
        return false;
      }
      aStopAfterArchiveClose = ShouldStopAfterFirstDamagedBlock(pContext);
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Inferred " + std::to_string(aLostTailBlocks) +
              " deleted tail block(s) in archive slot " +
              std::to_string(pCursor.mArchiveSlot) + ".");
    } else if (DecodeIntentAllowsSalvageV2(pContext.Request().mIntent)) {
      std::string aHealingError;
      const HealingActivationResultV2 aHealingResult =
          TryActivateDeepRecoverHealingAndYield(
              pContext,
              pCursor,
              "Inferred " + std::to_string(aLostTailBlocks) +
                  " missing tail block(s) in archive slot " +
                  std::to_string(pCursor.mArchiveSlot) +
                  "; waiting for healing scan before abandoning the active decode state.",
              aHealingError);
      if (aHealingResult == HealingActivationResultV2::kActivatedAndYielding) {
        return true;
      }
      if (aHealingResult == HealingActivationResultV2::kError) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(
                LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                ProgressStageV2::kArchiveDecode,
                "healing initialization failed: " + aHealingError));
        return false;
      }

      if (pCursor.mArchiveSlot < pCursor.mArchiveLostBlocksBySlot.size()) {
        pCursor.mArchiveLostBlocksBySlot[pCursor.mArchiveSlot] += aLostTailBlocks;
      }

      const PartialCloseResultV2 aPartialClose = CloseCurrentRecordAsPartial(pCursor);
      if (aPartialClose.mHadOpenFile || aPartialClose.mPromotedPartial) {
        EmitDecodeErrorMarkerEvent(
            pContext,
            "file_data_error",
            "Decode file encountered missing archive-tail block(s).",
            pArchive,
            pCursor.mArchiveSlot,
            aReadableBlockCount,
            SectionTypeLabel(SectionTypeV2::kArchiveData),
            aPartialClose.mReference);
        if (aPartialClose.mPromotedPartial) {
          EmitDecodeErrorMarkerEvent(
              pContext,
              "file_closed_partial",
              "Decode closed file as partial after missing archive-tail block(s).",
              pArchive,
              pCursor.mArchiveSlot,
              aReadableBlockCount,
              SectionTypeLabel(SectionTypeV2::kArchiveData),
              aPartialClose.mReference);
        }
      }
      ResetDecodeRecordStateAfterDamagedBlock(pCursor);
      SwitchToPessimistic(
          pContext,
          "archive ended before declared block count; inferred deleted tail block(s).");
      pCursor.mPendingRecoverResync = true;
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Inferred " + std::to_string(aLostTailBlocks) +
              " deleted tail block(s) in archive slot " +
              std::to_string(pCursor.mArchiveSlot) + ".");
    }
  }

  if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeArchiveFinished)) {
    EmitDecodeArchiveFinishedEvent(
        pContext, pArchive, pCursor.mArchiveSlot, pCursor.mArchiveBlocksRead);
  }
  ++pCursor.mArchivesCompleted;
  const LoggingStatV2 aArchiveClosedStat = BuildArchiveDecodeStat(
      pContext, pCursor.mFolderDecoder, pCursor.mFileDecoder, pCursor.mArchivesCompleted);
  if (ShouldEmitArchiveDecodeSlice(aArchiveClosedStat,
                                   pCursor.mNextArchiveLog,
                                   pCursor.mNextFileLog,
                                   pCursor.mNextFolderLog,
                                   pCursor.mNextByteLog)) {
    pContext.EmitLog(
        LogLevelV2::kInfo,
        DecodeStagePrefix(pContext.Request().mIntent,
                          ProgressStageV2::kArchiveDecode) +
            " " + BuildArchiveDecodeSummary(aArchiveClosedStat) + ".");
  }
  AdvanceDecodeArchiveCursor(pCursor);
  if (aStopAfterArchiveClose) {
    pCursor.mArchiveSlot = pContext.State().mDiscovery.mArchives.size();
    pCursor.mBlockIndex = 0u;
    pCursor.mArchiveBlocksRead = 0u;
    pCursor.mRead.reset();
    pCursor.mDeepMappedRead.reset();
    pCursor.mDeepMappedReadArchiveIndex =
        DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  }
  return true;
}

enum class DamagedBlockAdvanceResultV2 {
  kYielded = 0,
  kExhausted = 1,
  kError = 2,
};

DamagedBlockAdvanceResultV2 ContinueAfterDamagedBlock(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor,
    const DiscoveredArchiveFileV2& pArchive) {
  ConsumeCurrentDeepRecoverPackedBlock(pContext, pCursor);
  ++pCursor.mBlockIndex;
  if (pCursor.mBlockIndex >=
      CurrentDecodeReadableBlockCountForArchive(pContext, pCursor, pArchive)) {
    if (!CloseDecodeArchiveForCursor(pContext, pCursor, pArchive)) {
      return DamagedBlockAdvanceResultV2::kError;
    }
  }
  if (pCursor.mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
    pContext.ContinuePhaseOnNextHeartbeat();
    return DamagedBlockAdvanceResultV2::kYielded;
  }
  return DamagedBlockAdvanceResultV2::kExhausted;
}

DamagedBlockAdvanceResultV2 ContinueAfterDamagedBlockWithSkipLandingFallback(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor,
    const DiscoveredArchiveFileV2& pArchive) {
  const bool aAtPendingSkipTarget =
      pCursor.mHasPendingSkipLandingValidation &&
      pCursor.mArchiveSlot == pCursor.mPendingSkipTargetArchiveSlot &&
      pCursor.mBlockIndex == pCursor.mPendingSkipTargetBlockIndex;
  if (!aAtPendingSkipTarget) {
    return ContinueAfterDamagedBlock(pContext, pCursor, pArchive);
  }

  const std::vector<DiscoveredArchiveFileV2>& aArchives =
      pContext.State().mDiscovery.mArchives;
  const std::size_t aSourceArchiveSlot = pCursor.mPendingSkipSourceArchiveSlot;
  const std::uint64_t aSourceBlockIndex = pCursor.mPendingSkipSourceBlockIndex;

  ResetPendingSkipLandingValidation(pCursor);

  if (aSourceArchiveSlot >= aArchives.size()) {
    if (ShouldLogSkipLandingFallback(
            pCursor, DecodeArchiveDecodeCursorV2::kInvalidArchiveSlot, aSourceBlockIndex)) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Skip-record landing block failed, but source was unavailable. "
              "Continuing from current damaged block.");
    }
    return ContinueAfterDamagedBlock(pContext, pCursor, pArchive);
  }

  SwitchToPessimistic(
      pContext, "skip-record landing block failed validation/parse.");
  if (ShouldLogSkipLandingFallback(pCursor, aSourceArchiveSlot, aSourceBlockIndex)) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Skip-record landing block failed; resuming pessimistic walk from "
            "source block + 1.");
  }

  pCursor.mRead.reset();
  pCursor.mDeepMappedRead.reset();
  pCursor.mDeepMappedReadArchiveIndex =
      DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
  pCursor.mArchiveSlot = aSourceArchiveSlot;
  pCursor.mBlockIndex = aSourceBlockIndex;
  pCursor.mArchiveBlocksRead = 0u;
  pCursor.mArchiveAnnounced = false;
  pCursor.mHasPausedBlockBoundary = false;
  pCursor.mPausedSectionHeader = SectionHeaderV2{};
  pCursor.mPausedBlockPayloadOffset = 0u;
  pCursor.mPausedBlockPayloadEnd = 0u;
  pCursor.mPausedBoundaryRecordReference.clear();
  pCursor.mHasForcedBlockPayloadStart = false;
  pCursor.mForcedBlockPayloadStart = 0u;
  pCursor.mHasOpenFileContinuation = false;
  pCursor.mContinuationArchiveSlot = 0u;
  pCursor.mContinuationBlockIndex = 0u;
  // We are resuming from source block + 1 after a failed skip landing.
  // Keep recover-resync enabled so pessimistic mode can immediately evaluate
  // intra-block skip guidance on the resumed walk.
  pCursor.mPendingRecoverResync = true;

  return ContinueAfterDamagedBlock(
      pContext, pCursor, aArchives[aSourceArchiveSlot]);
}

enum class DeepRecoverBlockReadyV2 {
  kReady = 0,
  kWaiting = 1,
  kFallbackStarted = 2,
  kError = 3,
  kCancelFinalize = 4,
};

DeepRecoverBlockReadyV2 EnsureDeepRecoverBlockReady(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor,
    std::string& pOutError) {
  pOutError.clear();
  if (pCursor.mDeepCancelFinalizeRequested) {
    return DeepRecoverBlockReadyV2::kCancelFinalize;
  }
  if (!pCursor.mDeepDecodeFromTemp) {
    return DeepRecoverBlockReadyV2::kReady;
  }
  if (pCursor.mDeepOutputReplayActive) {
    return DeepRecoverBlockReadyV2::kReady;
  }

  if (TryMoveDecodeCursorToSealedHealingResumePoint(pContext, pCursor)) {
    return DeepRecoverBlockReadyV2::kReady;
  }

  if (pCursor.mDeepScanActive) {
    if (!RunDeepRecoverScanHeartbeat(pContext, pCursor, pOutError)) {
      return DeepRecoverBlockReadyV2::kError;
    }
    if (pCursor.mDeepCancelFinalizeRequested) {
      return DeepRecoverBlockReadyV2::kCancelFinalize;
    }
    if (TryMoveDecodeCursorToSealedHealingResumePoint(pContext, pCursor)) {
      pCursor.mDeepScanActive = false;
      pCursor.mDeepScanRead.reset();
      pCursor.mRead.reset();
      pCursor.mDeepMappedRead.reset();
      pCursor.mDeepMappedReadArchiveIndex =
          DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
      pCursor.mReadFromDeepTemp = false;
      pContext.EmitLog(
          LogLevelV2::kInfo,
          LogHealingResumeDecodeV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent)));
      EmitDecodeHealingModeExitedEvent(pContext, pCursor, "sealed_healing_archive");
      pContext.ContinuePhaseOnNextHeartbeat();
      return DeepRecoverBlockReadyV2::kWaiting;
    }
    return pCursor.mDeepScanActive ? DeepRecoverBlockReadyV2::kWaiting
                                   : DeepRecoverBlockReadyV2::kFallbackStarted;
  }

  if (!pCursor.mDeepScanExhausted) {
    return DeepRecoverBlockReadyV2::kReady;
  }
  if (TryMoveDecodeCursorToSealedHealingResumePoint(pContext, pCursor)) {
    pCursor.mRead.reset();
    pCursor.mDeepMappedRead.reset();
    pCursor.mDeepMappedReadArchiveIndex =
        DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
    pCursor.mReadFromDeepTemp = false;
    pContext.EmitLog(
        LogLevelV2::kInfo,
        LogHealingResumeDecodeV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent)));
    EmitDecodeHealingModeExitedEvent(pContext, pCursor, "sealed_healing_archive");
    pContext.ContinuePhaseOnNextHeartbeat();
    return DeepRecoverBlockReadyV2::kWaiting;
  }

  if (!StartDeepRecoverOutputPass(pContext, pCursor, pOutError)) {
    return DeepRecoverBlockReadyV2::kError;
  }
  return DeepRecoverBlockReadyV2::kFallbackStarted;
}

bool EnsureSourceDecodeReadStreamForCurrentArchive(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor,
    const DiscoveredArchiveFileV2& pArchive,
    std::string& pOutError) {
  pOutError.clear();
  if (pCursor.mRead != nullptr) {
    return true;
  }
  if (pArchive.mPath.empty()) {
    pOutError = "source archive path was empty";
    return false;
  }
  pCursor.mRead = pContext.FileSystem().OpenReadStream(pArchive.mPath);
  if (pCursor.mRead == nullptr || !pCursor.mRead->IsReady()) {
    pOutError = "failed opening source archive for read: " + pArchive.mPath;
    pCursor.mRead.reset();
    return false;
  }
  return true;
}

enum class DecodeCurrentBlockReadResultV2 {
  kSuccess = 0,
  kReadFailure = 1,
  kDeepTempFailure = 2,
};

DecodeCurrentBlockReadResultV2 ReadCurrentDecodeBlock(
    DecodeStageContextV2& pContext,
    DecodeArchiveDecodeCursorV2& pCursor,
    const DiscoveredArchiveFileV2& pArchive,
    std::string& pOutError) {
  pOutError.clear();
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const bool aShouldUseDeepTemp =
      ShouldDecodeArchiveFromDeepTemp(
          pCursor, pCursor.mArchiveSlot, pCursor.mBlockIndex);
  if (aShouldUseDeepTemp) {
    DeepRecoverPackedBlockRefV2 aPackedRef;
    if (!TryResolveDeepRecoverPackedBlock(
            pCursor, pCursor.mArchiveSlot, pCursor.mBlockIndex, aPackedRef)) {
      pOutError = "healing temp mapping was unavailable for requested block";
      return DecodeCurrentBlockReadResultV2::kDeepTempFailure;
    }
    if (aPackedRef.mTempArchiveIndex >= pCursor.mDeepPackedArchives.size()) {
      pOutError = "healing temp mapping referenced an invalid temp archive";
      return DecodeCurrentBlockReadResultV2::kDeepTempFailure;
    }
    const DeepRecoverPackedArchiveV2& aPackedArchive =
        pCursor.mDeepPackedArchives[aPackedRef.mTempArchiveIndex];
    if (aPackedArchive.mPath.empty()) {
      pOutError = "healing temp archive path was unavailable";
      return DecodeCurrentBlockReadResultV2::kDeepTempFailure;
    }
    if (pCursor.mDeepMappedRead == nullptr ||
        pCursor.mDeepMappedReadArchiveIndex != aPackedRef.mTempArchiveIndex) {
      pCursor.mDeepMappedRead =
          pContext.FileSystem().OpenReadStream(aPackedArchive.mPath);
      if (pCursor.mDeepMappedRead == nullptr ||
          !pCursor.mDeepMappedRead->IsReady()) {
        pOutError = "failed opening healing temp archive for read: " +
                    aPackedArchive.mPath;
        pCursor.mDeepMappedRead.reset();
        pCursor.mDeepMappedReadArchiveIndex =
            DeepRecoverPackedBlockRefV2::kInvalidTempArchiveIndex;
        return DecodeCurrentBlockReadResultV2::kDeepTempFailure;
      }
      pCursor.mDeepMappedReadArchiveIndex = aPackedRef.mTempArchiveIndex;
    }
    if (!ReadBlock(*pCursor.mDeepMappedRead,
                   static_cast<std::uint64_t>(aPackedRef.mTempBlockIndex),
                   aArchiveBlockBytes,
                   pCursor.mBlockBytes)) {
      pOutError = "failed reading healing temp archive block";
      return DecodeCurrentBlockReadResultV2::kDeepTempFailure;
    }
    pCursor.mReadFromDeepTemp = true;
    return DecodeCurrentBlockReadResultV2::kSuccess;
  }

  pCursor.mReadFromDeepTemp = false;
  std::string aSourceOpenError;
  if (EnsureSourceDecodeReadStreamForCurrentArchive(
          pContext, pCursor, pArchive, aSourceOpenError) &&
      ReadBlock(*pCursor.mRead, pCursor.mBlockIndex, aArchiveBlockBytes, pCursor.mBlockBytes)) {
    return DecodeCurrentBlockReadResultV2::kSuccess;
  }
  if (!pCursor.mDeepDecodeFromTemp) {
    pOutError = aSourceOpenError.empty() ? "failed reading source archive block"
                                         : aSourceOpenError;
    return DecodeCurrentBlockReadResultV2::kReadFailure;
  }

  std::memset(pCursor.mBlockBytes.Data(), 0, aArchiveBlockBytes);
  return DecodeCurrentBlockReadResultV2::kSuccess;
}

bool HandlePausedDecodeCheckpointCancel(DecodeStageContextV2& pContext,
                                        DecodeArchiveDecodeCursorV2& pCursor) {
  if (!pContext.IsCancelRequested() || !pCursor.mHasPausedBlockBoundary) {
    return false;
  }

  DecodeCancelStateV2& aCancel = pContext.State().mCancel;
  if (!aCancel.mObserved) {
    aCancel = DecodeCancelStateV2{};
    aCancel.mObserved = true;
    aCancel.mShouldFinalizeAfterCancel = true;
    aCancel.mCancelFileReference = pCursor.mPausedBoundaryRecordReference;
    if (aCancel.mCancelFileReference.empty()) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel requested at a checkpoint boundary; stopping before the next record.");
    } else {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Cancel requested after finishing file '" +
              aCancel.mCancelFileReference +
              "'; stopping before the next file begins.");
    }
  }

  return true;
}

bool FinalizeDecodeArchivePhase(DecodeStageContextV2& pContext,
                                std::shared_ptr<DecodeArchiveDecodeCursorV2>& pCursorPtr) {
  DecodeArchiveDecodeCursorV2& aCursor = *pCursorPtr;
  auto aFinalizeAndReleaseCursor = [&]() {
    CleanupDeepRecoverTempArchives(pContext, aCursor);
    pCursorPtr.reset();
  };
  if (pContext.IsCancelRequested()) {
    pContext.State().mCancel.mObserved = true;
    pContext.State().mCancel.mShouldFinalizeAfterCancel = true;
    if (aCursor.mFileDecoder.IsInsideFile()) {
      (void)aCursor.mFileDecoder.AbortCurrentFile();
    }
    SnapshotDecodeOutput(pContext, aCursor);
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Cancel requested after archive decode completed.");
    pContext.ClearPhaseContinuationRequest();
    aFinalizeAndReleaseCursor();
    return true;
  }

  std::string aFinalizeError;
  if (!aCursor.mPreviewDecoder.Finalize(aFinalizeError)) {
    if (DecodeIntentAllowsSalvageV2(pContext.Request().mIntent)) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Preview-manifest stream ended mid-record; discarding trailing "
              "preview fragment and continuing recover output.");
      aCursor.mPreviewDecoder.ResetAfterParseError();
      aFinalizeError.clear();
      if (!aCursor.mPreviewDecoder.Finalize(aFinalizeError)) {
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive decode failed: " + aFinalizeError);
        aFinalizeAndReleaseCursor();
        return false;
      }
    } else {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive decode failed: " + aFinalizeError);
      aFinalizeAndReleaseCursor();
      return false;
    }
  }
  if (!aCursor.mFolderDecoder.Finalize(aFinalizeError)) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: " + aFinalizeError);
    aFinalizeAndReleaseCursor();
    return false;
  }
  if (!aCursor.mFileDecoder.Finalize(aFinalizeError)) {
    const bool aCanPromoteTailPartial =
        DecodeIntentAllowsSalvageV2(pContext.Request().mIntent) ||
        pContext.Request().mIntent == DecodeIntentV2::kUnbundle;
    if (aCanPromoteTailPartial) {
      const bool aInsideFile = aCursor.mFileDecoder.IsInsideFile();
      const std::string aPartialReference =
          aCursor.mFileDecoder.CurrentFileReference();
      const bool aPromotedPartial =
          aInsideFile ? aCursor.mFileDecoder.AbortCurrentFile()
                      : aCursor.mFileDecoder.AbortCurrentRecordAsPartial();
      if (!aInsideFile && !aPromotedPartial) {
        aCursor.mFileDecoder.ResetAfterParseError();
      }
      aCursor.mHasOpenFileContinuation = false;
      aCursor.mContinuationArchiveSlot = 0u;
      aCursor.mContinuationBlockIndex = 0u;
      aCursor.mPendingRecoverResync = false;

      if (aInsideFile || aPromotedPartial) {
        pContext.EmitLog(
            LogLevelV2::kWarning,
            DecodeStagePrefix(pContext.Request().mIntent,
                              ProgressStageV2::kArchiveDecode) +
                " Reached end of readable blocks while writing '" +
                aPartialReference + "'. Keeping partial output.");
        if (!aPromotedPartial) {
          pContext.EmitLog(
              LogLevelV2::kWarning,
              DecodeStagePrefix(pContext.Request().mIntent,
                                ProgressStageV2::kArchiveDecode) +
                  " Partial output could not be promoted with its usual partial-file "
                  "rename marker.");
        }
      } else {
        pContext.EmitLog(
            LogLevelV2::kWarning,
            DecodeStagePrefix(pContext.Request().mIntent,
                              ProgressStageV2::kArchiveDecode) +
                " Reached end of readable blocks mid-record; discarding trailing "
                "incomplete record.");
      }

      aFinalizeError.clear();
      if (!aCursor.mFileDecoder.Finalize(aFinalizeError)) {
        pContext.EmitLog(LogLevelV2::kError,
                         "Archive decode failed: " + aFinalizeError);
        aFinalizeAndReleaseCursor();
        return false;
      }
    } else {
      if (aCursor.mFileDecoder.IsInsideFile()) {
        (void)aCursor.mFileDecoder.AbortCurrentFile();
      }
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive decode failed: " + aFinalizeError);
      aFinalizeAndReleaseCursor();
      return false;
    }
  }

  if (!PruneTransientDecodeOutputs(
          pContext, pContext.Request().mDestinationDirectory, aFinalizeError)) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Failed cleaning transient decode outputs: " + aFinalizeError);
    aFinalizeError.clear();
  }

  SnapshotDecodeOutput(pContext, aCursor);
  pContext.EmitLog(
      LogLevelV2::kInfo,
      DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
          " Wrote " +
          std::to_string(pContext.State().mOutput.mFilesWritten) + " files and " +
          std::to_string(pContext.State().mOutput.mFoldersCreated) + " folders.");
  pContext.EmitPhaseProgress(1.0, "Archive decode complete");
  pContext.ClearPhaseContinuationRequest();
  aFinalizeAndReleaseCursor();
  return true;
}

bool FinalizeDecodeArchiveAfterUnbundleDamage(
    DecodeStageContextV2& pContext,
    std::shared_ptr<DecodeArchiveDecodeCursorV2>& pCursorPtr) {
  if (!pCursorPtr) {
    return true;
  }

  DecodeArchiveDecodeCursorV2& aCursor = *pCursorPtr;
  const bool aInsideFile = aCursor.mFileDecoder.IsInsideFile();
  (void)(aInsideFile ? aCursor.mFileDecoder.AbortCurrentFile()
                     : aCursor.mFileDecoder.AbortCurrentRecordAsPartial());
  ResetDecodeRecordStateAfterDamagedBlock(aCursor);
  aCursor.mPendingRecoverResync = false;
  return FinalizeDecodeArchivePhase(pContext, pCursorPtr);
}

}  // namespace

bool DecodeArchiveDecodeV2::Run(DecodeStageContextV2& pContext) {
  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  const std::size_t aSectionPayloadBytes = pContext.Layout().SectionPayloadBytes();
  std::shared_ptr<DecodeArchiveDecodeCursorV2>& aCursorPtr =
      pContext.State().mCursor.mArchiveDecode;

  if (!aCursorPtr) {
    pContext.State().mManifest = DecodeManifestStateV2{};
    pContext.State().mOutput = DecodeOutputStateV2{};
    pContext.State().mCancel = DecodeCancelStateV2{};
    aCursorPtr = std::make_shared<DecodeArchiveDecodeCursorV2>(pContext);
    if (aCursorPtr->mBlockBytes.Empty() ||
        aCursorPtr->mDeepDecodeBlockBytes.Empty() ||
        aCursorPtr->mDeepResumeProbeRawBytes.Empty() ||
        aCursorPtr->mDeepResumeProbeDecodeBytes.Empty()) {
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive decode failed: block buffers could not be allocated.");
      aCursorPtr.reset();
      return false;
    }
    pContext.EmitLog(
        LogLevelV2::kInfo,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " START. " +
            BuildArchiveDecodeSummary(
                BuildArchiveDecodeStat(pContext,
                                       aCursorPtr->mFolderDecoder,
                                       aCursorPtr->mFileDecoder,
                                       0u)) +
            ".");
    StartUnbundleArchiveWalkAtFirstMainBlock(pContext, *aCursorPtr);
  }

  DecodeArchiveDecodeCursorV2& aCursor = *aCursorPtr;
  auto FailArchiveDecodePhase = [&](const std::string& pReason) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(
            LogActionFromDecodeIntentV2(pContext.Request().mIntent),
            ProgressStageV2::kArchiveDecode,
            pReason));
    CleanupDeepRecoverTempArchives(pContext, aCursor);
    aCursorPtr.reset();
    return false;
  };
  auto TryYieldToHealing = [&](const std::string& pWaitMessage,
                               bool& pOutDidYield) {
    pOutDidYield = false;
    std::string aHealingError;
    const HealingActivationResultV2 aHealingResult =
        TryActivateDeepRecoverHealingAndYield(
            pContext, aCursor, pWaitMessage, aHealingError);
    if (aHealingResult == HealingActivationResultV2::kError) {
      return FailArchiveDecodePhase("healing initialization failed: " +
                                    aHealingError);
    }
    pOutDidYield =
        aHealingResult == HealingActivationResultV2::kActivatedAndYielding;
    return true;
  };
  auto HandleDamagedAdvanceResult = [&](DamagedBlockAdvanceResultV2 pResult) {
    if (pResult == DamagedBlockAdvanceResultV2::kYielded) {
      return 1;
    }
    if (pResult == DamagedBlockAdvanceResultV2::kError) {
      CleanupDeepRecoverTempArchives(pContext, aCursor);
      aCursorPtr.reset();
      return -1;
    }
    return 0;
  };
  auto CloseArchiveOrFail = [&](const DiscoveredArchiveFileV2& pArchiveToClose) {
    if (CloseDecodeArchiveForCursor(pContext, aCursor, pArchiveToClose)) {
      return true;
    }
    CleanupDeepRecoverTempArchives(pContext, aCursor);
    aCursorPtr.reset();
    return false;
  };
  while (aCursor.mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
    const DiscoveredArchiveFileV2& aArchive =
        pContext.State().mDiscovery.mArchives[aCursor.mArchiveSlot];

    if (HandlePausedDecodeCheckpointCancel(pContext, aCursor)) {
      SnapshotDecodeOutput(pContext, aCursor);
      aCursorPtr.reset();
      return true;
    }

    if (!aCursor.mArchiveAnnounced) {
      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeArchiveStarted)) {
        EmitDecodeArchiveEvent(pContext, aArchive, aCursor.mArchiveSlot);
      }
      aCursor.mArchiveAnnounced = true;

      if (!aArchive.mIsPresent &&
          !ShouldDecodeArchiveFromDeepTemp(
              aCursor, aCursor.mArchiveSlot, aCursor.mBlockIndex) &&
          !aCursor.mDeepDecodeFromTemp) {
        bool aYieldedToHealing = false;
        if (!TryYieldToHealing(std::string(), aYieldedToHealing)) {
          return false;
        }
        if (aYieldedToHealing) {
          return true;
        }
        if (!CloseArchiveOrFail(aArchive)) {
          return false;
        }
        if (aCursor.mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
        break;
      }
      if (aArchive.mHasReadableHeader &&
          pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeArchiveHeaderRead)) {
        EmitDecodeArchiveHeaderEvent(pContext, aArchive, aCursor.mArchiveSlot);
      }
    }

    SectionHeaderV2 aSectionHeader;
    std::size_t aBlockPayloadStart = 0u;
    std::size_t aBlockPayloadEnd = 0u;
    std::uint64_t aLogicalBlockIndex = aCursor.mBlockIndex;
    const bool aResumingPausedBlock = aCursor.mHasPausedBlockBoundary;
    if (!aResumingPausedBlock) {
      const std::size_t aInitialArchiveSlot = aCursor.mArchiveSlot;
      const std::uint64_t aInitialBlockIndex = aCursor.mBlockIndex;
      std::string aDeepRecoverError;
      const DeepRecoverBlockReadyV2 aBlockReady =
          EnsureDeepRecoverBlockReady(pContext, aCursor, aDeepRecoverError);
      if (aBlockReady == DeepRecoverBlockReadyV2::kError) {
        return FailArchiveDecodePhase("healing scan failed: " + aDeepRecoverError);
      }
      if (aBlockReady == DeepRecoverBlockReadyV2::kCancelFinalize) {
        return FinalizeDecodeArchivePhase(pContext, aCursorPtr);
      }
      if (aBlockReady == DeepRecoverBlockReadyV2::kWaiting) {
        return true;
      }
      if (aBlockReady == DeepRecoverBlockReadyV2::kFallbackStarted) {
        continue;
      }
      if (aCursor.mArchiveSlot != aInitialArchiveSlot ||
          aCursor.mBlockIndex != aInitialBlockIndex) {
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }
    }

    const std::uint64_t aReadableBlockCount =
        CurrentDecodeReadableBlockCountForArchive(pContext, aCursor, aArchive);
    if (aCursor.mBlockIndex >= aReadableBlockCount) {
      if (!CloseArchiveOrFail(aArchive)) {
        return false;
      }
      if (aCursor.mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }
      break;
    }

    if (!aResumingPausedBlock) {
      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeBlockStarted)) {
        EmitDecodeBlockEvent(pContext,
                             RuntimeEventKindV2::kDecodeBlockStarted,
                             aArchive,
                             aCursor.mArchiveSlot,
                             aCursor.mBlockIndex,
                             nullptr,
                             std::string());
      }
      std::string aReadError;
      const DecodeCurrentBlockReadResultV2 aReadResult =
          ReadCurrentDecodeBlock(pContext, aCursor, aArchive, aReadError);
      if (aReadResult == DecodeCurrentBlockReadResultV2::kDeepTempFailure) {
        return FailArchiveDecodePhase("healing temp read failed: " + aReadError);
      }
      if (aReadResult == DecodeCurrentBlockReadResultV2::kReadFailure) {
        bool aYieldedToHealing = false;
        if (!TryYieldToHealing(std::string(), aYieldedToHealing)) {
          return false;
        }
        if (aYieldedToHealing) {
          return true;
        }
        const PartialCloseResultV2 aPartialClose = CloseCurrentRecordAsPartial(aCursor);
        if (aPartialClose.mHadOpenFile || aPartialClose.mPromotedPartial) {
          EmitDecodeErrorMarkerEvent(
              pContext,
              "file_data_error",
              "Decode file encountered data error due to missing block.",
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              SectionTypeLabel(SectionTypeV2::kArchiveData),
              aPartialClose.mReference);
          if (aPartialClose.mPromotedPartial) {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_closed_partial",
                "Decode closed file as partial after missing block.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
          }
        }
        ResetDecodeRecordStateAfterDamagedBlock(aCursor);
        EmitDecodeErrorMarkerEvent(
            pContext,
            "block_missing",
            "Decode block missing/unreadable at archive slot " +
                std::to_string(aCursor.mArchiveSlot) + ", block " +
                std::to_string(aCursor.mBlockIndex),
            aArchive,
            aCursor.mArchiveSlot,
            aCursor.mBlockIndex,
            nullptr,
            std::string());
        if (!HandleDamagedBlock(pContext, "a block could not be read from disk.")) {
          aCursorPtr.reset();
          return false;
        }
        if (ShouldStopAfterFirstDamagedBlock(pContext)) {
          return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
        }
        if (!TryYieldToHealing(std::string(), aYieldedToHealing)) {
          return false;
        }
        if (aYieldedToHealing) {
          return true;
        }
        SwitchToPessimistic(pContext, "a block could not be read from disk.");
        aCursor.mPendingRecoverResync = true;
        const int aAdvanceResult = HandleDamagedAdvanceResult(
            ContinueAfterDamagedBlockWithSkipLandingFallback(
                pContext, aCursor, aArchive));
        if (aAdvanceResult > 0) {
          return true;
        }
        if (aAdvanceResult < 0) {
          return false;
        }
        break;
      }

      bool aHasValidatedSectionHeader = false;
      if (ShouldTryPlaintextPreviewManifestBlock(pContext)) {
        if (TryReadValidatedSectionHeader(aCursor.mBlockBytes.Data(),
                                          aSectionPayloadBytes,
                                          aSectionHeader) &&
            aSectionHeader.mSectionType ==
                static_cast<std::uint8_t>(SectionTypeV2::kPreviewManifest)) {
          aHasValidatedSectionHeader = true;
        }
      }

      if (!aHasValidatedSectionHeader &&
          pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u &&
          !aCursor.mReadFromDeepTemp) {
        if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeDecryptionStarted)) {
          EmitDecodeDecryptionStartedEvent(
              pContext, aArchive, aCursor.mArchiveSlot, aCursor.mBlockIndex);
        }
        std::string aUnsealError;
        if (pContext.State().mCipher.mWorkerBuffer.Size() < aArchiveBlockBytes ||
            !pContext.State().mCipher.mCipher.Unseal(
                aCursor.mBlockBytes.Data(),
                pContext.State().mCipher.mWorkerBuffer.Data(),
                aCursor.mBlockBytes.Data(),
                aArchiveBlockBytes,
                &aUnsealError)) {
          bool aYieldedToHealing = false;
          if (!TryYieldToHealing("Found an invalid checksum, switching to healing scan.",
                                 aYieldedToHealing)) {
            return false;
          }
          if (aYieldedToHealing) {
            return true;
          }
          const PartialCloseResultV2 aPartialClose = CloseCurrentRecordAsPartial(aCursor);
          if (aPartialClose.mHadOpenFile || aPartialClose.mPromotedPartial) {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_data_error",
                "Decode file encountered data error due to decrypt/checksum failure.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
            if (aPartialClose.mPromotedPartial) {
              EmitDecodeErrorMarkerEvent(
                  pContext,
                  "file_closed_partial",
                  "Decode closed file as partial after decrypt/checksum failure.",
                  aArchive,
                  aCursor.mArchiveSlot,
                  aCursor.mBlockIndex,
                  SectionTypeLabel(SectionTypeV2::kArchiveData),
                  aPartialClose.mReference);
            }
          }
          ResetDecodeRecordStateAfterDamagedBlock(aCursor);
          EmitDecodeErrorMarkerEvent(
              pContext,
              "block_bad_checksum",
              "Decode block failed decryption/checksum at archive slot " +
                  std::to_string(aCursor.mArchiveSlot) + ", block " +
                  std::to_string(aCursor.mBlockIndex),
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              nullptr,
              std::string());
          if (!HandleDamagedBlock(pContext,
                                  aUnsealError.empty()
                                      ? "a block could not be unsealed."
                                      : "a block could not be unsealed: " +
                                            aUnsealError)) {
            aCursorPtr.reset();
            return false;
          }
          if (ShouldStopAfterFirstDamagedBlock(pContext)) {
            return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
          }
          if (!TryYieldToHealing("Found an invalid checksum, switching to healing scan.",
                                 aYieldedToHealing)) {
            return false;
          }
          if (aYieldedToHealing) {
            return true;
          }
          SwitchToPessimistic(
              pContext, "a block could not be unsealed or checksum-validated.");
          aCursor.mPendingRecoverResync = true;
          const int aAdvanceResult = HandleDamagedAdvanceResult(
              ContinueAfterDamagedBlockWithSkipLandingFallback(
                  pContext, aCursor, aArchive));
          if (aAdvanceResult > 0) {
            return true;
          }
          if (aAdvanceResult < 0) {
            return false;
          }
          break;
        }
        if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeDecryptionFinished)) {
          EmitDecodeDecryptionEvent(
              pContext, aArchive, aCursor.mArchiveSlot, aCursor.mBlockIndex);
        }
      }

      if (!aHasValidatedSectionHeader) {
        if (!TryReadValidatedSectionHeader(aCursor.mBlockBytes.Data(),
                                           aSectionPayloadBytes,
                                           aSectionHeader)) {
          bool aYieldedToHealing = false;
          if (!TryYieldToHealing("Found an invalid data sequencing, switching to healing scan.",
                                 aYieldedToHealing)) {
            return false;
          }
          if (aYieldedToHealing) {
            return true;
          }
          const PartialCloseResultV2 aPartialClose = CloseCurrentRecordAsPartial(aCursor);
          if (aPartialClose.mHadOpenFile || aPartialClose.mPromotedPartial) {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_data_error",
                "Decode file encountered data error due to section-header failure.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
            if (aPartialClose.mPromotedPartial) {
              EmitDecodeErrorMarkerEvent(
                  pContext,
                  "file_closed_partial",
                  "Decode closed file as partial after section-header failure.",
                  aArchive,
                  aCursor.mArchiveSlot,
                  aCursor.mBlockIndex,
                  SectionTypeLabel(SectionTypeV2::kArchiveData),
                  aPartialClose.mReference);
            }
          }
          ResetDecodeRecordStateAfterDamagedBlock(aCursor);
          EmitDecodeErrorMarkerEvent(
              pContext,
              "block_bad_checksum",
              "Decode block header checksum/validation failed at archive slot " +
                  std::to_string(aCursor.mArchiveSlot) + ", block " +
                  std::to_string(aCursor.mBlockIndex),
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              nullptr,
              std::string());
          if (!HandleDamagedBlock(pContext, "a section header failed validation.")) {
            aCursorPtr.reset();
            return false;
          }
          if (ShouldStopAfterFirstDamagedBlock(pContext)) {
            return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
          }
          if (!TryYieldToHealing(std::string(), aYieldedToHealing)) {
            return false;
          }
          if (aYieldedToHealing) {
            return true;
          }
          SwitchToPessimistic(
              pContext, "a section header failed validation.");
          aCursor.mPendingRecoverResync = true;
          const int aAdvanceResult = HandleDamagedAdvanceResult(
              ContinueAfterDamagedBlockWithSkipLandingFallback(
                  pContext, aCursor, aArchive));
          if (aAdvanceResult > 0) {
            return true;
          }
          if (aAdvanceResult < 0) {
            return false;
          }
          break;
        }
      }

      const std::uint64_t aExpectedLogicalBlockIndex =
          ExpectedLogicalBlockIndexForPhysical(
              aCursor, aCursor.mArchiveSlot, aCursor.mBlockIndex);
      const bool aHeaderIndexMatches =
          HeaderIndexMatchesCursor(
              aArchive, aSectionHeader, aExpectedLogicalBlockIndex);
      if (!aHeaderIndexMatches) {
        const bool aArchiveIndexMatched =
            aSectionHeader.mArchiveIndex ==
            static_cast<std::uint32_t>(aArchive.mArchiveIndex);
        const std::uint64_t aHeaderLogicalBlockIndex =
            static_cast<std::uint64_t>(aSectionHeader.mBlockIndex);
        const bool aCanInferLostBlocks =
            DecodeIntentAllowsSalvageV2(pContext.Request().mIntent) &&
            aArchiveIndexMatched &&
            aHeaderLogicalBlockIndex > aExpectedLogicalBlockIndex;
        if (aCanInferLostBlocks) {
          const std::uint64_t aLostDelta =
              aHeaderLogicalBlockIndex - aExpectedLogicalBlockIndex;
          bool aYieldedToHealing = false;
          if (!TryYieldToHealing(
                  "Observed logical gap of " + std::to_string(aLostDelta) +
                      " block(s) in archive slot " +
                      std::to_string(aCursor.mArchiveSlot) +
                      "; waiting for healing scan to stage the missing logical slot.",
                  aYieldedToHealing)) {
            return false;
          }
          if (aYieldedToHealing) {
            return true;
          }

          if (aCursor.mArchiveSlot < aCursor.mArchiveLostBlocksBySlot.size()) {
            aCursor.mArchiveLostBlocksBySlot[aCursor.mArchiveSlot] += aLostDelta;
          }

          const PartialCloseResultV2 aPartialClose = CloseCurrentRecordAsPartial(aCursor);
          if (aPartialClose.mHadOpenFile || aPartialClose.mPromotedPartial) {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_data_error",
                "Decode file encountered inferred missing block(s) before current header index.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
            if (aPartialClose.mPromotedPartial) {
              EmitDecodeErrorMarkerEvent(
                  pContext,
                  "file_closed_partial",
                  "Decode closed file as partial after inferred missing block(s).",
                  aArchive,
                  aCursor.mArchiveSlot,
                  aCursor.mBlockIndex,
                  SectionTypeLabel(SectionTypeV2::kArchiveData),
                  aPartialClose.mReference);
            }
          }
          ResetDecodeRecordStateAfterDamagedBlock(aCursor);

          SwitchToPessimistic(
              pContext,
              "section header index jumped forward; inferred deleted block(s).");
          pContext.EmitLog(
              LogLevelV2::kWarning,
              DecodeStagePrefix(pContext.Request().mIntent,
                                ProgressStageV2::kArchiveDecode) +
                  " Inferred " + std::to_string(aLostDelta) +
                  " deleted block(s) in archive slot " +
                  std::to_string(aCursor.mArchiveSlot) +
                  " from section header index jump (expected logical " +
                  std::to_string(aExpectedLogicalBlockIndex) + ", observed " +
                  std::to_string(aHeaderLogicalBlockIndex) + ").");
          aCursor.mPendingRecoverResync = true;
        } else {
          bool aYieldedToHealing = false;
          if (!TryYieldToHealing(std::string(), aYieldedToHealing)) {
            return false;
          }
          if (aYieldedToHealing) {
            return true;
          }
          const PartialCloseResultV2 aPartialClose = CloseCurrentRecordAsPartial(aCursor);
          if (aPartialClose.mHadOpenFile || aPartialClose.mPromotedPartial) {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_data_error",
                "Decode file encountered data error due to section-index mismatch.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
            if (aPartialClose.mPromotedPartial) {
              EmitDecodeErrorMarkerEvent(
                  pContext,
                  "file_closed_partial",
                  "Decode closed file as partial after section-index mismatch.",
                  aArchive,
                  aCursor.mArchiveSlot,
                  aCursor.mBlockIndex,
                  SectionTypeLabel(SectionTypeV2::kArchiveData),
                  aPartialClose.mReference);
            }
          }
          ResetDecodeRecordStateAfterDamagedBlock(aCursor);
          EmitDecodeErrorMarkerEvent(
              pContext,
              "block_index_mismatch",
              "Decode section header index mismatch at archive slot " +
                  std::to_string(aCursor.mArchiveSlot) + ", block " +
                  std::to_string(aCursor.mBlockIndex) + ".",
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              nullptr,
              std::string());
          if (!HandleDamagedBlock(
                  pContext,
                  "a section header index disagreed with archive/block position.")) {
            aCursorPtr.reset();
            return false;
          }
          if (ShouldStopAfterFirstDamagedBlock(pContext)) {
            return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
          }
          if (!TryYieldToHealing(std::string(), aYieldedToHealing)) {
            return false;
          }
          if (aYieldedToHealing) {
            return true;
          }
          SwitchToPessimistic(
              pContext,
              "section header index mismatched expected archive/block position.");
          aCursor.mPendingRecoverResync = true;
          const int aAdvanceResult = HandleDamagedAdvanceResult(
              ContinueAfterDamagedBlockWithSkipLandingFallback(
                  pContext, aCursor, aArchive));
          if (aAdvanceResult > 0) {
            return true;
          }
          if (aAdvanceResult < 0) {
            return false;
          }
          break;
        }
      }
      aLogicalBlockIndex = static_cast<std::uint64_t>(aSectionHeader.mBlockIndex);

      const std::string aFileReferenceBeforeDecode =
          static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
                  SectionTypeV2::kArchiveData
              ? std::string(aCursor.mFileDecoder.CurrentFileReference())
              : std::string();
      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeBlockHeaderRead)) {
        EmitDecodeBlockHeaderEvent(pContext,
                                   aArchive,
                                   aCursor.mArchiveSlot,
                                   aCursor.mBlockIndex,
                                   aSectionHeader,
                                   aFileReferenceBeforeDecode);
      }

      aBlockPayloadEnd = std::min<std::size_t>(
          aSectionPayloadBytes, static_cast<std::size_t>(aSectionHeader.mPayloadBytesUsed));

      if (aCursor.mHasForcedBlockPayloadStart) {
        const std::size_t aForcedOffset = aCursor.mForcedBlockPayloadStart;
        aCursor.mHasForcedBlockPayloadStart = false;
        aCursor.mForcedBlockPayloadStart = 0u;
        if (aForcedOffset >= aBlockPayloadEnd) {
          if (!HandleDamagedBlock(
                  pContext,
                  "a skip-record jump targeted an invalid payload offset.")) {
            aCursorPtr.reset();
            return false;
          }
          if (ShouldStopAfterFirstDamagedBlock(pContext)) {
            return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
          }
          const int aAdvanceResult = HandleDamagedAdvanceResult(
              ContinueAfterDamagedBlockWithSkipLandingFallback(
                  pContext, aCursor, aArchive));
          if (aAdvanceResult > 0) {
            return true;
          }
          if (aAdvanceResult < 0) {
            return false;
          }
          break;
        }
        aBlockPayloadStart = aForcedOffset;
      }
    } else {
      aSectionHeader = aCursor.mPausedSectionHeader;
      aBlockPayloadStart = aCursor.mPausedBlockPayloadOffset;
      aBlockPayloadEnd = aCursor.mPausedBlockPayloadEnd;
      aLogicalBlockIndex = static_cast<std::uint64_t>(aSectionHeader.mBlockIndex);
      aCursor.mHasPausedBlockBoundary = false;
      aCursor.mPausedSectionHeader = SectionHeaderV2{};
      aCursor.mPausedBlockPayloadOffset = 0u;
      aCursor.mPausedBlockPayloadEnd = 0u;
      aCursor.mPausedBoundaryRecordReference.clear();
    }

    if (aCursor.mPendingRecoverResync &&
        static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
            SectionTypeV2::kArchiveData) {
      if (TryApplyRecoverLocalSkipAnchor(pContext,
                                         aCursor,
                                         aSectionHeader,
                                         aCursor.mArchiveSlot,
                                         aLogicalBlockIndex,
                                         aBlockPayloadStart,
                                         aBlockPayloadEnd)) {
        aCursor.mPendingRecoverResync = false;
        SnapshotDecodeOutput(pContext, aCursor);
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }
      if (aCursor.mDeepOutputReplayActive) {
        const bool aAppliedSkip = TryApplyRecoverSkipRecord(pContext,
                                                            aCursor,
                                                            aSectionHeader,
                                                            aCursor.mArchiveSlot,
                                                            aCursor.mBlockIndex,
                                                            aLogicalBlockIndex,
                                                            aBlockPayloadStart,
                                                            aBlockPayloadEnd);
        if (aAppliedSkip) {
          aCursor.mPendingRecoverResync = false;
          SnapshotDecodeOutput(pContext, aCursor);
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
      }
      if (!IsFirstMainArchiveDataBlock(pContext, aSectionHeader)) {
        const int aAdvanceResult =
            HandleDamagedAdvanceResult(ContinueAfterDamagedBlock(pContext, aCursor, aArchive));
        if (aAdvanceResult > 0) {
          return true;
        }
        if (aAdvanceResult < 0) {
          return false;
        }
        break;
      }
    }

    const std::uint8_t aExpectedType = ExpectedSectionType(pContext);
    const bool aPhysicalRepairZone =
        aExpectedType == static_cast<std::uint8_t>(SectionTypeV2::kRepairData);

    if (pContext.State().mDiscovery.mMode == DecodeModeV2::kOptimistic) {
      if (!aPhysicalRepairZone && aSectionHeader.mSectionType != aExpectedType) {
        if (TryAcceptMissingPreviewManifest(pContext, aSectionHeader)) {
          // Expectations were updated to match the observed stream.
        } else {
          const PartialCloseResultV2 aPartialClose = CloseCurrentRecordAsPartial(aCursor);
          if (aPartialClose.mHadOpenFile || aPartialClose.mPromotedPartial) {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_data_error",
                "Decode file encountered data error due to section-type mismatch.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
            if (aPartialClose.mPromotedPartial) {
              EmitDecodeErrorMarkerEvent(
                  pContext,
                  "file_closed_partial",
                  "Decode closed file as partial after section-type mismatch.",
                  aArchive,
                  aCursor.mArchiveSlot,
                  aCursor.mBlockIndex,
                  SectionTypeLabel(SectionTypeV2::kArchiveData),
                  aPartialClose.mReference);
            }
          }
          if (!HandleDamagedBlock(
                  pContext,
                  "a section type disagreed with the optimistic family layout.")) {
            aCursorPtr.reset();
            return false;
          }
        }
        if (ShouldStopAfterFirstDamagedBlock(pContext)) {
          return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
        }
      }
    }

    if (aCursor.mHasOpenFileContinuation) {
      const bool aArchiveDataSection =
          static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
          SectionTypeV2::kArchiveData;
      const bool aExpectedContinuation =
          aArchiveDataSection &&
          IsExpectedContinuationBlock(pContext.State().mDiscovery.mArchives,
                                      aCursor.mContinuationArchiveSlot,
                                      aCursor.mContinuationBlockIndex,
                                      aCursor.mArchiveSlot,
                                      aLogicalBlockIndex);
      if (!aExpectedContinuation) {
        if (aPhysicalRepairZone && !aCursor.mFileDecoder.IsInsideFile()) {
          aCursor.mHasOpenFileContinuation = false;
        } else {
          const PartialCloseResultV2 aPartialClose = CloseCurrentRecordAsPartial(aCursor);
          aCursor.mHasOpenFileContinuation = false;
          EmitDecodeErrorMarkerEvent(
              pContext,
              "file_data_error",
              "Decode file continuation mismatch; file marked partial.",
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              SectionTypeLabel(SectionTypeV2::kArchiveData),
              aPartialClose.mReference);
          if (aPartialClose.mPromotedPartial) {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_closed_partial",
                "Decode closed file as partial after continuation mismatch.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
          }

          if (!DecodeIntentAllowsSalvageV2(pContext.Request().mIntent)) {
            if (!HandleDamagedBlock(
                    pContext,
                    "a continued file did not resume at the expected next block.")) {
              aCursorPtr.reset();
              return false;
            }
            if (ShouldStopAfterFirstDamagedBlock(pContext)) {
              return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
            }
            const int aAdvanceResult = HandleDamagedAdvanceResult(
                ContinueAfterDamagedBlockWithSkipLandingFallback(
                    pContext, aCursor, aArchive));
            if (aAdvanceResult > 0) {
              return true;
            }
            if (aAdvanceResult < 0) {
              return false;
            }
            break;
          }

          // In salvage mode, keep this block and continue parsing it as a fresh
          // boundary after closing the broken continuation as partial.
          SwitchToPessimistic(
              pContext,
              "a continued file did not resume at the expected next block.");
          aCursor.mPendingRecoverResync = true;
          if (aArchiveDataSection &&
              TryApplyRecoverSkipRecord(pContext,
                                        aCursor,
                                        aSectionHeader,
                                        aCursor.mArchiveSlot,
                                        aCursor.mBlockIndex,
                                        aLogicalBlockIndex,
                                        aBlockPayloadStart,
                                        aBlockPayloadEnd)) {
            aCursor.mPendingRecoverResync = false;
            SnapshotDecodeOutput(pContext, aCursor);
            pContext.ContinuePhaseOnNextHeartbeat();
            return true;
          }
        }
      }
    }

    bool aTerminated = false;
    bool aStoppedAtPadding = false;
    bool aParseError = false;
    std::string aParseErrorMessage;
    std::uint64_t aDataBytesWritten = 0u;
    if (aPhysicalRepairZone) {
      ++pContext.State().mManifest.mRepairBlocksProcessed;
      ++aCursor.mProcessedBlocks;
      ++aCursor.mArchiveBlocksRead;
      const LoggingStatV2 aStat = BuildArchiveDecodeStat(
          pContext,
          aCursor.mFolderDecoder,
          aCursor.mFileDecoder,
          aCursor.mArchivesCompleted);
      pContext.EmitPhaseProgress(
          pContext.State().mDiscovery.mTotalReadableBlocks == 0u
              ? 1.0
              : static_cast<double>(aCursor.mProcessedBlocks) /
                    static_cast<double>(pContext.State().mDiscovery.mTotalReadableBlocks),
          BuildArchiveDecodeProgressLabel(aStat));
      if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeBlockFinished)) {
        EmitDecodeBlockEvent(pContext,
                             RuntimeEventKindV2::kDecodeBlockFinished,
                             aArchive,
                             aCursor.mArchiveSlot,
                             aCursor.mBlockIndex,
                             SectionTypeLabel(SectionTypeV2::kRepairData),
                             std::string());
      }
      ConsumeCurrentDeepRecoverPackedBlock(pContext, aCursor);
      ++aCursor.mBlockIndex;
      if (aCursor.mBlockIndex >=
          CurrentDecodeReadableBlockCountForArchive(pContext, aCursor, aArchive)) {
        if (!CloseArchiveOrFail(aArchive)) {
          return false;
        }
      }
      if (aCursor.mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }
      break;
    }

    const std::string aBlockStartFileReference =
        static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
                SectionTypeV2::kArchiveData
            ? std::string(aCursor.mFileDecoder.CurrentFileReference())
            : std::string();
    bool aPausedAtBoundary = false;
    std::size_t aResumeOffset = aBlockPayloadStart;
    std::string aPausedRecordReference;
    const SectionTypeV2 aDecodedSectionType =
        static_cast<SectionTypeV2>(aSectionHeader.mSectionType);
    bool aArchiveDataAlreadyConsumed = false;

    if (aCursor.mPendingSingleByteTypeGap) {
      if (aDecodedSectionType != SectionTypeV2::kArchiveData) {
        ResetDecodeRecordStateAfterDamagedBlock(aCursor);
        if (!HandleDamagedBlock(
                pContext,
                "a deferred missing type byte was followed by a non-data section.")) {
          aCursorPtr.reset();
          return false;
        }
        if (ShouldStopAfterFirstDamagedBlock(pContext)) {
          return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
        }
        const int aAdvanceResult = HandleDamagedAdvanceResult(
            ContinueAfterDamagedBlockWithSkipLandingFallback(
                pContext, aCursor, aArchive));
        if (aAdvanceResult > 0) {
          return true;
        }
        if (aAdvanceResult < 0) {
          return false;
        }
        break;
      }

      if (aBlockPayloadEnd > aBlockPayloadStart) {
        const unsigned char* aPayloadBytes =
            aCursor.mBlockBytes.Data() + kSectionHeaderBytesV2 + aBlockPayloadStart;
        aCursor.mPendingSingleByteTypePayload.insert(
            aCursor.mPendingSingleByteTypePayload.end(),
            aPayloadBytes,
            aPayloadBytes + (aBlockPayloadEnd - aBlockPayloadStart));
      }

      const SingleByteTypeGapResolutionV2 aGapResolution =
          ResolveSingleByteTypeGap(aCursor.mFileDecoder.CurrentFileReference(),
                                   aCursor.mPendingSingleByteTypePayload,
                                   pContext.Layout());
      if (aGapResolution == SingleByteTypeGapResolutionV2::kImpossible ||
          aGapResolution == SingleByteTypeGapResolutionV2::kResolvedUnsupported) {
        const std::string aReason =
            aGapResolution == SingleByteTypeGapResolutionV2::kResolvedUnsupported
                ? "a deferred missing type byte resolved to an unsupported non-file record."
                : "a deferred missing type byte could not be reconciled with later record grammar.";
        ResetDecodeRecordStateAfterDamagedBlock(aCursor);
        if (!HandleDamagedBlock(pContext, aReason)) {
          aCursorPtr.reset();
          return false;
        }
        if (ShouldStopAfterFirstDamagedBlock(pContext)) {
          return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
        }
        const int aAdvanceResult = HandleDamagedAdvanceResult(
            ContinueAfterDamagedBlockWithSkipLandingFallback(
                pContext, aCursor, aArchive));
        if (aAdvanceResult > 0) {
          return true;
        }
        if (aAdvanceResult < 0) {
          return false;
        }
        break;
      }
      if (aGapResolution == SingleByteTypeGapResolutionV2::kResolvedFile) {
        std::string aReplayError;
        if (!ReplayPendingSingleByteTypePayload(
                pContext, aCursor, aReplayError)) {
          ResetDecodeRecordStateAfterDamagedBlock(aCursor);
          if (!HandleDamagedBlock(
                  pContext,
                  "archive-data deferred type replay failed: " + aReplayError)) {
            aCursorPtr.reset();
            return false;
          }
          if (ShouldStopAfterFirstDamagedBlock(pContext)) {
            return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
          }
          const int aAdvanceResult = HandleDamagedAdvanceResult(
              ContinueAfterDamagedBlockWithSkipLandingFallback(
                  pContext, aCursor, aArchive));
          if (aAdvanceResult > 0) {
            return true;
          }
          if (aAdvanceResult < 0) {
            return false;
          }
          break;
        }
        aArchiveDataAlreadyConsumed = true;
      }
    }

    if (!aCursor.mPendingSingleByteTypeGap && !aArchiveDataAlreadyConsumed) {
      switch (aDecodedSectionType) {
      case SectionTypeV2::kPreviewManifest:
        if (!aCursor.mPreviewDecoder.Consume(aCursor.mBlockBytes.Data() + kSectionHeaderBytesV2,
                                             aBlockPayloadStart,
                                             aBlockPayloadEnd,
                                             false,
                                             aTerminated,
                                             aStoppedAtPadding,
                                             aParseError,
                                             aParseErrorMessage,
                                             aDataBytesWritten,
                                             aPausedAtBoundary,
                                             aResumeOffset,
                                             aPausedRecordReference)) {
          EmitDecodeErrorMarkerEvent(
              pContext,
              "preview_record_skip_started",
              "Decode preview record started skip due to parse error.",
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              SectionTypeLabel(SectionTypeV2::kPreviewManifest),
              aCursor.mPreviewDecoder.CurrentFileReference());
          if (!HandleDamagedBlock(
                  pContext,
                  "preview logical record parse failed: " + aParseErrorMessage)) {
            aCursorPtr.reset();
            return false;
          }
          if (ShouldStopAfterFirstDamagedBlock(pContext)) {
            return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
          }
          aCursor.mPreviewDecoder.ResetAfterParseError();
          EmitDecodeErrorMarkerEvent(
              pContext,
              "preview_record_skip_finished",
              "Decode preview record finished skip; continuing recover walk.",
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              SectionTypeLabel(SectionTypeV2::kPreviewManifest),
              aCursor.mPreviewDecoder.CurrentFileReference());
          const int aAdvanceResult = HandleDamagedAdvanceResult(
              ContinueAfterDamagedBlockWithSkipLandingFallback(
                  pContext, aCursor, aArchive));
          if (aAdvanceResult > 0) {
            return true;
          }
          if (aAdvanceResult < 0) {
            return false;
          }
          aParseError = false;
          aParseErrorMessage.clear();
          continue;
        }
        break;

      case SectionTypeV2::kArchiveData:
        if (!aCursor.mFileDecoder.Consume(aCursor.mBlockBytes.Data() + kSectionHeaderBytesV2,
                                          aBlockPayloadStart,
                                          aBlockPayloadEnd,
                                          false,
                                          aTerminated,
                                          aStoppedAtPadding,
                                          aParseError,
                                          aParseErrorMessage,
                                          aDataBytesWritten,
                                          aPausedAtBoundary,
                                          aResumeOffset,
                                          aPausedRecordReference)) {
          const PartialCloseResultV2 aPartialClose = CloseCurrentRecordAsPartial(aCursor);
          if (aPartialClose.mHadOpenFile || aPartialClose.mPromotedPartial) {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_data_error",
                "Decode file encountered data error; file will be partial.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
            if (aPartialClose.mPromotedPartial) {
              EmitDecodeErrorMarkerEvent(
                  pContext,
                  "file_closed_partial",
                  "Decode closed file as partial after error boundary.",
                  aArchive,
                  aCursor.mArchiveSlot,
                  aCursor.mBlockIndex,
                  SectionTypeLabel(SectionTypeV2::kArchiveData),
                  aPartialClose.mReference);
            }
          } else {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_name_error",
                "Decode file encountered name/path error before output commit.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_discarded_name_boundary",
                "Decode discarded file due to name-boundary error.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialClose.mReference);
          }
          if (!HandleDamagedBlock(
                  pContext,
                  "archive-data logical record parse failed: " +
                      aParseErrorMessage)) {
            aCursorPtr.reset();
            return false;
          }
          if (ShouldStopAfterFirstDamagedBlock(pContext)) {
            return FinalizeDecodeArchiveAfterUnbundleDamage(pContext, aCursorPtr);
          }
          aCursor.mFileDecoder.ResetAfterParseError();
          if (TryApplyRecoverSkipRecord(pContext,
                                        aCursor,
                                        aSectionHeader,
                                        aCursor.mArchiveSlot,
                                        aCursor.mBlockIndex,
                                        aLogicalBlockIndex,
                                        aBlockPayloadStart,
                                        aBlockPayloadEnd)) {
            aCursor.mPendingRecoverResync = false;
            SnapshotDecodeOutput(pContext, aCursor);
            pContext.ContinuePhaseOnNextHeartbeat();
            return true;
          }
          aCursor.mPendingRecoverResync = true;
          const int aAdvanceResult = HandleDamagedAdvanceResult(
              ContinueAfterDamagedBlockWithSkipLandingFallback(
                  pContext, aCursor, aArchive));
          if (aAdvanceResult > 0) {
            return true;
          }
          if (aAdvanceResult < 0) {
            return false;
          }
          aParseError = false;
          aParseErrorMessage.clear();
          continue;
        }
        break;

      case SectionTypeV2::kRepairData:
        ++pContext.State().mManifest.mRepairBlocksProcessed;
        break;
      }
    }

    if (aParseError) {
      if (aDecodedSectionType == SectionTypeV2::kArchiveData &&
          aCursor.mFileDecoder.IsInsideFile()) {
        (void)aCursor.mFileDecoder.AbortCurrentFile();
      }
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive decode parse error: " + aParseErrorMessage);
      aCursorPtr.reset();
      return false;
    }

    if (aDecodedSectionType == SectionTypeV2::kArchiveData) {
      aCursor.mPendingRecoverResync = false;
    }

    if (aPausedAtBoundary) {
      aCursor.mHasPausedBlockBoundary = true;
      aCursor.mPausedSectionHeader = aSectionHeader;
      aCursor.mPausedBlockPayloadOffset = aResumeOffset;
      aCursor.mPausedBlockPayloadEnd = aBlockPayloadEnd;
      aCursor.mPausedBoundaryRecordReference = aPausedRecordReference;
      aCursor.mHasOpenFileContinuation = false;
      SnapshotDecodeOutput(pContext, aCursor);
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }

    switch (aDecodedSectionType) {
      case SectionTypeV2::kPreviewManifest:
        ++pContext.State().mManifest.mPreviewManifestBlocksProcessed;
        break;
      case SectionTypeV2::kArchiveData:
        ++pContext.State().mManifest.mArchiveDataBlocksProcessed;
        if (aTerminated) {
          pContext.State().mOutput.mArchiveTerminated = true;
        }
        break;
      case SectionTypeV2::kRepairData:
        break;
    }

    ++aCursor.mProcessedBlocks;
    ++aCursor.mArchiveBlocksRead;
    const LoggingStatV2 aStat = BuildArchiveDecodeStat(
        pContext, aCursor.mFolderDecoder, aCursor.mFileDecoder, aCursor.mArchivesCompleted);
    pContext.EmitPhaseProgress(
        pContext.State().mDiscovery.mTotalReadableBlocks == 0u
            ? 1.0
            : static_cast<double>(aCursor.mProcessedBlocks) /
                  static_cast<double>(pContext.State().mDiscovery.mTotalReadableBlocks),
        BuildArchiveDecodeProgressLabel(aStat));
    if (ShouldEmitArchiveDecodeSlice(aStat,
                                     aCursor.mNextArchiveLog,
                                     aCursor.mNextFileLog,
                                     aCursor.mNextFolderLog,
                                     aCursor.mNextByteLog)) {
      pContext.EmitLog(
          LogLevelV2::kInfo,
          DecodeStagePrefix(pContext.Request().mIntent,
                            ProgressStageV2::kArchiveDecode) +
              " " + BuildArchiveDecodeSummary(aStat) + ".");
    }
    const std::string aFileReferenceAfterBlock =
        static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
                SectionTypeV2::kArchiveData
            ? std::string(aCursor.mFileDecoder.CurrentFileReference())
            : std::string();
    if (pContext.WantsRuntimeEvent(RuntimeEventKindV2::kDecodeBlockFinished)) {
      EmitDecodeBlockEvent(pContext,
                           RuntimeEventKindV2::kDecodeBlockFinished,
                           aArchive,
                           aCursor.mArchiveSlot,
                           aCursor.mBlockIndex,
                           SectionTypeLabel(
                               static_cast<SectionTypeV2>(aSectionHeader.mSectionType)),
                           aFileReferenceAfterBlock);
    }
    if (HandleArchiveDecodeCancel(
            pContext,
            aCursor.mFileDecoder,
            aBlockStartFileReference,
            aFileReferenceAfterBlock)) {
      SnapshotDecodeOutput(pContext, aCursor);
      aCursorPtr.reset();
      return true;
    }

    if (aCursor.mFileDecoder.IsInsideFile()) {
      aCursor.mHasOpenFileContinuation = true;
      aCursor.mContinuationArchiveSlot = aCursor.mArchiveSlot;
      aCursor.mContinuationBlockIndex = aLogicalBlockIndex;
    } else {
      aCursor.mHasOpenFileContinuation = false;
    }

    if (aCursor.mHasPendingSkipLandingValidation &&
        aCursor.mArchiveSlot == aCursor.mPendingSkipTargetArchiveSlot &&
        aCursor.mBlockIndex == aCursor.mPendingSkipTargetBlockIndex) {
      EmitAppliedRecoverSkipRecordLog(pContext,
                                      aCursor.mPendingSkipTargetArchiveSlot,
                                      aCursor.mPendingSkipTargetLogicalBlockIndex,
                                      aCursor.mPendingSkipTargetBlockIndex,
                                      aCursor.mPendingSkipTargetPayloadOffset);
      ResetPendingSkipLandingValidation(aCursor);
    }

    ConsumeCurrentDeepRecoverPackedBlock(pContext, aCursor);
    ++aCursor.mBlockIndex;
    if (aCursor.mBlockIndex >=
        CurrentDecodeReadableBlockCountForArchive(pContext, aCursor, aArchive)) {
      if (!CloseArchiveOrFail(aArchive)) {
        return false;
      }
    }
    if (aCursor.mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
      pContext.ContinuePhaseOnNextHeartbeat();
      return true;
    }
    break;
  }

  return FinalizeDecodeArchivePhase(pContext, aCursorPtr);
}

}  // namespace peanutbutter
