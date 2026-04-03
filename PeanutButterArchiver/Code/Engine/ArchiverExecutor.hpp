#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "../Common/BundleRequest.hpp"
#include "../Common/CommandBus.hpp"
#include "../Common/DecodeRequest.hpp"
#include "../Common/EngineMessaging.hpp"
#include "../Common/RepairRequest.hpp"
#include "../Common/RuntimeEvent.hpp"
#include "../Common/SanityRequest.hpp"
#include "../Common/UiContracts.hpp"
#include "ArchiverEngine.hpp"

namespace peanutbutter {

class ArchiverExecutorDelegate {
 public:
  virtual ~ArchiverExecutorDelegate() = default;
  virtual void OnArchiverExecutorItemsAvailable() = 0;
};

class ArchiverExecutor final {
 public:
  explicit ArchiverExecutor(
      FileSystemV2* pFileSystem = nullptr,
      const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr)
      : mCommandBus(std::make_unique<LocalCommandBusV2>()),
        mEngine(std::make_unique<ArchiverEngine>(pFileSystem, pLayout, mCommandBus.get())),
        mListener(this) {
    mCommandBus->AddListener(&mListener);
  }

  ~ArchiverExecutor() {
    if (mEngine != nullptr) {
      mEngine->Dispose();
    }
    if (mCommandBus != nullptr) {
      mCommandBus->RemoveListener(&mListener);
    }
  }

  void SetDelegate(ArchiverExecutorDelegate* pDelegate) {
    std::lock_guard<std::mutex> aLock(mMutex);
    mDelegate = pDelegate;
  }

  void Heartbeat() {
    if (mEngine != nullptr) {
      mEngine->Heartbeat();
    }
  }

  void Dispose() {
    if (mEngine != nullptr) {
      mEngine->Dispose();
    }
  }

  CommandBusItemListV2 TakeItems() {
    if (mCommandBus == nullptr) {
      return CommandBusItemListV2{};
    }

    CommandBusItemListV2 aItems = mCommandBus->TakeItems();
    // The app consumes command bus items only; keep auxiliary event/log
    // history queues drained so they do not retain duplicate payloads.
    mCommandBus->DisposeAllEvents();
    mCommandBus->DisposeAllLogs();
    return aItems;
  }

  void EnqueueBundleRequest(const BundleRequestV2& pRequest) {
    if (mEngine != nullptr) {
      mEngine->EnqueueBundleRequest(pRequest);
    }
  }

  void EnqueueDecodeRequest(const DecodeRequestV2& pRequest) {
    if (mEngine != nullptr) {
      mEngine->EnqueueDecodeRequest(pRequest);
    }
  }

  void EnqueueManifestRequest(const DecodeRequestV2& pRequest) {
    if (mEngine != nullptr) {
      mEngine->EnqueueManifestRequest(pRequest);
    }
  }

  void EnqueueRepairRequest(const RepairRequestV2& pRequest) {
    if (mEngine != nullptr) {
      mEngine->EnqueueRepairRequest(pRequest);
    }
  }

  void EnqueueSanityRequest(const SanityRequestV2& pRequest) {
    if (mEngine != nullptr) {
      mEngine->EnqueueSanityRequest(pRequest);
    }
  }

  void EnqueuePromptResponse(const UiPromptResponseV2& pResponse) {
    if (mEngine != nullptr) {
      mEngine->EnqueuePromptResponse(pResponse);
    }
  }

  void EnqueueCancelRequest() {
    if (mEngine != nullptr) {
      mEngine->EnqueueCancelRequest();
    }
  }

  void EnqueueCheckpointDecision(const EngineCheckpointDecisionV2& pDecision) {
    if (mEngine != nullptr) {
      mEngine->EnqueueCheckpointDecision(pDecision);
    }
  }

  void ContinueCheckpoint(std::uint64_t pCheckpointId) {
    if (mEngine != nullptr) {
      mEngine->ContinueCheckpoint(pCheckpointId);
    }
  }

  void CancelCheckpoint(std::uint64_t pCheckpointId) {
    if (mEngine != nullptr) {
      mEngine->CancelCheckpoint(pCheckpointId);
    }
  }

  void SetCaptureVerboseRuntimeEvents(bool pEnabled) {
    if (mEngine != nullptr) {
      mEngine->SetCaptureVerboseRuntimeEvents(pEnabled);
    }
  }

  void SetBlockingCheckpointKinds(
      const std::vector<RuntimeEventKindV2>& pKinds) {
    if (mEngine != nullptr) {
      mEngine->SetBlockingCheckpointKinds(pKinds);
    }
  }

  bool CapturesVerboseRuntimeEvents() const {
    return mEngine != nullptr && mEngine->CapturesVerboseRuntimeEvents();
  }

  EngineSnapshotV2 Snapshot() const {
    return mEngine != nullptr ? mEngine->Snapshot() : EngineSnapshotV2{};
  }

  std::optional<EngineTerminalStateV2> TakeTerminalState() {
    return mEngine != nullptr ? mEngine->TakeTerminalState()
                              : std::optional<EngineTerminalStateV2>{};
  }

 private:
  class ExecutorCommandBusListener final : public CommandBusListenerV2 {
   public:
    explicit ExecutorCommandBusListener(ArchiverExecutor* pOwner)
        : mOwner(pOwner) {}

    void OnCommandBusEvent(const EngineEventV2&) override {
      if (mOwner != nullptr) {
        mOwner->NotifyItemsAvailable();
      }
    }

    void OnCommandBusLog(const LogEntryV2&,
                         const EngineSnapshotV2&) override {
      if (mOwner != nullptr) {
        mOwner->NotifyItemsAvailable();
      }
    }

   private:
    ArchiverExecutor* mOwner = nullptr;
  };

  void NotifyItemsAvailable() {
    ArchiverExecutorDelegate* aDelegate = nullptr;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      aDelegate = mDelegate;
    }
    if (aDelegate != nullptr) {
      aDelegate->OnArchiverExecutorItemsAvailable();
    }
  }

 private:
  mutable std::mutex mMutex;
  std::unique_ptr<LocalCommandBusV2> mCommandBus;
  std::unique_ptr<ArchiverEngineBase> mEngine;
  ExecutorCommandBusListener mListener;
  ArchiverExecutorDelegate* mDelegate = nullptr;
};

}  // namespace peanutbutter
