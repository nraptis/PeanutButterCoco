//
//  UnbundleTests_Regression.m
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#import <Foundation/Foundation.h>

//
//  UnbundleTests_Small.m
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#include "namespaces.hpp"
#include "FakeFile.hpp"
#include "Random.hpp"
#include "Words.hpp"
#include "TestBundle.hpp"
#include "TestBundleWithHooks.hpp"
#include "TestUnbundleWithHooks.hpp"

#include "JobBundle.hpp"
#include "WrappedArchiveAssembler.hpp"
#include "BundleVerify.hpp"
#include "UnbundleVerify.hpp"


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
    
    if (!BundleVerify::Execute(pJob, aRealArchives, aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!TestUnbundleWithHooks::PerformReal(
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
    if (!TestUnbundleWithHooks::CollectFiles(pJob, aFilesA, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        
        return NO;
    }
    
    if (!UnbundleVerify::Execute(aFilesA, pJob.mFileList, &aErrorString)) {
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

@end
