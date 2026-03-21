#include "LogCatalog.hpp"

#include <filesystem>
#include <sstream>

namespace peanutbutter {
namespace {

std::string FormatCount(std::uint64_t pCompleted,
                        std::uint64_t pTotal,
                        const char* pLabel) {
  if (pTotal == 0u) {
    return {};
  }
  return std::to_string(pCompleted) + " of " + std::to_string(pTotal) + " " +
         pLabel;
}

}  // namespace

std::string LogActionLabelV2(LogActionV2 pAction) {
  switch (pAction) {
    case LogActionV2::kBundle:
      return "Bundle";
    case LogActionV2::kDecode:
      return "Unbundle";
    case LogActionV2::kManifest:
      return "Read Manifest";
    case LogActionV2::kRepair:
      return "Repair";
    case LogActionV2::kSanity:
      return "Folder Compare";
  }
  return "Action";
}

std::string ProgressStageLabelV2(ProgressStageV2 pStage) {
  switch (pStage) {
    case ProgressStageV2::kPreflight: return "Preflight";
    case ProgressStageV2::kHeaderBootstrap: return "Header Bootstrap";
    case ProgressStageV2::kDiscovery: return "Discovery";
    case ProgressStageV2::kMemoryPlanning: return "Memory Planning";
    case ProgressStageV2::kDeriveCipherMaterial: return "Derive Cipher Material";
    case ProgressStageV2::kAssembleCipherStack: return "Assemble Cipher Stack";
    case ProgressStageV2::kArchiveManifest: return "Archive Manifest";
    case ProgressStageV2::kFolderPacking: return "Folder Packing";
    case ProgressStageV2::kManifestDiscovery: return "Manifest Discovery";
    case ProgressStageV2::kArchivePacking: return "Archive Packing";
    case ProgressStageV2::kArchiveDecode: return "Archive Decode";
    case ProgressStageV2::kRepairPacking: return "Repair Packing";
    case ProgressStageV2::kFinalizingHeaders: return "Finalizing Headers";
    case ProgressStageV2::kFinalize: return "Finalize";
    case ProgressStageV2::kCompare: return "Compare";
    default: return "Idle";
  }
}

std::string FormatHumanBytesV2(std::uint64_t pBytes) {
  static const char* kUnits[] = {"B", "KB", "MB", "GB", "TB", "PB"};
  double aValue = static_cast<double>(pBytes);
  std::size_t aUnitIndex = 0u;
  while (aValue >= 1024.0 && aUnitIndex + 1u < (sizeof(kUnits) / sizeof(kUnits[0]))) {
    aValue /= 1024.0;
    ++aUnitIndex;
  }
  std::ostringstream aOut;
  if (aUnitIndex == 0u) {
    aOut << static_cast<std::uint64_t>(aValue) << kUnits[aUnitIndex];
  } else {
    aOut.setf(std::ios::fixed);
    aOut.precision(3);
    aOut << aValue << " " << kUnits[aUnitIndex];
  }
  return aOut.str();
}

std::string FormatPathRelativeToRootV2(const std::string& pRootPath,
                                       const std::string& pPath) {
  if (pPath.empty()) {
    return {};
  }

  if (pRootPath.empty()) {
    return pPath;
  }

  const std::filesystem::path aRoot =
      std::filesystem::path(pRootPath).lexically_normal();
  const std::filesystem::path aPath =
      std::filesystem::path(pPath).lexically_normal();
  std::error_code aError;
  std::filesystem::path aRelative = std::filesystem::relative(aPath, aRoot, aError);
  if (aError) {
    aRelative = aPath.lexically_relative(aRoot);
  }

  const std::string aRelativeString = aRelative.generic_string();
  if (!aRelativeString.empty() && aRelativeString != ".") {
    if (aRelativeString == ".." || aRelativeString.rfind("../", 0u) == 0u) {
      return aRelativeString;
    }
    return "../" + aRelativeString;
  }

  return aPath.generic_string();
}

std::string BuildStatSummaryV2(const LoggingStatV2& pStat) {
  std::string aArchives = FormatCount(pStat.mArchivesCompleted, pStat.mArchivesTotal, "archives");
  std::string aFiles = FormatCount(pStat.mFilesCompleted, pStat.mFilesTotal, "files");
  std::string aFolders = FormatCount(pStat.mFoldersCompleted, pStat.mFoldersTotal, "folders");
  std::string aBytes;
  if (pStat.mBytesTotal > 0u) {
    aBytes = FormatHumanBytesV2(pStat.mBytesCompleted) + " of " +
             FormatHumanBytesV2(pStat.mBytesTotal);
  } else if (pStat.mBytesCompleted > 0u) {
    aBytes = FormatHumanBytesV2(pStat.mBytesCompleted);
  }

  std::ostringstream aOut;
  bool aFirst = true;
  for (const std::string& aPart : {aArchives, aFiles, aFolders, aBytes}) {
    if (aPart.empty()) {
      continue;
    }
    if (!aFirst) {
      aOut << ", ";
    }
    aOut << aPart;
    aFirst = false;
  }
  return aOut.str();
}

std::string LogActionAcceptedV2(LogActionV2 pAction) {
  return LogActionLabelV2(pAction) + " action accepted.";
}

std::string LogActionCompletedV2(LogActionV2 pAction) {
  return LogActionLabelV2(pAction) + " action completed.";
}

std::string LogActionFailedV2(LogActionV2 pAction) {
  return LogActionLabelV2(pAction) + " action failed.";
}

std::string LogActionCanceledV2(LogActionV2 pAction) {
  return LogActionLabelV2(pAction) + " action canceled.";
}

std::string LogActionStartDetailV2(LogActionV2 pAction,
                                   const std::string& pSourcePath,
                                   const std::string& pDestinationPath) {
  std::ostringstream aOut;
  aOut << "[" << LogActionLabelV2(pAction) << "] START: source='"
       << pSourcePath << "', destination='" << pDestinationPath << "'.";
  return aOut.str();
}

std::string LogActionEndDetailV2(LogActionV2 pAction,
                                 const std::string& pOutcome,
                                 const std::string& pSourcePath,
                                 const std::string& pDestinationPath) {
  std::ostringstream aOut;
  aOut << "[" << LogActionLabelV2(pAction) << "] END (" << pOutcome
       << "): source='" << pSourcePath << "', destination='"
       << pDestinationPath << "'.";
  return aOut.str();
}

std::string LogPreparingActionV2(LogActionV2 pAction) {
  return "Preparing " + LogActionLabelV2(pAction) + "...";
}

std::string LogUiLockedForActionV2(LogActionV2 pAction) {
  return "UI locked for " + LogActionLabelV2(pAction) + " action.";
}

std::string LogUiUnlockedV2(void) {
  return "UI unlocked.";
}

std::string LogCancelAcceptedV2(void) {
  return "Cancel accepted.";
}

std::string LogCancelRequestedV2(void) {
  return "Cancel requested.";
}

std::string LogCancelRejectedNoActionV2(void) {
  return "Cancel request rejected because no primary action is active.";
}

std::string LogCancelRejectedAlreadyPendingV2(void) {
  return "Cancel request rejected because cancel is already pending.";
}

std::string LogPrimaryRejectedWhileLockedV2(LogActionV2 pAction) {
  return LogActionLabelV2(pAction) + " request rejected while engine is locked.";
}

std::string LogPrimaryDirectorMissingV2(LogActionV2 pAction) {
  return LogActionLabelV2(pAction) + " director was not initialized.";
}

std::string LogPhaseStartedV2(LogActionV2 pAction, ProgressStageV2 pStage) {
  return "[" + LogActionLabelV2(pAction) + "][" + ProgressStageLabelV2(pStage) + "] START.";
}

std::string LogPhaseCompletedV2(LogActionV2 pAction, ProgressStageV2 pStage) {
  return "[" + LogActionLabelV2(pAction) + "][" + ProgressStageLabelV2(pStage) + "] DONE.";
}

std::string LogPhaseSkippedV2(LogActionV2 pAction,
                              ProgressStageV2 pStage,
                              const std::string& pReason) {
  return "[" + LogActionLabelV2(pAction) + "][" + ProgressStageLabelV2(pStage) +
         "] SKIP: " + pReason;
}

std::string LogPhaseFailedV2(LogActionV2 pAction,
                             ProgressStageV2 pStage,
                             const std::string& pReason) {
  return "[" + LogActionLabelV2(pAction) + "][" + ProgressStageLabelV2(pStage) +
         "] FAIL: " + pReason;
}

std::string LogSanityCompareStartV2(void) {
  return "[Folder Compare][Compare] 0 of 0 files, 0B START.";
}

std::string LogSanityCompareSliceV2(const LoggingStatV2& pStat) {
  return "[Folder Compare][Compare] " + BuildStatSummaryV2(pStat) + ".";
}

std::string LogSanityCompareEndV2(const LoggingStatV2& pStat) {
  return "[Folder Compare][Compare] " + BuildStatSummaryV2(pStat) + " DONE.";
}

std::string LogSanityDiscoverySliceV2(const LoggingStatV2& pStat) {
  return "[Folder Compare][Discovery] " + BuildStatSummaryV2(pStat) + ".";
}

std::string LogSanitySummaryHealthyV2(const LoggingStatV2& pStat) {
  return "[Folder Compare][Summary] Good: " + BuildStatSummaryV2(pStat) + ".";
}

std::string LogSanitySummaryMismatchV2(const LoggingStatV2& pStat,
                                       std::uint64_t pMismatchCount) {
  return "[Folder Compare][Summary] Fail: " + std::to_string(pMismatchCount) +
         " mismatches found. " + BuildStatSummaryV2(pStat) + ".";
}

std::string LogBundleDiscoverySummaryV2(std::uint64_t pFileCount,
                                        std::uint64_t pEmptyFolderCount) {
  return "[Bundle][Discovery] Found " + std::to_string(pFileCount) + " files and " +
         std::to_string(pEmptyFolderCount) + " empty folders.";
}

std::string LogBundleMemoryPlanSummaryV2(std::uint64_t pArchiveCount,
                                         std::uint64_t pFamilyBlockCount) {
  return "[Bundle][Memory Planning] Computed " + std::to_string(pArchiveCount) +
         " archives and " + std::to_string(pFamilyBlockCount) + " family blocks.";
}

std::string LogBundleArchiveManifestNoneV2(void) {
  return "[Bundle][Archive Manifest] No preview manifest blocks are planned.";
}

std::string LogBundleArchiveManifestSummaryV2(std::uint64_t pByteCount,
                                              std::uint64_t pBlockCount) {
  return "[Bundle][Archive Manifest] Planned " + std::to_string(pByteCount) +
         " plaintext preview bytes across " + std::to_string(pBlockCount) +
         " preview blocks.";
}

std::string LogBundleFolderPackingSummaryV2(std::uint64_t pByteCount) {
  return "[Bundle][Folder Packing] Planned " + std::to_string(pByteCount) +
         " bytes using tightly packed sentinel folder records.";
}

std::string LogBundleCipherDeriveDetailV2(void) {
  return "[Bundle][Derive Cipher Material] Deriving from password, encryption strength, and table strength.";
}

std::string LogBundleCipherAssembledV2(void) {
  return "[Bundle][Assemble Cipher Stack] Cipher stack assembled.";
}

std::string LogDecodeBootstrapSummaryV2(std::uint64_t pFamilyId,
                                        const std::string& pArchivePath) {
  return "[Decode][Header Bootstrap] Read family " + std::to_string(pFamilyId) +
         " from " + pArchivePath + ".";
}

std::string LogDecodeDiscoverySummaryV2(std::uint64_t pArchiveCount,
                                        std::uint64_t pReadableBlockCount) {
  return "[Decode][Discovery] Found " + std::to_string(pArchiveCount) +
         " archives and " + std::to_string(pReadableBlockCount) +
         " readable blocks.";
}

std::string LogDecodeManifestSummaryV2(std::uint64_t pPreviewBlockCount,
                                       std::uint64_t pRepairBlockCount) {
  return "[Decode][Manifest Discovery] Header plan expects " +
         std::to_string(pPreviewBlockCount) + " preview blocks and " +
         std::to_string(pRepairBlockCount) + " repair blocks.";
}

std::string LogDecodeFinalizeSummaryV2(std::uint64_t pBytesWritten) {
  return "[Decode][Finalize] Wrote " + std::to_string(pBytesWritten) + " bytes.";
}

std::string LogPessimisticSwitchV2(LogActionV2 pAction,
                                   const std::string& pReason) {
  return "[" + LogActionLabelV2(pAction) + "][Discovery] Switched to pessimistic mode: " +
         pReason;
}

LogActionV2 LogActionFromDecodeIntentV2(DecodeIntentV2 pIntent) {
  switch (pIntent) {
    case DecodeIntentV2::kRecover:
      return LogActionV2::kRepair;
    case DecodeIntentV2::kManifest:
      return LogActionV2::kManifest;
    case DecodeIntentV2::kUnbundle:
    default:
      return LogActionV2::kDecode;
  }
}

}  // namespace peanutbutter
