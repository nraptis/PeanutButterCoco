#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../Common/EngineFailure.hpp"
#include "../../Common/Logging.hpp"
#include "../../Common/Progress.hpp"
#include "../../Common/SanityRequest.hpp"
#include "../FileAccess/LocalFileSystem.hpp"

namespace peanutbutter {

class SanityDiscoveryCursorV2;
class SanityCompareCursorV2;

struct SanityEntryV2 {
  std::string mPath;
  std::string mRelativePath;
  bool mIsDirectory = false;
  std::uint64_t mLength = 0u;
};

struct SanityDiscoveryStateV2 {
  std::vector<SanityEntryV2> mLeftFiles;
  std::vector<SanityEntryV2> mRightFiles;
  std::vector<SanityEntryV2> mLeftFolders;
  std::vector<SanityEntryV2> mRightFolders;
  std::uint64_t mHiddenLeftFileCount = 0u;
  std::uint64_t mHiddenRightFileCount = 0u;
  std::uint64_t mHiddenLeftFolderCount = 0u;
  std::uint64_t mHiddenRightFolderCount = 0u;
  std::uint64_t mSkippedHiddenLeftFileCount = 0u;
  std::uint64_t mSkippedHiddenRightFileCount = 0u;
  std::uint64_t mSkippedHiddenLeftFolderCount = 0u;
  std::uint64_t mSkippedHiddenRightFolderCount = 0u;
};

struct SanityCompareStateV2 {
  LoggingStatV2 mStat{};
  std::uint64_t mMismatchCount = 0u;
  std::uint64_t mHiddenMismatchCount = 0u;
  std::uint64_t mNormalMismatchCount = 0u;
  std::vector<std::string> mHiddenFoldersMissingFromRight;
  std::vector<std::string> mHiddenFilesMissingFromRight;
  std::vector<std::string> mNormalFoldersMissingFromRight;
  std::vector<std::string> mNormalFilesMissingFromRight;
  std::vector<std::string> mHiddenFoldersMissingFromLeft;
  std::vector<std::string> mHiddenFilesMissingFromLeft;
  std::vector<std::string> mNormalFoldersMissingFromLeft;
  std::vector<std::string> mNormalFilesMissingFromLeft;
  std::vector<std::string> mHiddenInequalFiles;
  std::vector<std::string> mNormalInequalFiles;
};

struct SanityCursorStateV2 {
  std::shared_ptr<SanityDiscoveryCursorV2> mDiscovery;
  std::shared_ptr<SanityCompareCursorV2> mCompare;
};

struct SanityWorkStateV2 {
  SanityDiscoveryStateV2 mDiscovery{};
  SanityCompareStateV2 mCompare{};
  SanityCursorStateV2 mCursor{};
  std::uint64_t mWorkUnitsProcessed = 0u;
  FailureInfoV2 mFailure{};
};

class SanityRuntimeV2 {
 public:
  virtual ~SanityRuntimeV2() = default;
  virtual bool IsCancelRequested() const = 0;
  virtual void EmitLog(LogLevelV2 pLevel, const std::string& pMessage) = 0;
  virtual void EmitProgress(ProgressStageV2 pStage,
                            double pLocalFraction,
                            double pOverallFraction,
                            const std::string& pLabel) = 0;
};

class SanityStageContextV2 {
 public:
  SanityStageContextV2(const SanityRequestV2& pRequest,
                       SanityRuntimeV2* pRuntime);

  const SanityRequestV2& Request() const;
  SanityWorkStateV2& State();
  const SanityWorkStateV2& State() const;
  FileSystemV2& FileSystem();
  const FileSystemV2& FileSystem() const;
  bool IsCancelRequested() const;
  void EmitLog(LogLevelV2 pLevel, const std::string& pMessage) const;
  void SetActivePhase(ProgressStageV2 pStage,
                      std::size_t pPhaseIndex,
                      std::size_t pPhaseCount);
  void BeginWorkUnit();
  void ContinuePhaseOnNextHeartbeat();
  bool ActivePhaseNeedsMoreHeartbeats() const;
  void EmitPhaseProgress(double pLocalFraction,
                         const std::string& pLabel) const;
  bool ActivePhaseHasError() const;
  const std::string& LastErrorLog() const;
  const FailureInfoV2& Failure() const;

 private:
  SanityRequestV2 mRequest;
  SanityRuntimeV2* mRuntime = nullptr;
  LocalFileSystemV2 mFileSystem{};
  SanityWorkStateV2 mState{};
  ProgressStageV2 mActiveStage = ProgressStageV2::kIdle;
  std::size_t mActivePhaseIndex = 0u;
  std::size_t mActivePhaseCount = 1u;
  bool mActivePhaseNeedsMoreHeartbeats = false;
  bool mActivePhaseHasError = false;
  std::string mLastErrorLog;
};

}  // namespace peanutbutter
