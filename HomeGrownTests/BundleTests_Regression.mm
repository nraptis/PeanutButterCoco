//
//  BundleTests_Regression.m
//  HomeGrownTests
//
//  Created by Magneto on 4/4/26.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#include "TestBundle.hpp"
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
    
    if (!TestBundle::PerformReal(pJob, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    vector<WrappedArchive> aRealArchives = WrappedArchiveAssembler::Get(pJob.mDestination.ToString(),
                                                                        aFileSystem,
                                                                        pJob.mBlocksPerArchive,
                                                                        pJob.mPayloadBytesPerBlock + Layout::SectionHeaderSize());
    
    vector<FakeArchive> aMockArchives;
    if (!TestBundle::PerformMock(pJob, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!BundleVerify::Execute(pJob, aRealArchives, aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    return YES;
}

- (void)test_regression_small_window_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("CQ", "b");
    aJob.AddFile("V.s", "龙PA");
    aJob.AddFile("c.g", "Z永g");
    aJob.AddFile("rI", "");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("Ω.S", "");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_c {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFolder("Yxba");
    aJob.AddFolder("اi");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_d {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("a", "a");
    aJob.AddFile("b", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_e {
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

- (void)test_regression_small_window_f {
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

- (void)test_regression_small_window_g {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = true;

    // Files
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

- (void)test_regression_small_window_h {
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

- (void)test_regression_small_window_i {
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

- (void)test_regression_small_window_j {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = true;
    aJob.SetRepair80();

    // Files
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

- (void)test_regression_small_window_k {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    aJob.SetRepair80();

    // Files
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

@end
