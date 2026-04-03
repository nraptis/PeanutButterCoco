#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../Common/BundleRequest.hpp"
#include "../../Common/EngineFailure.hpp"
#include "../../Common/Logging.hpp"
#include "../../Common/Progress.hpp"
#include "../../Common/RuntimeEvent.hpp"
#include "../../Common/TransferTracking.hpp"
#include "../ArchiveBox/ArchiveBoxes.hpp"
#include "../Crypto/RotationMaskCipher.hpp"
#include "../FileAccess/LocalFileSystem.hpp"
#include "../MemoryLayout/ArchiveLayoutConfig.hpp"

namespace peanutbutter {

class BundleDiscoveryCursorV2;
class BundleArchivePackingCursorV2;
class BundleRepairPackingCursorV2;
class BundleFinalizingHeadersCursorV2;

struct BundleRecordEntryV2 {
  std::string mSourcePath;
  std::string mRelativePath;
  std::uint64_t mContentLength = 0u;
  bool mIsDirectory = false;
  bool mIsReference = false;
  std::uint8_t mReferenceKind = 1u;
  std::string mReferenceTargetRelativePath;
};

using PlannedArchiveFileV2 = ArchiveBox_BundleV2;

struct BundleDiscoveryStateV2 {
  bool mSourceExists = false;
  std::string mSourceStem;
  std::uint64_t mFileCount = 0u;
  std::uint64_t mEmptyFolderCount = 0u;
  std::uint64_t mTotalSourceBytes = 0u;
  std::vector<BundleRecordEntryV2> mFileRecords;
  std::vector<BundleRecordEntryV2> mEmptyFolderRecords;
};

struct BundleMemoryPlanV2 {
  std::uint64_t mArchiveDataLogicalBytes = 0u;
  std::uint64_t mEmptyFolderLogicalBytes = 0u;
  std::uint64_t mArchiveDataBlockCount = 0u;
  std::uint64_t mEmptyFolderBlockCount = 0u;
  std::uint64_t mPreviewManifestBlockCount = 0u;
  std::uint64_t mNonRepairFamilyBlockCount = 0u;
  std::uint64_t mRepairSectorBlockCount = 0u;
  std::uint64_t mTotalFamilyBlockCount = 0u;
  std::uint64_t mArchiveCount = 0u;
  std::uint64_t mArchiveFamilyId = 0u;
  std::uint8_t mFileCountMod256 = 0u;
  std::uint8_t mFolderCountMod256 = 0u;
  std::vector<std::uint32_t> mSourceArchiveBlockCounts;
  std::vector<std::uint32_t> mRepairCopyBlockCounts;
  std::vector<std::vector<std::uint32_t>> mRepairCopySourceLocalBlocks;
  std::vector<PlannedArchiveFileV2> mArchives;
};

struct BundleManifestStateV2 {
  std::string mPreviewManifestPayload;
  std::uint64_t mPreviewManifestBytes = 0u;
  std::uint64_t mPreviewManifestBlockCount = 0u;
  std::uint64_t mFolderPackingBytes = 0u;
  std::uint64_t mFolderPackingBlockCount = 0u;
};

struct BundleCipherStateV2 {
  bool mDerived = false;
  bool mAssembled = false;
  RotationMaskCipherV2 mCipher{};
  FixedBlockBufferV2 mWorkerBuffer{};
};

struct BundlePackingStateV2 {
  std::vector<std::string> mArchivePaths;
  std::uint64_t mArchivePackedBlockCount = 0u;
  std::uint64_t mRepairPackedBlockCount = 0u;
};

struct BundleCancelStateV2 {
  bool mObserved = false;
  bool mShouldFinalizeAfterCancel = false;
  std::string mCancelFileReference;
};

struct BundleFinalizeStateV2 {
  bool mHeadersFinalized = false;
};

struct BundleCursorStateV2 {
  std::shared_ptr<BundleDiscoveryCursorV2> mDiscovery;
  std::shared_ptr<BundleArchivePackingCursorV2> mArchivePacking;
  std::shared_ptr<BundleRepairPackingCursorV2> mRepairPacking;
  std::shared_ptr<BundleFinalizingHeadersCursorV2> mFinalizingHeaders;
};

struct BundleWorkStateV2 {
  BundleDiscoveryStateV2 mDiscovery{};
  BundleMemoryPlanV2 mMemoryPlan{};
  BundleManifestStateV2 mManifest{};
  BundleCipherStateV2 mCipher{};
  BundlePackingStateV2 mPacking{};
  BundleCancelStateV2 mCancel{};
  TransferTrackingStateV2 mTransfers{};
  BundleFinalizeStateV2 mFinalize{};
  BundleCursorStateV2 mCursor{};
  std::uint64_t mWorkUnitsProcessed = 0u;
  FailureInfoV2 mFailure{};
};

class BundleRuntimeV2 {
 public:
  virtual ~BundleRuntimeV2() = default;
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

class BundleStageContextV2 {
 public:
  BundleStageContextV2(const BundleRequestV2& pRequest,
                       BundleRuntimeV2* pRuntime,
                       FileSystemV2* pFileSystem = nullptr,
                       const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

  const BundleRequestV2& Request() const;
  BundleWorkStateV2& State();
  const BundleWorkStateV2& State() const;
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
  bool ActivePhaseNeedsMoreHeartbeats() const;
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
  BundleRequestV2 mRequest;
  BundleRuntimeV2* mRuntime = nullptr;
  LocalFileSystemV2 mLocalFileSystem{};
  FileSystemV2* mFileSystem = nullptr;
  const memory_layout::ArchiveLayoutConfigV2* mLayout = nullptr;
  BundleWorkStateV2 mState{};
  ProgressStageV2 mActiveStage = ProgressStageV2::kIdle;
  std::size_t mActivePhaseIndex = 0u;
  std::size_t mActivePhaseCount = 1u;
  bool mActivePhaseNeedsMoreHeartbeats = false;
  bool mActivePhaseHasError = false;
  std::string mLastErrorLog;
};

}  // namespace peanutbutter
