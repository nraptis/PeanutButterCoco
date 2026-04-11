//
//  UnbundleTests_Regression.m
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/6/26.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#include <filesystem>
#include <vector>
#include "namespaces.hpp"
#include "FakeFile.hpp"
#include "Random.hpp"
#include "Words.hpp"
#include "../PeanutButterArchiver/Code/Engine/TaskCommon.hpp"
#include "TestBundle.hpp"
#include "TestBundleWithHooks.hpp"
#include "TestUnbundleWithHooks.hpp"
#include "JobBundle.hpp"
#include "WrappedArchiveAssembler.hpp"
#include "BundleVerify.hpp"
#include "UnbundleVerify.hpp"

namespace {

class CapturingSanityRuntime final : public peanutbutter::SanityRuntimeV2 {
public:
    bool IsCancelRequested() const override {
        return false;
    }
    
    void EmitLog(peanutbutter::LogLevelV2 pLevel, const std::string& pMessage) override {
        peanutbutter::LogEntryV2 aEntry;
        aEntry.mLevel = pLevel;
        aEntry.mMessage = pMessage;
        mLogs.push_back(std::move(aEntry));
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
    
    std::vector<peanutbutter::LogEntryV2> mLogs;
};

struct SanityRunResult {
    peanutbutter::TaskDispositionV2 mDisposition = peanutbutter::TaskDispositionV2::kRunning;
    std::string mFailureMessage;
    std::vector<peanutbutter::LogEntryV2> mLogs;
};

std::filesystem::path CreateUniqueTempDirectory(NSString *pLabel) {
    NSString *aBase = NSTemporaryDirectory();
    NSString *aUniqueName = [NSString stringWithFormat:@"pb_sanity_%@_%@", pLabel, NSUUID.UUID.UUIDString];
    std::filesystem::path aPath([[aBase stringByAppendingPathComponent:aUniqueName] UTF8String]);
    std::filesystem::create_directories(aPath);
    return aPath;
}

BOOL WriteTempFile(const std::filesystem::path &pPath, const std::string &pContents) {
    std::filesystem::create_directories(pPath.parent_path());
    NSData *aData = [NSData dataWithBytes:pContents.data() length:pContents.size()];
    return [aData writeToFile:[NSString stringWithUTF8String:pPath.string().c_str()] atomically:YES];
}

SanityRunResult RunSanityCompare(const std::filesystem::path &pLeft,
                                 const std::filesystem::path &pRight,
                                 BOOL pIgnoreHidden = NO) {
    peanutbutter::SanityRequestV2 aRequest;
    aRequest.mLeftDirectory = pLeft.lexically_normal().generic_string();
    aRequest.mRightDirectory = pRight.lexically_normal().generic_string();
    aRequest.mIgnoreHidden = pIgnoreHidden;
    
    CapturingSanityRuntime aRuntime;
    peanutbutter::SanityTaskV2 aTask(aRequest, &aRuntime);
    while (aTask.Disposition() == peanutbutter::TaskDispositionV2::kRunning) {
        (void)aTask.Heartbeat();
    }
    
    SanityRunResult aResult;
    aResult.mDisposition = aTask.Disposition();
    aResult.mFailureMessage = aTask.FailureMessage();
    aResult.mLogs = std::move(aRuntime.mLogs);
    return aResult;
}

std::string FindFirstLogContaining(const std::vector<peanutbutter::LogEntryV2> &pLogs,
                                   const std::string &pNeedle) {
    for (const peanutbutter::LogEntryV2 &aLog : pLogs) {
        if (aLog.mMessage.find(pNeedle) != std::string::npos) {
            return aLog.mMessage;
        }
    }
    return std::string();
}

std::string FindFinalMatchedSummary(const std::vector<peanutbutter::LogEntryV2> &pLogs) {
    for (auto aIt = pLogs.rbegin(); aIt != pLogs.rend(); ++aIt) {
        if (aIt->mMessage.find("[Folder Compare][Summary]") != std::string::npos &&
            aIt->mMessage.find("Matched ") != std::string::npos) {
            return aIt->mMessage;
        }
    }
    return std::string();
}

}  // namespace

@interface UnbundleTests_Regression : XCTestCase
@end

@implementation UnbundleTests_Regression

- (BOOL)run: (JobBundle &)pJob {
    
    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;
    
    if (!TestBundleWithHooks::PerformReal(
                                          pJob,
                                          aFileSystem,
                                          [](const TestBundleWithHooks::PhaseBatchFeedback &pFeedback,
                                             peanutbutter::BundleStageContextV2 &pContext,
                                             SimpleBundleRuntime &pRuntime) {
                                                 (void)pContext;
                                                 (void)pRuntime;
                                             },
                                          &aErrorString)) {
                                              printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                                              return NO;
                                          }
    
    vector<WrappedArchive> aRealArchives = WrappedArchiveAssembler::Get(pJob.mArchived.ToString(),
                                                                        aFileSystem,
                                                                        pJob.mBlocksPerArchive,
                                                                        pJob.mPayloadBytesPerBlock + Layout::SectionHeaderSize());
    
    vector<FakeArchive> aMockArchives;
    if (!TestBundle::PerformMock(pJob, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!BundleVerify::Execute(pJob, &aRealArchives, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!TestUnbundleWithHooks::PerformRealUnbundle(
                                            pJob,
                                            aFileSystem,
                                            [](const TestUnbundleWithHooks::PhaseBatchFeedback &pFeedback,
                                               peanutbutter::DecodeStageContextV2 &pContext,
                                               SimpleDecodeRuntime &pRuntime) {
                                                   (void)pContext;
                                                   (void)pRuntime;
                                               },
                                            &aErrorString)) {
                                                printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                                                return NO;
                                            }
    
    vector <FakeFile> aFilesA;
    if (!TestUnbundleWithHooks::CollectFiles(pJob, &aFilesA, &aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        
        return NO;
    }
    
    if (!UnbundleVerify::Execute(&aFilesA, &pJob.mFileList, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    return YES;
}

- (void)test_unbundle_regression_small_window_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 2;
    aJob.mBlocksPerArchive = 4;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 80;
    
    aJob.AddFile("L.Z", "S");
    aJob.AddFile("U.o", "o");
    aJob.AddFile("bf.l", "🤖WBYoH");
    aJob.AddFile("eqB.qqG", "ゑcбtz");
    aJob.AddFile("wΩt.r", "PvWM");
    aJob.AddFile("yw.u", "sVаW");
    aJob.AddFile("ا.O", "Ex");
    aJob.AddFile("ぽt.J", "");
    aJob.AddFile("大.ez", "🌈d");
    aJob.AddFile("🌈.F", "zвWW");
    aJob.AddFile("🚀.ceE", "Itゑбu");
    
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mPreviewEnabled = true;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFile("Bld.pU", "");
    aJob.AddFile("cRrぽg.TE", "");
    aJob.AddFile("lhEL.K", "I");
    aJob.AddFile("山.ap", "");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_c {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 8;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 20;

    // Files
    aJob.AddFile("F", "");
    aJob.AddFile("Q", "LJ");
    aJob.AddFile("Ru", "k");
    aJob.AddFile("Ui", "B");
    aJob.AddFile("W", "");
    aJob.AddFile("WT", "X");
    aJob.AddFile("Z", "");
    aJob.AddFile("a", "x");
    aJob.AddFile("ol", "n");
    aJob.AddFile("q", "G");
    aJob.AddFile("ry", "bp");
    aJob.AddFile("t", "");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_d {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;

    // Files
    aJob.AddFolder("IgJXyNثFQ");
    aJob.AddFolder("LqI电jRmV");
    aJob.AddFile("Y.b", "dΨQh龙iVWjl");
    aJob.AddFile("Y.r", "sleLjتxA大O");
    aJob.AddFile("n.a", "ILIvp");
    aJob.AddFolder("sgmгTvK");
    aJob.AddFolder("wGzba");
    aJob.AddFile("x.T", "SSlw");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_e {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFolder("KrU");
    aJob.AddFolder("bqp");
    aJob.AddFolder("iIg");
    aJob.AddFolder("oHP");
    aJob.AddFolder("uOL");
    aJob.AddFolder("wKS");
    aJob.AddFolder("ΩH");
    aJob.AddFile("🧬L.q", "u");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_f {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    aJob.SetRepair80();
    aJob.AddFile("D", "OQ");
    aJob.AddFile("D.I", "明zE");
    aJob.AddFile("I", "zhzいXm");
    aJob.AddFile("I.P", "");
    aJob.AddFile("O.R", "ぷv");
    aJob.AddFile("OJ", "⭐bw");
    aJob.AddFile("fC", "K大lfy");
    aJob.AddFile("t.Hw", "冰wΩdi");
    
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_g {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mPreviewEnabled = true;
    aJob.AddFolder("Talf");
    aJob.AddFolder("aбb");
    aJob.AddFolder("oCACM🛸zhQ");
    aJob.AddFile("бtв.VGod", "sAぽa🏠Mbpz");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_h {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("D.S", "Δجい🏠M");
    aJob.AddFolder("جSвbгKoka");
    aJob.AddFolder("ゑبDeゑJ");
    aJob.AddFolder("🛸jyt");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_i {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 5;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    aJob.AddFile("H.CBewCXzDmw", "Cw");
    aJob.AddFolder("K");
    aJob.AddFolder("TI");
    aJob.AddFolder("いU永Jb");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_j {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;
    aJob.AddFile("T", "冰ΩfUvAΞBW");
    aJob.AddFile("W.fjQo", "🚀BаWh");
    aJob.AddFile("Xi.AqB", "");
    aJob.AddFile("u", "бZf冰m");
    aJob.AddFile("y.تD", "C⭐z力H");
    aJob.AddFile("yJm.gkA", "⚙️NsO");
    aJob.AddFile("zO.POQU", "zΞo明تTFZmJ");
    aJob.AddFile("вΨゑ.гC", "⚙️ΩzvRf");
    aJob.AddFile("⭐Ω.m", "Fyゑ🧬ゐCh");
    aJob.AddFile("おz.めk", "S龙NMc🤖تXd");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_k {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 0;
    aJob.AddFolder("a");
    aJob.AddFolder("b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_small_window_l {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 5;
    aJob.mBlocksPerArchive = 1000;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 0;
    aJob.AddFile("ab", "cd");
    aJob.AddFolder("e");
    aJob.AddFolder("f");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_unbundle_regression_rejects_same_source_and_destination_without_deleting_archives {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 0;
    aJob.AddFile("keep.txt", "payload");
    aJob.mUnarchived = aJob.mArchived;

    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;

    const BOOL aBundleSucceeded = TestBundleWithHooks::PerformReal(aJob, aFileSystem, &aErrorString);
    XCTAssertTrue(aBundleSucceeded);

    const std::vector<std::string> aArchiveFiles =
        aHardDrive.ListFilesRecursive(aJob.mArchived.ToString());
    XCTAssertFalse(aArchiveFiles.empty());

    const BOOL aUnbundleSucceeded =
        TestUnbundleWithHooks::PerformRealUnbundle(aJob, aFileSystem, &aErrorString);
    XCTAssertFalse(aUnbundleSucceeded);

    for (const std::string& aArchivePath : aArchiveFiles) {
        XCTAssertTrue(aFileSystem.IsFile(aArchivePath));
    }
}

- (void)test_unbundle_regression_sanity_warns_on_empty_folder_difference {
    const std::filesystem::path aLeft = CreateUniqueTempDirectory(@"empty_folder_left");
    const std::filesystem::path aRight = CreateUniqueTempDirectory(@"empty_folder_right");
    const std::filesystem::path aSharedFileLeft = aLeft / "shared.txt";
    const std::filesystem::path aSharedFileRight = aRight / "shared.txt";
    const std::filesystem::path aEmptyFolderLeft = aLeft / "only_left_empty";
    
    XCTAssertTrue(WriteTempFile(aSharedFileLeft, "same"));
    XCTAssertTrue(WriteTempFile(aSharedFileRight, "same"));
    std::filesystem::create_directories(aEmptyFolderLeft);
    
    const SanityRunResult aResult = RunSanityCompare(aLeft, aRight);
    XCTAssertTrue(aResult.mDisposition == peanutbutter::TaskDispositionV2::kCompleted);
    XCTAssertTrue(aResult.mFailureMessage.empty());
    
    const std::string aSummary = FindFinalMatchedSummary(aResult.mLogs);
    XCTAssertFalse(aSummary.empty());
    XCTAssertTrue(aSummary.find("Warn:") != std::string::npos);
    XCTAssertTrue(aSummary.find("Matched 1 files and 0 folders") != std::string::npos);
    
    std::filesystem::remove_all(aLeft);
    std::filesystem::remove_all(aRight);
}

- (void)test_unbundle_regression_sanity_ignores_shortcuts_and_symlinks {
    const std::filesystem::path aLeft = CreateUniqueTempDirectory(@"shortcut_left");
    const std::filesystem::path aRight = CreateUniqueTempDirectory(@"shortcut_right");
    const std::filesystem::path aSharedFileLeft = aLeft / "shared.txt";
    const std::filesystem::path aSharedFileRight = aRight / "shared.txt";
    const std::filesystem::path aSymlinkPath = aLeft / "left_only.symlink";
    const std::filesystem::path aShortcutPath = aLeft / "left_only.webloc";
    
    XCTAssertTrue(WriteTempFile(aSharedFileLeft, "same"));
    XCTAssertTrue(WriteTempFile(aSharedFileRight, "same"));
    XCTAssertTrue(WriteTempFile(aShortcutPath, "shortcut"));
    std::filesystem::create_symlink(aSharedFileLeft.filename(), aSymlinkPath);
    
    const SanityRunResult aResult = RunSanityCompare(aLeft, aRight);
    XCTAssertTrue(aResult.mDisposition == peanutbutter::TaskDispositionV2::kCompleted);
    XCTAssertTrue(aResult.mFailureMessage.empty());
    
    const std::string aIgnoredLog = FindFirstLogContaining(aResult.mLogs, "Ignored ");
    XCTAssertFalse(aIgnoredLog.empty());
    const std::string aSummary = FindFinalMatchedSummary(aResult.mLogs);
    XCTAssertFalse(aSummary.empty());
    XCTAssertTrue(aSummary.find("Good:") != std::string::npos);
    XCTAssertTrue(aSummary.find("Matched 1 files and 0 folders") != std::string::npos);
    
    std::filesystem::remove_all(aLeft);
    std::filesystem::remove_all(aRight);
}

- (void)test_unbundle_regression_sanity_reports_matched_counts_on_file_failure {
    const std::filesystem::path aLeft = CreateUniqueTempDirectory(@"mismatch_left");
    const std::filesystem::path aRight = CreateUniqueTempDirectory(@"mismatch_right");
    
    XCTAssertTrue(WriteTempFile(aLeft / "same.txt", "same"));
    XCTAssertTrue(WriteTempFile(aRight / "same.txt", "same"));
    XCTAssertTrue(WriteTempFile(aLeft / "different.txt", "left"));
    XCTAssertTrue(WriteTempFile(aRight / "different.txt", "right"));
    
    const SanityRunResult aResult = RunSanityCompare(aLeft, aRight);
    XCTAssertTrue(aResult.mDisposition == peanutbutter::TaskDispositionV2::kFailed);
    
    const std::string aSummary = FindFinalMatchedSummary(aResult.mLogs);
    XCTAssertFalse(aSummary.empty());
    XCTAssertTrue(aSummary.find("Fail:") != std::string::npos);
    XCTAssertTrue(aSummary.find("Matched 1 files and 0 folders") != std::string::npos);
    
    std::filesystem::remove_all(aLeft);
    std::filesystem::remove_all(aRight);
}

@end
