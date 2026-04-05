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

- (int) TEST_C {
    return 50;
}

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
        
        
        if (file.mIsFolder) {
            
            NSLog(@"    aJob.AddFolder(\"%@\");", escapedName);
            
        } else {
            NSString *escapedContent = [[content stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"]
                                             stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
            
            NSLog(@"    aJob.AddFile(\"%@\", \"%@\");", escapedName, escapedContent);
        }
        
        
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
    
    vector<FakeFileBlockSpan> aBlockSpans;
    if (!TestBundle::GetBlockSpans(pJob, &aBlockSpans, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
        
        
    
    return YES;
}

- (void)test_100_full_spectrum {
    
    for (int aPayloadBytesPerBlock=1;aPayloadBytesPerBlock<=4;aPayloadBytesPerBlock++) {
        printf("aPayloadBytesPerBlock = %d\n", aPayloadBytesPerBlock);
        
        for (int aBlocksPerArchive=1;aBlocksPerArchive<=4;aBlocksPerArchive++) {
            printf("\taBlocksPerArchive = %d\n", aBlocksPerArchive);
            
            for (int aRepair=0;aRepair<=80;aRepair+=20) {
                for (int aPreview=0;aPreview<2;aPreview++) {
                    for (int aTestIndex=0;aTestIndex<[self TEST_C];aTestIndex++) {
                        
                        int aFileCount = Random::Get(1, 8);
                        vector<FakeFile> aFiles = Words::GetRandomFiles(aFileCount, 1, 4, 0, 8);
                        JobBundle aJob;
                        for (auto aFile: aFiles) {
                            aJob.mFileList.push_back(aFile);
                        }
                        aJob.mPayloadBytesPerBlock = aPayloadBytesPerBlock;
                        aJob.mBlocksPerArchive = aBlocksPerArchive;
                        
                        if (aPreview == 0) {
                            aJob.mPreviewEnabled = false;
                        } else {
                            aJob.mPreviewEnabled = true;
                        }
                        
                        if (aRepair == 20) {
                            aJob.SetRepair20();
                        } else if (aRepair == 40) {
                            aJob.SetRepair40();
                        } else if (aRepair == 60) {
                            aJob.SetRepair60();
                        } else if (aRepair == 80) {
                            aJob.SetRepair80();
                        } else {
                            aJob.SetRepairOff();
                        }
                        
                        if ([self run: aJob] == NO) {
                            XCTFail(@"The bundle job returned an error.");
                            return;
                        }
                    }
                }
            }
        }
    }
}

@end
