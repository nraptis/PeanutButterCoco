//
//  RecoverTests_Small.m
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
#include "FakeMangleTool.hpp"

@interface RecoverTests_Small : XCTestCase
@end

@implementation RecoverTests_Small

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
    NSLog(@"\n    MockHardDrive aHardDrive;");
    NSLog(@"    vector<FakeFileBlockSpan> aBlockSpans;");
    NSLog(@"    vector<FakeArchive> aMockArchives;");
    NSLog(@"");
    NSLog(@"    if ([self run_stepA:aJob");
    NSLog(@"          withHardDrive:aHardDrive");
    NSLog(@"         withFakeArchives:&aMockArchives");
    NSLog(@"         withBlockSpans:&aBlockSpans] == NO) {");
    NSLog(@"        XCTFail(@\"Step A failed.\");");
    NSLog(@"        return;");
    NSLog(@"    }");
    NSLog(@"");
    NSLog(@"    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {");
    NSLog(@"        XCTFail(@\"Step B failed.\");");
    NSLog(@"        return;");
    NSLog(@"    }");
    NSLog(@"");
    NSLog(@"    int aArchiveIndex1 = 0;");
    NSLog(@"    int aBlockIndex1 = 0;");
    NSLog(@"    string aArchiveName1 = \"asdf\";");
    NSLog(@"");
    NSLog(@"    ByteString aErrorString;");
    NSLog(@"    if (!aHardDrive.MangleBlock(aArchiveName1, aBlockIndex1, aJob, &aErrorString)) {");
    NSLog(@"        printf(\"Error: %%.*s\\n\", aErrorString.mLength, (char*)aErrorString.mData);");
    NSLog(@"        XCTFail(@\"Hard-drive mangle failed.\");");
    NSLog(@"        return;");
    NSLog(@"    }");
    NSLog(@"");
    NSLog(@"    if (!FakeMangleTool::MangleBlock(aArchiveIndex1,");
    NSLog(@"                                    aBlockIndex1,");
    NSLog(@"                                    &aJob.mFileList,");
    NSLog(@"                                    &aMockArchives,");
    NSLog(@"                                    &aBlockSpans,");
    NSLog(@"                                    &aErrorString)) {");
    NSLog(@"        printf(\"Error: %%.*s\\n\", aErrorString.mLength, (char*)aErrorString.mData);");
    NSLog(@"        XCTFail(@\"Fake mangle failed.\");");
    NSLog(@"        return;");
    NSLog(@"    }");
    NSLog(@"");
    NSLog(@"    if ([self run_stepC:aJob withHardDrive:aHardDrive] == NO) {");
    NSLog(@"        XCTFail(@\"Step C failed.\");");
    NSLog(@"        return;");
    NSLog(@"    }");
    NSLog(@"");
    NSLog(@"    // Optional: replace placeholders with concrete values from the failing run.");
    NSLog(@"");
    NSLog(@"    NSLog(@\"Regression case reproduced.\");");
    NSLog(@"}");
    
    NSLog(@"\n🔥 END REPRO CODE 🔥\n");
}

- (BOOL) run_stepA: (JobBundle &)pJob
     withHardDrive: (MockHardDrive &)pHardDrive
      withFakeArchives: (vector<FakeArchive> *) pFakeArchives
    withBlockSpans: (vector<FakeFileBlockSpan> *) pBlockSpans {
    
    if (pFakeArchives == NULL) {
        printf("Error: fake archive vector missing...\n");
        [self logRegression: pJob];
        return NO;
    }
    
    
    if (pBlockSpans == NULL) {
        printf("Error: block span vector missing...\n");
        [self logRegression: pJob];
        return NO;
        
    }
    
    MockFileSystem aFileSystem(&pHardDrive);
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
    
    if (!TestBundle::PerformMock(pJob, pFakeArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    if (!BundleVerify::Execute(pJob, aRealArchives, *pFakeArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
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
                                                [self logRegression: pJob];
                                                return NO;
                                            }
    
    vector <FakeFile> aFiles;
    if (!TestUnbundleWithHooks::CollectFiles(pJob, aFiles, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    if (!UnbundleVerify::Execute(aFiles, pJob.mFileList, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    if (!TestBundle::GetBlockSpans(pJob, pBlockSpans, pFakeArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    return YES;
}

- (BOOL) run_stepB: (JobBundle &)pJob withHardDrive: (MockHardDrive &)pHardDrive {
    
    MockFileSystem aFileSystem(&pHardDrive);
    ByteString aErrorString;
    pHardDrive.ClearDirectory(pJob.mUnarchived.ToString());
    
    if (!TestUnbundleWithHooks::PerformRealRecover(
                                            pJob,
                                            aFileSystem,
                                            [](const TestUnbundleWithHooks::PhaseBatchFeedback &pFeedback,
                                               peanutbutter::DecodeStageContextV2 &pContext,
                                               SimpleDecodeRuntime &pRuntime) {
                                                   (void)pFeedback;
                                                   (void)pContext;
                                                   (void)pRuntime;
                                               },
                                            &aErrorString)) {
                                                printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                                                [self logRegression: pJob];
                                                return NO;
                                            }
    
    vector <FakeFile> aFiles;
    if (!TestUnbundleWithHooks::CollectFiles(pJob, aFiles, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    if (!UnbundleVerify::Execute(aFiles, pJob.mFileList, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    return YES;
}

- (BOOL) run_stepC: (JobBundle &)pJob withHardDrive: (MockHardDrive &)pHardDrive {
    
    MockFileSystem aFileSystem(&pHardDrive);
    ByteString aErrorString;
    pHardDrive.ClearDirectory(pJob.mUnarchived.ToString());
    
    const bool aRecoverSucceeded = TestUnbundleWithHooks::PerformRealRecover(
                                                                          pJob,
                                                                          aFileSystem,
                                                                          [](const TestUnbundleWithHooks::PhaseBatchFeedback &pFeedback,
                                                                             peanutbutter::DecodeStageContextV2 &pContext,
                                                                             SimpleDecodeRuntime &pRuntime) {
                                                                                 (void)pRuntime;
                                                                                 if (!pFeedback.mRunSucceeded) {
                                                                                     const string aError = pContext.LastErrorLog();
                                                                                     printf("Recover failed in phase '%s' (stage=%d): %s\n",
                                                                                            pFeedback.mPhase,
                                                                                            (int)pFeedback.mStage,
                                                                                            aError.c_str());
                                                                                 }
                                                                             },
                                                                          &aErrorString);
    if (!aRecoverSucceeded) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        printf("Recover returned an error; validating damaged output state anyway.\n");
    }

    vector <FakeFile> aFiles;
    if (!TestUnbundleWithHooks::CollectFiles(pJob, aFiles, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    if (!UnbundleVerify::Execute_Damaged(aFiles, pJob.mFileList, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        [self logRegression: pJob];
        return NO;
    }
    
    return YES;
}

- (void)test_100_full_spectrum {
    
    for (int aPayloadBytesPerBlock=10;aPayloadBytesPerBlock<=24;aPayloadBytesPerBlock+=1) {
        printf("aPayloadBytesPerBlock = %d\n", aPayloadBytesPerBlock);
        
        for (int aBlocksPerArchive=2;aBlocksPerArchive<=8;aBlocksPerArchive+=1) {
            printf("\taBlocksPerArchive = %d\n", aBlocksPerArchive);
            
            for (int aRepair=0;aRepair<=80;aRepair+=20) {
            //for (int aRepair=0;aRepair<=60;aRepair+=40) {
                
                for (int aPreview=0;aPreview<2;aPreview++) {
                    for (int aTestIndex=0;aTestIndex<36;aTestIndex++) {
                        
                        int aFileCount = Random::Get(0, 3);
                        int aFolderCount = Random::Get(0, 3);
                        
                        if ((aFileCount == 0) && (aFolderCount == 0)) {
                            if (Random::Get(2) == 0) {
                                aFileCount = 1;
                            } else {
                                aFolderCount = 1;
                            }
                        }
                        
                        //vector<FakeFile> aFiles = Words::GetRandomFiles(aFileCount, 1, 4, 0, 8);
                        
                        int aFileNameLo = Random::Get(1, 4);
                        int aFileNameHi = aFileNameLo + Random::Get(0, 4);
                        
                        int aFileContentLo = Random::Get(0, 8);
                        int aFileContentHi = aFileContentLo + Random::Get(0, 4);
                        
                        int aFolderNameLo = Random::Get(1, 4);
                        int aFolderNameHi = aFolderNameLo + Random::Get(0, 4);
                        
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
                        
                        MockHardDrive aHardDrive;
                        vector<FakeFileBlockSpan> aBlockSpans;
                        vector<FakeArchive> aMockArchives;
                        
                        if ([self run_stepA:aJob
                              withHardDrive:aHardDrive
                             withFakeArchives: &aMockArchives
                             withBlockSpans:&aBlockSpans] == NO) {
                            XCTFail(@"The recover job returned an error.");
                            return;
                        }
                        
                        if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
                            XCTFail(@"The recover job returned an error.");
                            return;
                        }
                        
                        int aArchiveIndex = Random::Get((int)aMockArchives.size());
                        int aBlockIndex = Random::Get((int)aMockArchives[aArchiveIndex].mBlocks.size());
                        
                        
                        
                        auto aArchiveFiles = aHardDrive.ListFilesRecursive(aJob.mArchived.ToString());
                        
                        string aArchiveName = aArchiveFiles[aArchiveIndex];
                        
                        
                        
                        ByteString aErrorString;
                        if (!aHardDrive.MangleBlock(aArchiveName, aBlockIndex, aJob, &aErrorString)) {
                            printf("Failed By Deleting: %s [index %d]\n", aArchiveName.c_str(), aArchiveIndex);
                            printf("Failed By Deleting: %s [block %d]\n", aArchiveName.c_str(), aBlockIndex);
                            printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                            [self logRegression: aJob];
                            XCTFail(@"The bundle job returned an error.");
                            return;
                        }
                        
                        if (!FakeMangleTool::MangleBlock(aArchiveIndex,
                                                        aBlockIndex,
                                                        &aJob.mFileList,
                                                        &aMockArchives,
                                                        &aBlockSpans,
                                                        &aErrorString)) {
                            printf("Failed By Deleting: %s [index %d]\n", aArchiveName.c_str(), aArchiveIndex);
                            printf("Failed By Deleting: %s [block %d]\n", aArchiveName.c_str(), aBlockIndex);
                            printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                            [self logRegression: aJob];
                            XCTFail(@"The bundle job returned an error.");
                            return;
                        }
                        
                        if ([self run_stepC:aJob withHardDrive:aHardDrive] == NO) {
                            printf("Failed By Deleting: %s [index %d]\n", aArchiveName.c_str(), aArchiveIndex);
                            printf("Failed By Deleting: %s [block %d]\n", aArchiveName.c_str(), aBlockIndex);
                            XCTFail(@"The bundle job returned an error.");
                            return;
                            
                        }
                        
                        // aHardDrive.MangleFile(<#const std::string &pPath#>, <#ByteString *pError#>)
                        
                        
                        
                    }
                }
            }
        }
    }
}

- (void)test_100_full_spectrum_two {
    
    for (int aPayloadBytesPerBlock=4;aPayloadBytesPerBlock<=24;aPayloadBytesPerBlock+=4) {
        printf("aPayloadBytesPerBlock = %d\n", aPayloadBytesPerBlock);
        
        for (int aBlocksPerArchive=4;aBlocksPerArchive<=8;aBlocksPerArchive+=4) {
            printf("\taBlocksPerArchive = %d\n", aBlocksPerArchive);
            
            for (int aRepair=0;aRepair<=80;aRepair+=20) {
            //for (int aRepair=0;aRepair<=60;aRepair+=40) {
                
                for (int aPreview=0;aPreview<2;aPreview++) {
                    for (int aTestIndex=0;aTestIndex<36;aTestIndex++) {
                        
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
                        
                        MockHardDrive aHardDrive;
                        vector<FakeFileBlockSpan> aBlockSpans;
                        vector<FakeArchive> aMockArchives;
                        
                        if ([self run_stepA:aJob
                              withHardDrive:aHardDrive
                             withFakeArchives: &aMockArchives
                             withBlockSpans:&aBlockSpans] == NO) {
                            XCTFail(@"The recover job returned an error.");
                            return;
                        }
                        
                        if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
                            XCTFail(@"The recover job returned an error.");
                            return;
                        }
                        
                        int aArchiveIndex1 = Random::Get((int)aMockArchives.size());
                        int aBlockIndex1 = Random::Get((int)aMockArchives[aArchiveIndex1].mBlocks.size());
                        
                        int aArchiveIndex2 = Random::Get((int)aMockArchives.size());
                        int aBlockIndex2 = Random::Get((int)aMockArchives[aArchiveIndex2].mBlocks.size());
                        
                        if ((aArchiveIndex1 == aArchiveIndex2) && (aBlockIndex1 == aBlockIndex2)) {
                            continue;
                        }
                        
                        auto aArchiveFiles = aHardDrive.ListFilesRecursive(aJob.mArchived.ToString());
                        
                        string aArchiveName1 = aArchiveFiles[aArchiveIndex1];
                        string aArchiveName2 = aArchiveFiles[aArchiveIndex2];
                        
                        int aAction1 = Random::Get(2);
                        int aAction2 = Random::Get(2);
                        
                        int aDeleteCount = 0;
                        if (aAction1 == 1) aDeleteCount++;
                        if (aAction2 == 1) aDeleteCount++;
                        
                        if (aDeleteCount > 1) {
                            if (aArchiveIndex1 == aArchiveIndex2) {
                                
                                if (aArchiveIndex1 >= 0 && aArchiveIndex1 < aMockArchives[aArchiveIndex1].mBlocks.size()) {
                                    int aBlockCount = (int)aMockArchives[aArchiveIndex1].mBlocks.size();
                                    if (aBlockIndex1 >= (aBlockCount - 2)) {
                                        printf("i bad case: over-deleting. %d %d | %d\n", aBlockIndex1, aBlockIndex2, aBlockCount);
                                        continue;
                                    }
                                    if (aBlockIndex2 >= (aBlockCount - 2)) {
                                        printf("i bad case: over-deleting. %d %d | %d\n", aBlockIndex1, aBlockIndex2, aBlockCount);
                                        continue;
                                    }
                                }
                            }
                        } else if (aDeleteCount == 1) {
                            if (aArchiveIndex1 == aArchiveIndex2) {
                                
                                if (aArchiveIndex1 >= 0 && aArchiveIndex1 < aMockArchives[aArchiveIndex1].mBlocks.size()) {
                                    int aBlockCount = (int)aMockArchives[aArchiveIndex1].mBlocks.size();
                                    if (aBlockIndex1 >= (aBlockCount - 1)) {
                                        printf("ii bad case: over-deleting. %d %d | %d\n", aBlockIndex1, aBlockIndex2, aBlockCount);
                                        continue;
                                    }
                                    if (aBlockIndex2 >= (aBlockCount - 1)) {
                                        printf("ii bad case: over-deleting. %d %d | %d\n", aBlockIndex1, aBlockIndex2, aBlockCount);
                                        continue;
                                    }
                                }
                            }
                        }
                        
                        
                        ByteString aErrorString;
                        
                        if (aAction1 == 0) {
                            if (!aHardDrive.MangleBlock(aArchiveName1, aBlockIndex1, aJob, &aErrorString)) {
                                
                                printf("aAction1: [index %d]\n", aAction1);
                                printf("aAction2: [index %d]\n", aAction2);
                                
                                printf("aName1: %s\n", aArchiveName1.c_str());
                                printf("aName2: %s\n", aArchiveName2.c_str());
                                
                                
                                printf("aArchiveIndex1: %d\n", aArchiveIndex1);
                                printf("aArchiveIndex2: %d\n", aArchiveIndex2);
                                
                                printf("aBlockIndex1: %d\n", aBlockIndex1);
                                printf("aBlockIndex2: %d\n", aBlockIndex2);

                                printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                                [self logRegression: aJob];
                                XCTFail(@"The bundle job returned an error.");
                                return;
                            }
                        } else {
                            if (!aHardDrive.DeleteBlock(aArchiveName1, aBlockIndex1, aJob, &aErrorString)) {
                                printf("aAction1: [index %d]\n", aAction1);
                                printf("aAction2: [index %d]\n", aAction2);
                                
                                printf("aName1: %s\n", aArchiveName1.c_str());
                                printf("aName2: %s\n", aArchiveName2.c_str());
                                
                                
                                printf("aArchiveIndex1: %d\n", aArchiveIndex1);
                                printf("aArchiveIndex2: %d\n", aArchiveIndex2);
                                
                                printf("aBlockIndex1: %d\n", aBlockIndex1);
                                printf("aBlockIndex2: %d\n", aBlockIndex2);

                                printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                                [self logRegression: aJob];
                                XCTFail(@"The bundle job returned an error.");
                                return;
                            }
                        }
                        
                        if (aAction2 == 0) {
                            if (!aHardDrive.MangleBlock(aArchiveName2, aBlockIndex2, aJob, &aErrorString)) {
                                printf("aAction1: [index %d]\n", aAction1);
                                printf("aAction2: [index %d]\n", aAction2);
                                
                                printf("aName1: %s\n", aArchiveName1.c_str());
                                printf("aName2: %s\n", aArchiveName2.c_str());
                                
                                
                                printf("aArchiveIndex1: %d\n", aArchiveIndex1);
                                printf("aArchiveIndex2: %d\n", aArchiveIndex2);
                                
                                printf("aBlockIndex1: %d\n", aBlockIndex1);
                                printf("aBlockIndex2: %d\n", aBlockIndex2);

                                printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                                [self logRegression: aJob];
                                XCTFail(@"The bundle job returned an error.");
                                return;
                            }
                        } else {
                            if (!aHardDrive.DeleteBlock(aArchiveName2, aBlockIndex2, aJob, &aErrorString)) {
                                printf("aAction1: [index %d]\n", aAction1);
                                printf("aAction2: [index %d]\n", aAction2);
                                
                                printf("aName1: %s\n", aArchiveName1.c_str());
                                printf("aName2: %s\n", aArchiveName2.c_str());
                                
                                
                                printf("aArchiveIndex1: %d\n", aArchiveIndex1);
                                printf("aArchiveIndex2: %d\n", aArchiveIndex2);
                                
                                printf("aBlockIndex1: %d\n", aBlockIndex1);
                                printf("aBlockIndex2: %d\n", aBlockIndex2);

                                [self logRegression: aJob];
                                XCTFail(@"The bundle job returned an error.");
                                return;
                            }
                        }
                        
                        const bool aFakeAction1 = (aAction1 == 0)
                            ? FakeMangleTool::MangleBlock(aArchiveIndex1,
                                                          aBlockIndex1,
                                                          &aJob.mFileList,
                                                          &aMockArchives,
                                                          &aBlockSpans,
                                                          &aErrorString)
                            : FakeMangleTool::DeleteBlock(aArchiveIndex1,
                                                          aBlockIndex1,
                                                          &aJob.mFileList,
                                                          &aMockArchives,
                                                          &aBlockSpans,
                                                          &aErrorString);
                        if (!aFakeAction1) {
                            printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                            [self logRegression: aJob];
                            printf("aAction1: [index %d]\n", aAction1);
                            printf("aAction2: [index %d]\n", aAction2);
                            
                            printf("aName1: %s\n", aArchiveName1.c_str());
                            printf("aName2: %s\n", aArchiveName2.c_str());
                            
                            
                            printf("aArchiveIndex1: %d\n", aArchiveIndex1);
                            printf("aArchiveIndex2: %d\n", aArchiveIndex2);
                            
                            printf("aBlockIndex1: %d\n", aBlockIndex1);
                            printf("aBlockIndex2: %d\n", aBlockIndex2);

                            XCTFail(@"The bundle job returned an error.");
                            return;
                        }
                        
                        const bool aFakeAction2 = (aAction2 == 0)
                            ? FakeMangleTool::MangleBlock(aArchiveIndex2,
                                                          aBlockIndex2,
                                                          &aJob.mFileList,
                                                          &aMockArchives,
                                                          &aBlockSpans,
                                                          &aErrorString)
                            : FakeMangleTool::DeleteBlock(aArchiveIndex2,
                                                          aBlockIndex2,
                                                          &aJob.mFileList,
                                                          &aMockArchives,
                                                          &aBlockSpans,
                                                          &aErrorString);
                        if (!aFakeAction2) {
                            printf("aAction1: [index %d]\n", aAction1);
                            printf("aAction2: [index %d]\n", aAction2);
                            
                            printf("aName1: %s\n", aArchiveName1.c_str());
                            printf("aName2: %s\n", aArchiveName2.c_str());
                            
                            
                            printf("aArchiveIndex1: %d\n", aArchiveIndex1);
                            printf("aArchiveIndex2: %d\n", aArchiveIndex2);
                            
                            printf("aBlockIndex1: %d\n", aBlockIndex1);
                            printf("aBlockIndex2: %d\n", aBlockIndex2);

                            printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                            [self logRegression: aJob];
                            XCTFail(@"The bundle job returned an error.");
                            return;
                        }
                        
                        if ([self run_stepC:aJob withHardDrive:aHardDrive] == NO) {
                            printf("aAction1: [index %d]\n", aAction1);
                            printf("aAction2: [index %d]\n", aAction2);
                            
                            printf("aName1: %s\n", aArchiveName1.c_str());
                            printf("aName2: %s\n", aArchiveName2.c_str());
                            
                            
                            printf("aArchiveIndex1: %d\n", aArchiveIndex1);
                            printf("aArchiveIndex2: %d\n", aArchiveIndex2);
                            
                            printf("aBlockIndex1: %d\n", aBlockIndex1);
                            printf("aBlockIndex2: %d\n", aBlockIndex2);

                            XCTFail(@"The bundle job returned an error.");
                            return;
                            
                        }
                        
                        // aHardDrive.MangleFile(<#const std::string &pPath#>, <#ByteString *pError#>)
                        
                        
                        
                    }
                }
            }
        }
    }
}

@end
