#include "Decode_ArchiveDecode.hpp"

#include <array>
#include <cstring>
#include <memory>
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

const char* SectionTypeLabel(SectionTypeV2 pSectionType) {
  switch (pSectionType) {
    case SectionTypeV2::kEmptyFolderManifest:
      return "empty_folder_manifest";
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
                 PackedUint48ToUInt64(pArchive.mHeader.mArchiveDataBlockCount));
  const std::uint64_t aLegacyEmptyFolderBlocks =
      PackedUint48ToUInt64(pArchive.mHeader.mEmptyFolderBlockCount);
  if (aLegacyEmptyFolderBlocks > 0u) {
    aEvent.SetInfo("legacy_empty_folder_block_count", aLegacyEmptyFolderBlocks);
  }
  aEvent.SetInfo("preview_manifest_block_count",
                 PackedUint48ToUInt64(pArchive.mHeader.mPreviewManifestBlockCount));
  aEvent.SetInfo("repair_block_count",
                 PackedUint48ToUInt64(pArchive.mHeader.mRepairSectorBlockCount));
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
  if (aManifest.mEmptyFolderBlocksProcessed <
      aBootstrap.mExpectedEmptyFolderBlockCount) {
    return static_cast<std::uint8_t>(SectionTypeV2::kEmptyFolderManifest);
  }
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
  const std::uint64_t aPreviousArchiveBlockCount =
      aPreviousArchive.mArchiveBlockCount != 0u ? aPreviousArchive.mArchiveBlockCount
                                                : aPreviousArchive.mReadableBlockCount;
  return aPreviousArchiveBlockCount != 0u &&
         (pPreviousBlockIndex + 1u) >= aPreviousArchiveBlockCount;
}

bool SkipRecordIsZero(const SkipRecordV2& pSkipRecord) {
  return pSkipRecord.mArchiveDistance == 0u &&
         pSkipRecord.mBlockDistance == 0u &&
         GetSkipRecordByteDistance(pSkipRecord) == 0u;
}

bool TryApplyRecoverSkipRecord(DecodeStageContextV2& pContext,
                               DecodeArchiveDecodeCursorV2& pCursor,
                               const SectionHeaderV2& pSectionHeader,
                               std::size_t pCurrentArchiveSlot,
                               std::uint64_t pCurrentBlockIndex,
                               std::size_t pCurrentPayloadStart,
                               std::size_t pCurrentPayloadEnd);

bool HandleDamagedBlock(DecodeStageContextV2& pContext,
                        const std::string& pReason) {
  if (!ShouldContinuePastDamagedBlock(pContext)) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionFromDecodeIntentV2(pContext.Request().mIntent),
                         ProgressStageV2::kArchiveDecode,
                         pReason));
    return false;
  }

  SwitchToPessimistic(pContext, pReason);
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

class DecodeArchiveDecodeCursorV2 {
 public:
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
        mBlockBytes(pContext.Layout().mArchiveBlockBytes) {
    mNextArchiveLog = kDecodeProgressArchiveLogIntervalV2;
    mNextFileLog = kDecodeProgressFileLogIntervalV2;
    mNextFolderLog = kDecodeProgressFolderLogIntervalV2;
    mNextByteLog = kDecodeProgressByteLogIntervalV2;
  }

  DecodeLogicalRecordDecoderV2 mFolderDecoder;
  DecodeLogicalRecordDecoderV2 mFileDecoder;
  DecodeLogicalRecordDecoderV2 mPreviewDecoder;
  FixedBlockBufferV2 mBlockBytes;
  std::unique_ptr<FileReadStreamV2> mRead;
  std::size_t mArchiveSlot = 0u;
  std::uint64_t mBlockIndex = 0u;
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
};

namespace {

bool TryApplyRecoverSkipRecord(DecodeStageContextV2& pContext,
                               DecodeArchiveDecodeCursorV2& pCursor,
                               const SectionHeaderV2& pSectionHeader,
                               std::size_t pCurrentArchiveSlot,
                               std::uint64_t pCurrentBlockIndex,
                               std::size_t pCurrentPayloadStart,
                               std::size_t pCurrentPayloadEnd) {
  if (!DecodeIntentAllowsSalvageV2(pContext.Request().mIntent) ||
      SkipRecordIsZero(pSectionHeader.mSkipRecord)) {
    return false;
  }

  const std::vector<DiscoveredArchiveFileV2>& aArchives =
      pContext.State().mDiscovery.mArchives;
  const std::size_t aTargetArchiveSlot =
      pCurrentArchiveSlot +
      static_cast<std::size_t>(pSectionHeader.mSkipRecord.mArchiveDistance);
  const std::uint64_t aTargetBlockIndex =
      static_cast<std::uint64_t>(pSectionHeader.mSkipRecord.mBlockDistance);
  const std::size_t aTargetPayloadOffset = static_cast<std::size_t>(
      GetSkipRecordByteDistance(pSectionHeader.mSkipRecord));
  const std::size_t aPayloadBytesPerBlock = pContext.Layout().SectionPayloadBytes();

  if (aTargetArchiveSlot >= aArchives.size()) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Ignoring skip record: target archive slot out of range.");
    return false;
  }
  if (aTargetPayloadOffset >= aPayloadBytesPerBlock) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Ignoring skip record: target payload offset out of range.");
    return false;
  }

  const DiscoveredArchiveFileV2& aTargetArchive = aArchives[aTargetArchiveSlot];
  if (aTargetBlockIndex >= aTargetArchive.mReadableBlockCount) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Ignoring skip record: target block index out of readable range.");
    return false;
  }

  const bool aForward =
      (aTargetArchiveSlot > pCurrentArchiveSlot) ||
      (aTargetArchiveSlot == pCurrentArchiveSlot &&
       (aTargetBlockIndex > pCurrentBlockIndex ||
        (aTargetBlockIndex == pCurrentBlockIndex &&
         aTargetPayloadOffset > pCurrentPayloadStart)));
  if (!aForward) {
    pContext.EmitLog(
        LogLevelV2::kWarning,
        DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
            " Ignoring skip record: target is not forward-progressing.");
    return false;
  }

  if (aTargetArchiveSlot == pCurrentArchiveSlot &&
      aTargetBlockIndex == pCurrentBlockIndex) {
    if (aTargetPayloadOffset >= pCurrentPayloadEnd) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
              " Ignoring skip record: intra-block target offset beyond payload end.");
      return false;
    }
    pCursor.mHasPausedBlockBoundary = true;
    pCursor.mPausedSectionHeader = pSectionHeader;
    pCursor.mPausedBlockPayloadOffset = aTargetPayloadOffset;
    pCursor.mPausedBlockPayloadEnd = pCurrentPayloadEnd;
    pCursor.mPausedBoundaryRecordReference.clear();
    pCursor.mHasForcedBlockPayloadStart = false;
    pCursor.mForcedBlockPayloadStart = 0u;
  } else {
    pCursor.mRead.reset();
    pCursor.mArchiveSlot = aTargetArchiveSlot;
    pCursor.mBlockIndex = aTargetBlockIndex;
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

  pContext.EmitLog(
      LogLevelV2::kInfo,
      DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
          " Applied skip record jump to archive slot " +
          std::to_string(aTargetArchiveSlot) + ", block " +
          std::to_string(aTargetBlockIndex) + ", payload offset " +
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
  pCursor.mArchiveAnnounced = false;
  pCursor.mHasPausedBlockBoundary = false;
  pCursor.mPausedSectionHeader = SectionHeaderV2{};
  pCursor.mPausedBlockPayloadOffset = 0u;
  pCursor.mPausedBlockPayloadEnd = 0u;
  pCursor.mPausedBoundaryRecordReference.clear();
}

bool CloseDecodeArchiveForCursor(DecodeStageContextV2& pContext,
                                 DecodeArchiveDecodeCursorV2& pCursor,
                                 const DiscoveredArchiveFileV2& pArchive) {
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
  return true;
}

bool ContinueAfterDamagedBlock(DecodeStageContextV2& pContext,
                               DecodeArchiveDecodeCursorV2& pCursor,
                               const DiscoveredArchiveFileV2& pArchive) {
  ++pCursor.mBlockIndex;
  if (pCursor.mBlockIndex >= pArchive.mReadableBlockCount) {
    (void)CloseDecodeArchiveForCursor(pContext, pCursor, pArchive);
  }
  if (pCursor.mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
    pContext.ContinuePhaseOnNextHeartbeat();
    return true;
  }
  return false;
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
    pCursorPtr.reset();
    return true;
  }

  std::string aFinalizeError;
  if (!aCursor.mPreviewDecoder.Finalize(aFinalizeError)) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: " + aFinalizeError);
    pCursorPtr.reset();
    return false;
  }
  if (!aCursor.mFolderDecoder.Finalize(aFinalizeError)) {
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: " + aFinalizeError);
    pCursorPtr.reset();
    return false;
  }
  if (!aCursor.mFileDecoder.Finalize(aFinalizeError)) {
    if (aCursor.mFileDecoder.IsInsideFile()) {
      (void)aCursor.mFileDecoder.AbortCurrentFile();
    }
    pContext.EmitLog(LogLevelV2::kError,
                     "Archive decode failed: " + aFinalizeError);
    pCursorPtr.reset();
    return false;
  }

  SnapshotDecodeOutput(pContext, aCursor);
  pContext.EmitLog(
      LogLevelV2::kInfo,
      DecodeStagePrefix(pContext.Request().mIntent, ProgressStageV2::kArchiveDecode) +
          " Wrote " +
          std::to_string(pContext.State().mOutput.mFilesWritten) + " files and " +
          std::to_string(pContext.State().mOutput.mFoldersCreated) + " folders.");
  pContext.EmitPhaseProgress(1.0, "Archive decode complete");
  pCursorPtr.reset();
  return true;
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
    if (aCursorPtr->mBlockBytes.Empty()) {
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
  }

  DecodeArchiveDecodeCursorV2& aCursor = *aCursorPtr;
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

      if (!aArchive.mIsPresent) {
        (void)CloseDecodeArchiveForCursor(pContext, aCursor, aArchive);
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
      aCursor.mRead = pContext.FileSystem().OpenReadStream(aArchive.mPath);
      if (aCursor.mRead == nullptr || !aCursor.mRead->IsReady()) {
        if (!HandleDamagedBlock(pContext, "a source archive could not be opened for read.")) {
          aCursorPtr.reset();
          return false;
        }
        (void)CloseDecodeArchiveForCursor(pContext, aCursor, aArchive);
        if (aCursor.mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
          pContext.ContinuePhaseOnNextHeartbeat();
          return true;
        }
        break;
      }
    }

    if (aCursor.mBlockIndex >= aArchive.mReadableBlockCount) {
      (void)CloseDecodeArchiveForCursor(pContext, aCursor, aArchive);
      if (aCursor.mArchiveSlot < pContext.State().mDiscovery.mArchives.size()) {
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }
      break;
    }

    SectionHeaderV2 aSectionHeader;
    std::size_t aBlockPayloadStart = 0u;
    std::size_t aBlockPayloadEnd = 0u;
    const bool aResumingPausedBlock = aCursor.mHasPausedBlockBoundary;
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
      if (!ReadBlock(
              *aCursor.mRead, aCursor.mBlockIndex, aArchiveBlockBytes, aCursor.mBlockBytes)) {
        if (aCursor.mFileDecoder.IsInsideFile()) {
          const std::string aPartialReference =
              aCursor.mFileDecoder.CurrentFileReference();
          const bool aPromotedPartial = aCursor.mFileDecoder.AbortCurrentFile();
          aCursor.mHasOpenFileContinuation = false;
          EmitDecodeErrorMarkerEvent(
              pContext,
              "file_data_error",
              "Decode file encountered data error due to missing block.",
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              SectionTypeLabel(SectionTypeV2::kArchiveData),
              aPartialReference);
          if (aPromotedPartial) {
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_closed_partial",
                "Decode closed file as partial after missing block.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialReference);
          }
        }
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
        if (ContinueAfterDamagedBlock(pContext, aCursor, aArchive)) {
          return true;
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
          pContext.State().mBootstrap.mFirstHeader.mIsEncrypted != 0u) {
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
          if (aCursor.mFileDecoder.IsInsideFile()) {
            const std::string aPartialReference =
                aCursor.mFileDecoder.CurrentFileReference();
            const bool aPromotedPartial = aCursor.mFileDecoder.AbortCurrentFile();
            aCursor.mHasOpenFileContinuation = false;
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_data_error",
                "Decode file encountered data error due to decrypt/checksum failure.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialReference);
            if (aPromotedPartial) {
              EmitDecodeErrorMarkerEvent(
                  pContext,
                  "file_closed_partial",
                  "Decode closed file as partial after decrypt/checksum failure.",
                  aArchive,
                  aCursor.mArchiveSlot,
                  aCursor.mBlockIndex,
                  SectionTypeLabel(SectionTypeV2::kArchiveData),
                  aPartialReference);
            }
          }
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
          aCursor.mPendingRecoverResync = true;
          if (ContinueAfterDamagedBlock(pContext, aCursor, aArchive)) {
            return true;
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
          if (aCursor.mFileDecoder.IsInsideFile()) {
            const std::string aPartialReference =
                aCursor.mFileDecoder.CurrentFileReference();
            const bool aPromotedPartial = aCursor.mFileDecoder.AbortCurrentFile();
            aCursor.mHasOpenFileContinuation = false;
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_data_error",
                "Decode file encountered data error due to section-header failure.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aPartialReference);
            if (aPromotedPartial) {
              EmitDecodeErrorMarkerEvent(
                  pContext,
                  "file_closed_partial",
                  "Decode closed file as partial after section-header failure.",
                  aArchive,
                  aCursor.mArchiveSlot,
                  aCursor.mBlockIndex,
                  SectionTypeLabel(SectionTypeV2::kArchiveData),
                  aPartialReference);
            }
          }
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
          aCursor.mPendingRecoverResync = true;
          if (ContinueAfterDamagedBlock(pContext, aCursor, aArchive)) {
            return true;
          }
          break;
        }
      }

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
          if (ContinueAfterDamagedBlock(pContext, aCursor, aArchive)) {
            return true;
          }
          break;
        }
        aBlockPayloadStart = aForcedOffset;
      }
    } else {
      aSectionHeader = aCursor.mPausedSectionHeader;
      aBlockPayloadStart = aCursor.mPausedBlockPayloadOffset;
      aBlockPayloadEnd = aCursor.mPausedBlockPayloadEnd;
      aCursor.mHasPausedBlockBoundary = false;
      aCursor.mPausedSectionHeader = SectionHeaderV2{};
      aCursor.mPausedBlockPayloadOffset = 0u;
      aCursor.mPausedBlockPayloadEnd = 0u;
      aCursor.mPausedBoundaryRecordReference.clear();
    }

    if (aCursor.mPendingRecoverResync &&
        static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
            SectionTypeV2::kArchiveData) {
      const bool aAppliedSkip = TryApplyRecoverSkipRecord(pContext,
                                                          aCursor,
                                                          aSectionHeader,
                                                          aCursor.mArchiveSlot,
                                                          aCursor.mBlockIndex,
                                                          aBlockPayloadStart,
                                                          aBlockPayloadEnd);
      aCursor.mPendingRecoverResync = false;
      if (aAppliedSkip) {
        SnapshotDecodeOutput(pContext, aCursor);
        pContext.ContinuePhaseOnNextHeartbeat();
        return true;
      }
    }

    const std::uint8_t aExpectedType = ExpectedSectionType(pContext);
    const bool aPhysicalRepairZone =
        aExpectedType == static_cast<std::uint8_t>(SectionTypeV2::kRepairData);

    if (pContext.State().mDiscovery.mMode == DecodeModeV2::kOptimistic) {
      if (!aPhysicalRepairZone && aSectionHeader.mSectionType != aExpectedType) {
        if (TryAcceptMissingPreviewManifest(pContext, aSectionHeader)) {
          // Expectations were updated to match the observed stream.
        } else if (!HandleDamagedBlock(
                       pContext,
                       "a section type disagreed with the optimistic family layout.")) {
          aCursorPtr.reset();
          return false;
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
                                      aCursor.mBlockIndex);
      if (!aExpectedContinuation) {
        if (aPhysicalRepairZone && !aCursor.mFileDecoder.IsInsideFile()) {
          aCursor.mHasOpenFileContinuation = false;
        } else {
        const std::string aContinuationReference =
            aCursor.mFileDecoder.CurrentFileReference();
        const bool aPromotedPartial = aCursor.mFileDecoder.AbortCurrentFile();
        aCursor.mHasOpenFileContinuation = false;
        EmitDecodeErrorMarkerEvent(
            pContext,
            "file_data_error",
            "Decode file continuation mismatch; file marked partial.",
            aArchive,
            aCursor.mArchiveSlot,
            aCursor.mBlockIndex,
            SectionTypeLabel(SectionTypeV2::kArchiveData),
            aContinuationReference);
        if (aPromotedPartial) {
          EmitDecodeErrorMarkerEvent(
              pContext,
              "file_closed_partial",
              "Decode closed file as partial after continuation mismatch.",
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              SectionTypeLabel(SectionTypeV2::kArchiveData),
              aContinuationReference);
        }
        if (!HandleDamagedBlock(
                pContext,
                "a continued file did not resume at the expected next block.")) {
          aCursorPtr.reset();
          return false;
        }
        if (ContinueAfterDamagedBlock(pContext, aCursor, aArchive)) {
          return true;
        }
        break;
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
      ++aCursor.mBlockIndex;
      if (aCursor.mBlockIndex >= aArchive.mReadableBlockCount) {
        (void)CloseDecodeArchiveForCursor(pContext, aCursor, aArchive);
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

    switch (static_cast<SectionTypeV2>(aSectionHeader.mSectionType)) {
      case SectionTypeV2::kEmptyFolderManifest:
        if (!aCursor.mFolderDecoder.Consume(aCursor.mBlockBytes.Data() + kSectionHeaderBytesV2,
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
              "empty_folder_name_error",
              "Decode empty-folder record encountered a name/path error.",
              aArchive,
              aCursor.mArchiveSlot,
              aCursor.mBlockIndex,
              SectionTypeLabel(SectionTypeV2::kEmptyFolderManifest),
              aCursor.mFolderDecoder.CurrentFileReference());
          if (!HandleDamagedBlock(
                  pContext,
                  "empty-folder logical record parse failed: " +
                      aParseErrorMessage)) {
            aCursorPtr.reset();
            return false;
          }
          aCursor.mFolderDecoder.ResetAfterParseError();
          if (ContinueAfterDamagedBlock(pContext, aCursor, aArchive)) {
            return true;
          }
          aParseError = false;
          aParseErrorMessage.clear();
          continue;
        }
        break;

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
          if (ContinueAfterDamagedBlock(pContext, aCursor, aArchive)) {
            return true;
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
          const bool aHadOpenFile = aCursor.mFileDecoder.IsInsideFile();
          const std::string aFailedReference =
              aCursor.mFileDecoder.CurrentFileReference();
          if (aHadOpenFile) {
            const bool aPromotedPartial = aCursor.mFileDecoder.AbortCurrentFile();
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_data_error",
                "Decode file encountered data error; file will be partial.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aFailedReference);
            if (aPromotedPartial) {
              EmitDecodeErrorMarkerEvent(
                  pContext,
                  "file_closed_partial",
                  "Decode closed file as partial after error boundary.",
                  aArchive,
                  aCursor.mArchiveSlot,
                  aCursor.mBlockIndex,
                  SectionTypeLabel(SectionTypeV2::kArchiveData),
                  aFailedReference);
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
                aFailedReference);
            EmitDecodeErrorMarkerEvent(
                pContext,
                "file_discarded_name_boundary",
                "Decode discarded file due to name-boundary error.",
                aArchive,
                aCursor.mArchiveSlot,
                aCursor.mBlockIndex,
                SectionTypeLabel(SectionTypeV2::kArchiveData),
                aFailedReference);
          }
          if (!HandleDamagedBlock(
                  pContext,
                  "archive-data logical record parse failed: " +
                      aParseErrorMessage)) {
            aCursorPtr.reset();
            return false;
          }
          aCursor.mFileDecoder.ResetAfterParseError();
          if (TryApplyRecoverSkipRecord(pContext,
                                        aCursor,
                                        aSectionHeader,
                                        aCursor.mArchiveSlot,
                                        aCursor.mBlockIndex,
                                        aBlockPayloadStart,
                                        aBlockPayloadEnd)) {
            aCursor.mPendingRecoverResync = false;
            SnapshotDecodeOutput(pContext, aCursor);
            pContext.ContinuePhaseOnNextHeartbeat();
            return true;
          }
          aCursor.mPendingRecoverResync = true;
          if (ContinueAfterDamagedBlock(pContext, aCursor, aArchive)) {
            return true;
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

    if (aParseError) {
      if (static_cast<SectionTypeV2>(aSectionHeader.mSectionType) ==
              SectionTypeV2::kArchiveData &&
          aCursor.mFileDecoder.IsInsideFile()) {
        (void)aCursor.mFileDecoder.AbortCurrentFile();
      }
      pContext.EmitLog(LogLevelV2::kError,
                       "Archive decode parse error: " + aParseErrorMessage);
      aCursorPtr.reset();
      return false;
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

    switch (static_cast<SectionTypeV2>(aSectionHeader.mSectionType)) {
      case SectionTypeV2::kEmptyFolderManifest:
        ++pContext.State().mManifest.mEmptyFolderBlocksProcessed;
        break;
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
      aCursor.mContinuationBlockIndex = aCursor.mBlockIndex;
    } else {
      aCursor.mHasOpenFileContinuation = false;
    }

    ++aCursor.mBlockIndex;
    if (aCursor.mBlockIndex >= aArchive.mReadableBlockCount) {
      (void)CloseDecodeArchiveForCursor(pContext, aCursor, aArchive);
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
