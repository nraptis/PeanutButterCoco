#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../Common/BundleRequest.hpp"
#include "../Common/DecodeRequest.hpp"
#include "../Common/EngineFailure.hpp"
#include "../Common/Progress.hpp"
#include "../Common/RepairRequest.hpp"
#include "../Common/SanityRequest.hpp"
#include "Bundle/Bundle_Context.hpp"
#include "Decode/Decode_Context.hpp"
#include "Decode/Decode_Workflow.hpp"
#include "FileAccess/FileSystem.hpp"
#include "MemoryLayout/ArchiveLayoutConfig.hpp"
#include "Repair/Repair_Workflow.hpp"
#include "Sanity/Sanity_Context.hpp"
#include "Sanity/Sanity_Workflow.hpp"

namespace peanutbutter {

enum class TaskDispositionV2 {
  kRunning = 0,
  kCompleted = 1,
  kFailed = 2,
  kCanceled = 3,
};

struct EngineTaskTerminalSnapshotV2 {
  std::string mFailureMessage;
  FailureInfoV2 mFailure{};
  std::shared_ptr<BundleWorkStateV2> mBundleState;
  std::shared_ptr<DecodeWorkStateV2> mDecodeState;
  std::shared_ptr<SanityWorkStateV2> mSanityState;
};

class EngineTaskV2 {
 public:
  virtual ~EngineTaskV2() = default;

  virtual TaskDispositionV2 Heartbeat() = 0;
  virtual TaskDispositionV2 Disposition() const = 0;
  virtual const std::string& FailureMessage() const = 0;
  virtual EngineTaskTerminalSnapshotV2 BuildTerminalSnapshot() const = 0;
};

class BundleTaskV2 final : public EngineTaskV2 {
 public:
  BundleTaskV2(const BundleRequestV2& pRequest,
               BundleRuntimeV2* pRuntime,
               FileSystemV2* pFileSystem = nullptr,
               const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

  TaskDispositionV2 Heartbeat() override;
  TaskDispositionV2 Disposition() const override;
  const std::string& FailureMessage() const override;
  EngineTaskTerminalSnapshotV2 BuildTerminalSnapshot() const override;
  const BundleWorkStateV2& State() const;

 private:
  using PhaseRunner = bool (*)(BundleStageContextV2&);

  struct PhaseEntry {
    ProgressStageV2 mStage = ProgressStageV2::kIdle;
    PhaseRunner mRun = nullptr;
  };

  void BuildPhaseList();
  bool RunCurrentPhase();
  bool ShouldDeferCancelForCurrentPhase() const;
  std::size_t FindPhaseIndex(ProgressStageV2 pStage) const;
  void MarkFailed();

  BundleStageContextV2 mContext;
  std::vector<PhaseEntry> mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  TaskDispositionV2 mDisposition = TaskDispositionV2::kRunning;
  std::string mFailureMessage;
};

class DecodeTaskV2 final : public EngineTaskV2 {
 public:
  DecodeTaskV2(const DecodeRequestV2& pRequest,
               DecodeRuntimeV2* pRuntime,
               FileSystemV2* pFileSystem = nullptr,
               const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

  TaskDispositionV2 Heartbeat() override;
  TaskDispositionV2 Disposition() const override;
  const std::string& FailureMessage() const override;
  EngineTaskTerminalSnapshotV2 BuildTerminalSnapshot() const override;
  const DecodeWorkStateV2& State() const;

 private:
  void BuildPhaseList();
  bool RunCurrentPhase();
  bool ShouldDeferCancelForCurrentPhase() const;
  std::size_t FindPhaseIndex(ProgressStageV2 pStage) const;
  void MarkFailed();

  DecodeStageContextV2 mContext;
  decode_workflow::DecodePhaseListViewV2 mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  TaskDispositionV2 mDisposition = TaskDispositionV2::kRunning;
  std::string mFailureMessage;
};

class ManifestTaskV2 final : public EngineTaskV2 {
 public:
  ManifestTaskV2(const DecodeRequestV2& pRequest,
                 DecodeRuntimeV2* pRuntime,
                 FileSystemV2* pFileSystem = nullptr,
                 const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

  TaskDispositionV2 Heartbeat() override;
  TaskDispositionV2 Disposition() const override;
  const std::string& FailureMessage() const override;
  EngineTaskTerminalSnapshotV2 BuildTerminalSnapshot() const override;
  const DecodeWorkStateV2& State() const;

 private:
  void BuildPhaseList();
  bool RunCurrentPhase();
  void MarkFailed();

  DecodeStageContextV2 mContext;
  decode_workflow::DecodePhaseListViewV2 mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  TaskDispositionV2 mDisposition = TaskDispositionV2::kRunning;
  std::string mFailureMessage;
};

class RepairTaskV2 final : public EngineTaskV2 {
 public:
  RepairTaskV2(const RepairRequestV2& pRequest,
               DecodeRuntimeV2* pRuntime,
               FileSystemV2* pFileSystem = nullptr,
               const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

  TaskDispositionV2 Heartbeat() override;
  TaskDispositionV2 Disposition() const override;
  const std::string& FailureMessage() const override;
  EngineTaskTerminalSnapshotV2 BuildTerminalSnapshot() const override;
  const DecodeWorkStateV2& State() const;

 private:
  void BuildPhaseList();
  bool RunCurrentPhase();
  bool ShouldDeferCancelForCurrentPhase() const;
  std::size_t FindPhaseIndex(ProgressStageV2 pStage) const;
  void MarkFailed();

  DecodeStageContextV2 mContext;
  repair_workflow::RepairPhaseListViewV2 mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  TaskDispositionV2 mDisposition = TaskDispositionV2::kRunning;
  std::string mFailureMessage;
};

class SanityTaskV2 final : public EngineTaskV2 {
 public:
  SanityTaskV2(const SanityRequestV2& pRequest,
               SanityRuntimeV2* pRuntime);

  TaskDispositionV2 Heartbeat() override;
  TaskDispositionV2 Disposition() const override;
  const std::string& FailureMessage() const override;
  EngineTaskTerminalSnapshotV2 BuildTerminalSnapshot() const override;
  const SanityWorkStateV2& State() const;

 private:
  void BuildPhaseList();
  bool RunCurrentPhase();
  void MarkFailed();

  SanityStageContextV2 mContext;
  sanity_workflow::SanityPhaseListViewV2 mPhases;
  std::size_t mCurrentPhaseIndex = 0u;
  TaskDispositionV2 mDisposition = TaskDispositionV2::kRunning;
  std::string mFailureMessage;
};

}  // namespace peanutbutter
