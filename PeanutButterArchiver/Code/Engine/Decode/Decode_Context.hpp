#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../Common/DecodeRequest.hpp"
#include "../../Common/Logging.hpp"
#include "../../Common/Progress.hpp"
#include "../Crypto/RotationMaskCipher.hpp"
#include "../FileAccess/LocalFileSystem.hpp"
#include "../MemoryLayout/ArchiveHeader.hpp"

namespace peanutbutter {

enum class DecodeModeV2 {
  kOptimistic = 0,
  kPessimistic = 1,
};

struct DiscoveredArchiveFileV2 {
  std::string mPath;
  std::uint64_t mFileLength = 0u;
  std::uint64_t mReadableBlockCount = 0u;
  memory_layout::ArchiveHeaderV2 mHeader{};
};

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
};

struct DecodeOutputStateV2 {
  std::uint64_t mFilesWritten = 0u;
  std::uint64_t mFoldersCreated = 0u;
  std::uint64_t mBytesWritten = 0u;
  bool mArchiveTerminated = false;
};

struct DecodeWorkStateV2 {
  DecodeBootstrapStateV2 mBootstrap{};
  DecodeDiscoveryStateV2 mDiscovery{};
  DecodeManifestStateV2 mManifest{};
  DecodeCipherStateV2 mCipher{};
  DecodeOutputStateV2 mOutput{};
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
};

class DecodeStageContextV2 {
 public:
  DecodeStageContextV2(const DecodeRequestV2& pRequest,
                       DecodeRuntimeV2* pRuntime);

  const DecodeRequestV2& Request() const;
  DecodeWorkStateV2& State();
  const DecodeWorkStateV2& State() const;
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

 private:
  DecodeRequestV2 mRequest;
  DecodeRuntimeV2* mRuntime = nullptr;
  LocalFileSystemV2 mFileSystem{};
  DecodeWorkStateV2 mState{};
  ProgressStageV2 mActiveStage = ProgressStageV2::kIdle;
  std::size_t mActivePhaseIndex = 0u;
  std::size_t mActivePhaseCount = 1u;
};

}  // namespace peanutbutter
