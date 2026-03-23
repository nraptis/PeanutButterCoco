#include "Archive/ArchiveFinalize.hpp"

#include <algorithm>
#include <string>

#include "AppShell_ArchiveFormat.hpp"

namespace peanutbutter {
namespace {

OperationResult MakeSuccess() {
  OperationResult aResult;
  aResult.mSucceeded = true;
  aResult.mErrorCode = ErrorCode::kNone;
  return aResult;
}

OperationResult MakeFailure(ErrorCode pCode,
                            const std::string& pMessage,
                            Logger* pLogger) {
  OperationResult aResult;
  aResult.mSucceeded = false;
  aResult.mErrorCode = pCode;
  aResult.mFailureMessage = pMessage;
  if (pLogger != nullptr) {
    pLogger->LogError(std::string(ErrorCodeToString(pCode)) + ": " + pMessage);
  }
  return aResult;
}

std::string FinalizingDoneMessage(DirtyType pDirtyType) {
  switch (pDirtyType) {
    case DirtyType::kFinished:
      return "[Finalizing] Done! Your archive is good to go!";
    case DirtyType::kFinishedWithError:
      return "[Finalizing] Done! Your archive is tagged as erroneous.";
    case DirtyType::kFinishedWithCancel:
      return "[Finalizing] Done! Your archive is tagged as cancelled.";
    case DirtyType::kFinishedWithCancelAndError:
      return "[Finalizing] Done! Your archive is tagged as erroneous and cancelled.";
    case DirtyType::kInvalid:
      break;
  }
  return "[Finalizing] Done! Your archive remains invalid.";
}

}  // namespace

ArchiveHeader BuildArchiveHeaderForPlan(const BundleRequest& pRequest,
                                        const BundleDiscovery& pDiscovery,
                                        const BundleArchivePlan& pPlan,
                                        std::uint32_t pArchiveCount,
                                        std::uint32_t pPayloadLength,
                                        DirtyType pDirtyType) {
  ArchiveHeader aHeader{};
  aHeader.mMagic = kMagicHeaderBytes;
  aHeader.mVersionMajor = static_cast<std::uint16_t>(kMajorVersion & 0xFFFFu);
  aHeader.mVersionMinor = static_cast<std::uint16_t>(kMinorVersion & 0xFFFFu);
  aHeader.mArchiverVersion = static_cast<std::uint8_t>(kArchiverVersion & 0xFFu);
  aHeader.mPasswordExpanderVersion =
      static_cast<std::uint8_t>(kPasswordExpanderVersion & 0xFFu);
  aHeader.mCipherStackVersion =
      static_cast<std::uint8_t>(kCipherStackVersion & 0xFFu);
  aHeader.mEncryptionStrength = pRequest.mEncryptionStrength;
  aHeader.mExpansionStrength = pRequest.mExpansionStrength;
  aHeader.mRecordCountMod256 = pPlan.mRecordCountMod256;
  aHeader.mFolderCountMod256 = pPlan.mFolderCountMod256;
  aHeader.mDirtyType = pDirtyType;
  aHeader.mArchiveIndex = pPlan.mArchiveIndex;
  aHeader.mArchiveCount = pArchiveCount;
  aHeader.mPayloadLength = pPayloadLength;
  aHeader.mReserved32 = 0u;
  aHeader.mReservedB = 0u;
  aHeader.mArchiveFamilyId = pDiscovery.mArchiveFamilyId;
  return aHeader;
}

OperationResult FinalizeArchiveHeaders(const BundleDiscovery& pDiscovery,
                                       std::size_t pArchiveCountToFinalize,
                                       DirtyType pDirtyType,
                                       FileSystem& pFileSystem,
                                       Logger* pLogger,
                                       CancelCoordinator* pCancelCoordinator) {
  const std::size_t aHeaderCount =
      std::min<std::size_t>(pArchiveCountToFinalize, pDiscovery.mArchives.size());
  const unsigned char aDirtyByte = static_cast<unsigned char>(pDirtyType);
  bool aLoggedFinalizingCancel = false;

  if (pLogger != nullptr) {
    pLogger->LogStatus("[Finalizing] Updating all archive headers.");
    pLogger->LogStatus("[Finalizing] Updated 0 of " +
                       std::to_string(aHeaderCount) + " headers.");
    ReportProgress(*pLogger,
                   "Bundle",
                   ProgressProfileKind::kBundle,
                   ProgressPhase::kFinalizing,
                   aHeaderCount == 0u ? 1.0 : 0.0,
                   "Updating archive headers.");
  }

  for (std::size_t aIndex = 0u; aIndex < aHeaderCount; ++aIndex) {
    if (pCancelCoordinator != nullptr && pCancelCoordinator->IsCancelRequested() &&
        pLogger != nullptr && !aLoggedFinalizingCancel) {
      pLogger->LogStatus("[Cancel] Bundle job is finalizing, cannot cancel.");
      aLoggedFinalizingCancel = true;
    }

    const BundleArchivePlan& aPlan = pDiscovery.mArchives[aIndex];
    if (!pFileSystem.OverwriteFileRegion(
            aPlan.mArchivePath, 15u, &aDirtyByte, 1u)) {
      return MakeFailure(ErrorCode::kFileSystem,
                         "failed overwriting archive dirty flag during finalization: " +
                             aPlan.mArchivePath,
                         pLogger);
    }

    const std::size_t aCompleted = aIndex + 1u;
    if (pLogger != nullptr && (((aCompleted % 10000u) == 0u) ||
                               aCompleted == aHeaderCount)) {
      pLogger->LogStatus("[Finalizing] Updated " +
                         std::to_string(aCompleted) + " of " +
                         std::to_string(aHeaderCount) + " headers.");
    }
    if (pLogger != nullptr) {
      ReportProgress(*pLogger,
                     "Bundle",
                     ProgressProfileKind::kBundle,
                     ProgressPhase::kFinalizing,
                     aHeaderCount == 0u
                         ? 1.0
                         : (static_cast<double>(aCompleted) /
                            static_cast<double>(aHeaderCount)),
                     "Updating archive headers.");
    }
  }

  if (pLogger != nullptr) {
    pLogger->LogStatus(FinalizingDoneMessage(pDirtyType));
    ReportProgress(*pLogger,
                   "Bundle",
                   ProgressProfileKind::kBundle,
                   ProgressPhase::kFinalizing,
                   1.0,
                   "Archive header finalization complete.");
  }
  return MakeSuccess();
}

}  // namespace peanutbutter
