#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../Common/BundleRequest.hpp"
#include "../../Common/Logging.hpp"
#include "../../Common/Progress.hpp"
#include "../Crypto/RotationMaskCipher.hpp"
#include "../FileAccess/LocalFileSystem.hpp"

namespace peanutbutter {

struct BundleRecordEntryV2 {
  std::string mSourcePath;
  std::string mRelativePath;
  std::uint64_t mContentLength = 0u;
  bool mIsDirectory = false;
};

struct PlannedArchiveFileV2 {
  std::string mPath;
  std::uint64_t mArchiveIndex = 0u;
  std::uint64_t mFamilyBlockStart = 0u;
  std::uint32_t mBlockCount = 0u;
};

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
  std::uint64_t mRepairSectorBlockCount = 0u;
  std::uint64_t mTotalFamilyBlockCount = 0u;
  std::uint64_t mArchiveCount = 0u;
  std::uint64_t mArchiveFamilyId = 0u;
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
};

struct BundlePackingStateV2 {
  std::vector<std::string> mArchivePaths;
  std::uint64_t mArchivePackedBlockCount = 0u;
  std::uint64_t mRepairPackedBlockCount = 0u;
};

struct BundleFinalizeStateV2 {
  bool mHeadersFinalized = false;
};

struct BundleWorkStateV2 {
  BundleDiscoveryStateV2 mDiscovery{};
  BundleMemoryPlanV2 mMemoryPlan{};
  BundleManifestStateV2 mManifest{};
  BundleCipherStateV2 mCipher{};
  BundlePackingStateV2 mPacking{};
  BundleFinalizeStateV2 mFinalize{};
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
};

class BundleStageContextV2 {
 public:
  BundleStageContextV2(const BundleRequestV2& pRequest,
                       BundleRuntimeV2* pRuntime);

  const BundleRequestV2& Request() const;
  BundleWorkStateV2& State();
  const BundleWorkStateV2& State() const;
  FileSystemV2& FileSystem();
  const FileSystemV2& FileSystem() const;

  bool IsCancelRequested() const;
  void EmitLog(LogLevelV2 pLevel, const std::string& pMessage) const;
  void SetActivePhase(ProgressStageV2 pStage,
                      std::size_t pPhaseIndex,
                      std::size_t pPhaseCount);
  void EmitPhaseProgress(double pLocalFraction,
                         const std::string& pLabel) const;
 void EmitProgress(ProgressStageV2 pStage,
                    double pLocalFraction,
                    double pOverallFraction,
                    const std::string& pLabel) const;
  bool ActivePhaseHasError() const;
  const std::string& LastErrorLog() const;

 private:
  BundleRequestV2 mRequest;
  BundleRuntimeV2* mRuntime = nullptr;
  LocalFileSystemV2 mFileSystem{};
  BundleWorkStateV2 mState{};
  ProgressStageV2 mActiveStage = ProgressStageV2::kIdle;
  std::size_t mActivePhaseIndex = 0u;
  std::size_t mActivePhaseCount = 1u;
  bool mActivePhaseHasError = false;
  std::string mLastErrorLog;
};

}  // namespace peanutbutter
