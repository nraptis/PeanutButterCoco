#pragma once

#include <cstddef>
#include <vector>

#include "../../Common/DecodeRequest.hpp"
#include "../../Common/RepairRequest.hpp"
#include "Decode_Context.hpp"

namespace peanutbutter {

class DecodeDirector final {
 public:
  DecodeDirector(const DecodeRequestV2& pRequest,
                 DecodeRuntimeV2* pRuntime,
                 FileSystemV2* pFileSystem = nullptr,
                 const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

  bool Step();
  bool IsFinished() const;
  bool HasFailed() const;
  bool WasCanceled() const;
  const DecodeWorkStateV2& State() const;

 private:
  using PhaseRunner = bool (*)(DecodeStageContextV2&);

  struct PhaseEntry {
    ProgressStageV2 mStage = ProgressStageV2::kIdle;
    PhaseRunner mRun = nullptr;
  };

  void BuildPhaseList();
  bool RunCurrentPhase();
  bool ShouldDeferCancelForCurrentPhase() const;
  std::size_t FindPhaseIndex(ProgressStageV2 pStage) const;

 private:
  DecodeStageContextV2 mContext;
  std::vector<PhaseEntry> mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  bool mIsFinished = false;
  bool mHasFailed = false;
  bool mWasCanceled = false;
};

class RepairDirector final {
 public:
  RepairDirector(const RepairRequestV2& pRequest,
                 DecodeRuntimeV2* pRuntime,
                 FileSystemV2* pFileSystem = nullptr,
                 const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

  bool Step();
  bool IsFinished() const;
  bool HasFailed() const;
  bool WasCanceled() const;
  const DecodeWorkStateV2& State() const;

 private:
  using PhaseRunner = bool (*)(DecodeStageContextV2&);

  struct PhaseEntry {
    ProgressStageV2 mStage = ProgressStageV2::kIdle;
    PhaseRunner mRun = nullptr;
  };

  void BuildPhaseList();
  bool RunCurrentPhase();
  bool ShouldDeferCancelForCurrentPhase() const;
  std::size_t FindPhaseIndex(ProgressStageV2 pStage) const;

 private:
  DecodeStageContextV2 mContext;
  std::vector<PhaseEntry> mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  bool mIsFinished = false;
  bool mHasFailed = false;
  bool mWasCanceled = false;
};

class ManifestDirector final {
 public:
  ManifestDirector(const DecodeRequestV2& pRequest,
                   DecodeRuntimeV2* pRuntime,
                   FileSystemV2* pFileSystem = nullptr,
                   const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

  bool Step();
  bool IsFinished() const;
  bool HasFailed() const;
  bool WasCanceled() const;
  const DecodeWorkStateV2& State() const;

 private:
  using PhaseRunner = bool (*)(DecodeStageContextV2&);

  struct PhaseEntry {
    ProgressStageV2 mStage = ProgressStageV2::kIdle;
    PhaseRunner mRun = nullptr;
  };

  void BuildPhaseList();
  bool RunCurrentPhase();

 private:
  DecodeStageContextV2 mContext;
  std::vector<PhaseEntry> mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  bool mIsFinished = false;
  bool mHasFailed = false;
  bool mWasCanceled = false;
};

}  // namespace peanutbutter
