#import <XCTest/XCTest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../PeanutButterArchiver/Code/Common/BundleRequest.hpp"
#include "../PeanutButterArchiver/Code/Common/DecodeRequest.hpp"
#include "../PeanutButterArchiver/Code/Common/RepairRequest.hpp"
#include "../PeanutButterArchiver/Code/Engine/Bundle/Bundle_Execution.hpp"
#include "../PeanutButterArchiver/Code/Engine/Decode/Decode_Execution.hpp"
#include "../PeanutButterArchiver/Code/Engine/FileAccess/LocalFileSystem.hpp"
#include "../PeanutButterArchiver/Code/Engine/MemoryLayout/ArchiveHeader.hpp"
#include "../PeanutButterArchiver/Code/Engine/MemoryLayout/ArchiveLayoutConfig.hpp"
#include "../PeanutButterArchiver/Code/Engine/MemoryLayout/FormatUtilities.hpp"
#include "../PeanutButterArchiver/Code/Knobs.hpp"
#include "[mocks]/MockFileSystem.hpp"
#include "[mocks]/MockHardDrive.hpp"

namespace {

using peanutbutter::BundleExecutionResultV2;
using peanutbutter::BundleRequestV2;
using peanutbutter::DecodeExecutionResultV2;
using peanutbutter::DecodeIntentV2;
using peanutbutter::DecodeRequestV2;
using peanutbutter::LocalFileSystemV2;
using peanutbutter::RepairCoveragePresetV2;
using peanutbutter::RepairRequestV2;
using peanutbutter::RuntimeEventKindV2;
using peanutbutter::RuntimeEventV2;
using peanutbutter::StrengthPresetV2;

class DemoRuntime final : public peanutbutter::BundleRuntimeV2,
                          public peanutbutter::DecodeRuntimeV2 {
 public:
  bool IsCancelRequested() const override { return false; }

  void EmitLog(peanutbutter::LogLevelV2 pLevel,
               const std::string& pMessage) override {
    (void)pLevel;
    mLogs.push_back(pMessage);
  }

  void EmitProgress(peanutbutter::ProgressStageV2 pStage,
                    double pLocalFraction,
                    double pOverallFraction,
                    const std::string& pLabel) override {
    (void)pStage;
    (void)pLocalFraction;
    (void)pOverallFraction;
    (void)pLabel;
  }

  bool WantsRuntimeEvent(RuntimeEventKindV2 pKind) const override {
    (void)pKind;
    return true;
  }

  bool EmitRuntimeEvent(const RuntimeEventV2& pEvent) override {
    mEvents.push_back(pEvent);
    return true;
  }

  bool SawEvent(RuntimeEventKindV2 pKind) const {
    for (const RuntimeEventV2& aEvent : mEvents) {
      if (aEvent.mKind == pKind) {
        return true;
      }
    }
    return false;
  }

  std::vector<RuntimeEventV2> mEvents;
  std::vector<std::string> mLogs;
};

class QuietBundleRuntime final : public peanutbutter::BundleRuntimeV2 {
 public:
  bool IsCancelRequested() const override { return false; }

  void EmitLog(peanutbutter::LogLevelV2 pLevel,
               const std::string& pMessage) override {
    if (pLevel == peanutbutter::LogLevelV2::kError) {
      mErrors.push_back(pMessage);
    }
  }

  void EmitProgress(peanutbutter::ProgressStageV2 pStage,
                    double pLocalFraction,
                    double pOverallFraction,
                    const std::string& pLabel) override {
    (void)pStage;
    (void)pLocalFraction;
    (void)pOverallFraction;
    (void)pLabel;
  }

  bool WantsRuntimeEvent(RuntimeEventKindV2 pKind) const override {
    (void)pKind;
    return false;
  }

  bool EmitRuntimeEvent(const RuntimeEventV2& pEvent) override {
    (void)pEvent;
    return true;
  }

  std::vector<std::string> mErrors;
};

class KnobGuard final {
 public:
  KnobGuard()
      : mArchiveBlockBytes(peanutbutter::knobs::kArchiveBlockBytesV2),
        mPayloadBytes(peanutbutter::knobs::kSectionPayloadBytesV2),
        mBundleBatch(peanutbutter::knobs::kBatchSizeBundleV2),
        mDecodeBatch(peanutbutter::knobs::kBatchSizeDecodeV2),
        mBundleRepairBatch(peanutbutter::knobs::kBatchSizeBundleRepairV2),
        mRepairBatch(peanutbutter::knobs::kBatchSizeRepairV2) {}

  ~KnobGuard() {
    peanutbutter::knobs::kArchiveBlockBytesV2 = mArchiveBlockBytes;
    peanutbutter::knobs::kSectionPayloadBytesV2 = mPayloadBytes;
    peanutbutter::knobs::kBatchSizeBundleV2 = mBundleBatch;
    peanutbutter::knobs::kBatchSizeDecodeV2 = mDecodeBatch;
    peanutbutter::knobs::kBatchSizeBundleRepairV2 = mBundleRepairBatch;
    peanutbutter::knobs::kBatchSizeRepairV2 = mRepairBatch;
  }

 private:
  std::size_t mArchiveBlockBytes = 0u;
  std::size_t mPayloadBytes = 0u;
  std::uint32_t mBundleBatch = 1u;
  std::uint32_t mDecodeBatch = 1u;
  std::uint32_t mBundleRepairBatch = 1u;
  std::uint32_t mRepairBatch = 1u;
};

std::string MakePatternedPayload(std::size_t pByteCount, char pSeed) {
  std::string aPayload;
  aPayload.reserve(pByteCount);
  const unsigned char aOffset = static_cast<unsigned char>(pSeed);
  for (std::size_t aIndex = 0u; aIndex < pByteCount; ++aIndex) {
    const unsigned char aChar =
        static_cast<unsigned char>('A' + ((aIndex + aOffset) % 26u));
    aPayload.push_back(static_cast<char>(aChar));
  }
  return aPayload;
}

bool WriteText(MockHardDrive& pDrive,
               const std::string& pPath,
               const std::string& pText) {
  if (!pDrive.ClearFileBytes(pPath)) {
    return false;
  }
  return pDrive.AppendFileBytes(
      pPath,
      reinterpret_cast<const unsigned char*>(pText.data()),
      pText.size());
}

std::string ReadText(const MockHardDrive& pDrive, const std::string& pPath) {
  const std::string aNormalized = pDrive.Normalize(pPath);
  const auto aIt = pDrive.mFiles.find(aNormalized);
  if (aIt == pDrive.mFiles.end()) {
    return {};
  }
  return std::string(aIt->second.begin(), aIt->second.end());
}

std::string JoinMessages(const std::vector<std::string>& pMessages) {
  std::string aJoined;
  for (std::size_t aIndex = 0u; aIndex < pMessages.size(); ++aIndex) {
    if (aIndex > 0u) {
      aJoined += " | ";
    }
    aJoined += pMessages[aIndex];
  }
  return aJoined;
}

bool MessagesContain(const std::vector<std::string>& pMessages,
                     const std::string& pNeedle) {
  for (const std::string& aMessage : pMessages) {
    if (aMessage.find(pNeedle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool WritePatternFile(const std::filesystem::path& pPath,
                      std::size_t pBytes,
                      std::uint8_t pSeed) {
  std::ofstream aStream(pPath, std::ios::binary);
  if (!aStream.is_open()) {
    return false;
  }

  constexpr std::size_t kChunkBytes = 64u * 1024u;
  std::vector<unsigned char> aChunk(kChunkBytes);
  std::size_t aWritten = 0u;
  while (aWritten < pBytes) {
    const std::size_t aChunkBytes =
        std::min<std::size_t>(kChunkBytes, pBytes - aWritten);
    for (std::size_t aIndex = 0u; aIndex < aChunkBytes; ++aIndex) {
      aChunk[aIndex] = static_cast<unsigned char>((aIndex + aWritten + pSeed) % 251u);
    }
    aStream.write(reinterpret_cast<const char*>(aChunk.data()),
                  static_cast<std::streamsize>(aChunkBytes));
    if (!aStream.good()) {
      return false;
    }
    aWritten += aChunkBytes;
  }
  return true;
}

bool WriteTextFilePath(const std::filesystem::path& pPath,
                       const std::string& pText) {
  std::ofstream aStream(pPath, std::ios::binary);
  if (!aStream.is_open()) {
    return false;
  }
  aStream.write(pText.data(), static_cast<std::streamsize>(pText.size()));
  return aStream.good();
}

std::string ReadTextFilePath(const std::filesystem::path& pPath) {
  std::ifstream aStream(pPath, std::ios::binary);
  if (!aStream.is_open()) {
    return {};
  }
  std::string aContents;
  aStream.seekg(0, std::ios::end);
  const std::streamoff aLength = aStream.tellg();
  if (aLength < 0) {
    return {};
  }
  aContents.resize(static_cast<std::size_t>(aLength));
  aStream.seekg(0, std::ios::beg);
  aStream.read(aContents.data(), aLength);
  if (!aStream.good() && !aStream.eof()) {
    return {};
  }
  return aContents;
}

void TunePayloadAndBatchKnobs(std::size_t pPayloadBytes,
                              std::uint32_t pBundleBatch,
                              std::uint32_t pDecodeBatch,
                              std::uint32_t pBundleRepairBatch,
                              std::uint32_t pRepairBatch) {
  peanutbutter::knobs::SetPayloadSizePerBlockV2(pPayloadBytes);
  peanutbutter::knobs::SetBatchSizesForTestingV2(
      pBundleBatch,
      pDecodeBatch,
      pBundleRepairBatch,
      pRepairBatch);
}

peanutbutter::memory_layout::ArchiveLayoutConfigV2 MakeLayout(
    std::size_t pPayloadBytes,
    std::uint32_t pMaxBlocksPerArchive) {
  peanutbutter::memory_layout::ArchiveLayoutConfigV2 aLayout =
      peanutbutter::memory_layout::DefaultArchiveLayoutConfigV2();
  aLayout.SetPayloadSizePerBlock(pPayloadBytes);
  aLayout.mMaxBlocksPerArchive = pMaxBlocksPerArchive;
  return aLayout;
}

std::map<std::string, std::string> SeedSourceTree(MockHardDrive& pDrive,
                                                  const std::string& pRoot,
                                                  std::size_t pPayloadBytes) {
  pDrive.EnsureDirectory(pRoot);
  pDrive.EnsureDirectory(pRoot + "/nested");
  pDrive.EnsureDirectory(pRoot + "/nested/deeper");
  pDrive.EnsureDirectory(pRoot + "/empty_leaf");

  std::map<std::string, std::string> aExpected;
  aExpected["top_alpha.txt"] = MakePatternedPayload(pPayloadBytes * 5u + 11u, 'a');
  aExpected["nested/beta.bin"] =
      MakePatternedPayload(pPayloadBytes * 3u + 7u, 'b');
  aExpected["nested/deeper/gamma.log"] =
      MakePatternedPayload(pPayloadBytes * 2u + 17u, 'c');

  for (const auto& aPair : aExpected) {
    const std::string aPath = pRoot + "/" + aPair.first;
    (void)WriteText(pDrive, aPath, aPair.second);
  }

  return aExpected;
}

BundleExecutionResultV2 RunBundleNoEncryption(MockFileSystem& pFileSystem,
                                              DemoRuntime& pRuntime,
                                              const std::string& pSourceDirectory,
                                              const std::string& pDestinationDirectory,
                                              const std::string& pFilePrefix,
                                              std::uint32_t pBlocksPerArchive,
                                              std::uint64_t pCancelFinishBlocks,
                                              bool pSafeModeEnabled,
                                              bool pIncludePreviewManifest,
                                              RepairCoveragePresetV2 pRepairCoverage,
                                              const peanutbutter::memory_layout::ArchiveLayoutConfigV2& pLayout) {
  BundleRequestV2 aRequest;
  aRequest.mSourceDirectory = pSourceDirectory;
  aRequest.mDestinationDirectory = pDestinationDirectory;
  aRequest.mClearDestinationBeforeWrite = true;
  aRequest.mEncryptionEnabled = false;
  aRequest.mEncryptionStrength = StrengthPresetV2::kLow;
  aRequest.mTableStrength = StrengthPresetV2::kMedium;
  aRequest.mRepairEnabled = true;
  aRequest.mRepairCoverage = pRepairCoverage;
  aRequest.mIncludePreviewManifest = pIncludePreviewManifest;
  aRequest.mSafeModeEnabled = pSafeModeEnabled;
  aRequest.mBlockCount = pBlocksPerArchive;
  aRequest.mCancelFinishBlocks = pCancelFinishBlocks;
  aRequest.mFilePrefix = pFilePrefix;
  aRequest.mPassword = "ignored-because-plaintext";

  return peanutbutter::ExecuteBundleV2(
      aRequest,
      &pRuntime,
      &pFileSystem,
      &pLayout);
}

DecodeExecutionResultV2 RunDecodeNoEncryption(
    MockFileSystem& pFileSystem,
    DemoRuntime& pRuntime,
    const std::string& pSourcePath,
    const std::string& pDestinationDirectory,
    DecodeIntentV2 pIntent,
    std::uint64_t pCancelFinishBlocks,
    bool pAggressive,
    const peanutbutter::memory_layout::ArchiveLayoutConfigV2& pLayout) {
  DecodeRequestV2 aRequest;
  aRequest.mSourcePath = pSourcePath;
  aRequest.mDestinationDirectory = pDestinationDirectory;
  aRequest.mClearDestinationBeforeWrite = true;
  aRequest.mEncryptionEnabled = false;
  aRequest.mAggressive = pAggressive;
  aRequest.mCancelFinishBlocks = pCancelFinishBlocks;
  aRequest.mPassword = "ignored-because-plaintext";
  aRequest.mIntent = pIntent;

  return peanutbutter::ExecuteDecodeV2(
      aRequest,
      &pRuntime,
      &pFileSystem,
      &pLayout);
}

}  // namespace

@interface DemoHomeGrownTests : XCTestCase
@end

@implementation DemoHomeGrownTests

- (void)testDemoBundleTweaksNoEncryption {
  KnobGuard aGuard;
  TunePayloadAndBatchKnobs(
      /*payload bytes*/ 193u,
      /*bundle batch*/ 3u,
      /*decode batch*/ 2u,
      /*bundle repair batch*/ 4u,
      /*repair batch*/ 5u);
  const auto aLayout = MakeLayout(/*payload bytes*/ 193u,
                                  /*max blocks per archive*/ 96u);

  MockHardDrive aDrive;
  MockFileSystem aFileSystem(&aDrive);
  const std::string aSource = "/root/demo_bundle_src";
  const std::string aDestination = "/root/demo_bundle_archives";
  (void)SeedSourceTree(aDrive, aSource, 193u);

  DemoRuntime aRuntime;
  const BundleExecutionResultV2 aBundle = RunBundleNoEncryption(
      aFileSystem,
      aRuntime,
      aSource,
      aDestination,
      "demo_bundle_",
      /*blocks per archive*/ 7u,
      /*cancel finish blocks*/ 2u,
      /*safe mode*/ false,
      /*include preview*/ true,
      RepairCoveragePresetV2::k40,
      aLayout);

  XCTAssertTrue(aBundle.Succeeded(), @"Bundle failed: %s", aBundle.mFailureMessage.c_str());
  XCTAssertTrue(aRuntime.SawEvent(RuntimeEventKindV2::kBundleArchiveStarted));
  XCTAssertFalse(aBundle.ArchivePaths().empty());

  const std::string aFirstArchivePath = aBundle.ArchivePaths().front();
  const std::string aFirstArchiveNormalized = aDrive.Normalize(aFirstArchivePath);
  const auto aIt = aDrive.mFiles.find(aFirstArchiveNormalized);
  XCTAssertTrue(aIt != aDrive.mFiles.end());
  XCTAssertGreaterThan(aIt->second.size(), peanutbutter::memory_layout::kArchiveHeaderBytesV2);

  peanutbutter::memory_layout::ArchiveHeaderV2 aHeader{};
  const bool aReadHeader = peanutbutter::memory_layout::ReadArchiveHeader(
      aIt->second.data(),
      aIt->second.size(),
      aHeader,
      nullptr);
  XCTAssertTrue(aReadHeader);
  XCTAssertEqual(aHeader.mIsEncrypted, static_cast<std::uint8_t>(0u));
}

- (void)testDemoUnbundleTweaksNoEncryption {
  KnobGuard aGuard;
  TunePayloadAndBatchKnobs(
      /*payload bytes*/ 157u,
      /*bundle batch*/ 2u,
      /*decode batch*/ 3u,
      /*bundle repair batch*/ 3u,
      /*repair batch*/ 2u);
  const auto aLayout = MakeLayout(/*payload bytes*/ 157u,
                                  /*max blocks per archive*/ 88u);

  MockHardDrive aDrive;
  MockFileSystem aFileSystem(&aDrive);
  const std::string aSource = "/root/demo_unbundle_src";
  const std::string aBundleOutput = "/root/demo_unbundle_archives";
  const std::string aUnbundleOutput = "/root/demo_unbundle_out";
  const auto aExpected = SeedSourceTree(aDrive, aSource, 157u);

  DemoRuntime aBundleRuntime;
  const BundleExecutionResultV2 aBundle = RunBundleNoEncryption(
      aFileSystem,
      aBundleRuntime,
      aSource,
      aBundleOutput,
      "demo_unbundle_",
      /*blocks per archive*/ 5u,
      /*cancel finish blocks*/ 3u,
      /*safe mode*/ true,
      /*include preview*/ false,
      RepairCoveragePresetV2::k20,
      aLayout);
  XCTAssertTrue(aBundle.Succeeded(), @"Bundle setup failed: %s", aBundle.mFailureMessage.c_str());

  DemoRuntime aDecodeRuntime;
  const DecodeExecutionResultV2 aDecode = RunDecodeNoEncryption(
      aFileSystem,
      aDecodeRuntime,
      aBundleOutput,
      aUnbundleOutput,
      DecodeIntentV2::kUnbundle,
      /*cancel finish blocks*/ 4u,
      /*aggressive*/ true,
      aLayout);

  XCTAssertTrue(aDecode.Succeeded(), @"Unbundle failed: %s", aDecode.mFailureMessage.c_str());
  XCTAssertTrue(aDecodeRuntime.SawEvent(RuntimeEventKindV2::kDecodeArchiveStarted));

  for (const auto& aPair : aExpected) {
    const std::string aDecodedPath = aUnbundleOutput + "/" + aPair.first;
    XCTAssertEqual(ReadText(aDrive, aDecodedPath), aPair.second);
  }
}

- (void)testDemoRecoverTweaksNoEncryption {
  KnobGuard aGuard;
  TunePayloadAndBatchKnobs(
      /*payload bytes*/ 173u,
      /*bundle batch*/ 4u,
      /*decode batch*/ 1u,
      /*bundle repair batch*/ 2u,
      /*repair batch*/ 3u);
  const auto aLayout = MakeLayout(/*payload bytes*/ 173u,
                                  /*max blocks per archive*/ 72u);

  MockHardDrive aDrive;
  MockFileSystem aFileSystem(&aDrive);
  const std::string aSource = "/root/demo_recover_src";
  const std::string aBundleOutput = "/root/demo_recover_archives";
  const std::string aRecoverOutput = "/root/demo_recover_out";
  const auto aExpected = SeedSourceTree(aDrive, aSource, 173u);

  DemoRuntime aBundleRuntime;
  const BundleExecutionResultV2 aBundle = RunBundleNoEncryption(
      aFileSystem,
      aBundleRuntime,
      aSource,
      aBundleOutput,
      "demo_recover_",
      /*blocks per archive*/ 6u,
      /*cancel finish blocks*/ 5u,
      /*safe mode*/ false,
      /*include preview*/ true,
      RepairCoveragePresetV2::k60,
      aLayout);
  XCTAssertTrue(aBundle.Succeeded(), @"Bundle setup failed: %s", aBundle.mFailureMessage.c_str());

  DemoRuntime aRecoverRuntime;
  const DecodeExecutionResultV2 aRecover = RunDecodeNoEncryption(
      aFileSystem,
      aRecoverRuntime,
      aBundleOutput,
      aRecoverOutput,
      DecodeIntentV2::kRecover,
      /*cancel finish blocks*/ 6u,
      /*aggressive*/ false,
      aLayout);

  XCTAssertTrue(aRecover.Succeeded(), @"Recover failed: %s", aRecover.mFailureMessage.c_str());
  XCTAssertTrue(aRecoverRuntime.SawEvent(RuntimeEventKindV2::kDecodeArchiveStarted));

  for (const auto& aPair : aExpected) {
    const std::string aRecoveredPath = aRecoverOutput + "/" + aPair.first;
    XCTAssertEqual(ReadText(aDrive, aRecoveredPath), aPair.second);
  }
}

- (void)testDemoRepairTweaksNoEncryption {
  KnobGuard aGuard;
  TunePayloadAndBatchKnobs(
      /*payload bytes*/ 181u,
      /*bundle batch*/ 2u,
      /*decode batch*/ 2u,
      /*bundle repair batch*/ 5u,
      /*repair batch*/ 1u);
  const auto aLayout = MakeLayout(/*payload bytes*/ 181u,
                                  /*max blocks per archive*/ 90u);

  MockHardDrive aDrive;
  MockFileSystem aFileSystem(&aDrive);
  const std::string aSource = "/root/demo_repair_src";
  const std::string aBundleOutput = "/root/demo_repair_archives";
  const std::string aRepairOutput = "/root/demo_repair_out";
  (void)SeedSourceTree(aDrive, aSource, 181u);

  DemoRuntime aBundleRuntime;
  const BundleExecutionResultV2 aBundle = RunBundleNoEncryption(
      aFileSystem,
      aBundleRuntime,
      aSource,
      aBundleOutput,
      "demo_repair_",
      /*blocks per archive*/ 4u,
      /*cancel finish blocks*/ 4u,
      /*safe mode*/ true,
      /*include preview*/ true,
      RepairCoveragePresetV2::k80,
      aLayout);
  XCTAssertTrue(aBundle.Succeeded(), @"Bundle setup failed: %s", aBundle.mFailureMessage.c_str());
  XCTAssertFalse(aBundle.ArchivePaths().empty());

  const std::string aFirstArchiveNormalized = aDrive.Normalize(aBundle.ArchivePaths().front());
  auto aArchiveIt = aDrive.mFiles.find(aFirstArchiveNormalized);
  XCTAssertTrue(aArchiveIt != aDrive.mFiles.end());
  const std::size_t aFirstPayloadOffset =
      peanutbutter::memory_layout::kArchiveHeaderBytesV2 +
      peanutbutter::memory_layout::kSectionHeaderBytesV2;
  XCTAssertGreaterThan(aArchiveIt->second.size(), aFirstPayloadOffset + 7u);

  // Force a payload checksum mismatch (not header corruption) so repair has work to do.
  aArchiveIt->second[aFirstPayloadOffset + 7u] ^= 0x5au;

  DemoRuntime aRepairRuntime;
  RepairRequestV2 aRepairRequest;
  aRepairRequest.mSourcePath = aBundleOutput;
  aRepairRequest.mDestinationDirectory = aRepairOutput;
  aRepairRequest.mEncryptionEnabled = false;
  aRepairRequest.mAggressive = true;
  aRepairRequest.mCancelFinishBlocks = 7u;
  aRepairRequest.mPassword = "ignored-because-plaintext";

  const DecodeExecutionResultV2 aRepair = peanutbutter::ExecuteRepairV2(
      aRepairRequest,
      &aRepairRuntime,
      &aFileSystem,
      &aLayout);

  XCTAssertTrue(aRepair.Succeeded(), @"Repair failed: %s", aRepair.mFailureMessage.c_str());
  XCTAssertTrue(aRepairRuntime.SawEvent(RuntimeEventKindV2::kRepairArchiveStarted));
  XCTAssertTrue(aFileSystem.DirectoryHasEntries(aRepairOutput));
  XCTAssertGreaterThan(aRepair.mState.mRepair.mRepairableBlocks, 0u);
}

- (void)testBundleUnbundleInternalSymlinkReferencesLocalFileSystem {
  namespace fs = std::filesystem;

  const fs::path aRoot = fs::path("/tmp") / "pb_symlink_reference_v2";
  const fs::path aSource = aRoot / "source";
  const fs::path aArchives = aRoot / "archives";
  const fs::path aDecoded = aRoot / "decoded";
  std::error_code aFsError;
  fs::remove_all(aRoot, aFsError);
  aFsError.clear();
  XCTAssertTrue(fs::create_directories(aSource / "docs", aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));
  XCTAssertTrue(fs::create_directories(aArchives, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));
  XCTAssertTrue(WriteTextFilePath(aSource / "docs" / "readme.txt", "hello-link-target"));

  aFsError.clear();
  fs::create_symlink(fs::path("docs/readme.txt"), aSource / "readme_link", aFsError);
  XCTAssertFalse(static_cast<bool>(aFsError));
  aFsError.clear();
  fs::create_symlink(fs::path("docs"), aSource / "docs_link", aFsError);
  XCTAssertFalse(static_cast<bool>(aFsError));

  LocalFileSystemV2 aFileSystem;
  DemoRuntime aBundleRuntime;
  BundleRequestV2 aBundleRequest;
  aBundleRequest.mSourceDirectory = aSource.string();
  aBundleRequest.mDestinationDirectory = aArchives.string();
  aBundleRequest.mFilePrefix = "link_bundle";
  aBundleRequest.mEncryptionEnabled = false;
  aBundleRequest.mRepairEnabled = false;
  aBundleRequest.mIncludePreviewManifest = true;
  aBundleRequest.mSafeModeEnabled = true;
  aBundleRequest.mBlockCount = 4u;

  const BundleExecutionResultV2 aBundle = peanutbutter::ExecuteBundleV2(
      aBundleRequest,
      &aBundleRuntime,
      &aFileSystem,
      nullptr);
  XCTAssertTrue(aBundle.Succeeded(), @"Bundle failed: %s", aBundle.mFailureMessage.c_str());
  XCTAssertFalse(aBundle.ArchivePaths().empty());

  DemoRuntime aDecodeRuntime;
  DecodeRequestV2 aDecodeRequest;
  aDecodeRequest.mSourcePath = aArchives.string();
  aDecodeRequest.mDestinationDirectory = aDecoded.string();
  aDecodeRequest.mClearDestinationBeforeWrite = true;
  aDecodeRequest.mEncryptionEnabled = false;
  aDecodeRequest.mIntent = DecodeIntentV2::kUnbundle;
  const DecodeExecutionResultV2 aDecode = peanutbutter::ExecuteDecodeV2(
      aDecodeRequest,
      &aDecodeRuntime,
      &aFileSystem,
      nullptr);
  XCTAssertTrue(aDecode.Succeeded(), @"Decode failed: %s", aDecode.mFailureMessage.c_str());

  const fs::path aDecodedLink = aDecoded / "readme_link";
  const fs::path aDecodedDirLink = aDecoded / "docs_link";
  XCTAssertTrue(fs::is_symlink(aDecodedLink, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));
  aFsError.clear();
  XCTAssertTrue(fs::is_symlink(aDecodedDirLink, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));

  aFsError.clear();
  const fs::path aStoredTarget = fs::read_symlink(aDecodedLink, aFsError);
  XCTAssertFalse(static_cast<bool>(aFsError));
  const std::string aStoredTargetText = aStoredTarget.generic_string();
  XCTAssertFalse(aStoredTarget.is_absolute());
  XCTAssertEqual(ReadTextFilePath(aDecoded / "readme_link"), std::string("hello-link-target"));
  XCTAssertEqual(ReadTextFilePath(aDecoded / "docs" / "readme.txt"), std::string("hello-link-target"));
  XCTAssertEqual(aStoredTargetText.find("Users"), std::string::npos);
  XCTAssertEqual(aStoredTargetText.find(aSource.string()), std::string::npos);

  fs::remove_all(aRoot, aFsError);
}

- (void)testBundleUnbundleInternalAliasReferencesLocalFileSystem {
  namespace fs = std::filesystem;

  const fs::path aRoot = fs::path("/tmp") / "pb_alias_reference_v2";
  const fs::path aSource = aRoot / "source";
  const fs::path aArchives = aRoot / "archives";
  const fs::path aDecoded = aRoot / "decoded";
  std::error_code aFsError;
  fs::remove_all(aRoot, aFsError);
  aFsError.clear();
  XCTAssertTrue(fs::create_directories(aSource / "docs", aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));
  XCTAssertTrue(fs::create_directories(aArchives, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));
  XCTAssertTrue(WriteTextFilePath(aSource / "docs" / "readme.txt", "hello-alias-target"));

  LocalFileSystemV2 aFileSystem;
  const fs::path aSourceAlias = aSource / "readme_alias";
  XCTAssertTrue(aFileSystem.CreateAlias(aSourceAlias.string(),
                                        (aSource / "docs" / "readme.txt").string(),
                                        false));
  XCTAssertTrue(aFileSystem.IsAlias(aSourceAlias.string()));

  DemoRuntime aBundleRuntime;
  BundleRequestV2 aBundleRequest;
  aBundleRequest.mSourceDirectory = aSource.string();
  aBundleRequest.mDestinationDirectory = aArchives.string();
  aBundleRequest.mFilePrefix = "alias_bundle";
  aBundleRequest.mEncryptionEnabled = false;
  aBundleRequest.mRepairEnabled = false;
  aBundleRequest.mIncludePreviewManifest = true;
  aBundleRequest.mSafeModeEnabled = true;
  aBundleRequest.mBlockCount = 4u;

  const BundleExecutionResultV2 aBundle = peanutbutter::ExecuteBundleV2(
      aBundleRequest,
      &aBundleRuntime,
      &aFileSystem,
      nullptr);
  XCTAssertTrue(aBundle.Succeeded(), @"Bundle failed: %s", aBundle.mFailureMessage.c_str());
  XCTAssertFalse(aBundle.ArchivePaths().empty());

  DemoRuntime aDecodeRuntime;
  DecodeRequestV2 aDecodeRequest;
  aDecodeRequest.mSourcePath = aArchives.string();
  aDecodeRequest.mDestinationDirectory = aDecoded.string();
  aDecodeRequest.mClearDestinationBeforeWrite = true;
  aDecodeRequest.mEncryptionEnabled = false;
  aDecodeRequest.mIntent = DecodeIntentV2::kUnbundle;
  const DecodeExecutionResultV2 aDecode = peanutbutter::ExecuteDecodeV2(
      aDecodeRequest,
      &aDecodeRuntime,
      &aFileSystem,
      nullptr);
  XCTAssertTrue(aDecode.Succeeded(), @"Decode failed: %s", aDecode.mFailureMessage.c_str());

  const fs::path aDecodedAlias = aDecoded / "readme_alias";
  XCTAssertTrue(aFileSystem.IsAlias(aDecodedAlias.string()));

  std::string aAliasResolvedTarget;
  XCTAssertTrue(aFileSystem.TryReadAliasTarget(aDecodedAlias.string(), aAliasResolvedTarget));
  XCTAssertFalse(aAliasResolvedTarget.empty());

  std::error_code aCanonicalError;
  const fs::path aResolvedCanonical =
      fs::weakly_canonical(fs::path(aAliasResolvedTarget), aCanonicalError);
  XCTAssertFalse(static_cast<bool>(aCanonicalError));
  std::error_code aExpectedError;
  const fs::path aExpectedCanonical =
      fs::weakly_canonical(aDecoded / "docs" / "readme.txt", aExpectedError);
  XCTAssertFalse(static_cast<bool>(aExpectedError));
  XCTAssertEqual(aResolvedCanonical.generic_string(), aExpectedCanonical.generic_string());
  XCTAssertEqual(ReadTextFilePath(aDecoded / "docs" / "readme.txt"),
                 std::string("hello-alias-target"));

  const std::string aAliasBytes = ReadTextFilePath(aDecodedAlias);
  XCTAssertEqual(aAliasBytes.find(aSource.string()), std::string::npos);

  fs::remove_all(aRoot, aFsError);
}

- (void)testBundleUnbundleExternalAliasReferencesLocalFileSystem {
  namespace fs = std::filesystem;

  const fs::path aRoot = fs::path("/tmp") / "pb_alias_external_reference_v2";
  const fs::path aSource = aRoot / "source";
  const fs::path aArchives = aRoot / "archives";
  const fs::path aDecoded = aRoot / "decoded";
  std::error_code aFsError;
  fs::remove_all(aRoot, aFsError);
  aFsError.clear();
  XCTAssertTrue(fs::create_directories(aSource, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));
  XCTAssertTrue(fs::create_directories(aArchives, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));
  XCTAssertTrue(WriteTextFilePath(aSource / "inside.txt", "inside-data"));

  const char* aHomeEnv = std::getenv("HOME");
  XCTAssertNotEqual(aHomeEnv, nullptr);
  const fs::path aHomePath = fs::path(aHomeEnv == nullptr ? "/" : aHomeEnv);
  const fs::path aExternalTarget = aHomePath / "pb_alias_external_target_v2.txt";
  XCTAssertTrue(WriteTextFilePath(aExternalTarget, "external-home-target"));

  LocalFileSystemV2 aFileSystem;
  const fs::path aSourceAlias = aSource / "[[Work, Etc]]";
  XCTAssertTrue(aFileSystem.CreateAlias(aSourceAlias.string(),
                                        aExternalTarget.string(),
                                        false));
  XCTAssertTrue(aFileSystem.IsAlias(aSourceAlias.string()));

  DemoRuntime aBundleRuntime;
  BundleRequestV2 aBundleRequest;
  aBundleRequest.mSourceDirectory = aSource.string();
  aBundleRequest.mDestinationDirectory = aArchives.string();
  aBundleRequest.mFilePrefix = "alias_external_bundle";
  aBundleRequest.mEncryptionEnabled = false;
  aBundleRequest.mRepairEnabled = false;
  aBundleRequest.mIncludePreviewManifest = true;
  aBundleRequest.mSafeModeEnabled = true;
  aBundleRequest.mBlockCount = 4u;

  const BundleExecutionResultV2 aBundle = peanutbutter::ExecuteBundleV2(
      aBundleRequest,
      &aBundleRuntime,
      &aFileSystem,
      nullptr);
  XCTAssertTrue(aBundle.Succeeded(), @"Bundle failed: %s", aBundle.mFailureMessage.c_str());
  XCTAssertFalse(MessagesContain(aBundleRuntime.mLogs, "Skipped link '[[Work, Etc]]'"));

  DemoRuntime aDecodeRuntime;
  DecodeRequestV2 aDecodeRequest;
  aDecodeRequest.mSourcePath = aArchives.string();
  aDecodeRequest.mDestinationDirectory = aDecoded.string();
  aDecodeRequest.mClearDestinationBeforeWrite = true;
  aDecodeRequest.mEncryptionEnabled = false;
  aDecodeRequest.mIntent = DecodeIntentV2::kUnbundle;
  const DecodeExecutionResultV2 aDecode = peanutbutter::ExecuteDecodeV2(
      aDecodeRequest,
      &aDecodeRuntime,
      &aFileSystem,
      nullptr);
  XCTAssertTrue(aDecode.Succeeded(), @"Decode failed: %s", aDecode.mFailureMessage.c_str());

  const fs::path aDecodedAlias = aDecoded / "[[Work, Etc]]";
  XCTAssertTrue(aFileSystem.IsAlias(aDecodedAlias.string()) ||
                aFileSystem.IsSymlink(aDecodedAlias.string()));

  std::string aDecodedAliasTarget;
  XCTAssertTrue(aFileSystem.TryReadAliasTarget(aDecodedAlias.string(), aDecodedAliasTarget));
  XCTAssertFalse(aDecodedAliasTarget.empty());
  XCTAssertEqual(aDecodedAliasTarget.find("/Users/"), std::size_t(0u));
  XCTAssertEqual(aDecodedAliasTarget.find(aSource.string()), std::string::npos);

  fs::remove(aExternalTarget, aFsError);
  fs::remove_all(aRoot, aFsError);
}

- (void)testBundleUnbundleExternalAliasReferencesWhenTargetMissingLocalFileSystem {
  namespace fs = std::filesystem;

  const fs::path aRoot = fs::path("/tmp") / "pb_alias_missing_target_reference_v2";
  const fs::path aSource = aRoot / "source";
  const fs::path aArchives = aRoot / "archives";
  const fs::path aDecoded = aRoot / "decoded";
  std::error_code aFsError;
  fs::remove_all(aRoot, aFsError);
  aFsError.clear();
  XCTAssertTrue(fs::create_directories(aSource, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));
  XCTAssertTrue(fs::create_directories(aArchives, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));

  const char* aHomeEnv = std::getenv("HOME");
  XCTAssertNotEqual(aHomeEnv, nullptr);
  const fs::path aHomePath = fs::path(aHomeEnv == nullptr ? "/" : aHomeEnv);
  const fs::path aExternalTarget = aHomePath / "pb_alias_missing_target_v2.txt";
  XCTAssertTrue(WriteTextFilePath(aExternalTarget, "will-be-deleted-before-decode"));

  LocalFileSystemV2 aFileSystem;
  const fs::path aSourceAlias = aSource / "[[Work, Etc]]";
  XCTAssertTrue(aFileSystem.CreateAlias(aSourceAlias.string(),
                                        aExternalTarget.string(),
                                        false));
  XCTAssertTrue(aFileSystem.IsAlias(aSourceAlias.string()));

  DemoRuntime aBundleRuntime;
  BundleRequestV2 aBundleRequest;
  aBundleRequest.mSourceDirectory = aSource.string();
  aBundleRequest.mDestinationDirectory = aArchives.string();
  aBundleRequest.mFilePrefix = "alias_missing_target_bundle";
  aBundleRequest.mEncryptionEnabled = false;
  aBundleRequest.mRepairEnabled = false;
  aBundleRequest.mIncludePreviewManifest = true;
  aBundleRequest.mSafeModeEnabled = true;
  aBundleRequest.mBlockCount = 4u;

  const BundleExecutionResultV2 aBundle = peanutbutter::ExecuteBundleV2(
      aBundleRequest,
      &aBundleRuntime,
      &aFileSystem,
      nullptr);
  XCTAssertTrue(aBundle.Succeeded(), @"Bundle failed: %s", aBundle.mFailureMessage.c_str());

  fs::remove(aExternalTarget, aFsError);

  DemoRuntime aDecodeRuntime;
  DecodeRequestV2 aDecodeRequest;
  aDecodeRequest.mSourcePath = aArchives.string();
  aDecodeRequest.mDestinationDirectory = aDecoded.string();
  aDecodeRequest.mClearDestinationBeforeWrite = true;
  aDecodeRequest.mEncryptionEnabled = false;
  aDecodeRequest.mIntent = DecodeIntentV2::kUnbundle;
  const DecodeExecutionResultV2 aDecode = peanutbutter::ExecuteDecodeV2(
      aDecodeRequest,
      &aDecodeRuntime,
      &aFileSystem,
      nullptr);
  XCTAssertTrue(aDecode.Succeeded(), @"Decode failed: %s", aDecode.mFailureMessage.c_str());

  const fs::path aDecodedAlias = aDecoded / "[[Work, Etc]]";
  XCTAssertTrue(aFileSystem.IsAlias(aDecodedAlias.string()) ||
                aFileSystem.IsSymlink(aDecodedAlias.string()));

  fs::remove_all(aRoot, aFsError);
}

- (void)testBundleMediumEncryptedLocalFileSystem {
  namespace fs = std::filesystem;

  const fs::path aRoot = fs::path("/tmp") / "pb_medium_bundle_stress_v2";
  const fs::path aSource = aRoot / "source";
  const fs::path aDestination = aRoot / "destination";
  std::error_code aFsError;
  fs::remove_all(aRoot, aFsError);
  aFsError.clear();
  XCTAssertTrue(fs::create_directories(aSource, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));
  XCTAssertTrue(fs::create_directories(aDestination, aFsError));
  XCTAssertFalse(static_cast<bool>(aFsError));

  bool aSeeded = true;
  for (std::size_t aFolderIndex = 0u; aFolderIndex < 4u; ++aFolderIndex) {
    const fs::path aFolder = aSource / ("bucket_" + std::to_string(aFolderIndex));
    fs::create_directories(aFolder, aFsError);
    if (aFsError) {
      aSeeded = false;
      break;
    }
    for (std::size_t aFileIndex = 0u; aFileIndex < 6u; ++aFileIndex) {
      const fs::path aFilePath =
          aFolder / ("sample_" + std::to_string(aFileIndex) + ".bin");
      // ~2MB per file, ~48MB total source payload.
      if (!WritePatternFile(aFilePath,
                            (2u * 1024u * 1024u) + (aFileIndex * 307u),
                            static_cast<std::uint8_t>(aFolderIndex * 17u + aFileIndex))) {
        aSeeded = false;
        break;
      }
    }
    if (!aSeeded) {
      break;
    }
  }
  XCTAssertTrue(aSeeded);

  LocalFileSystemV2 aFileSystem;
  QuietBundleRuntime aRuntime;
  BundleRequestV2 aRequest;
  aRequest.mSourceDirectory = aSource.string();
  aRequest.mDestinationDirectory = aDestination.string();
  aRequest.mFilePrefix = "medium_stress";
  aRequest.mEncryptionEnabled = true;
  aRequest.mEncryptionStrength = StrengthPresetV2::kHigh;
  aRequest.mTableStrength = StrengthPresetV2::kHigh;
  aRequest.mRepairEnabled = true;
  aRequest.mRepairCoverage = RepairCoveragePresetV2::k40;
  aRequest.mIncludePreviewManifest = true;
  aRequest.mSafeModeEnabled = true;
  aRequest.mBlockCount = 8u;
  aRequest.mPassword = "medium-stress-password";

  const BundleExecutionResultV2 aResult = peanutbutter::ExecuteBundleV2(
      aRequest,
      &aRuntime,
      &aFileSystem,
      nullptr);

  XCTAssertTrue(aResult.Succeeded(),
                @"Medium encrypted local-fs bundle failed: %s",
                JoinMessages(aRuntime.mErrors).c_str());
  XCTAssertFalse(aResult.ArchivePaths().empty());

  fs::remove_all(aRoot, aFsError);
}

@end
