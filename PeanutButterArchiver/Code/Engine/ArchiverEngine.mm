#import "ArchiverEngine.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <thread>
#include <utility>

#include "../Common/LogCatalog.hpp"

namespace peanutbutter {

namespace {

enum class LaunchSignalV2 {
  kGreen = 0,
  kYellow = 1,
  kRed = 2,
};

struct LaunchDecisionV2 {
  LaunchSignalV2 mSignal = LaunchSignalV2::kGreen;
  std::string mTitle;
  std::string mMessage;
};

std::string CurrentResolutionRoot() {
  std::error_code aError;
  const std::filesystem::path aCurrentPath = std::filesystem::current_path(aError);
  if (aError) {
    return ".";
  }
  return aCurrentPath.lexically_normal().generic_string();
}

std::string TrimmedCopy(const std::string& pValue) {
  std::size_t aStart = 0u;
  while (aStart < pValue.size() &&
         std::isspace(static_cast<unsigned char>(pValue[aStart])) != 0) {
    ++aStart;
  }

  std::size_t aEnd = pValue.size();
  while (aEnd > aStart &&
         std::isspace(static_cast<unsigned char>(pValue[aEnd - 1u])) != 0) {
    --aEnd;
  }
  return pValue.substr(aStart, aEnd - aStart);
}

bool PathExists(const std::filesystem::path& pPath) {
  std::error_code aError;
  return std::filesystem::exists(pPath, aError) && !aError;
}

bool PathIsDirectory(const std::filesystem::path& pPath) {
  std::error_code aError;
  return std::filesystem::is_directory(pPath, aError) && !aError;
}

bool PathIsFile(const std::filesystem::path& pPath) {
  std::error_code aError;
  return std::filesystem::is_regular_file(pPath, aError) && !aError;
}

bool RelativePathHasHiddenSegment(const std::filesystem::path& pRelativePath) {
  for (const std::filesystem::path& aPart : pRelativePath) {
    const std::string aName = aPart.generic_string();
    if (aName.empty() || aName == "." || aName == "..") {
      continue;
    }
    if (aName[0] == '.') {
      return true;
    }
  }
  return false;
}

bool DirectoryHasVisibleEntries(const std::filesystem::path& pPath) {
  std::error_code aRootError;
  if (!std::filesystem::is_directory(pPath, aRootError) || aRootError) {
    return false;
  }

  std::error_code aIteratorError;
  std::filesystem::recursive_directory_iterator aIterator(
      pPath,
      std::filesystem::directory_options::skip_permission_denied,
      aIteratorError);
  std::filesystem::recursive_directory_iterator aEnd;
  while (!aIteratorError && aIterator != aEnd) {
    const std::filesystem::path aEntryPath = aIterator->path();
    std::error_code aRelativeError;
    const std::filesystem::path aRelative =
        std::filesystem::relative(aEntryPath, pPath, aRelativeError);
    if (!aRelativeError && !aRelative.empty() &&
        !RelativePathHasHiddenSegment(aRelative)) {
      return true;
    }
    aIterator.increment(aIteratorError);
  }
  return false;
}

std::string LastPathComponent(const std::string& pValue) {
  const std::filesystem::path aPath(pValue);
  const std::string aName = aPath.filename().generic_string();
  return aName.empty() ? pValue : aName;
}

bool LooksDirectoryLike(const std::string& pValue) {
  return std::filesystem::path(LastPathComponent(pValue)).extension().empty();
}

std::filesystem::path MakeCandidatePath(const std::string& pRawPath,
                                        const std::filesystem::path& pRootPath) {
  const std::filesystem::path aPath(pRawPath);
  if (aPath.is_absolute()) {
    return aPath.lexically_normal();
  }
  return (pRootPath / aPath).lexically_normal();
}

std::filesystem::path FindDescendantNamed(const std::filesystem::path& pRootPath,
                                          const std::string& pName,
                                          bool pDirectoriesOnly) {
  if (pName.empty() || !PathIsDirectory(pRootPath)) {
    return {};
  }

  std::error_code aIteratorError;
  std::filesystem::recursive_directory_iterator aIterator(
      pRootPath,
      std::filesystem::directory_options::skip_permission_denied,
      aIteratorError);
  std::filesystem::recursive_directory_iterator aEnd;
  while (!aIteratorError && aIterator != aEnd) {
    const std::filesystem::directory_entry aEntry = *aIterator;
    std::error_code aEntryError;
    const std::string aFileName = aEntry.path().filename().generic_string();
    if (aFileName == pName) {
      const bool aIsDirectory = aEntry.is_directory(aEntryError) && !aEntryError;
      if (!pDirectoriesOnly || aIsDirectory) {
        return aEntry.path().lexically_normal();
      }
    }
    aIterator.increment(aIteratorError);
  }

  return {};
}

std::string ResolveBundleSourcePath(const std::string& pRawPath) {
  const std::string aTrimmed = TrimmedCopy(pRawPath);
  if (aTrimmed.empty()) {
    return {};
  }

  const std::filesystem::path aRootPath(CurrentResolutionRoot());
  const std::filesystem::path aCandidatePath = MakeCandidatePath(aTrimmed, aRootPath);
  return aCandidatePath.generic_string();
}

std::string ResolveBundleDestinationPath(const std::string& pRawPath) {
  const std::string aTrimmed = TrimmedCopy(pRawPath);
  if (aTrimmed.empty()) {
    return {};
  }

  const std::filesystem::path aRootPath(CurrentResolutionRoot());
  const std::filesystem::path aCandidatePath = MakeCandidatePath(aTrimmed, aRootPath);
  const bool aLooksDirectory = LooksDirectoryLike(aTrimmed);
  if (PathExists(aCandidatePath)) {
    if (aLooksDirectory || PathIsDirectory(aCandidatePath)) {
      return aCandidatePath.generic_string();
    }
    return aCandidatePath.parent_path().lexically_normal().generic_string();
  }

  if (aLooksDirectory) {
    return aCandidatePath.generic_string();
  }
  return aCandidatePath.parent_path().lexically_normal().generic_string();
}

BundleRequestV2 ResolveBundleRequestPaths(const BundleRequestV2& pRequest) {
  BundleRequestV2 aResolved = pRequest;
  aResolved.mSourceDirectory = ResolveBundleSourcePath(pRequest.mSourceDirectory);
  aResolved.mDestinationDirectory =
      ResolveBundleDestinationPath(pRequest.mDestinationDirectory);
  return aResolved;
}

LaunchDecisionV2 CheckBundleLaunch(const BundleRequestV2& pRequest) {
  if (pRequest.mSourceDirectory.empty() || pRequest.mDestinationDirectory.empty()) {
    return {LaunchSignalV2::kRed, "Bundle blocked",
            "Bundle source and destination are required."};
  }

  const std::filesystem::path aSourcePath(pRequest.mSourceDirectory);
  if (!PathExists(aSourcePath) ||
      (!PathIsDirectory(aSourcePath) && !PathIsFile(aSourcePath))) {
    return {LaunchSignalV2::kRed, "Bundle blocked",
            "Bundle source must be an existing file or folder."};
  }

  const std::filesystem::path aDestinationPath(pRequest.mDestinationDirectory);
  if (PathExists(aDestinationPath) && !PathIsDirectory(aDestinationPath)) {
    return {LaunchSignalV2::kRed, "Bundle blocked",
            "Bundle destination must be a folder path."};
  }

  if (PathExists(aDestinationPath) && DirectoryHasVisibleEntries(aDestinationPath)) {
    return {LaunchSignalV2::kYellow, "Bundle Destination",
            "Choose how to use the destination folder:\n" +
                pRequest.mDestinationDirectory};
  }

  return {LaunchSignalV2::kGreen, "Bundle ready", "Bundle can proceed."};
}

}  // namespace

class ArchiverEngine::ActiveRuntimeV2 final : public BundleRuntimeV2,
                                              public DecodeRuntimeV2,
                                              public SanityRuntimeV2 {
 public:
  explicit ActiveRuntimeV2(ArchiverEngine* pOwner)
      : mOwner(pOwner) {}

  bool IsCancelRequested() const override {
    return mOwner != nullptr && mOwner->Snapshot().mIsCancelPending;
  }

  void EmitLog(LogLevelV2 pLevel, const std::string& pMessage) override {
    if (mOwner == nullptr) {
      return;
    }
    mOwner->EmitLogLocked(pLevel, pMessage);
  }

  void EmitProgress(ProgressStageV2 pStage,
                    double pLocalFraction,
                    double pOverallFraction,
                    const std::string& pLabel) override {
    if (mOwner == nullptr) {
      return;
    }
    ProgressSnapshotV2 aSnapshot;
    aSnapshot.mStage = pStage;
    aSnapshot.mLocalFraction = std::max(0.0, std::min(1.0, pLocalFraction));
    aSnapshot.mOverallFraction = std::max(0.0, std::min(1.0, pOverallFraction));
    aSnapshot.mLabel = pLabel;
    mOwner->EmitProgressLocked(aSnapshot);
  }

 private:
  ArchiverEngine* mOwner = nullptr;
};

ArchiverEngine::ArchiverEngine() {
  mWorkerThread = std::thread([this]() { WorkerLoop(); });
}

ArchiverEngine::~ArchiverEngine() {
  {
    std::lock_guard<std::recursive_mutex> aLock(mMutex);
    mShouldStopWorker = true;
  }
  mWorkerCv.notify_all();
  if (mWorkerThread.joinable()) {
    mWorkerThread.join();
  }
}

void ArchiverEngine::EnqueueBundleRequest(const BundleRequestV2& pRequest) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartBundle;
  aCommand.mBundleRequest = pRequest;
  mIncomingCommands.push(std::move(aCommand));
  mWorkerCv.notify_all();
}

void ArchiverEngine::EnqueueDecodeRequest(const DecodeRequestV2& pRequest) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartDecode;
  aCommand.mDecodeRequest = pRequest;
  mIncomingCommands.push(std::move(aCommand));
  mWorkerCv.notify_all();
}

void ArchiverEngine::EnqueueManifestRequest(const DecodeRequestV2& pRequest) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartManifest;
  aCommand.mManifestRequest = pRequest;
  mIncomingCommands.push(std::move(aCommand));
  mWorkerCv.notify_all();
}

void ArchiverEngine::EnqueueRepairRequest(const RepairRequestV2& pRequest) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartRepair;
  aCommand.mRepairRequest = pRequest;
  mIncomingCommands.push(std::move(aCommand));
  mWorkerCv.notify_all();
}

void ArchiverEngine::EnqueueSanityRequest(const SanityRequestV2& pRequest) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kStartSanity;
  aCommand.mSanityRequest = pRequest;
  mIncomingCommands.push(std::move(aCommand));
  mWorkerCv.notify_all();
}

void ArchiverEngine::EnqueuePromptResponse(const UiPromptResponseV2& pResponse) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kPromptResponse;
  aCommand.mPromptResponse = pResponse;
  mIncomingCommands.push(std::move(aCommand));
  mWorkerCv.notify_all();
}

void ArchiverEngine::EnqueueCancelRequest() {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineCommandV2 aCommand;
  aCommand.mType = EngineCommandTypeV2::kCancel;
  mIncomingCommands.push(std::move(aCommand));
  mWorkerCv.notify_all();
}

EngineEventListV2 ArchiverEngine::Poll() {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineEventListV2 aEvents = std::move(mPendingEvents);
  mPendingEvents.clear();
  return aEvents;
}

EngineSnapshotV2 ArchiverEngine::Snapshot() const {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  return BuildSnapshotLocked();
}

bool ArchiverEngine::HasPendingWorkLocked() const {
  return !mIncomingCommands.empty() ||
         (mCurrentPrimaryAction != EnginePrimaryActionV2::kNone &&
          ShouldStepCurrentActionLocked());
}

bool ArchiverEngine::ShouldStepCurrentActionLocked() const {
  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone) {
    return false;
  }
  return !(mCurrentPrimaryAction == EnginePrimaryActionV2::kBundle &&
           mPendingBundlePromptRequest.has_value() &&
           !mIsCancelPending);
}

void ArchiverEngine::WorkerLoop() {
  std::unique_lock<std::recursive_mutex> aLock(mMutex);
  while (!mShouldStopWorker) {
    mWorkerCv.wait(aLock, [this]() {
      return mShouldStopWorker || HasPendingWorkLocked();
    });
    if (mShouldStopWorker) {
      break;
    }

    ProcessIncomingCommandsLocked();
    if (mShouldStopWorker) {
      break;
    }

    if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone) {
      continue;
    }

    if (!ShouldStepCurrentActionLocked()) {
      ProcessCurrentActionLocked();
      continue;
    }

    const EnginePrimaryActionV2 aAction = mCurrentPrimaryAction;
    BundleDirector* const aBundleDirector = mBundleDirector.get();
    DecodeDirector* const aDecodeDirector = mDecodeDirector.get();
    RepairDirector* const aRepairDirector = mRepairDirector.get();
    ManifestDirector* const aManifestDirector = mManifestDirector.get();
    SanityDirector* const aSanityDirector = mSanityDirector.get();
    mSkipCurrentActionStep = true;

    aLock.unlock();
    switch (aAction) {
      case EnginePrimaryActionV2::kBundle:
        if (aBundleDirector != nullptr) {
          (void)aBundleDirector->Step();
        }
        break;
      case EnginePrimaryActionV2::kDecode:
        if (aDecodeDirector != nullptr) {
          (void)aDecodeDirector->Step();
        }
        break;
      case EnginePrimaryActionV2::kManifest:
        if (aManifestDirector != nullptr) {
          (void)aManifestDirector->Step();
        }
        break;
      case EnginePrimaryActionV2::kRepair:
        if (aRepairDirector != nullptr) {
          (void)aRepairDirector->Step();
        }
        break;
      case EnginePrimaryActionV2::kSanity:
        if (aSanityDirector != nullptr) {
          (void)aSanityDirector->Step();
        }
        break;
      case EnginePrimaryActionV2::kNone:
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    aLock.lock();

    ProcessIncomingCommandsLocked();
    ProcessCurrentActionLocked();
  }
}

void ArchiverEngine::ProcessIncomingCommandsLocked() {
  while (!mIncomingCommands.empty()) {
    const EngineCommandV2 aCommand = std::move(mIncomingCommands.front());
    mIncomingCommands.pop();

    switch (aCommand.mType) {
      case EngineCommandTypeV2::kStartBundle:
        if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone &&
            aCommand.mBundleRequest.has_value()) {
          AcceptBundleLocked(*aCommand.mBundleRequest);
        } else {
          RejectPrimaryActionLocked(LogPrimaryRejectedWhileLockedV2(LogActionV2::kBundle));
        }
        break;
      case EngineCommandTypeV2::kStartDecode:
        if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone &&
            aCommand.mDecodeRequest.has_value()) {
          AcceptDecodeLocked(*aCommand.mDecodeRequest);
        } else {
          RejectPrimaryActionLocked(LogPrimaryRejectedWhileLockedV2(LogActionV2::kDecode));
        }
        break;
      case EngineCommandTypeV2::kStartManifest:
        if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone &&
            aCommand.mManifestRequest.has_value()) {
          AcceptManifestLocked(*aCommand.mManifestRequest);
        } else {
          RejectPrimaryActionLocked(LogPrimaryRejectedWhileLockedV2(LogActionV2::kManifest));
        }
        break;
      case EngineCommandTypeV2::kStartRepair:
        if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone &&
            aCommand.mRepairRequest.has_value()) {
          AcceptRepairLocked(*aCommand.mRepairRequest);
        } else {
          RejectPrimaryActionLocked(LogPrimaryRejectedWhileLockedV2(LogActionV2::kRepair));
        }
        break;
      case EngineCommandTypeV2::kStartSanity:
        if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone &&
            aCommand.mSanityRequest.has_value()) {
          AcceptSanityLocked(*aCommand.mSanityRequest);
        } else {
          RejectPrimaryActionLocked(LogPrimaryRejectedWhileLockedV2(LogActionV2::kSanity));
        }
        break;
      case EngineCommandTypeV2::kCancel:
        if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone) {
          RejectCancelLocked(LogCancelRejectedNoActionV2());
        } else if (mIsCancelPending) {
          RejectCancelLocked(LogCancelRejectedAlreadyPendingV2());
        } else {
          AcceptCancelLocked();
        }
        break;
      case EngineCommandTypeV2::kPromptResponse:
        if (aCommand.mPromptResponse.has_value()) {
          HandlePromptResponseLocked(*aCommand.mPromptResponse);
        }
        break;
    }
  }
}

void ArchiverEngine::ProcessCurrentActionLocked() {
  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone) {
    mSkipCurrentActionStep = false;
    return;
  }

  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kDecode) {
    if (mDecodeDirector == nullptr) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogPrimaryDirectorMissingV2(LogActionV2::kDecode));
      return;
    }

    const bool aShouldStep = !mSkipCurrentActionStep;
    mSkipCurrentActionStep = false;
    if (aShouldStep) {
      (void)mDecodeDirector->Step();
    }

    if (mDecodeDirector->WasCanceled() || mIsCancelPending) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                LogActionCanceledV2(LogActionV2::kDecode));
      return;
    }

    if (mDecodeDirector->HasFailed()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogActionFailedV2(LogActionV2::kDecode));
      return;
    }

    if (mDecodeDirector->IsFinished()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCompleted,
                                LogActionCompletedV2(LogActionV2::kDecode));
    }
    return;
  }

  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kManifest) {
    if (mManifestDirector == nullptr) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogPrimaryDirectorMissingV2(LogActionV2::kManifest));
      return;
    }

    const bool aShouldStep = !mSkipCurrentActionStep;
    mSkipCurrentActionStep = false;
    if (aShouldStep) {
      (void)mManifestDirector->Step();
    }

    if (mManifestDirector->WasCanceled() || mIsCancelPending) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                LogActionCanceledV2(LogActionV2::kManifest));
      return;
    }

    if (mManifestDirector->HasFailed()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogActionFailedV2(LogActionV2::kManifest));
      return;
    }

    if (mManifestDirector->IsFinished()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCompleted,
                                LogActionCompletedV2(LogActionV2::kManifest));
    }
    return;
  }

  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kSanity) {
    if (mSanityDirector == nullptr) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogPrimaryDirectorMissingV2(LogActionV2::kSanity));
      return;
    }

    const bool aShouldStep = !mSkipCurrentActionStep;
    mSkipCurrentActionStep = false;
    if (aShouldStep) {
      (void)mSanityDirector->Step();
    }
    if (mSanityDirector->WasCanceled() || mIsCancelPending) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                LogActionCanceledV2(LogActionV2::kSanity));
      return;
    }
    if (mSanityDirector->HasFailed()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogActionFailedV2(LogActionV2::kSanity));
      return;
    }
    if (mSanityDirector->IsFinished()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCompleted,
                                LogActionCompletedV2(LogActionV2::kSanity));
    }
    return;
  }

  if (mIsCancelPending && mCurrentPrimaryAction != EnginePrimaryActionV2::kBundle &&
      mCurrentPrimaryAction != EnginePrimaryActionV2::kRepair) {
    FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                              LogActionCanceledV2(LogActionV2::kSanity));
    return;
  }

  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kBundle) {
    if (mPendingBundlePromptRequest.has_value()) {
      if (mIsCancelPending) {
        FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                  LogActionCanceledV2(LogActionV2::kBundle));
      }
      return;
    }

    if (mBundleDirector == nullptr) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogPrimaryDirectorMissingV2(LogActionV2::kBundle));
      return;
    }

    const bool aShouldStep = !mSkipCurrentActionStep;
    mSkipCurrentActionStep = false;
    if (aShouldStep) {
      (void)mBundleDirector->Step();
    }

    if (mBundleDirector->WasCanceled() || mIsCancelPending) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                LogActionCanceledV2(LogActionV2::kBundle));
      return;
    }

    if (mBundleDirector->HasFailed()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogActionFailedV2(LogActionV2::kBundle));
      return;
    }

    if (mBundleDirector->IsFinished()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCompleted,
                                LogActionCompletedV2(LogActionV2::kBundle));
    }
    return;
  }

  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kRepair) {
    if (mRepairDirector == nullptr) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogPrimaryDirectorMissingV2(LogActionV2::kRepair));
      return;
    }

    const bool aShouldStep = !mSkipCurrentActionStep;
    mSkipCurrentActionStep = false;
    if (aShouldStep) {
      (void)mRepairDirector->Step();
    }

    if (mRepairDirector->WasCanceled() || mIsCancelPending) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                LogActionCanceledV2(LogActionV2::kRepair));
      return;
    }

    if (mRepairDirector->HasFailed()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogActionFailedV2(LogActionV2::kRepair));
      return;
    }

    if (mRepairDirector->IsFinished()) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCompleted,
                                LogActionCompletedV2(LogActionV2::kRepair));
    }
    return;
  }

  if (mBundleDirector == nullptr) {
    FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                              LogActionFailedV2(LogActionV2::kBundle));
  }
}

void ArchiverEngine::AcceptBundleLocked(const BundleRequestV2& pRequest) {
  const BundleRequestV2 aResolvedRequest = ResolveBundleRequestPaths(pRequest);
  const LaunchDecisionV2 aDecision = CheckBundleLaunch(aResolvedRequest);
  if (aDecision.mSignal == LaunchSignalV2::kRed) {
    RejectPrimaryActionLocked(aDecision.mMessage);
    UiEffectV2 aShowDialog;
    aShowDialog.mType = UiEffectTypeV2::kShowDialog;
    aShowDialog.mDialog.mKind = UiDialogKindV2::kError;
    aShowDialog.mDialog.mTitle = aDecision.mTitle;
    aShowDialog.mDialog.mMessage = aDecision.mMessage;
    EmitUiEffectLocked(aShowDialog, EngineEventTypeV2::kUiStateChanged,
                       aDecision.mMessage);
    return;
  }

  if (aDecision.mSignal == LaunchSignalV2::kYellow) {
    mCurrentPrimaryAction = EnginePrimaryActionV2::kBundle;
    mIsUiLocked = true;
    mIsCancelPending = false;
    mBundleDirector.reset();
    mDecodeDirector.reset();
    mRepairDirector.reset();
    mManifestDirector.reset();
    mSanityDirector.reset();
    mActiveRuntime.reset();
    mPendingBundlePromptRequest = aResolvedRequest;
    mPendingPromptId = mNextPromptId++;

    EngineEventV2 aAccepted;
    aAccepted.mType = EngineEventTypeV2::kActionAccepted;
    aAccepted.mSnapshot = BuildSnapshotLocked();
    aAccepted.mMessage = LogActionAcceptedV2(LogActionV2::kBundle);
    PushEventLocked(std::move(aAccepted));

    UiEffectV2 aShowPrompt;
    aShowPrompt.mType = UiEffectTypeV2::kShowPrompt;
    aShowPrompt.mPrompt.mPromptId = *mPendingPromptId;
    aShowPrompt.mPrompt.mKind = UiPromptKindV2::kDestinationAction;
    aShowPrompt.mPrompt.mTitle = aDecision.mTitle;
    aShowPrompt.mPrompt.mMessage = aDecision.mMessage;
    aShowPrompt.mPrompt.mPrimaryLabel = "Clear";
    aShowPrompt.mPrompt.mSecondaryLabel = "Merge";
    aShowPrompt.mPrompt.mCancelLabel = "Cancel";
    EmitUiEffectLocked(aShowPrompt, EngineEventTypeV2::kUiStateChanged,
                       aDecision.mMessage);
    return;
  }

  StartBundleExecutionLocked(aResolvedRequest, true);
}

void ArchiverEngine::StartBundleExecutionLocked(const BundleRequestV2& pRequest,
                                                bool pEmitAccepted) {
  mCurrentPrimaryAction = EnginePrimaryActionV2::kBundle;
  mCurrentLogAction = LogActionV2::kBundle;
  mCurrentSourcePath = pRequest.mSourceDirectory;
  mCurrentDestinationPath = pRequest.mDestinationDirectory;
  mIsUiLocked = true;
  mIsCancelPending = false;
  mPendingBundlePromptRequest.reset();
  mPendingPromptId.reset();
  mDecodeDirector.reset();
  mRepairDirector.reset();
  mManifestDirector.reset();
  mSanityDirector.reset();
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mBundleDirector =
      std::make_unique<BundleDirector>(pRequest, mActiveRuntime.get());

  if (pEmitAccepted) {
    EngineEventV2 aAccepted;
    aAccepted.mType = EngineEventTypeV2::kActionAccepted;
    aAccepted.mSnapshot = BuildSnapshotLocked();
    aAccepted.mMessage = LogActionAcceptedV2(LogActionV2::kBundle);
    PushEventLocked(std::move(aAccepted));
  }

  EmitLogLocked(LogLevelV2::kInfo,
                LogActionStartDetailV2(LogActionV2::kBundle,
                                       pRequest.mSourceDirectory,
                                       pRequest.mDestinationDirectory));

  UiEffectV2 aShowLoading;
  aShowLoading.mType = UiEffectTypeV2::kShowLoading;
  aShowLoading.mLabel = LogPreparingActionV2(LogActionV2::kBundle);
  EmitUiEffectLocked(aShowLoading, EngineEventTypeV2::kUiStateChanged,
                     LogUiLockedForActionV2(LogActionV2::kBundle));
}

void ArchiverEngine::AcceptDecodeLocked(const DecodeRequestV2& pRequest) {
  DecodeRequestV2 aDecodeRequest = pRequest;
  mCurrentPrimaryAction = EnginePrimaryActionV2::kDecode;
  mCurrentLogAction = LogActionV2::kDecode;
  mCurrentSourcePath = aDecodeRequest.mSourcePath;
  mCurrentDestinationPath = aDecodeRequest.mDestinationDirectory;
  mIsUiLocked = true;
  mIsCancelPending = false;
  mBundleDirector.reset();
  mRepairDirector.reset();
  mManifestDirector.reset();
  mSanityDirector.reset();
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mDecodeDirector =
      std::make_unique<DecodeDirector>(aDecodeRequest, mActiveRuntime.get());

  EngineEventV2 aAccepted;
  aAccepted.mType = EngineEventTypeV2::kActionAccepted;
  aAccepted.mSnapshot = BuildSnapshotLocked();
  aAccepted.mMessage = LogActionAcceptedV2(LogActionV2::kDecode);
  PushEventLocked(std::move(aAccepted));

  EmitLogLocked(LogLevelV2::kInfo,
                LogActionStartDetailV2(LogActionV2::kDecode,
                                       aDecodeRequest.mSourcePath,
                                       aDecodeRequest.mDestinationDirectory));

  UiEffectV2 aShowLoading;
  aShowLoading.mType = UiEffectTypeV2::kShowLoading;
  aShowLoading.mLabel = LogPreparingActionV2(LogActionV2::kDecode);
  EmitUiEffectLocked(aShowLoading, EngineEventTypeV2::kUiStateChanged,
                     LogUiLockedForActionV2(LogActionV2::kDecode));
}

void ArchiverEngine::AcceptManifestLocked(const DecodeRequestV2& pRequest) {
  DecodeRequestV2 aManifestRequest = pRequest;
  aManifestRequest.mIntent = DecodeIntentV2::kManifest;
  mCurrentPrimaryAction = EnginePrimaryActionV2::kManifest;
  mCurrentLogAction = LogActionV2::kManifest;
  mCurrentSourcePath = aManifestRequest.mSourcePath;
  mCurrentDestinationPath = aManifestRequest.mDestinationDirectory;
  mIsUiLocked = true;
  mIsCancelPending = false;
  mBundleDirector.reset();
  mDecodeDirector.reset();
  mRepairDirector.reset();
  mManifestDirector.reset();
  mSanityDirector.reset();
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mManifestDirector =
      std::make_unique<ManifestDirector>(aManifestRequest, mActiveRuntime.get());

  EngineEventV2 aAccepted;
  aAccepted.mType = EngineEventTypeV2::kActionAccepted;
  aAccepted.mSnapshot = BuildSnapshotLocked();
  aAccepted.mMessage = LogActionAcceptedV2(LogActionV2::kManifest);
  PushEventLocked(std::move(aAccepted));

  EmitLogLocked(LogLevelV2::kInfo,
                LogActionStartDetailV2(LogActionV2::kManifest,
                                       aManifestRequest.mSourcePath,
                                       aManifestRequest.mDestinationDirectory));

  UiEffectV2 aShowLoading;
  aShowLoading.mType = UiEffectTypeV2::kShowLoading;
  aShowLoading.mLabel = LogPreparingActionV2(LogActionV2::kManifest);
  EmitUiEffectLocked(aShowLoading, EngineEventTypeV2::kUiStateChanged,
                     LogUiLockedForActionV2(LogActionV2::kManifest));
}

void ArchiverEngine::AcceptRepairLocked(const RepairRequestV2& pRequest) {
  mCurrentPrimaryAction = EnginePrimaryActionV2::kRepair;
  mCurrentLogAction = LogActionV2::kRepair;
  mCurrentSourcePath = pRequest.mSourcePath;
  mCurrentDestinationPath = pRequest.mDestinationDirectory;
  mIsUiLocked = true;
  mIsCancelPending = false;
  mBundleDirector.reset();
  mDecodeDirector.reset();
  mManifestDirector.reset();
  mSanityDirector.reset();
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mRepairDirector =
      std::make_unique<RepairDirector>(pRequest, mActiveRuntime.get());

  EngineEventV2 aAccepted;
  aAccepted.mType = EngineEventTypeV2::kActionAccepted;
  aAccepted.mSnapshot = BuildSnapshotLocked();
  aAccepted.mMessage = LogActionAcceptedV2(LogActionV2::kRepair);
  PushEventLocked(std::move(aAccepted));

  EmitLogLocked(LogLevelV2::kInfo,
                LogActionStartDetailV2(LogActionV2::kRepair,
                                       pRequest.mSourcePath,
                                       pRequest.mDestinationDirectory));

  UiEffectV2 aShowLoading;
  aShowLoading.mType = UiEffectTypeV2::kShowLoading;
  aShowLoading.mLabel = LogPreparingActionV2(LogActionV2::kRepair);
  EmitUiEffectLocked(aShowLoading, EngineEventTypeV2::kUiStateChanged,
                     LogUiLockedForActionV2(LogActionV2::kRepair));
}

void ArchiverEngine::AcceptSanityLocked(const SanityRequestV2& pRequest) {
  mCurrentPrimaryAction = EnginePrimaryActionV2::kSanity;
  mCurrentLogAction = LogActionV2::kSanity;
  mCurrentSourcePath = pRequest.mLeftDirectory;
  mCurrentDestinationPath = pRequest.mRightDirectory;
  mIsUiLocked = true;
  mIsCancelPending = false;
  mBundleDirector.reset();
  mDecodeDirector.reset();
  mRepairDirector.reset();
  mManifestDirector.reset();
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mSanityDirector =
      std::make_unique<SanityDirector>(pRequest, mActiveRuntime.get());

  EngineEventV2 aAccepted;
  aAccepted.mType = EngineEventTypeV2::kActionAccepted;
  aAccepted.mSnapshot = BuildSnapshotLocked();
  aAccepted.mMessage = LogActionAcceptedV2(LogActionV2::kSanity);
  PushEventLocked(std::move(aAccepted));

  EmitLogLocked(LogLevelV2::kInfo,
                LogActionStartDetailV2(LogActionV2::kSanity,
                                       pRequest.mLeftDirectory,
                                       pRequest.mRightDirectory));

  UiEffectV2 aShowLoading;
  aShowLoading.mType = UiEffectTypeV2::kShowLoading;
  aShowLoading.mLabel = LogPreparingActionV2(LogActionV2::kSanity);
  EmitUiEffectLocked(aShowLoading, EngineEventTypeV2::kUiStateChanged,
                     LogUiLockedForActionV2(LogActionV2::kSanity));
}

void ArchiverEngine::HandlePromptResponseLocked(const UiPromptResponseV2& pResponse) {
  if (!mPendingPromptId.has_value() || !mPendingBundlePromptRequest.has_value()) {
    return;
  }
  if (pResponse.mPromptId != *mPendingPromptId ||
      pResponse.mKind != UiPromptKindV2::kDestinationAction) {
    return;
  }

  if (pResponse.mChoice == UiPromptChoiceV2::kCancel) {
    FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                              LogActionCanceledV2(LogActionV2::kBundle));
    return;
  }

  BundleRequestV2 aRequest = *mPendingBundlePromptRequest;
  aRequest.mClearDestinationBeforeWrite =
      (pResponse.mChoice == UiPromptChoiceV2::kClear);
  StartBundleExecutionLocked(aRequest, false);
}

void ArchiverEngine::RejectPrimaryActionLocked(const std::string& pReason) {
  EngineEventV2 aRejected;
  aRejected.mType = EngineEventTypeV2::kActionRejected;
  aRejected.mSnapshot = BuildSnapshotLocked();
  aRejected.mMessage = pReason;
  PushEventLocked(std::move(aRejected));
}

void ArchiverEngine::AcceptCancelLocked() {
  mIsCancelPending = true;

  EngineEventV2 aAccepted;
  aAccepted.mType = EngineEventTypeV2::kCancelAccepted;
  aAccepted.mSnapshot = BuildSnapshotLocked();
  aAccepted.mMessage = LogCancelAcceptedV2();
  PushEventLocked(std::move(aAccepted));

  EmitLogLocked(LogLevelV2::kWarning, LogCancelRequestedV2());
}

void ArchiverEngine::RejectCancelLocked(const std::string& pReason) {
  EngineEventV2 aRejected;
  aRejected.mType = EngineEventTypeV2::kCancelRejected;
  aRejected.mSnapshot = BuildSnapshotLocked();
  aRejected.mMessage = pReason;
  PushEventLocked(std::move(aRejected));
}

void ArchiverEngine::FinishCurrentActionLocked(EngineEventTypeV2 pType,
                                               const std::string& pMessage) {
  std::string aOutcome = "finished";
  switch (pType) {
    case EngineEventTypeV2::kActionCompleted:
      aOutcome = "completed";
      break;
    case EngineEventTypeV2::kActionFailed:
      aOutcome = "failed";
      break;
    case EngineEventTypeV2::kActionCanceled:
      aOutcome = "canceled";
      break;
    default:
      break;
  }
  if (!mCurrentSourcePath.empty() || !mCurrentDestinationPath.empty()) {
    EmitLogLocked(LogLevelV2::kInfo,
                  LogActionEndDetailV2(mCurrentLogAction, aOutcome,
                                       mCurrentSourcePath,
                                       mCurrentDestinationPath));
  }

  EngineEventV2 aFinished;
  aFinished.mType = pType;
  aFinished.mMessage = pMessage;

  mCurrentPrimaryAction = EnginePrimaryActionV2::kNone;
  mIsUiLocked = false;
  mIsCancelPending = false;
  mBundleDirector.reset();
  mDecodeDirector.reset();
  mRepairDirector.reset();
  mManifestDirector.reset();
  mSanityDirector.reset();
  mActiveRuntime.reset();
  mPendingPromptId.reset();
  mPendingBundlePromptRequest.reset();
  mCurrentSourcePath.clear();
  mCurrentDestinationPath.clear();

  aFinished.mSnapshot = BuildSnapshotLocked();
  PushEventLocked(std::move(aFinished));

  UiEffectV2 aHideLoading;
  aHideLoading.mType = UiEffectTypeV2::kHideLoading;
  EmitUiEffectLocked(aHideLoading, EngineEventTypeV2::kUiStateChanged,
                     LogUiUnlockedV2());
}

void ArchiverEngine::EmitUiEffectLocked(const UiEffectV2& pEffect,
                                        EngineEventTypeV2 pType,
                                        const std::string& pMessage) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineEventV2 aEvent;
  aEvent.mType = pType;
  aEvent.mSnapshot = BuildSnapshotLocked();
  aEvent.mMessage = pMessage;
  aEvent.mUiEffect = pEffect;
  PushEventLocked(std::move(aEvent));
}

void ArchiverEngine::EmitLogLocked(LogLevelV2 pLevel,
                                   const std::string& pMessage) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineEventV2 aEvent;
  aEvent.mType = EngineEventTypeV2::kLog;
  aEvent.mSnapshot = BuildSnapshotLocked();
  aEvent.mMessage = pMessage;
  aEvent.mLogEntry.mLevel = pLevel;
  aEvent.mLogEntry.mMessage = pMessage;
  PushEventLocked(std::move(aEvent));
}

void ArchiverEngine::EmitProgressLocked(const ProgressSnapshotV2& pSnapshot) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EngineEventV2 aEvent;
  aEvent.mType = EngineEventTypeV2::kProgress;
  aEvent.mSnapshot = BuildSnapshotLocked();
  aEvent.mMessage = pSnapshot.mLabel;
  aEvent.mProgress = pSnapshot;
  PushEventLocked(std::move(aEvent));
}

void ArchiverEngine::PushEventLocked(EngineEventV2 pEvent) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  pEvent.mSnapshot = BuildSnapshotLocked();
  mPendingEvents.push_back(std::move(pEvent));
}

EngineSnapshotV2 ArchiverEngine::BuildSnapshotLocked() const {
  EngineSnapshotV2 aSnapshot;
  aSnapshot.mIsBusy = mCurrentPrimaryAction != EnginePrimaryActionV2::kNone;
  aSnapshot.mIsUiLocked = mIsUiLocked;
  aSnapshot.mIsCancelPending = mIsCancelPending;
  aSnapshot.mCurrentPrimaryAction = mCurrentPrimaryAction;
  return aSnapshot;
}

}  // namespace peanutbutter
