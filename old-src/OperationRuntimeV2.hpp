#ifndef PEANUT_BUTTER_ULTIMA_OPERATION_RUNTIME_V2_HPP_
#define PEANUT_BUTTER_ULTIMA_OPERATION_RUNTIME_V2_HPP_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace peanutbutter {
namespace runtime_v2 {

struct LoggingStatV2 {
  std::uint64_t mArchivesCompleted = 0u;
  std::uint64_t mArchivesTotal = 0u;
  std::uint64_t mBytesCompleted = 0u;
  std::uint64_t mBytesTotal = 0u;

  bool HasArchiveStat() const {
    return mArchivesTotal > 0u;
  }

  bool HasByteStat() const {
    return mBytesTotal > 0u;
  }

  std::string FormatArchiveStat() const {
    if (!HasArchiveStat()) {
      return std::string();
    }
    return std::to_string(mArchivesCompleted) + " of " +
           std::to_string(mArchivesTotal) + " archives";
  }

  std::string FormatByteStat() const {
    if (!HasByteStat()) {
      return std::string();
    }

    static const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double aCompleted = static_cast<double>(mBytesCompleted);
    double aTotal = static_cast<double>(mBytesTotal);
    std::size_t aUnitIndex = 0u;
    while (aTotal >= 1024.0 &&
           aUnitIndex + 1u < (sizeof(kUnits) / sizeof(kUnits[0]))) {
      aCompleted /= 1024.0;
      aTotal /= 1024.0;
      ++aUnitIndex;
    }

    std::ostringstream aOut;
    aOut << std::fixed << std::setprecision(3)
         << aCompleted << " of " << aTotal << " " << kUnits[aUnitIndex];
    return aOut.str();
  }

  std::string FormatSummary() const {
    const std::string aArchive = FormatArchiveStat();
    const std::string aBytes = FormatByteStat();
    if (aArchive.empty()) {
      return aBytes;
    }
    if (aBytes.empty()) {
      return aArchive;
    }
    return aArchive + ", " + aBytes;
  }
};

struct ProgressSnapshotV2 {
  std::string mRelayName;
  double mRelayFraction = 0.0;
  double mOverallFraction = 0.0;
  std::uint64_t mEstimatedWorkUnits = 0u;
  std::uint64_t mCompletedWorkUnits = 0u;
  std::string mDetail;
  LoggingStatV2 mStat{};
};

class LoggingRelayV2 final {
 public:
  using StatusSink = std::function<void(const std::string&)>;
  using ProgressSink = std::function<void(const ProgressSnapshotV2&)>;

  void SetStatusSink(StatusSink pSink) {
    std::lock_guard<std::mutex> aLock(mMutex);
    mStatusSink = std::move(pSink);
  }

  void SetWarnSink(StatusSink pSink) {
    std::lock_guard<std::mutex> aLock(mMutex);
    mWarnSink = std::move(pSink);
  }

  void SetErrorSink(StatusSink pSink) {
    std::lock_guard<std::mutex> aLock(mMutex);
    mErrorSink = std::move(pSink);
  }

  void SetProgressSink(ProgressSink pSink) {
    std::lock_guard<std::mutex> aLock(mMutex);
    mProgressSink = std::move(pSink);
  }

  void LogStatus(const std::string& pMessage) {
    StatusSink aSink;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      mStatusMessages.push_back(pMessage);
      aSink = mStatusSink;
    }
    if (aSink) {
      aSink(pMessage);
    }
  }

  void LogWarn(const std::string& pMessage) {
    StatusSink aSink;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      mWarnMessages.push_back(pMessage);
      aSink = mWarnSink;
    }
    if (aSink) {
      aSink(pMessage);
    }
  }

  void LogError(const std::string& pMessage) {
    StatusSink aSink;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      mErrorMessages.push_back(pMessage);
      aSink = mErrorSink;
    }
    if (aSink) {
      aSink(pMessage);
    }
  }

  void RelayProgress(const ProgressSnapshotV2& pSnapshot) {
    ProgressSink aSink;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      mProgressSnapshots.push_back(pSnapshot);
      aSink = mProgressSink;
    }
    if (aSink) {
      aSink(pSnapshot);
    }
  }

  const std::vector<std::string>& StatusMessages() const {
    return mStatusMessages;
  }

  const std::vector<std::string>& WarnMessages() const {
    return mWarnMessages;
  }

  const std::vector<std::string>& ErrorMessages() const {
    return mErrorMessages;
  }

  const std::vector<ProgressSnapshotV2>& ProgressSnapshots() const {
    return mProgressSnapshots;
  }

 private:
  mutable std::mutex mMutex;
  StatusSink mStatusSink;
  StatusSink mWarnSink;
  StatusSink mErrorSink;
  ProgressSink mProgressSink;
  std::vector<std::string> mStatusMessages;
  std::vector<std::string> mWarnMessages;
  std::vector<std::string> mErrorMessages;
  std::vector<ProgressSnapshotV2> mProgressSnapshots;
};

class ProgressManagerV2;

class ProgressRelayV2 final {
 public:
  const std::string& Name() const {
    return mName;
  }

  std::uint64_t EstimatedWorkUnits() const {
    return mEstimatedWorkUnits;
  }

  void Report(double pLocalFraction,
              const std::string& pDetail = std::string(),
              const LoggingStatV2& pStat = LoggingStatV2{}) const;

  void Complete(const std::string& pDetail = std::string(),
                const LoggingStatV2& pStat = LoggingStatV2{}) const {
    Report(1.0, pDetail, pStat);
  }

 private:
  friend class ProgressManagerV2;

  ProgressRelayV2(ProgressManagerV2* pManager,
                  std::size_t pIndex,
                  std::string pName,
                  std::uint64_t pEstimatedWorkUnits)
      : mManager(pManager),
        mIndex(pIndex),
        mName(std::move(pName)),
        mEstimatedWorkUnits(pEstimatedWorkUnits) {}

 private:
  ProgressManagerV2* mManager = nullptr;
  std::size_t mIndex = 0u;
  std::string mName;
  std::uint64_t mEstimatedWorkUnits = 0u;
};

class ProgressManagerV2 final {
 public:
  struct RelayState {
    std::string mName;
    std::uint64_t mEstimatedWorkUnits = 0u;
    double mLocalFraction = 0.0;
    std::string mDetail;
    LoggingStatV2 mStat{};
  };

  explicit ProgressManagerV2(LoggingRelayV2* pLoggingRelay = nullptr)
      : mLoggingRelay(pLoggingRelay) {}

  void AttachLoggingRelay(LoggingRelayV2* pLoggingRelay) {
    std::lock_guard<std::mutex> aLock(mMutex);
    mLoggingRelay = pLoggingRelay;
  }

  std::shared_ptr<ProgressRelayV2> CreateRelay(const std::string& pName,
                                               std::uint64_t pEstimatedWorkUnits) {
    std::lock_guard<std::mutex> aLock(mMutex);
    const std::size_t aIndex = mRelayStates.size();
    RelayState aState;
    aState.mName = pName;
    aState.mEstimatedWorkUnits = pEstimatedWorkUnits;
    mRelayStates.push_back(aState);
    std::shared_ptr<ProgressRelayV2> aRelay =
        std::shared_ptr<ProgressRelayV2>(
            new ProgressRelayV2(this, aIndex, pName, pEstimatedWorkUnits));
    mRelays.push_back(aRelay);
    return aRelay;
  }

  const std::vector<std::shared_ptr<ProgressRelayV2>>& Relays() const {
    return mRelays;
  }

  ProgressSnapshotV2 Snapshot() const {
    std::lock_guard<std::mutex> aLock(mMutex);
    return BuildSnapshotLocked(mMostRecentRelayIndex);
  }

 private:
  friend class ProgressRelayV2;

  void UpdateRelay(std::size_t pIndex,
                   double pLocalFraction,
                   const std::string& pDetail,
                   const LoggingStatV2& pStat) {
    LoggingRelayV2* aLoggingRelay = nullptr;
    ProgressSnapshotV2 aSnapshot;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      if (pIndex >= mRelayStates.size()) {
        return;
      }
      RelayState& aState = mRelayStates[pIndex];
      aState.mLocalFraction = std::max(0.0, std::min(1.0, pLocalFraction));
      aState.mDetail = pDetail;
      aState.mStat = pStat;
      mMostRecentRelayIndex = pIndex;
      aLoggingRelay = mLoggingRelay;
      aSnapshot = BuildSnapshotLocked(pIndex);
    }
    if (aLoggingRelay != nullptr) {
      aLoggingRelay->RelayProgress(aSnapshot);
    }
  }

  ProgressSnapshotV2 BuildSnapshotLocked(std::size_t pRelayIndex) const {
    ProgressSnapshotV2 aSnapshot;
    if (mRelayStates.empty() || pRelayIndex >= mRelayStates.size()) {
      return aSnapshot;
    }

    std::uint64_t aEstimatedTotal = 0u;
    double aCompletedUnits = 0.0;
    for (const RelayState& aState : mRelayStates) {
      const std::uint64_t aWeight =
          aState.mEstimatedWorkUnits > 0u ? aState.mEstimatedWorkUnits : 1u;
      aEstimatedTotal += aWeight;
      aCompletedUnits += static_cast<double>(aWeight) * aState.mLocalFraction;
    }

    const RelayState& aState = mRelayStates[pRelayIndex];
    const std::uint64_t aRelayWeight =
        aState.mEstimatedWorkUnits > 0u ? aState.mEstimatedWorkUnits : 1u;

    aSnapshot.mRelayName = aState.mName;
    aSnapshot.mRelayFraction = aState.mLocalFraction;
    aSnapshot.mOverallFraction =
        aEstimatedTotal > 0u
            ? std::max(0.0, std::min(1.0,
                                     aCompletedUnits /
                                         static_cast<double>(aEstimatedTotal)))
            : 0.0;
    aSnapshot.mEstimatedWorkUnits = aEstimatedTotal;
    aSnapshot.mCompletedWorkUnits =
        static_cast<std::uint64_t>(std::max(0.0, aCompletedUnits));
    aSnapshot.mDetail = aState.mDetail;
    aSnapshot.mStat = aState.mStat;
    (void)aRelayWeight;
    return aSnapshot;
  }

 private:
  mutable std::mutex mMutex;
  LoggingRelayV2* mLoggingRelay = nullptr;
  std::vector<std::shared_ptr<ProgressRelayV2>> mRelays;
  std::vector<RelayState> mRelayStates;
  std::size_t mMostRecentRelayIndex = 0u;
};

inline void ProgressRelayV2::Report(double pLocalFraction,
                                    const std::string& pDetail,
                                    const LoggingStatV2& pStat) const {
  if (mManager == nullptr) {
    return;
  }
  mManager->UpdateRelay(mIndex, pLocalFraction, pDetail, pStat);
}

class CancelManagerV2;

class CancelRelayV2 final {
 public:
  const std::string& Name() const {
    return mName;
  }

  bool IsCancelRequested() const;

  void RequestCancel(const std::string& pReason = std::string()) const;

 private:
  friend class CancelManagerV2;

  CancelRelayV2(CancelManagerV2* pManager,
                std::string pName)
      : mManager(pManager),
        mName(std::move(pName)) {}

 private:
  CancelManagerV2* mManager = nullptr;
  std::string mName;
};

class CancelManagerV2 final {
 public:
  explicit CancelManagerV2(LoggingRelayV2* pLoggingRelay = nullptr)
      : mLoggingRelay(pLoggingRelay) {}

  void AttachLoggingRelay(LoggingRelayV2* pLoggingRelay) {
    std::lock_guard<std::mutex> aLock(mMutex);
    mLoggingRelay = pLoggingRelay;
  }

  std::shared_ptr<CancelRelayV2> CreateRelay(const std::string& pName) {
    std::lock_guard<std::mutex> aLock(mMutex);
    std::shared_ptr<CancelRelayV2> aRelay =
        std::shared_ptr<CancelRelayV2>(new CancelRelayV2(this, pName));
    mRelays.push_back(aRelay);
    return aRelay;
  }

  const std::vector<std::shared_ptr<CancelRelayV2>>& Relays() const {
    return mRelays;
  }

  bool IsCancelRequested() const {
    std::lock_guard<std::mutex> aLock(mMutex);
    return mCancelRequested;
  }

  const std::string& CancelReason() const {
    return mCancelReason;
  }

  void RequestCancel(const std::string& pReason) {
    LoggingRelayV2* aLoggingRelay = nullptr;
    bool aShouldLog = false;
    {
      std::lock_guard<std::mutex> aLock(mMutex);
      if (!mCancelRequested) {
        mCancelRequested = true;
        mCancelReason = pReason;
        aLoggingRelay = mLoggingRelay;
        aShouldLog = true;
      }
    }
    if (aShouldLog && aLoggingRelay != nullptr) {
      aLoggingRelay->LogWarn(
          pReason.empty() ? "Cancel requested." : "Cancel requested: " + pReason);
    }
  }

 private:
  friend class CancelRelayV2;

 private:
  mutable std::mutex mMutex;
  LoggingRelayV2* mLoggingRelay = nullptr;
  bool mCancelRequested = false;
  std::string mCancelReason;
  std::vector<std::shared_ptr<CancelRelayV2>> mRelays;
};

inline bool CancelRelayV2::IsCancelRequested() const {
  return mManager != nullptr && mManager->IsCancelRequested();
}

inline void CancelRelayV2::RequestCancel(const std::string& pReason) const {
  if (mManager == nullptr) {
    return;
  }
  const std::string aReason =
      pReason.empty() ? ("requested by relay '" + mName + "'") : pReason;
  mManager->RequestCancel(aReason);
}

}  // namespace runtime_v2
}  // namespace peanutbutter

#endif  // PEANUT_BUTTER_ULTIMA_OPERATION_RUNTIME_V2_HPP_
