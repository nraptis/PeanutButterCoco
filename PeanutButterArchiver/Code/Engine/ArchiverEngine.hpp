#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <vector>

#include "../Common/CommandBus.hpp"
#include "../Common/EngineFailure.hpp"
#include "../Common/EngineMessaging.hpp"
#include "../Common/LogCatalog.hpp"
#include "TaskCommon.hpp"
#include "FileAccess/LocalFileSystem.hpp"

namespace peanutbutter {

struct EngineTerminalStateV2 {
  EnginePrimaryActionV2 mAction = EnginePrimaryActionV2::kNone;
  EngineEventTypeV2 mTerminalType = EngineEventTypeV2::kLog;
  std::string mFailureMessage;
  FailureInfoV2 mFailure{};
  std::shared_ptr<BundleWorkStateV2> mBundleState;
  std::shared_ptr<DecodeWorkStateV2> mDecodeState;
  std::shared_ptr<SanityWorkStateV2> mSanityState;
};

enum class ArchiverEngineTypeV2 {
  kBase = 0,
  kBundle = 1,
  kDecode = 2,
  kRepair = 3,
  kSanity = 4,
};

class ArchiverEngineBase {
 public:
  explicit ArchiverEngineBase(
      ArchiverEngineTypeV2 pEngineType,
      FileSystemV2* pFileSystem = nullptr,
      const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr,
      CommandBusV2* pCommandBus = nullptr);
  virtual ~ArchiverEngineBase();

  const ArchiverEngineTypeV2 mEngineType;

  void EnqueueBundleRequest(const BundleRequestV2& pRequest);
  void EnqueueDecodeRequest(const DecodeRequestV2& pRequest);
  void EnqueueManifestRequest(const DecodeRequestV2& pRequest);
  void EnqueueRepairRequest(const RepairRequestV2& pRequest);
  void EnqueueSanityRequest(const SanityRequestV2& pRequest);
  void EnqueuePromptResponse(const UiPromptResponseV2& pResponse);
  void EnqueueCancelRequest();
  void EnqueueCheckpointDecision(const EngineCheckpointDecisionV2& pDecision);
  void ContinueCheckpoint(std::uint64_t pCheckpointId);
  void CancelCheckpoint(std::uint64_t pCheckpointId);
  void Dispose();

  void Heartbeat();
  void SetCaptureVerboseRuntimeEvents(bool pEnabled);
  void SetBlockingCheckpointKinds(
      const std::vector<RuntimeEventKindV2>& pKinds);
  EngineEventListV2 Poll();
  bool CapturesVerboseRuntimeEvents() const;
  EngineSnapshotV2 Snapshot() const;
  std::optional<EngineTerminalStateV2> TakeTerminalState();
  CommandBusV2& CommandBus();
  const CommandBusV2& CommandBus() const;

 private:
  class ActiveRuntimeV2;

 protected:
  bool SupportsPrimaryActionLocked(EnginePrimaryActionV2 pAction) const;
  std::string UnsupportedActionMessageLocked(EnginePrimaryActionV2 pAction) const;

 private:
  void ProcessIncomingCommandsLocked();
  bool TryHandleStartCommandLocked(const EngineCommandV2& pCommand);
  void EvaluateCurrentActionLocked();
  bool ShouldHeartbeatCurrentActionLocked() const;
  bool HasBundleStartRequestLocked(const EngineCommandV2& pCommand) const;
  bool HasDecodeStartRequestLocked(const EngineCommandV2& pCommand) const;
  bool HasManifestStartRequestLocked(const EngineCommandV2& pCommand) const;
  bool HasRepairStartRequestLocked(const EngineCommandV2& pCommand) const;
  bool HasSanityStartRequestLocked(const EngineCommandV2& pCommand) const;
  void AcceptBundleStartCommandLocked(const EngineCommandV2& pCommand);
  void AcceptDecodeStartCommandLocked(const EngineCommandV2& pCommand);
  void AcceptManifestStartCommandLocked(const EngineCommandV2& pCommand);
  void AcceptRepairStartCommandLocked(const EngineCommandV2& pCommand);
  void AcceptSanityStartCommandLocked(const EngineCommandV2& pCommand);
  void AcceptBundleLocked(const BundleRequestV2& pRequest);
  void AcceptDecodeLocked(const DecodeRequestV2& pRequest);
  void AcceptManifestLocked(const DecodeRequestV2& pRequest);
  void AcceptRepairLocked(const RepairRequestV2& pRequest);
  void AcceptSanityLocked(const SanityRequestV2& pRequest);
  void HandlePromptResponseLocked(const UiPromptResponseV2& pResponse);
  void HandleCheckpointDecisionLocked(
      const EngineCheckpointDecisionV2& pDecision);
  void StartBundleExecutionLocked(const BundleRequestV2& pRequest,
                                  bool pEmitAccepted);
  void StartDecodeExecutionLocked(const DecodeRequestV2& pRequest,
                                  bool pEmitAccepted);
  void PrimeActionStateLocked(EnginePrimaryActionV2 pPrimaryAction,
                              LogActionV2 pLogAction,
                              const std::string& pSourcePath,
                              const std::string& pDestinationPath);
  void EmitActionAcceptedLocked(LogActionV2 pLogAction);
  void EmitActionStartLocked(LogActionV2 pLogAction,
                             const std::string& pSourcePath,
                             const std::string& pDestinationPath);
  void EmitErrorDialogLocked(const std::string& pTitle,
                             const std::string& pMessage);
  void EmitDestinationPromptLocked(const std::string& pTitle,
                                   const std::string& pMessage);
  void RejectPrimaryActionLocked(const std::string& pReason);
  void AcceptCancelLocked();
  void RejectCancelLocked(const std::string& pReason);
  void FinishCurrentActionLocked(EngineEventTypeV2 pType,
                                 const std::string& pMessage);
  void EmitUiEffectLocked(const UiEffectV2& pEffect,
                          EngineEventTypeV2 pType,
                          const std::string& pMessage);
  void EmitLogLocked(LogLevelV2 pLevel,
                     const std::string& pMessage);
  bool EmitRuntimeEventLocked(const RuntimeEventV2& pEvent);
  void EmitProgressLocked(const ProgressSnapshotV2& pSnapshot);
  void PushEventLocked(EngineEventV2 pEvent);
  bool WantsRuntimeEventKind(RuntimeEventKindV2 pKind) const;
  bool WantsRuntimeEventKindLocked(RuntimeEventKindV2 pKind) const;
  bool IsBlockingCheckpointKindLocked(RuntimeEventKindV2 pKind) const;
  EngineSnapshotV2 BuildSnapshotLocked() const;

 private:
  mutable std::recursive_mutex mMutex;
  LocalFileSystemV2 mLocalFileSystem{};
  LocalCommandBusV2 mOwnedCommandBus{};
  FileSystemV2* mFileSystem = nullptr;
  const memory_layout::ArchiveLayoutConfigV2* mLayout = nullptr;
  CommandBusV2* mCommandBus = nullptr;
  std::optional<EngineTerminalStateV2> mLastTerminalState;
  EnginePrimaryActionV2 mCurrentPrimaryAction = EnginePrimaryActionV2::kNone;
  bool mIsUiLocked = false;
  bool mIsCancelPending = false;
  bool mCaptureVerboseRuntimeEvents = false;
  std::unique_ptr<EngineTaskV2> mActiveTask;
  std::unique_ptr<ActiveRuntimeV2> mActiveRuntime;
  std::optional<std::uint64_t> mPendingPromptId;
  std::optional<BundleRequestV2> mPendingBundlePromptRequest;
  std::optional<DecodeRequestV2> mPendingDecodePromptRequest;
  std::optional<EngineCheckpointRequestV2> mPendingCheckpointRequest;
  std::uint64_t mNextPromptId = 1u;
  std::uint64_t mNextCheckpointId = 1u;
  LogActionV2 mCurrentLogAction = LogActionV2::kBundle;
  std::string mCurrentSourcePath;
  std::string mCurrentDestinationPath;
  std::vector<RuntimeEventKindV2> mBlockingCheckpointKinds;
  bool mIsDisposed = false;
};

class ArchiverEngine_Bundle final : public ArchiverEngineBase {
 public:
  explicit ArchiverEngine_Bundle(
      FileSystemV2* pFileSystem = nullptr,
      const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr,
      CommandBusV2* pCommandBus = nullptr)
      : ArchiverEngineBase(
            ArchiverEngineTypeV2::kBundle,
            pFileSystem,
            pLayout,
            pCommandBus) {}
};

class ArchiverEngine_Decode final : public ArchiverEngineBase {
 public:
  explicit ArchiverEngine_Decode(
      FileSystemV2* pFileSystem = nullptr,
      const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr,
      CommandBusV2* pCommandBus = nullptr)
      : ArchiverEngineBase(
            ArchiverEngineTypeV2::kDecode,
            pFileSystem,
            pLayout,
            pCommandBus) {}
};

class ArchiverEngine_Repair final : public ArchiverEngineBase {
 public:
  explicit ArchiverEngine_Repair(
      FileSystemV2* pFileSystem = nullptr,
      const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr,
      CommandBusV2* pCommandBus = nullptr)
      : ArchiverEngineBase(
            ArchiverEngineTypeV2::kRepair,
            pFileSystem,
            pLayout,
            pCommandBus) {}
};

class ArchiverEngine_Sanity final : public ArchiverEngineBase {
 public:
  explicit ArchiverEngine_Sanity(
      FileSystemV2* pFileSystem = nullptr,
      const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr,
      CommandBusV2* pCommandBus = nullptr)
      : ArchiverEngineBase(
            ArchiverEngineTypeV2::kSanity,
            pFileSystem,
            pLayout,
            pCommandBus) {}
};

class ArchiverEngine final : public ArchiverEngineBase {
 public:
  explicit ArchiverEngine(
      FileSystemV2* pFileSystem = nullptr,
      const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr,
      CommandBusV2* pCommandBus = nullptr)
      : ArchiverEngineBase(
            ArchiverEngineTypeV2::kBase,
            pFileSystem,
            pLayout,
            pCommandBus) {}
};

}  // namespace peanutbutter
