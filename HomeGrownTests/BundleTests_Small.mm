//
//  BundleTests_Small.m
//  HomeGrownTests
//
//  Created by Magneto on 4/4/26.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#include "namespaces.hpp"
#include "FakeFile.hpp"
#include "Random.hpp"
#include "Words.hpp"
#include "TestBundle.hpp"
#include "JobBundle.hpp"
#include "WrappedArchiveAssembler.hpp"
#include "BundleVerify.hpp"

@interface BundleTests_Small : XCTestCase
@end

@implementation BundleTests_Small

- (void) logRegression: (JobBundle &)pJob {
    
    NSLog(@"\n🔥 TEST FAILURE REPRO CODE 🔥\n");
    
    // Header
    NSLog(@"- (void)test_regression_auto_generated {");
    NSLog(@"    JobBundle aJob;");
    
    NSLog(@"    aJob.mPayloadBytesPerBlock = %d;", pJob.mPayloadBytesPerBlock);
    NSLog(@"    aJob.mBlocksPerArchive = %d;", pJob.mBlocksPerArchive);
    NSLog(@"\n    // Files");
    
    // Files
    for (const FakeFile &file : pJob.mFileList) {
        
        NSString *name = [NSString stringWithUTF8String:file.mName.ToString().c_str()];
        NSString *content = [NSString stringWithUTF8String:file.mContent.ToString().c_str()];
        
        // Escape quotes + backslashes so the output is valid code
        NSString *escapedName = [[name stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"]
                                      stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
        
        NSString *escapedContent = [[content stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"]
                                         stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
        
        NSLog(@"    aJob.AddFile(\"%@\", \"%@\");", escapedName, escapedContent);
    }
    
    // Footer
    NSLog(@"\n    if ([self run:aJob] == NO) {");
    NSLog(@"        XCTFail(@\"Regression test failed.\");");
    NSLog(@"    }");
    NSLog(@"}");
    
    NSLog(@"\n🔥 END REPRO CODE 🔥\n");
}

- (BOOL) run: (JobBundle &)pJob {
    
    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;
    
    if (!TestBundle::PerformReal(pJob, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    vector<WrappedArchive> aRealArchives = WrappedArchiveAssembler::Get(pJob.mDestination.ToString(),
                                                                        aFileSystem,
                                                                        pJob.mBlocksPerArchive,
                                                                        pJob.mPayloadBytesPerBlock + Layout::SectionHeaderSize());
    
    vector<FakeArchive> aMockArchives;
    if (!TestBundle::PerformMock(pJob, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    if (!BundleVerify::Execute(pJob, aRealArchives, aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    return YES;
}

- (void)test_100_1_1_filesOnly {
    
    for (int aTestIndex=0;aTestIndex<100;aTestIndex++) {
        int aFileCount = Random::Get(1, 8);
        vector<FakeFile> aFiles = Words::GetRandomFiles(aFileCount, 1, 4, 0, 8);
        
        JobBundle aJob;
        for (auto aFile: aFiles) {
            aJob.mFileList.push_back(aFile);
        }
        aJob.mPayloadBytesPerBlock = 1;
        aJob.mBlocksPerArchive = 1;
        
        if ([self run: aJob] == NO) {
            XCTFail(@"The bundle job returned an error.");
            return;
        }
        
        NSLog(@"PASSED!!!");
    }
}

@end
