//
//  BundleTests_Regression.m
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/4/26.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#include "TestBundle.hpp"
#include "TestBundleWithHooks.hpp"
#include "JobBundle.hpp"
#include "WrappedArchiveAssembler.hpp"
#include "BundleVerify.hpp"
#include "FakeFile.hpp"

@interface BundleTests_Regression : XCTestCase
@end

@implementation BundleTests_Regression

- (BOOL) run: (JobBundle &)pJob {
    
    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;
    
    if (!TestBundleWithHooks::PerformReal(pJob, aFileSystem, &aErrorString)) {
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
    
    return YES;
}

- (void)test_bundle_regression_small_window_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("CQ", "b");
    aJob.AddFile("V.s", "龙PA");
    aJob.AddFile("c.g", "Z永g");
    aJob.AddFile("rI", "");
    aJob.mBatchSize = 20;
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_bundle_regression_small_window_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("Ω.S", "");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_bundle_regression_small_window_c {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFolder("Yxba");
    aJob.AddFolder("اi");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_bundle_regression_small_window_d {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("a", "a");
    aJob.AddFile("b", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_bundle_regression_small_window_e {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4096;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("a.txt", "a");
    aJob.AddFile("b.dat", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_bundle_regression_small_window_f {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 2;
    aJob.mBlocksPerArchive = 4096;
    aJob.mPreviewEnabled = true;
    aJob.SetRepair40();
    aJob.AddFile("a.txt", "a");
    aJob.AddFile("b.dat", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_bundle_regression_small_window_g {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("GK", "E力W");
    aJob.AddFile("I.PD", "c");
    aJob.AddFile("Q", "WyX");
    aJob.AddFile("bj", "Z");
    aJob.AddFile("q", "qd");
    aJob.AddFile("sk.G", "z");
    aJob.AddFile("xh.J", "f");
    aJob.AddFile("б.I", "Q");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_bundle_regression_small_window_h {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    aJob.SetRepair40();
    aJob.AddFile("a.txt", "a");
    aJob.AddFile("b.dat", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_bundle_regression_small_window_i {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = true;
    aJob.SetRepair40();
    aJob.AddFile("a.txt", "a");
    aJob.AddFile("b.dat", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_bundle_regression_small_window_j {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = true;
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

- (void)test_bundle_regression_small_window_k {
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

- (void)test_bundle_regression_small_window_l {
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

- (void)test_bundle_regression_small_window_m {
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

- (void)test_bundle_regression_small_window_n {
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

- (void)test_bundle_regression_small_window_o {
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

- (void)test_bundle_regression_small_window_p {
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

- (void)test_bundle_regression_small_window_q {
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

- (void)test_bundle_regression_rejects_same_source_and_destination_without_deleting_source {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mClearDestination = true;
    aJob.mArchived = aJob.mInput;
    aJob.AddFile("keep.txt", "payload");

    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;

    const BOOL aSucceeded = TestBundleWithHooks::PerformReal(aJob, aFileSystem, &aErrorString);
    XCTAssertFalse(aSucceeded);

    const std::string aSourceFilePath =
        aFileSystem.JoinPath(aJob.mInput.ToString(), "keep.txt");
    XCTAssertTrue(aFileSystem.IsFile(aSourceFilePath));
    XCTAssertTrue(aFileSystem.Load(aSourceFilePath).ToString() == "payload");
}

- (void)test_bundle_regression_does_not_clear_existing_destination_contents {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mClearDestination = true;
    aJob.AddFile("a.txt", "A");

    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    const std::string aSentinelPath =
        aFileSystem.JoinPath(aJob.mArchived.ToString(), "sentinel.txt");
    XCTAssertTrue(aFileSystem.WriteFile(
        aSentinelPath,
        reinterpret_cast<const unsigned char*>("sentinel"),
        8u));

    ByteString aErrorString;
    const BOOL aSucceeded = TestBundleWithHooks::PerformReal(aJob, aFileSystem, &aErrorString);
    XCTAssertTrue(aSucceeded);
    XCTAssertTrue(aFileSystem.IsFile(aSentinelPath));
    XCTAssertTrue(aFileSystem.Load(aSentinelPath).ToString() == "sentinel");
}

@end
