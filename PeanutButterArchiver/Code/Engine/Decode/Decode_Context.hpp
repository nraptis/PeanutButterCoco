#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../Common/DecodeRequest.hpp"
#include "../../Common/EngineFailure.hpp"
#include "../../Common/Logging.hpp"
#include "../../Common/Progress.hpp"
#include "../../Common/RuntimeEvent.hpp"
#include "../../Common/TransferTracking.hpp"
#include "../ArchiveBox/ArchiveBoxes.hpp"
#include "../Crypto/RotationMaskCipher.hpp"
#include "../FileAccess/LocalFileSystem.hpp"
#include "../MemoryLayout/ArchiveLayoutConfig.hpp"
#include "../MemoryLayout/ArchiveHeader.hpp"
#include "../MemoryLayout/SectionHeader.hpp"

namespace peanutbutter {

class DecodeDiscoveryCursorV2;
class DecodeHeaderBootstrapCursorV2;
class DecodeInspectionCursorV2;
class DecodeArchiveDecodeCursorV2;
class DecodeRepairApplyCursorV2;

enum class DecodeModeV2 {
  kOptimistic = 0,
  kPessimistic = 1,
};

using DiscoveredArchiveFileV2 = ArchiveBox_DecodeV2;

struct DecodeBootstrapStateV2 {
  bool mHeaderRead = false;
  std::string mSourceDirectory;
  std::string mBootstrapArchivePath;
  memory_layout::ArchiveHeaderV2 mFirstHeader{};
  std::uint64_t mExpectedArchiveCount = 0u;
  std::uint64_t mExpectedEmptyFolderBlockCount = 0u;
  std::uint64_t mExpectedPreviewManifestBlockCount = 0u;
  std::uint64_t mExpectedArchiveDataBlockCount = 0u;
  std::uint64_t mExpectedRepairBlockCount = 0u;
};

struct DecodeDiscoveryStateV2 {
  std::vector<DiscoveredArchiveFileV2> mArchives;
  DecodeModeV2 mMode = DecodeModeV2::kOptimistic;
  std::uint64_t mTotalReadableBlocks = 0u;
};

struct DecodeInspectionStateV2 {
  bool mHasValidSection = false;
  std::uint64_t mArchiveIndex = 0u;
  std::uint64_t mBlockIndex = 0u;
  memory_layout::SectionHeaderV2 mSectionHeader{};
};

struct DecodeManifestStateV2 {
  std::uint64_t mEmptyFolderBlocksProcessed = 0u;
  std::uint64_t mPreviewManifestBlocksProcessed = 0u;
  std::uint64_t mArchiveDataBlocksProcessed = 0u;
  std::uint64_t mRepairBlocksProcessed = 0u;
};

struct DecodeCipherStateV2 {
  bool mDerived = false;
  bool mAssembled = false;
  RotationMaskCipherV2 mCipher{};
  FixedBlockBufferV2 mWorkerBuffer{};
};

struct DecodeOutputStateV2 {
  std::uint64_t mFilesWritten = 0u;
  std::uint64_t mFoldersCreated = 0u;
  std::uint64_t mBytesWritten = 0u;
  bool mArchiveTerminated = false;
};

struct DecodeRepairStateV2 {
  std::uint64_t mArchivesCompleted = 0u;
  std::uint64_t mArchivesTotal = 0u;
  std::uint64_t mArchivesSynthesized = 0u;
  std::uint64_t mArchivesExpanded = 0u;
  std::uint64_t mRepairableBlocks = 0u;
  std::uint64_t mPatchedBlocks = 0u;
  std::uint64_t mRepairableBytes = 0u;
  std::uint64_t mPatchedBytes = 0u;
  std::uint64_t mRepairBlocksApplied = 0u;
  std::uint64_t mRepairBlocksSkippedValidTarget = 0u;
};

struct DecodeCancelStateV2 {
  bool mObserved = false;
  bool mShouldFinalizeAfterCancel = false;
  std::string mCancelFileReference;
};

struct DecodeCursorStateV2 {
  std::shared_ptr<DecodeDiscoveryCursorV2> mDiscovery;
  std::shared_ptr<DecodeHeaderBootstrapCursorV2> mHeaderBootstrap;
  std::shared_ptr<DecodeInspectionCursorV2> mInspection;
  std::shared_ptr<DecodeArchiveDecodeCursorV2> mArchiveDecode;
  std::shared_ptr<DecodeRepairApplyCursorV2> mRepairApply;
};

struct DecodeWorkStateV2 {
  DecodeBootstrapStateV2 mBootstrap{};
  DecodeDiscoveryStateV2 mDiscovery{};
  DecodeInspectionStateV2 mInspection{};
  DecodeManifestStateV2 mManifest{};
  DecodeCipherStateV2 mCipher{};
  DecodeOutputStateV2 mOutput{};
  DecodeRepairStateV2 mRepair{};
  DecodeCancelStateV2 mCancel{};
  TransferTrackingStateV2 mTransfers{};
  DecodeCursorStateV2 mCursor{};
  std::uint64_t mWorkUnitsProcessed = 0u;
  FailureInfoV2 mFailure{};
};

class DecodeRuntimeV2 {
 public:
  virtual ~DecodeRuntimeV2() = default;
  virtual bool IsCancelRequested() const = 0;
  virtual void EmitLog(LogLevelV2 pLevel, const std::string& pMessage) = 0;
  virtual void EmitProgress(ProgressStageV2 pStage,
                            double pLocalFraction,
                            double pOverallFraction,
                            const std::string& pLabel) = 0;
  virtual bool WantsRuntimeEvent(RuntimeEventKindV2 pKind) const {
    (void)pKind;
    return true;
  }
  virtual bool EmitRuntimeEvent(const RuntimeEventV2& pEvent) = 0;
};

class DecodeStageContextV2 {
 public:
  DecodeStageContextV2(const DecodeRequestV2& pRequest,
                       DecodeRuntimeV2* pRuntime,
                       FileSystemV2* pFileSystem = nullptr,
                       const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

  const DecodeRequestV2& Request() const;
  DecodeWorkStateV2& State();
  const DecodeWorkStateV2& State() const;
  FileSystemV2& FileSystem();
  const FileSystemV2& FileSystem() const;
  const memory_layout::ArchiveLayoutConfigV2& Layout() const;

  bool IsCancelRequested() const;
  void EmitLog(LogLevelV2 pLevel, const std::string& pMessage) const;
  bool WantsRuntimeEvent(RuntimeEventKindV2 pKind) const;
  bool EmitRuntimeEvent(const RuntimeEventV2& pEvent) const;
  void SetActivePhase(ProgressStageV2 pStage,
                      std::size_t pPhaseIndex,
                      std::size_t pPhaseCount);
  void BeginWorkUnit();
  void ContinuePhaseOnNextHeartbeat();
  void ClearPhaseContinuationRequest();
  bool ActivePhaseNeedsMoreHeartbeats() const;
  void RequestBatchYield();
  bool ActivePhaseBatchYieldRequested() const;
  void EmitPhaseProgress(double pLocalFraction,
                         const std::string& pLabel) const;
  void EmitProgress(ProgressStageV2 pStage,
                    double pLocalFraction,
                    double pOverallFraction,
                    const std::string& pLabel) const;
  bool ActivePhaseHasError() const;
  const std::string& LastErrorLog() const;
  const FailureInfoV2& Failure() const;

 private:
  DecodeRequestV2 mRequest;
  DecodeRuntimeV2* mRuntime = nullptr;
  LocalFileSystemV2 mLocalFileSystem{};
  FileSystemV2* mFileSystem = nullptr;
  const memory_layout::ArchiveLayoutConfigV2* mLayout = nullptr;
  DecodeWorkStateV2 mState{};
  ProgressStageV2 mActiveStage = ProgressStageV2::kIdle;
  std::size_t mActivePhaseIndex = 0u;
  std::size_t mActivePhaseCount = 1u;
  bool mActivePhaseNeedsMoreHeartbeats = false;
  bool mActivePhaseBatchYieldRequested = false;
  bool mActivePhaseHasError = false;
  std::string mLastErrorLog;
};

}  // namespace peanutbutter
