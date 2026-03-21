#pragma once

#include <cstdint>
#include <string>

#include "DecodeRequest.hpp"
#include "Logging.hpp"
#include "Progress.hpp"

namespace peanutbutter {

enum class LogActionV2 {
  kBundle = 0,
  kDecode = 1,
  kManifest = 2,
  kRepair = 3,
  kSanity = 4,
};

std::string LogActionLabelV2(LogActionV2 pAction);
std::string ProgressStageLabelV2(ProgressStageV2 pStage);
std::string FormatHumanBytesV2(std::uint64_t pBytes);
std::string FormatPathRelativeToRootV2(const std::string& pRootPath,
                                       const std::string& pPath);
std::string BuildStatSummaryV2(const LoggingStatV2& pStat);

std::string LogActionAcceptedV2(LogActionV2 pAction);
std::string LogActionCompletedV2(LogActionV2 pAction);
std::string LogActionFailedV2(LogActionV2 pAction);
std::string LogActionCanceledV2(LogActionV2 pAction);
std::string LogActionStartDetailV2(LogActionV2 pAction,
                                   const std::string& pSourcePath,
                                   const std::string& pDestinationPath);
std::string LogActionEndDetailV2(LogActionV2 pAction,
                                 const std::string& pOutcome,
                                 const std::string& pSourcePath,
                                 const std::string& pDestinationPath);
std::string LogPreparingActionV2(LogActionV2 pAction);
std::string LogUiLockedForActionV2(LogActionV2 pAction);
std::string LogUiUnlockedV2(void);
std::string LogCancelAcceptedV2(void);
std::string LogCancelRequestedV2(void);
std::string LogCancelRejectedNoActionV2(void);
std::string LogCancelRejectedAlreadyPendingV2(void);
std::string LogPrimaryRejectedWhileLockedV2(LogActionV2 pAction);
std::string LogPrimaryDirectorMissingV2(LogActionV2 pAction);

std::string LogPhaseStartedV2(LogActionV2 pAction, ProgressStageV2 pStage);
std::string LogPhaseCompletedV2(LogActionV2 pAction, ProgressStageV2 pStage);
std::string LogPhaseSkippedV2(LogActionV2 pAction,
                              ProgressStageV2 pStage,
                              const std::string& pReason);
std::string LogPhaseFailedV2(LogActionV2 pAction,
                             ProgressStageV2 pStage,
                             const std::string& pReason);
std::string LogSanityCompareStartV2(void);
std::string LogSanityCompareSliceV2(const LoggingStatV2& pStat);
std::string LogSanityCompareEndV2(const LoggingStatV2& pStat);
std::string LogSanityDiscoverySliceV2(const LoggingStatV2& pStat);
std::string LogSanitySummaryHealthyV2(const LoggingStatV2& pStat);
std::string LogSanitySummaryMismatchV2(const LoggingStatV2& pStat,
                                       std::uint64_t pMismatchCount);
std::string LogBundleDiscoverySummaryV2(std::uint64_t pFileCount,
                                        std::uint64_t pEmptyFolderCount);
std::string LogBundleMemoryPlanSummaryV2(std::uint64_t pArchiveCount,
                                         std::uint64_t pFamilyBlockCount);
std::string LogBundleArchiveManifestNoneV2(void);
std::string LogBundleArchiveManifestSummaryV2(std::uint64_t pByteCount,
                                              std::uint64_t pBlockCount);
std::string LogBundleFolderPackingSummaryV2(std::uint64_t pByteCount);
std::string LogBundleCipherDeriveDetailV2(void);
std::string LogBundleCipherAssembledV2(void);
std::string LogDecodeBootstrapSummaryV2(std::uint64_t pFamilyId,
                                        const std::string& pArchivePath);
std::string LogDecodeDiscoverySummaryV2(std::uint64_t pArchiveCount,
                                        std::uint64_t pReadableBlockCount);
std::string LogDecodeManifestSummaryV2(std::uint64_t pPreviewBlockCount,
                                       std::uint64_t pRepairBlockCount);
std::string LogDecodeFinalizeSummaryV2(std::uint64_t pBytesWritten);
std::string LogPessimisticSwitchV2(LogActionV2 pAction,
                                   const std::string& pReason);
LogActionV2 LogActionFromDecodeIntentV2(DecodeIntentV2 pIntent);

}  // namespace peanutbutter
