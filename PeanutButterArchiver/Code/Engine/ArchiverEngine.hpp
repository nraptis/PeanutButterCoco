#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "../Common/EngineMessaging.hpp"
#include "../Common/LogCatalog.hpp"
#include "Bundle/Bundle_Director.hpp"
#include "Decode/Decode_Director.hpp"
#include "Sanity/Sanity_Director.hpp"

namespace peanutbutter {

class ArchiverEngine final {
 public:
  ArchiverEngine();
  ~ArchiverEngine();

  void EnqueueBundleRequest(const BundleRequestV2& pRequest);
  void EnqueueDecodeRequest(const DecodeRequestV2& pRequest);
  void EnqueueManifestRequest(const DecodeRequestV2& pRequest);
  void EnqueueRepairRequest(const RepairRequestV2& pRequest);
  void EnqueueSanityRequest(const SanityRequestV2& pRequest);
  void EnqueuePromptResponse(const UiPromptResponseV2& pResponse);
  void EnqueueCancelRequest();

  EngineEventListV2 Poll();
  EngineSnapshotV2 Snapshot() const;

 private:
  class ActiveRuntimeV2;

  void ProcessIncomingCommandsLocked();
  void ProcessCurrentActionLocked();
  bool HasPendingWorkLocked() const;
  bool ShouldStepCurrentActionLocked() const;
  void WorkerLoop();
  void AcceptBundleLocked(const BundleRequestV2& pRequest);
  void AcceptDecodeLocked(const DecodeRequestV2& pRequest);
  void AcceptManifestLocked(const DecodeRequestV2& pRequest);
  void AcceptRepairLocked(const RepairRequestV2& pRequest);
  void AcceptSanityLocked(const SanityRequestV2& pRequest);
  void HandlePromptResponseLocked(const UiPromptResponseV2& pResponse);
  void StartBundleExecutionLocked(const BundleRequestV2& pRequest,
                                  bool pEmitAccepted);
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
  void EmitProgressLocked(const ProgressSnapshotV2& pSnapshot);
  void PushEventLocked(EngineEventV2 pEvent);
  EngineSnapshotV2 BuildSnapshotLocked() const;

 private:
  mutable std::recursive_mutex mMutex;
  std::condition_variable_any mWorkerCv;
  std::thread mWorkerThread;
  bool mShouldStopWorker = false;
  bool mSkipCurrentActionStep = false;
  std::queue<EngineCommandV2> mIncomingCommands;
  EngineEventListV2 mPendingEvents;
  EnginePrimaryActionV2 mCurrentPrimaryAction = EnginePrimaryActionV2::kNone;
  bool mIsUiLocked = false;
  bool mIsCancelPending = false;
  std::unique_ptr<BundleDirector> mBundleDirector;
  std::unique_ptr<DecodeDirector> mDecodeDirector;
  std::unique_ptr<RepairDirector> mRepairDirector;
  std::unique_ptr<ManifestDirector> mManifestDirector;
  std::unique_ptr<SanityDirector> mSanityDirector;
  std::unique_ptr<ActiveRuntimeV2> mActiveRuntime;
  std::optional<std::uint64_t> mPendingPromptId;
  std::optional<BundleRequestV2> mPendingBundlePromptRequest;
  std::uint64_t mNextPromptId = 1u;
  LogActionV2 mCurrentLogAction = LogActionV2::kBundle;
  std::string mCurrentSourcePath;
  std::string mCurrentDestinationPath;
};

}  // namespace peanutbutter
