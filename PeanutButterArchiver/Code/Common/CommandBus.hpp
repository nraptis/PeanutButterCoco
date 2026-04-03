#pragma once

#include <algorithm>
#include <mutex>
#include <queue>
#include <vector>

#include "EngineMessaging.hpp"

namespace peanutbutter {

enum class CommandBusItemTypeV2 {
  kEvent = 0,
  kLog = 1,
};

struct CommandBusItemV2 {
  CommandBusItemTypeV2 mType = CommandBusItemTypeV2::kEvent;
  EngineSnapshotV2 mSnapshot{};
  EngineEventV2 mEvent{};
  LogEntryV2 mLog{};
};

using CommandBusItemListV2 = std::vector<CommandBusItemV2>;

class CommandBusListenerV2 {
 public:
  virtual ~CommandBusListenerV2() = default;
  virtual void OnCommandBusCommand(const EngineCommandV2&) {}
  virtual void OnCommandBusEvent(const EngineEventV2&) {}
  virtual void OnCommandBusLog(const LogEntryV2&, const EngineSnapshotV2&) {}
};

class CommandBusV2 {
 public:
  virtual ~CommandBusV2() = default;

  virtual void EnqueueCommand(const EngineCommandV2& pCommand) = 0;
  virtual bool TryDequeueCommand(EngineCommandV2* pOutCommand) = 0;

  virtual void PublishEvent(const EngineEventV2& pEvent) = 0;
  virtual void PublishLog(const LogEntryV2& pEntry,
                          const EngineSnapshotV2& pSnapshot) = 0;

  virtual CommandBusItemListV2 TakeItems() = 0;
  virtual EngineEventListV2 TakeEvents() = 0;
  virtual std::vector<LogEntryV2> TakeLogs() = 0;

  virtual void DisposeAllItems() = 0;
  virtual void DisposeAllEvents() = 0;
  virtual void DisposeAllLogs() = 0;

  virtual void AddListener(CommandBusListenerV2* pListener) = 0;
  virtual void RemoveListener(CommandBusListenerV2* pListener) = 0;

  void Cancel() {
    EnqueueCommand(MakeCancelCommandV2());
  }

  void ContinueCheckpoint(std::uint64_t pCheckpointId) {
    EnqueueCommand(MakeContinueCheckpointCommandV2(pCheckpointId));
  }

  void CancelCheckpoint(std::uint64_t pCheckpointId) {
    EnqueueCommand(MakeCancelCheckpointCommandV2(pCheckpointId));
  }
};

class BasicCommandBusV2 : public CommandBusV2 {
 public:
  void EnqueueCommand(const EngineCommandV2& pCommand) override {
    std::vector<CommandBusListenerV2*> aListeners;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      mCommands.push(pCommand);
      aListeners = mListeners;
    }

    for (CommandBusListenerV2* aListener : aListeners) {
      if (aListener != nullptr) {
        aListener->OnCommandBusCommand(pCommand);
      }
    }
  }

  bool TryDequeueCommand(EngineCommandV2* pOutCommand) override {
    if (pOutCommand == nullptr) {
      return false;
    }

    std::lock_guard<std::mutex> aLock(mMutex);
    if (mCommands.empty()) {
      return false;
    }

    *pOutCommand = std::move(mCommands.front());
    mCommands.pop();
    return true;
  }

  void PublishEvent(const EngineEventV2& pEvent) override {
    std::vector<CommandBusListenerV2*> aListeners;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      mEvents.push_back(pEvent);

      CommandBusItemV2 aItem;
      aItem.mType = CommandBusItemTypeV2::kEvent;
      aItem.mSnapshot = pEvent.mSnapshot;
      aItem.mEvent = pEvent;
      mItems.push_back(std::move(aItem));
      aListeners = mListeners;
    }

    for (CommandBusListenerV2* aListener : aListeners) {
      if (aListener != nullptr) {
        aListener->OnCommandBusEvent(pEvent);
      }
    }
  }

  void PublishLog(const LogEntryV2& pEntry,
                  const EngineSnapshotV2& pSnapshot) override {
    std::vector<CommandBusListenerV2*> aListeners;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      mLogs.push_back(pEntry);

      CommandBusItemV2 aItem;
      aItem.mType = CommandBusItemTypeV2::kLog;
      aItem.mSnapshot = pSnapshot;
      aItem.mLog = pEntry;
      mItems.push_back(std::move(aItem));
      aListeners = mListeners;
    }

    for (CommandBusListenerV2* aListener : aListeners) {
      if (aListener != nullptr) {
        aListener->OnCommandBusLog(pEntry, pSnapshot);
      }
    }
  }

  CommandBusItemListV2 TakeItems() override {
    std::lock_guard<std::mutex> aLock(mMutex);
    CommandBusItemListV2 aItems = std::move(mItems);
    mItems.clear();
    return aItems;
  }

  EngineEventListV2 TakeEvents() override {
    std::lock_guard<std::mutex> aLock(mMutex);
    EngineEventListV2 aEvents = std::move(mEvents);
    mEvents.clear();
    return aEvents;
  }

  std::vector<LogEntryV2> TakeLogs() override {
    std::lock_guard<std::mutex> aLock(mMutex);
    std::vector<LogEntryV2> aLogs = std::move(mLogs);
    mLogs.clear();
    return aLogs;
  }

  void DisposeAllItems() override {
    std::lock_guard<std::mutex> aLock(mMutex);
    mItems.clear();
  }

  void DisposeAllEvents() override {
    std::lock_guard<std::mutex> aLock(mMutex);
    mEvents.clear();
  }

  void DisposeAllLogs() override {
    std::lock_guard<std::mutex> aLock(mMutex);
    mLogs.clear();
  }

  void AddListener(CommandBusListenerV2* pListener) override {
    if (pListener == nullptr) {
      return;
    }

    std::lock_guard<std::mutex> aLock(mMutex);
    if (std::find(mListeners.begin(), mListeners.end(), pListener) !=
        mListeners.end()) {
      return;
    }
    mListeners.push_back(pListener);
  }

  void RemoveListener(CommandBusListenerV2* pListener) override {
    std::lock_guard<std::mutex> aLock(mMutex);
    mListeners.erase(
        std::remove(mListeners.begin(), mListeners.end(), pListener),
        mListeners.end());
  }

 private:
  std::mutex mMutex;
  std::queue<EngineCommandV2> mCommands;
  CommandBusItemListV2 mItems;
  EngineEventListV2 mEvents;
  std::vector<LogEntryV2> mLogs;
  std::vector<CommandBusListenerV2*> mListeners;
};

class LocalCommandBusV2 final : public BasicCommandBusV2 {};

class MockCommandBusV2 final : public BasicCommandBusV2 {};

}  // namespace peanutbutter
