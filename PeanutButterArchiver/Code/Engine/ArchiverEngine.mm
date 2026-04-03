#import "ArchiverEngine.hpp"

#include <algorithm>
#include <array>
#include <ctime>
#include <filesystem>
#include <system_error>
#include <utility>

#include "../Common/LogCatalog.hpp"
#include "../Knobs.hpp"

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

using StartCommandHasRequestFnV2 =
    bool (ArchiverEngineBase::*)(const EngineCommandV2&) const;
using StartCommandAcceptFnV2 =
    void (ArchiverEngineBase::*)(const EngineCommandV2&);

struct StartCommandDescriptorV2 {
  EngineCommandTypeV2 mType = EngineCommandTypeV2::kCancel;
  EnginePrimaryActionV2 mPrimaryAction = EnginePrimaryActionV2::kNone;
  LogActionV2 mLogAction = LogActionV2::kBundle;
  StartCommandHasRequestFnV2 mHasRequest = nullptr;
  StartCommandAcceptFnV2 mAccept = nullptr;
};

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

bool PathExists(const FileSystemV2& pFileSystem, const std::string& pPath) {
  return pFileSystem.Exists(pPath);
}

bool PathIsDirectory(const FileSystemV2& pFileSystem,
                     const std::string& pPath) {
  return pFileSystem.IsDirectory(pPath);
}

bool PathIsFile(const FileSystemV2& pFileSystem, const std::string& pPath) {
  return pFileSystem.IsFile(pPath);
}

bool PathComponentIsHidden(const std::string& pName) {
  return !pName.empty() && pName[0] == '.';
}

bool DirectoryHasVisibleEntries(const FileSystemV2& pFileSystem,
                                const std::string& pPath) {
  if (!pFileSystem.IsDirectory(pPath)) {
    return false;
  }

  for (const DirectoryEntryV2& aEntry : pFileSystem.ListDirectoryEntries(pPath)) {
    if (!PathComponentIsHidden(aEntry.mRelativePath)) {
      return true;
    }
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

std::string MakeCandidatePath(const FileSystemV2& pFileSystem,
                              const std::string& pRawPath,
                              const std::string& pRootPath) {
  const std::filesystem::path aPath(pRawPath);
  if (aPath.is_absolute()) {
    return aPath.lexically_normal().generic_string();
  }
  return pFileSystem.JoinPath(pRootPath, pRawPath);
}

std::string ResolveBundleSourcePath(const std::string& pRawPath,
                                    const FileSystemV2& pFileSystem) {
  const std::string aTrimmed = TrimmedCopy(pRawPath);
  if (aTrimmed.empty()) {
    return {};
  }

  const std::string aRootPath = pFileSystem.CurrentWorkingDirectory();
  return MakeCandidatePath(pFileSystem, aTrimmed, aRootPath);
}

std::string ResolveBundleDestinationPath(const std::string& pRawPath,
                                         const FileSystemV2& pFileSystem) {
  const std::string aTrimmed = TrimmedCopy(pRawPath);
  if (aTrimmed.empty()) {
    return {};
  }

  const std::string aRootPath = pFileSystem.CurrentWorkingDirectory();
  const std::string aCandidatePath =
      MakeCandidatePath(pFileSystem, aTrimmed, aRootPath);
  const bool aLooksDirectory = LooksDirectoryLike(aTrimmed);
  if (PathExists(pFileSystem, aCandidatePath)) {
    if (aLooksDirectory || PathIsDirectory(pFileSystem, aCandidatePath)) {
      return aCandidatePath;
    }
    return pFileSystem.ParentPath(aCandidatePath);
  }

  if (aLooksDirectory) {
    return aCandidatePath;
  }
  return pFileSystem.ParentPath(aCandidatePath);
}

BundleRequestV2 ResolveBundleRequestPaths(const BundleRequestV2& pRequest,
                                          const FileSystemV2& pFileSystem) {
  BundleRequestV2 aResolved = pRequest;
  aResolved.mSourceDirectory =
      ResolveBundleSourcePath(pRequest.mSourceDirectory, pFileSystem);
  aResolved.mDestinationDirectory =
      ResolveBundleDestinationPath(pRequest.mDestinationDirectory, pFileSystem);
  return aResolved;
}

LaunchDecisionV2 CheckBundleLaunch(const BundleRequestV2& pRequest,
                                   const FileSystemV2& pFileSystem) {
  if (pRequest.mSourceDirectory.empty() || pRequest.mDestinationDirectory.empty()) {
    return {LaunchSignalV2::kRed, "Bundle blocked",
            "Bundle source and destination are required."};
  }

  if (!PathExists(pFileSystem, pRequest.mSourceDirectory) ||
      (!PathIsDirectory(pFileSystem, pRequest.mSourceDirectory) &&
       !PathIsFile(pFileSystem, pRequest.mSourceDirectory))) {
    return {LaunchSignalV2::kRed, "Bundle blocked",
            "Bundle source must be an existing file or folder."};
  }

  if (PathExists(pFileSystem, pRequest.mDestinationDirectory) &&
      !PathIsDirectory(pFileSystem, pRequest.mDestinationDirectory)) {
    return {LaunchSignalV2::kRed, "Bundle blocked",
            "Bundle destination must be a folder path."};
  }

  if (PathExists(pFileSystem, pRequest.mDestinationDirectory) &&
      DirectoryHasVisibleEntries(pFileSystem, pRequest.mDestinationDirectory)) {
    return {LaunchSignalV2::kYellow, "Bundle Destination",
            "Choose how to use the destination folder:\n" +
                pRequest.mDestinationDirectory};
  }

  return {LaunchSignalV2::kGreen, "Bundle ready", "Bundle can proceed."};
}

LaunchDecisionV2 CheckDecodeLaunch(const DecodeRequestV2& pRequest,
                                   const FileSystemV2& pFileSystem) {
  const LogActionV2 aAction = LogActionFromDecodeIntentV2(pRequest.mIntent);
  const std::string aActionLabel = LogActionLabelV2(aAction);
  if (pRequest.mSourcePath.empty() || pRequest.mDestinationDirectory.empty()) {
    return {LaunchSignalV2::kRed,
            aActionLabel + " blocked",
            aActionLabel + " source and destination are required."};
  }

  if (!PathExists(pFileSystem, pRequest.mSourcePath) ||
      (!PathIsDirectory(pFileSystem, pRequest.mSourcePath) &&
       !PathIsFile(pFileSystem, pRequest.mSourcePath))) {
    return {LaunchSignalV2::kRed,
            aActionLabel + " blocked",
            aActionLabel + " source must be an existing file or folder."};
  }

  if (PathExists(pFileSystem, pRequest.mDestinationDirectory) &&
      !PathIsDirectory(pFileSystem, pRequest.mDestinationDirectory)) {
    return {LaunchSignalV2::kRed,
            aActionLabel + " blocked",
            aActionLabel + " destination must be a folder path."};
  }

  if (PathExists(pFileSystem, pRequest.mDestinationDirectory) &&
      DirectoryHasVisibleEntries(pFileSystem, pRequest.mDestinationDirectory)) {
    return {LaunchSignalV2::kYellow,
            aActionLabel + " Destination",
            "Choose how to use the destination folder:\n" +
                pRequest.mDestinationDirectory};
  }

  return {LaunchSignalV2::kGreen,
          aActionLabel + " ready",
          aActionLabel + " can proceed."};
}

const char* EngineTypeLabel(ArchiverEngineTypeV2 pEngineType) {
  switch (pEngineType) {
    case ArchiverEngineTypeV2::kBase:
      return "base";
    case ArchiverEngineTypeV2::kBundle:
      return "bundle";
    case ArchiverEngineTypeV2::kDecode:
      return "decode";
    case ArchiverEngineTypeV2::kRepair:
      return "repair";
    case ArchiverEngineTypeV2::kSanity:
      return "sanity";
  }
  return "unknown";
}

std::string CurrentClockPrefixV2() {
  if (!knobs::kLogShowTimestampsV2) {
    return {};
  }

  const std::time_t aNow = std::time(nullptr);
  std::tm aLocalTime{};
#if defined(_WIN32)
  if (localtime_s(&aLocalTime, &aNow) != 0) {
    return {};
  }
#else
  if (localtime_r(&aNow, &aLocalTime) == nullptr) {
    return {};
  }
#endif

  std::array<char, 16u> aTimeBuffer{};
  if (std::strftime(aTimeBuffer.data(),
                    aTimeBuffer.size(),
                    "%H:%M:%S",
                    &aLocalTime) == 0u) {
    return {};
  }

  return "[" + std::string(aTimeBuffer.data()) + "]";
}

void PrefixEventMessageWithClockIfEnabled(EngineEventV2& pEvent) {
  if (!knobs::kLogShowTimestampsV2 || pEvent.mMessage.empty()) {
    return;
  }

  if (pEvent.mType == EngineEventTypeV2::kProgress ||
      pEvent.mType == EngineEventTypeV2::kRuntimeEvent ||
      pEvent.mType == EngineEventTypeV2::kUiStateChanged) {
    return;
  }

  const std::string aPrefix = CurrentClockPrefixV2();
  if (aPrefix.empty()) {
    return;
  }

  pEvent.mMessage = aPrefix + pEvent.mMessage;
  if (pEvent.mType == EngineEventTypeV2::kLog) {
    pEvent.mLogEntry.mMessage = pEvent.mMessage;
  }
}

}  // namespace

class ArchiverEngineBase::ActiveRuntimeV2 final : public BundleRuntimeV2,
                                              public DecodeRuntimeV2,
                                              public SanityRuntimeV2 {
 public:
  explicit ActiveRuntimeV2(ArchiverEngineBase* pOwner)
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

  bool WantsRuntimeEvent(RuntimeEventKindV2 pKind) const override {
    return mOwner != nullptr && mOwner->WantsRuntimeEventKind(pKind);
  }

  bool EmitRuntimeEvent(const RuntimeEventV2& pEvent) override {
    if (mOwner == nullptr) {
      return true;
    }
    return mOwner->EmitRuntimeEventLocked(pEvent);
  }

 private:
  ArchiverEngineBase* mOwner = nullptr;
};

ArchiverEngineBase::ArchiverEngineBase(
    ArchiverEngineTypeV2 pEngineType,
    FileSystemV2* pFileSystem,
    const memory_layout::ArchiveLayoutConfigV2* pLayout,
    CommandBusV2* pCommandBus)
    : mEngineType(pEngineType),
      mFileSystem(pFileSystem != nullptr ? pFileSystem : &mLocalFileSystem),
      mCommandBus(pCommandBus != nullptr ? pCommandBus : &mOwnedCommandBus),
      mLayout(pLayout != nullptr ? pLayout
                                 : &memory_layout::DefaultArchiveLayoutConfigV2()) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  EmitLogLocked(LogLevelV2::kInfo,
                "[App] Working directory: " +
                    FormatPathForLogV2(mFileSystem->CurrentWorkingDirectory()));
  EmitLogLocked(LogLevelV2::kInfo,
                std::string("[App] Engine type: ") + EngineTypeLabel(mEngineType));
}

ArchiverEngineBase::~ArchiverEngineBase() = default;

void ArchiverEngineBase::EnqueueBundleRequest(const BundleRequestV2& pRequest) {
  mCommandBus->EnqueueCommand(MakeBundleCommandV2(pRequest));
}

void ArchiverEngineBase::EnqueueDecodeRequest(const DecodeRequestV2& pRequest) {
  mCommandBus->EnqueueCommand(MakeDecodeCommandV2(pRequest));
}

void ArchiverEngineBase::EnqueueManifestRequest(const DecodeRequestV2& pRequest) {
  mCommandBus->EnqueueCommand(MakeManifestCommandV2(pRequest));
}

void ArchiverEngineBase::EnqueueRepairRequest(const RepairRequestV2& pRequest) {
  mCommandBus->EnqueueCommand(MakeRepairCommandV2(pRequest));
}

void ArchiverEngineBase::EnqueueSanityRequest(const SanityRequestV2& pRequest) {
  mCommandBus->EnqueueCommand(MakeSanityCommandV2(pRequest));
}

void ArchiverEngineBase::EnqueuePromptResponse(const UiPromptResponseV2& pResponse) {
  mCommandBus->EnqueueCommand(MakePromptResponseCommandV2(pResponse));
}

void ArchiverEngineBase::EnqueueCancelRequest() {
  mCommandBus->Cancel();
}

void ArchiverEngineBase::EnqueueCheckpointDecision(
    const EngineCheckpointDecisionV2& pDecision) {
  mCommandBus->EnqueueCommand(MakeCheckpointDecisionCommandV2(pDecision));
}

void ArchiverEngineBase::ContinueCheckpoint(std::uint64_t pCheckpointId) {
  mCommandBus->ContinueCheckpoint(pCheckpointId);
}

void ArchiverEngineBase::CancelCheckpoint(std::uint64_t pCheckpointId) {
  mCommandBus->CancelCheckpoint(pCheckpointId);
}

void ArchiverEngineBase::Dispose() {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  if (mIsDisposed) {
    return;
  }

  mIsDisposed = true;
  mPendingCheckpointRequest.reset();
  if (mCurrentPrimaryAction != EnginePrimaryActionV2::kNone) {
    FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled, "Engine disposed.");
  } else {
    mIsUiLocked = false;
    mIsCancelPending = false;
    mActiveTask.reset();
    mActiveRuntime.reset();
    mPendingPromptId.reset();
    mPendingBundlePromptRequest.reset();
    mPendingDecodePromptRequest.reset();
  }

  EmitLogLocked(LogLevelV2::kInfo, "[App] Engine disposed.");
}

void ArchiverEngineBase::Heartbeat() {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  ProcessIncomingCommandsLocked();
  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone) {
    return;
  }

  if (!ShouldHeartbeatCurrentActionLocked()) {
    EvaluateCurrentActionLocked();
    return;
  }

  if (mActiveTask != nullptr) {
    (void)mActiveTask->Heartbeat();
  }

  ProcessIncomingCommandsLocked();
  EvaluateCurrentActionLocked();
}

void ArchiverEngineBase::SetCaptureVerboseRuntimeEvents(bool pEnabled) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  mCaptureVerboseRuntimeEvents = pEnabled;
}

void ArchiverEngineBase::SetBlockingCheckpointKinds(
    const std::vector<RuntimeEventKindV2>& pKinds) {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  mBlockingCheckpointKinds = pKinds;
  std::sort(mBlockingCheckpointKinds.begin(), mBlockingCheckpointKinds.end());
  mBlockingCheckpointKinds.erase(
      std::unique(mBlockingCheckpointKinds.begin(),
                  mBlockingCheckpointKinds.end()),
      mBlockingCheckpointKinds.end());
}

EngineEventListV2 ArchiverEngineBase::Poll() {
  const CommandBusItemListV2 aItems = mCommandBus->TakeItems();
  EngineEventListV2 aEvents;
  aEvents.reserve(aItems.size());
  for (const CommandBusItemV2& aItem : aItems) {
    if (aItem.mType == CommandBusItemTypeV2::kEvent) {
      aEvents.push_back(aItem.mEvent);
      continue;
    }

    EngineEventV2 aEvent;
    aEvent.mType = EngineEventTypeV2::kLog;
    aEvent.mSnapshot = aItem.mSnapshot;
    aEvent.mMessage = aItem.mLog.mMessage;
    aEvent.mLogEntry = aItem.mLog;
    aEvents.push_back(std::move(aEvent));
  }
  return aEvents;
}

bool ArchiverEngineBase::CapturesVerboseRuntimeEvents() const {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  return mCaptureVerboseRuntimeEvents;
}

EngineSnapshotV2 ArchiverEngineBase::Snapshot() const {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  return BuildSnapshotLocked();
}

std::optional<EngineTerminalStateV2> ArchiverEngineBase::TakeTerminalState() {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  std::optional<EngineTerminalStateV2> aTerminal = std::move(mLastTerminalState);
  mLastTerminalState.reset();
  return aTerminal;
}

CommandBusV2& ArchiverEngineBase::CommandBus() {
  return *mCommandBus;
}

const CommandBusV2& ArchiverEngineBase::CommandBus() const {
  return *mCommandBus;
}

bool ArchiverEngineBase::WantsRuntimeEventKind(RuntimeEventKindV2 pKind) const {
  std::lock_guard<std::recursive_mutex> aLock(mMutex);
  return WantsRuntimeEventKindLocked(pKind);
}

bool ArchiverEngineBase::SupportsPrimaryActionLocked(
    EnginePrimaryActionV2 pAction) const {
  if (mIsDisposed) {
    return false;
  }
  switch (mEngineType) {
    case ArchiverEngineTypeV2::kBase:
      return true;
    case ArchiverEngineTypeV2::kBundle:
      return pAction == EnginePrimaryActionV2::kBundle;
    case ArchiverEngineTypeV2::kDecode:
      return pAction == EnginePrimaryActionV2::kDecode ||
             pAction == EnginePrimaryActionV2::kManifest;
    case ArchiverEngineTypeV2::kRepair:
      return pAction == EnginePrimaryActionV2::kRepair;
    case ArchiverEngineTypeV2::kSanity:
      return pAction == EnginePrimaryActionV2::kSanity;
  }
  return false;
}

std::string ArchiverEngineBase::UnsupportedActionMessageLocked(
    EnginePrimaryActionV2 pAction) const {
  if (mIsDisposed) {
    return "Engine is disposed.";
  }

  const char* aActionLabel = "action";
  switch (pAction) {
    case EnginePrimaryActionV2::kBundle:
      aActionLabel = "bundle";
      break;
    case EnginePrimaryActionV2::kDecode:
      aActionLabel = "decode";
      break;
    case EnginePrimaryActionV2::kManifest:
      aActionLabel = "manifest";
      break;
    case EnginePrimaryActionV2::kRepair:
      aActionLabel = "repair";
      break;
    case EnginePrimaryActionV2::kSanity:
      aActionLabel = "sanity";
      break;
    case EnginePrimaryActionV2::kNone:
      aActionLabel = "none";
      break;
  }

  return "This " + std::string(EngineTypeLabel(mEngineType)) +
         " engine does not accept " + aActionLabel + " actions.";
}

bool ArchiverEngineBase::ShouldHeartbeatCurrentActionLocked() const {
  if (mIsDisposed) {
    return false;
  }
  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone) {
    return false;
  }
  if (mPendingCheckpointRequest.has_value()) {
    return false;
  }
  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kBundle &&
      mPendingBundlePromptRequest.has_value() &&
      !mIsCancelPending) {
    return false;
  }
  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kDecode &&
      mPendingDecodePromptRequest.has_value() &&
      !mIsCancelPending) {
    return false;
  }
  return mActiveTask != nullptr;
}

void ArchiverEngineBase::ProcessIncomingCommandsLocked() {
  EngineCommandV2 aCommand;
  while (mCommandBus->TryDequeueCommand(&aCommand)) {
    if (TryHandleStartCommandLocked(aCommand)) {
      continue;
    }

    switch (aCommand.mType) {
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
      case EngineCommandTypeV2::kCheckpointDecision:
        if (aCommand.mCheckpointDecision.has_value()) {
          HandleCheckpointDecisionLocked(*aCommand.mCheckpointDecision);
        }
        break;
      case EngineCommandTypeV2::kStartBundle:
      case EngineCommandTypeV2::kStartDecode:
      case EngineCommandTypeV2::kStartManifest:
      case EngineCommandTypeV2::kStartRepair:
      case EngineCommandTypeV2::kStartSanity:
        break;
    }
  }
}

bool ArchiverEngineBase::TryHandleStartCommandLocked(const EngineCommandV2& pCommand) {
  static const std::array<StartCommandDescriptorV2, 5u> kDescriptors = {{
      {EngineCommandTypeV2::kStartBundle,
       EnginePrimaryActionV2::kBundle,
       LogActionV2::kBundle,
       &ArchiverEngineBase::HasBundleStartRequestLocked,
       &ArchiverEngineBase::AcceptBundleStartCommandLocked},
      {EngineCommandTypeV2::kStartDecode,
       EnginePrimaryActionV2::kDecode,
       LogActionV2::kDecode,
       &ArchiverEngineBase::HasDecodeStartRequestLocked,
       &ArchiverEngineBase::AcceptDecodeStartCommandLocked},
      {EngineCommandTypeV2::kStartManifest,
       EnginePrimaryActionV2::kManifest,
       LogActionV2::kManifest,
       &ArchiverEngineBase::HasManifestStartRequestLocked,
       &ArchiverEngineBase::AcceptManifestStartCommandLocked},
      {EngineCommandTypeV2::kStartRepair,
       EnginePrimaryActionV2::kRepair,
       LogActionV2::kRepair,
       &ArchiverEngineBase::HasRepairStartRequestLocked,
       &ArchiverEngineBase::AcceptRepairStartCommandLocked},
      {EngineCommandTypeV2::kStartSanity,
       EnginePrimaryActionV2::kSanity,
       LogActionV2::kSanity,
       &ArchiverEngineBase::HasSanityStartRequestLocked,
       &ArchiverEngineBase::AcceptSanityStartCommandLocked},
  }};

  for (const StartCommandDescriptorV2& aDescriptor : kDescriptors) {
    if (aDescriptor.mType != pCommand.mType) {
      continue;
    }

    const bool aHasRequest =
        aDescriptor.mHasRequest != nullptr &&
        (this->*aDescriptor.mHasRequest)(pCommand);
    if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone && aHasRequest) {
      if (SupportsPrimaryActionLocked(aDescriptor.mPrimaryAction)) {
        (this->*aDescriptor.mAccept)(pCommand);
      } else {
        RejectPrimaryActionLocked(
            UnsupportedActionMessageLocked(aDescriptor.mPrimaryAction));
      }
    } else {
      RejectPrimaryActionLocked(
          LogPrimaryRejectedWhileLockedV2(aDescriptor.mLogAction));
    }
    return true;
  }

  return false;
}

bool ArchiverEngineBase::HasBundleStartRequestLocked(
    const EngineCommandV2& pCommand) const {
  return pCommand.mBundleRequest.has_value();
}

bool ArchiverEngineBase::HasDecodeStartRequestLocked(
    const EngineCommandV2& pCommand) const {
  return pCommand.mDecodeRequest.has_value();
}

bool ArchiverEngineBase::HasManifestStartRequestLocked(
    const EngineCommandV2& pCommand) const {
  return pCommand.mManifestRequest.has_value();
}

bool ArchiverEngineBase::HasRepairStartRequestLocked(
    const EngineCommandV2& pCommand) const {
  return pCommand.mRepairRequest.has_value();
}

bool ArchiverEngineBase::HasSanityStartRequestLocked(
    const EngineCommandV2& pCommand) const {
  return pCommand.mSanityRequest.has_value();
}

void ArchiverEngineBase::AcceptBundleStartCommandLocked(
    const EngineCommandV2& pCommand) {
  if (!pCommand.mBundleRequest.has_value()) {
    return;
  }
  AcceptBundleLocked(*pCommand.mBundleRequest);
}

void ArchiverEngineBase::AcceptDecodeStartCommandLocked(
    const EngineCommandV2& pCommand) {
  if (!pCommand.mDecodeRequest.has_value()) {
    return;
  }
  AcceptDecodeLocked(*pCommand.mDecodeRequest);
}

void ArchiverEngineBase::AcceptManifestStartCommandLocked(
    const EngineCommandV2& pCommand) {
  if (!pCommand.mManifestRequest.has_value()) {
    return;
  }
  AcceptManifestLocked(*pCommand.mManifestRequest);
}

void ArchiverEngineBase::AcceptRepairStartCommandLocked(
    const EngineCommandV2& pCommand) {
  if (!pCommand.mRepairRequest.has_value()) {
    return;
  }
  AcceptRepairLocked(*pCommand.mRepairRequest);
}

void ArchiverEngineBase::AcceptSanityStartCommandLocked(
    const EngineCommandV2& pCommand) {
  if (!pCommand.mSanityRequest.has_value()) {
    return;
  }
  AcceptSanityLocked(*pCommand.mSanityRequest);
}

void ArchiverEngineBase::EvaluateCurrentActionLocked() {
  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kNone) {
    return;
  }

  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kBundle &&
      mPendingBundlePromptRequest.has_value()) {
    if (mIsCancelPending) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                LogActionCanceledV2(LogActionV2::kBundle));
    }
    return;
  }
  if (mCurrentPrimaryAction == EnginePrimaryActionV2::kDecode &&
      mPendingDecodePromptRequest.has_value()) {
    if (mIsCancelPending) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                LogActionCanceledV2(LogActionV2::kDecode));
    }
    return;
  }

  if (mPendingCheckpointRequest.has_value()) {
    return;
  }

  if (mActiveTask == nullptr) {
    FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                              LogPrimaryDirectorMissingV2(mCurrentLogAction));
    return;
  }

  switch (mActiveTask->Disposition()) {
    case TaskDispositionV2::kRunning:
      return;
    case TaskDispositionV2::kCompleted:
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCompleted,
                                LogActionCompletedV2(mCurrentLogAction));
      return;
    case TaskDispositionV2::kFailed:
      FinishCurrentActionLocked(EngineEventTypeV2::kActionFailed,
                                LogActionFailedV2(mCurrentLogAction));
      return;
    case TaskDispositionV2::kCanceled:
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                LogActionCanceledV2(mCurrentLogAction));
      return;
  }
}

void ArchiverEngineBase::PrimeActionStateLocked(EnginePrimaryActionV2 pPrimaryAction,
                                                LogActionV2 pLogAction,
                                                const std::string& pSourcePath,
                                                const std::string& pDestinationPath) {
  mLastTerminalState.reset();
  mCurrentPrimaryAction = pPrimaryAction;
  mCurrentLogAction = pLogAction;
  mCurrentSourcePath = pSourcePath;
  mCurrentDestinationPath = pDestinationPath;
  mIsUiLocked = true;
  mIsCancelPending = false;
  mActiveTask.reset();
  mActiveRuntime.reset();
  mPendingBundlePromptRequest.reset();
  mPendingDecodePromptRequest.reset();
  mPendingPromptId.reset();
  mPendingCheckpointRequest.reset();
}

void ArchiverEngineBase::EmitActionAcceptedLocked(LogActionV2 pLogAction) {
  EngineEventV2 aAccepted;
  aAccepted.mType = EngineEventTypeV2::kActionAccepted;
  aAccepted.mSnapshot = BuildSnapshotLocked();
  aAccepted.mMessage = LogActionAcceptedV2(pLogAction);
  PushEventLocked(std::move(aAccepted));
}

void ArchiverEngineBase::EmitActionStartLocked(LogActionV2 pLogAction,
                                               const std::string& pSourcePath,
                                               const std::string& pDestinationPath) {
  EmitLogLocked(LogLevelV2::kInfo,
                LogActionStartDetailV2(pLogAction,
                                       pSourcePath,
                                       pDestinationPath));

  UiEffectV2 aShowLoading;
  aShowLoading.mType = UiEffectTypeV2::kShowLoading;
  aShowLoading.mLabel = LogPreparingActionV2(pLogAction);
  EmitUiEffectLocked(aShowLoading, EngineEventTypeV2::kUiStateChanged,
                     LogUiLockedForActionV2(pLogAction));
}

void ArchiverEngineBase::EmitErrorDialogLocked(const std::string& pTitle,
                                                const std::string& pMessage) {
  UiEffectV2 aShowDialog;
  aShowDialog.mType = UiEffectTypeV2::kShowDialog;
  aShowDialog.mDialog.mKind = UiDialogKindV2::kError;
  aShowDialog.mDialog.mTitle = pTitle;
  aShowDialog.mDialog.mMessage = pMessage;
  EmitUiEffectLocked(aShowDialog, EngineEventTypeV2::kUiStateChanged, pMessage);
}

void ArchiverEngineBase::EmitDestinationPromptLocked(const std::string& pTitle,
                                                      const std::string& pMessage) {
  if (!mPendingPromptId.has_value()) {
    return;
  }

  UiEffectV2 aShowPrompt;
  aShowPrompt.mType = UiEffectTypeV2::kShowPrompt;
  aShowPrompt.mPrompt.mPromptId = *mPendingPromptId;
  aShowPrompt.mPrompt.mKind = UiPromptKindV2::kDestinationAction;
  aShowPrompt.mPrompt.mTitle = pTitle;
  aShowPrompt.mPrompt.mMessage = pMessage;
  aShowPrompt.mPrompt.mPrimaryLabel = "Clear";
  aShowPrompt.mPrompt.mSecondaryLabel = "Merge";
  aShowPrompt.mPrompt.mCancelLabel = "Cancel";
  EmitUiEffectLocked(aShowPrompt, EngineEventTypeV2::kUiStateChanged, pMessage);
}

void ArchiverEngineBase::AcceptBundleLocked(const BundleRequestV2& pRequest) {
  mLastTerminalState.reset();
  const BundleRequestV2 aResolvedRequest =
      ResolveBundleRequestPaths(pRequest, *mFileSystem);
  const LaunchDecisionV2 aDecision = CheckBundleLaunch(aResolvedRequest, *mFileSystem);
  if (aDecision.mSignal == LaunchSignalV2::kRed) {
    RejectPrimaryActionLocked(aDecision.mMessage);
    EmitErrorDialogLocked(aDecision.mTitle, aDecision.mMessage);
    return;
  }

  if (aDecision.mSignal == LaunchSignalV2::kYellow) {
    PrimeActionStateLocked(EnginePrimaryActionV2::kBundle,
                           LogActionV2::kBundle,
                           std::string{},
                           std::string{});
    mPendingBundlePromptRequest = aResolvedRequest;
    mPendingPromptId = mNextPromptId++;
    EmitActionAcceptedLocked(LogActionV2::kBundle);
    EmitDestinationPromptLocked(aDecision.mTitle, aDecision.mMessage);
    return;
  }

  StartBundleExecutionLocked(aResolvedRequest, true);
}

void ArchiverEngineBase::StartBundleExecutionLocked(const BundleRequestV2& pRequest,
                                                    bool pEmitAccepted) {
  PrimeActionStateLocked(EnginePrimaryActionV2::kBundle,
                         LogActionV2::kBundle,
                         pRequest.mSourceDirectory,
                         pRequest.mDestinationDirectory);
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mActiveTask = std::make_unique<BundleTaskV2>(pRequest,
                                                mActiveRuntime.get(),
                                                mFileSystem,
                                                mLayout);

  if (pEmitAccepted) {
    EmitActionAcceptedLocked(LogActionV2::kBundle);
  }
  EmitActionStartLocked(LogActionV2::kBundle,
                        pRequest.mSourceDirectory,
                        pRequest.mDestinationDirectory);
}

void ArchiverEngineBase::AcceptDecodeLocked(const DecodeRequestV2& pRequest) {
  mLastTerminalState.reset();
  const LaunchDecisionV2 aDecision = CheckDecodeLaunch(pRequest, *mFileSystem);
  if (aDecision.mSignal == LaunchSignalV2::kRed) {
    RejectPrimaryActionLocked(aDecision.mMessage);
    EmitErrorDialogLocked(aDecision.mTitle, aDecision.mMessage);
    return;
  }

  if (aDecision.mSignal == LaunchSignalV2::kYellow) {
    PrimeActionStateLocked(EnginePrimaryActionV2::kDecode,
                           LogActionV2::kDecode,
                           std::string{},
                           std::string{});
    mPendingDecodePromptRequest = pRequest;
    mPendingPromptId = mNextPromptId++;
    EmitActionAcceptedLocked(LogActionV2::kDecode);
    EmitDestinationPromptLocked(aDecision.mTitle, aDecision.mMessage);
    return;
  }

  StartDecodeExecutionLocked(pRequest, true);
}

void ArchiverEngineBase::StartDecodeExecutionLocked(const DecodeRequestV2& pRequest,
                                                    bool pEmitAccepted) {
  DecodeRequestV2 aDecodeRequest = pRequest;
  PrimeActionStateLocked(EnginePrimaryActionV2::kDecode,
                         LogActionV2::kDecode,
                         aDecodeRequest.mSourcePath,
                         aDecodeRequest.mDestinationDirectory);
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mActiveTask = std::make_unique<DecodeTaskV2>(aDecodeRequest,
                                                mActiveRuntime.get(),
                                                mFileSystem,
                                                mLayout);

  if (pEmitAccepted) {
    EmitActionAcceptedLocked(LogActionV2::kDecode);
  }
  EmitActionStartLocked(LogActionV2::kDecode,
                        aDecodeRequest.mSourcePath,
                        aDecodeRequest.mDestinationDirectory);
}

void ArchiverEngineBase::AcceptManifestLocked(const DecodeRequestV2& pRequest) {
  DecodeRequestV2 aManifestRequest = pRequest;
  aManifestRequest.mIntent = DecodeIntentV2::kManifest;
  PrimeActionStateLocked(EnginePrimaryActionV2::kManifest,
                         LogActionV2::kManifest,
                         aManifestRequest.mSourcePath,
                         aManifestRequest.mDestinationDirectory);
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mActiveTask = std::make_unique<ManifestTaskV2>(aManifestRequest,
                                                  mActiveRuntime.get(),
                                                  mFileSystem,
                                                  mLayout);

  EmitActionAcceptedLocked(LogActionV2::kManifest);
  EmitActionStartLocked(LogActionV2::kManifest,
                        aManifestRequest.mSourcePath,
                        aManifestRequest.mDestinationDirectory);
}

void ArchiverEngineBase::AcceptRepairLocked(const RepairRequestV2& pRequest) {
  PrimeActionStateLocked(EnginePrimaryActionV2::kRepair,
                         LogActionV2::kRepair,
                         pRequest.mSourcePath,
                         pRequest.mDestinationDirectory);
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mActiveTask = std::make_unique<RepairTaskV2>(pRequest,
                                                mActiveRuntime.get(),
                                                mFileSystem,
                                                mLayout);

  EmitActionAcceptedLocked(LogActionV2::kRepair);
  EmitActionStartLocked(LogActionV2::kRepair,
                        pRequest.mSourcePath,
                        pRequest.mDestinationDirectory);
}

void ArchiverEngineBase::AcceptSanityLocked(const SanityRequestV2& pRequest) {
  PrimeActionStateLocked(EnginePrimaryActionV2::kSanity,
                         LogActionV2::kSanity,
                         pRequest.mLeftDirectory,
                         pRequest.mRightDirectory);
  mActiveRuntime = std::make_unique<ActiveRuntimeV2>(this);
  mActiveTask = std::make_unique<SanityTaskV2>(pRequest, mActiveRuntime.get());

  EmitActionAcceptedLocked(LogActionV2::kSanity);
  EmitActionStartLocked(LogActionV2::kSanity,
                        pRequest.mLeftDirectory,
                        pRequest.mRightDirectory);
}

void ArchiverEngineBase::HandlePromptResponseLocked(const UiPromptResponseV2& pResponse) {
  if (!mPendingPromptId.has_value() ||
      (!mPendingBundlePromptRequest.has_value() &&
       !mPendingDecodePromptRequest.has_value())) {
    return;
  }
  if (pResponse.mPromptId != *mPendingPromptId ||
      pResponse.mKind != UiPromptKindV2::kDestinationAction) {
    return;
  }

  if (mPendingBundlePromptRequest.has_value()) {
    if (pResponse.mChoice == UiPromptChoiceV2::kCancel) {
      FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                                LogActionCanceledV2(LogActionV2::kBundle));
      return;
    }

    BundleRequestV2 aRequest = *mPendingBundlePromptRequest;
    aRequest.mClearDestinationBeforeWrite =
        (pResponse.mChoice == UiPromptChoiceV2::kClear);
    StartBundleExecutionLocked(aRequest, false);
    return;
  }

  if (!mPendingDecodePromptRequest.has_value()) {
    return;
  }

  if (pResponse.mChoice == UiPromptChoiceV2::kCancel) {
    FinishCurrentActionLocked(EngineEventTypeV2::kActionCanceled,
                              LogActionCanceledV2(LogActionV2::kDecode));
    return;
  }

  DecodeRequestV2 aRequest = *mPendingDecodePromptRequest;
  aRequest.mClearDestinationBeforeWrite =
      (pResponse.mChoice == UiPromptChoiceV2::kClear);
  StartDecodeExecutionLocked(aRequest, false);
}

void ArchiverEngineBase::HandleCheckpointDecisionLocked(
    const EngineCheckpointDecisionV2& pDecision) {
  if (!mPendingCheckpointRequest.has_value() ||
      pDecision.mCheckpointId != mPendingCheckpointRequest->mCheckpointId) {
    return;
  }

  mPendingCheckpointRequest.reset();
  if (pDecision.mKind == EngineCheckpointDecisionKindV2::kCancel &&
      !mIsCancelPending) {
    AcceptCancelLocked();
  }
}

void ArchiverEngineBase::RejectPrimaryActionLocked(const std::string& pReason) {
  EngineEventV2 aRejected;
  aRejected.mType = EngineEventTypeV2::kActionRejected;
  aRejected.mSnapshot = BuildSnapshotLocked();
  aRejected.mMessage = pReason;
  PushEventLocked(std::move(aRejected));
}

void ArchiverEngineBase::AcceptCancelLocked() {
  mPendingCheckpointRequest.reset();
  mIsCancelPending = true;

  EngineEventV2 aAccepted;
  aAccepted.mType = EngineEventTypeV2::kCancelAccepted;
  aAccepted.mSnapshot = BuildSnapshotLocked();
  aAccepted.mMessage = LogCancelAcceptedV2();
  PushEventLocked(std::move(aAccepted));

  EmitLogLocked(LogLevelV2::kWarning, LogCancelRequestedV2());
}

void ArchiverEngineBase::RejectCancelLocked(const std::string& pReason) {
  EngineEventV2 aRejected;
  aRejected.mType = EngineEventTypeV2::kCancelRejected;
  aRejected.mSnapshot = BuildSnapshotLocked();
  aRejected.mMessage = pReason;
  PushEventLocked(std::move(aRejected));
}

void ArchiverEngineBase::FinishCurrentActionLocked(EngineEventTypeV2 pType,
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

  EngineTerminalStateV2 aTerminal;
  aTerminal.mAction = mCurrentPrimaryAction;
  aTerminal.mTerminalType = pType;
  if (mActiveTask != nullptr) {
    const EngineTaskTerminalSnapshotV2 aTaskTerminal =
        mActiveTask->BuildTerminalSnapshot();
    aTerminal.mBundleState = aTaskTerminal.mBundleState;
    aTerminal.mDecodeState = aTaskTerminal.mDecodeState;
    aTerminal.mSanityState = aTaskTerminal.mSanityState;
    aTerminal.mFailureMessage = aTaskTerminal.mFailureMessage;
    aTerminal.mFailure = aTaskTerminal.mFailure;
  }
  if (pType == EngineEventTypeV2::kActionCanceled &&
      !aTerminal.mFailure.HasFailure()) {
    aTerminal.mFailure.mFamily = FailureFamilyV2::kCanceled;
    aTerminal.mFailure.mMessage = pMessage;
  }
  if (aTerminal.mFailureMessage.empty() &&
      pType == EngineEventTypeV2::kActionFailed) {
    aTerminal.mFailureMessage = pMessage;
  }
  if (pType == EngineEventTypeV2::kActionFailed &&
      !aTerminal.mFailure.HasFailure()) {
    aTerminal.mFailure.mFamily = FailureFamilyV2::kInternal;
    aTerminal.mFailure.mMessage = aTerminal.mFailureMessage;
  }
  mLastTerminalState = std::move(aTerminal);

  EngineEventV2 aFinished;
  aFinished.mType = pType;
  aFinished.mMessage = pMessage;

  mCurrentPrimaryAction = EnginePrimaryActionV2::kNone;
  mIsUiLocked = false;
  mIsCancelPending = false;
  mActiveTask.reset();
  mActiveRuntime.reset();
  mPendingPromptId.reset();
  mPendingBundlePromptRequest.reset();
  mPendingDecodePromptRequest.reset();
  mPendingCheckpointRequest.reset();
  mCurrentSourcePath.clear();
  mCurrentDestinationPath.clear();

  aFinished.mSnapshot = BuildSnapshotLocked();
  PushEventLocked(std::move(aFinished));

  UiEffectV2 aHideLoading;
  aHideLoading.mType = UiEffectTypeV2::kHideLoading;
  EmitUiEffectLocked(aHideLoading, EngineEventTypeV2::kUiStateChanged,
                     LogUiUnlockedV2());
}

void ArchiverEngineBase::EmitUiEffectLocked(const UiEffectV2& pEffect,
                                        EngineEventTypeV2 pType,
                                        const std::string& pMessage) {
  EngineEventV2 aEvent;
  aEvent.mType = pType;
  aEvent.mMessage = pMessage;
  aEvent.mUiEffect = pEffect;
  PushEventLocked(std::move(aEvent));
}

void ArchiverEngineBase::EmitLogLocked(LogLevelV2 pLevel,
                                   const std::string& pMessage) {
  LogEntryV2 aEntry;
  aEntry.mLevel = pLevel;
  aEntry.mMessage = pMessage;
  EngineEventV2 aEvent;
  aEvent.mType = EngineEventTypeV2::kLog;
  aEvent.mMessage = pMessage;
  aEvent.mLogEntry = aEntry;
  PushEventLocked(std::move(aEvent));
}

void ArchiverEngineBase::EmitProgressLocked(const ProgressSnapshotV2& pSnapshot) {
  EngineEventV2 aEvent;
  aEvent.mType = EngineEventTypeV2::kProgress;
  aEvent.mMessage = pSnapshot.mLabel;
  aEvent.mProgress = pSnapshot;
  PushEventLocked(std::move(aEvent));
}

void ArchiverEngineBase::PushEventLocked(EngineEventV2 pEvent) {
  PrefixEventMessageWithClockIfEnabled(pEvent);
  pEvent.mSnapshot = BuildSnapshotLocked();
  mCommandBus->PublishEvent(pEvent);
}

bool ArchiverEngineBase::EmitRuntimeEventLocked(const RuntimeEventV2& pEvent) {
  EngineEventV2 aEvent;
  aEvent.mType = EngineEventTypeV2::kRuntimeEvent;
  aEvent.mMessage = pEvent.mLabel.empty() ? RuntimeEventKindLabelV2(pEvent.mKind)
                                          : pEvent.mLabel;
  aEvent.mRuntimeEvent = pEvent;
  PushEventLocked(std::move(aEvent));

  if (!IsBlockingCheckpointKindLocked(pEvent.mKind) ||
      mCurrentPrimaryAction == EnginePrimaryActionV2::kNone ||
      mIsCancelPending || mPendingCheckpointRequest.has_value()) {
    return true;
  }

  EngineCheckpointRequestV2 aRequest;
  aRequest.mCheckpointId = mNextCheckpointId++;
  aRequest.mRuntimeEvent = pEvent;
  mPendingCheckpointRequest = aRequest;

  EngineEventV2 aCheckpoint;
  aCheckpoint.mType = EngineEventTypeV2::kCheckpointRequested;
  aCheckpoint.mCheckpointRequest = *mPendingCheckpointRequest;
  aCheckpoint.mMessage =
      "Checkpoint requested after " +
      std::string(pEvent.mLabel.empty() ? RuntimeEventKindLabelV2(pEvent.mKind)
                                        : pEvent.mLabel);
  PushEventLocked(std::move(aCheckpoint));
  return false;
}

bool ArchiverEngineBase::WantsRuntimeEventKindLocked(RuntimeEventKindV2 pKind) const {
  if (IsBlockingCheckpointKindLocked(pKind)) {
    return true;
  }
  if (mCaptureVerboseRuntimeEvents) {
    return true;
  }
  if (knobs::kEngineEmitRuntimeEventsByDefaultV2) {
    return !RuntimeEventKindIsVerboseV2(pKind);
  }
  return false;
}

bool ArchiverEngineBase::IsBlockingCheckpointKindLocked(RuntimeEventKindV2 pKind) const {
  return std::find(mBlockingCheckpointKinds.begin(),
                   mBlockingCheckpointKinds.end(),
                   pKind) != mBlockingCheckpointKinds.end();
}

EngineSnapshotV2 ArchiverEngineBase::BuildSnapshotLocked() const {
  EngineSnapshotV2 aSnapshot;
  aSnapshot.mIsBusy = mCurrentPrimaryAction != EnginePrimaryActionV2::kNone;
  aSnapshot.mIsUiLocked = mIsUiLocked;
  aSnapshot.mIsCancelPending = mIsCancelPending;
  aSnapshot.mIsAwaitingCheckpointDecision = mPendingCheckpointRequest.has_value();
  aSnapshot.mPendingCheckpointId =
      mPendingCheckpointRequest.has_value()
          ? mPendingCheckpointRequest->mCheckpointId
          : 0u;
  aSnapshot.mCurrentPrimaryAction = mCurrentPrimaryAction;
  return aSnapshot;
}

}  // namespace peanutbutter
