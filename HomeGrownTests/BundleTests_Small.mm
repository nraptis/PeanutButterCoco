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
#include "TestBundleWithHooks.hpp"
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
    NSLog(@"    aJob.mPreviewEnabled = %d;", pJob.mPreviewEnabled);
    NSLog(@"    aJob.mRepairCoverage = %d;", pJob.mRepairCoverage);
    
    
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
                
                /*
                if (pFeedback.mBatch == 1u) {
                    printf("[bundle-hooks] phase start: %s\n", pFeedback.mPhase);
                }
                
                printf("[bundle-hooks] phase tick: %s batch=%zu\n",
                       pFeedback.mPhase,
                       pFeedback.mBatch);
                
                if (!pFeedback.mNeedsMoreHeartbeats) {
                    printf("[bundle-hooks] phase end: %s\n", pFeedback.mPhase);
                }
                */
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
    
    for (int aPayloadBytesPerBlock=1;aPayloadBytesPerBlock<=12;aPayloadBytesPerBlock+=1) {
        printf("aPayloadBytesPerBlock = %d\n", aPayloadBytesPerBlock);
        
        for (int aBlocksPerArchive=4;aBlocksPerArchive<=8;aBlocksPerArchive+=1) {
            printf("\taBlocksPerArchive = %d\n", aBlocksPerArchive);
            
            //for (int aRepair=0;aRepair<=80;aRepair+=20) {
            for (int aRepair=0;aRepair<=60;aRepair+=40) {
                
                for (int aPreview=0;aPreview<2;aPreview++) {
                    for (int aTestIndex=0;aTestIndex<4;aTestIndex++) {
                        
                        int aFileCount = Random::Get(0, 12);
                        int aFolderCount = Random::Get(0, 12);
                        
                        if ((aFileCount == 0) && (aFolderCount == 0)) {
                            if (Random::Get(2) == 0) {
                                aFileCount = 1;
                            } else {
                                aFolderCount = 1;
                            }
                        }
                        
                        //vector<FakeFile> aFiles = Words::GetRandomFiles(aFileCount, 1, 4, 0, 8);
                        
                        int aFileNameLo = Random::Get(1, 4);
                        int aFileNameHi = aFileNameLo + Random::Get(0, 12);
                        
                        int aFileContentLo = Random::Get(0, 8);
                        int aFileContentHi = aFileContentLo + Random::Get(0, 12);
                        
                        int aFolderNameLo = Random::Get(1, 4);
                        int aFolderNameHi = aFolderNameLo + Random::Get(0, 12);
                        
                        vector<FakeFile> aFiles = Words::GetRandomFilesAndFolders(aFileCount, aFileNameLo, aFileNameHi,
                                                                                  aFileContentLo, aFileContentHi,
                                                                                  aFolderCount, aFolderNameLo, aFolderNameHi);
                        
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
