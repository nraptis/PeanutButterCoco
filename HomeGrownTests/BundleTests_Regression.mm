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

    // Files
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

    // Files
    aJob.AddFile("Ω.S", "");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

@end
