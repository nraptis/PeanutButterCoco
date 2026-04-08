//
//  RoundTripTests_Medium.m
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
#include "MutationTool.hpp"
#include "RoundTripTests.h"

@interface RoundTripTests_Medium : XCTestCase
@end

@implementation RoundTripTests_Medium

- (void) logRegression: (JobBundle &)pJob withMutations: (vector<FakeMutation> *)pMutations {
    
    NSLog(@"\n🔥 TEST FAILURE REPRO CODE 🔥\n");
    
    NSLog(@"- (void)test_fulltrip_regression_med_AAA {");
    NSLog(@"    JobBundle aJob;");
    NSLog(@"    aJob.mPayloadBytesPerBlock = %d;", pJob.mPayloadBytesPerBlock);
    NSLog(@"    aJob.mBlocksPerArchive = %d;", pJob.mBlocksPerArchive);
    
    if (pJob.mPreviewEnabled) {
        NSLog(@"    aJob.mPreviewEnabled = true;");
    }
    if (pJob.mRepairCoverage != 0) {
        NSLog(@"    aJob.mRepairCoverage = %d;", (int)pJob.mRepairCoverage);
    }
    
    for (const FakeFile &file : pJob.mFileList) {
        NSString *name = [NSString stringWithUTF8String:file.mName.ToString().c_str()];
        NSString *content = [NSString stringWithUTF8String:file.mContent.ToString().c_str()];
        if (name == nil) {
            name = @"";
        }
        if (content == nil) {
            content = @"";
        }
        
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
    
    NSLog(@"");
    NSLog(@"    vector<FakeArchive> aMockArchives;");
    NSLog(@"    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {");
    NSLog(@"        XCTFail(@\"Round trip regression failed on happy flow.\");");
    NSLog(@"        return;");
    NSLog(@"    }");
    NSLog(@"    vector<FakeMutation> aMutations;");
    if ((pMutations == NULL) || (pMutations->size() <= 0)) {
        NSLog(@"    // No mutations were captured for this failure point.");
    } else {
        for (const FakeMutation &aMutation : *pMutations) {
            NSString *aPrimaryArchiveName =
                [NSString stringWithUTF8String:aMutation.mPrimaryArchiveFileName.ToString().c_str()];
            NSString *aSecondaryArchiveName =
                [NSString stringWithUTF8String:aMutation.mSecondaryArchiveFileName.ToString().c_str()];
            if (aPrimaryArchiveName == nil) {
                aPrimaryArchiveName = @"";
            }
            if (aSecondaryArchiveName == nil) {
                aSecondaryArchiveName = @"";
            }
            NSString *aPrimaryArchiveNameEscaped =
                [[aPrimaryArchiveName stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"]
                 stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
            NSString *aSecondaryArchiveNameEscaped =
                [[aSecondaryArchiveName stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"]
                 stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
            
            NSLog(@"    {");
            NSLog(@"        FakeMutation aMutation;");
            
            if (aMutation.mMutationKind == FakeMutation::MutationKind::kMangleBlock) {
                NSLog(@"        aMutation.SetMangleBlock(\"%@\", %d, %d);",
                      aPrimaryArchiveNameEscaped,
                      aMutation.mArchiveIndex,
                      aMutation.mBlockIndex);
            } else if (aMutation.mMutationKind == FakeMutation::MutationKind::kDeleteBlock) {
                NSLog(@"        aMutation.SetDeleteBlock(\"%@\", %d, %d);",
                      aPrimaryArchiveNameEscaped,
                      aMutation.mArchiveIndex,
                      aMutation.mBlockIndex);
            } else if (aMutation.mMutationKind == FakeMutation::MutationKind::kMangleArchive) {
                NSLog(@"        aMutation.SetMangleArchive(\"%@\", %d);",
                      aPrimaryArchiveNameEscaped,
                      aMutation.mArchiveIndex);
            } else if (aMutation.mMutationKind == FakeMutation::MutationKind::kDeleteArchive) {
                NSLog(@"        aMutation.SetDeleteArchive(\"%@\", %d);",
                      aPrimaryArchiveNameEscaped,
                      aMutation.mArchiveIndex);
            } else if (aMutation.mMutationKind == FakeMutation::MutationKind::kSwapBlocks) {
                NSLog(@"        aMutation.SetSwapBlocks(\"%@\", %d, %d, \"%@\", %d, %d);",
                      aPrimaryArchiveNameEscaped,
                      aMutation.mArchiveIndex,
                      aMutation.mBlockIndex,
                      aSecondaryArchiveNameEscaped,
                      aMutation.mArchiveIndexB,
                      aMutation.mBlockIndexB);
            }
            
            else {
                NSString *aKind = @"kNone";
                if (aMutation.mMutationKind == FakeMutation::MutationKind::kSwapBlocks) {
                    aKind = @"kSwapBlocks";
                } else if (aMutation.mMutationKind == FakeMutation::MutationKind::kDeleteArchive) {
                    aKind = @"kDeleteArchive";
                } else if (aMutation.mMutationKind == FakeMutation::MutationKind::kMangleArchive) {
                    aKind = @"kMangleArchive";
                }
                
                NSLog(@"        aMutation.mMutationKind = FakeMutation::MutationKind::%@;", aKind);
                NSLog(@"        aMutation.mArchiveIndex = %d;", aMutation.mArchiveIndex);
                NSLog(@"        aMutation.mBlockIndex = %d;", aMutation.mBlockIndex);
                NSLog(@"        aMutation.mArchiveIndexB = %d;", aMutation.mArchiveIndexB);
                NSLog(@"        aMutation.mBlockIndexB = %d;", aMutation.mBlockIndexB);
                if (aMutation.mPrimaryArchiveFileName.mLength > 0) {
                    NSLog(@"        aMutation.mPrimaryArchiveFileName.Set(\"%@\");",
                          aPrimaryArchiveNameEscaped);
                }
                if (aMutation.mSecondaryArchiveFileName.mLength > 0) {
                    NSLog(@"        aMutation.mSecondaryArchiveFileName.Set(\"%@\");",
                          aSecondaryArchiveNameEscaped);
                }
            }
            
            NSLog(@"        aMutations.push_back(aMutation);");
            NSLog(@"    }");
        }
    }
    NSLog(@"");
    NSLog(@"    if (![RoundTripTests run_CorruptUnbundle:aJob withMutations:&aMutations]) {");
    NSLog(@"        XCTFail(@\"Round trip regression failed on corrupt unbundle.\");");
    NSLog(@"        return;");
    NSLog(@"    }");
    NSLog(@"");
    NSLog(@"    if (![RoundTripTests run_CorruptRecover:aJob withMutations:&aMutations]) {");
    NSLog(@"        XCTFail(@\"Round trip regression failed on corrupt recover.\");");
    NSLog(@"        return;");
    NSLog(@"    }");
    NSLog(@"}");
    
    NSLog(@"\n🔥 END REPRO CODE 🔥\n");
}

- (void)test_100_full_mangle_one_archive {
    
    for (int aPayloadBytesPerBlock=24;aPayloadBytesPerBlock<=48;aPayloadBytesPerBlock+=4) {
        printf("aPayloadBytesPerBlock = %d\n", aPayloadBytesPerBlock);
        
        for (int aBlocksPerArchive=6;aBlocksPerArchive<=16;aBlocksPerArchive+=2) {
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
                         
                         int aFileNameLo = Random::Get(1, 8);
                         int aFileNameHi = aFileNameLo + Random::Get(0, 8);
                         
                         int aFileContentLo = Random::Get(0, 8);
                         int aFileContentHi = aFileContentLo + Random::Get(0, 8);
                         
                         int aFolderNameLo = Random::Get(1, 8);
                         int aFolderNameHi = aFolderNameLo + Random::Get(0, 8);
                         
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
                        
                        
                        //vector<FakeArchive> aMockArchives;
                        
                        vector<FakeArchive> aMockArchives;
                        if (![RoundTripTests run_HappyFlow: aJob withArchives: &aMockArchives]) {
                            [self logRegression:aJob withMutations:NULL];
                            XCTFail(@"Round trip medium failed on happy flow.");
                            return;
                        }
                        
                        //int aArchiveIndex = Random::Get((int)aMockArchives.size());
                        //int aBlockIndex = Random::Get((int)aMockArchives[aArchiveIndex].mBlocks.size());
                        //run_CorruptUnbundle
                        
                        
                        //MockHardDrive aHardDrive;
                        //vector<FakeFileBlockSpan> aBlockSpans;
                        
                        //int aArchiveIndex = Random::Get((int)aMockArchives.size());
                        //int aBlockIndex = Random::Get((int)aMockArchives[aArchiveIndex].mBlocks.size());
                        //string aArchiveName = aMockArchives[aArchiveIndex].mFilePath.ToString();
                        
                        //FakeMutation aMutation;
                        //aMutation.SetMangleBlock(aArchiveName, aArchiveIndex, aBlockIndex);
                        
                        vector<FakeMutation> aMutations;
                        
                        /*
                        FakeMutation::AttemptGenerateRandomArchiveDestruction(Random::Get(3),
                                                                          &aMockArchives,
                                                                          &aMutations);
                        */
                        
                        //FakeMutation::AttemptGenerateRandomBlockSwaps(3, &aMockArchives, &aMutations);

                        //FakeMutation::AttemptGenerateRandomArchiveDeletions(4, &aMockArchives, &aMutations);
                        //FakeMutation::AttemptGenerateRandomArchiveMangles(4, &aMockArchives, &aMutations);
                        //FakeMutation::AttemptGenerateRandomBlockDeletions(4, &aMockArchives, &aMutations);
                        //FakeMutation::AttemptGenerateRandomBlockMangles(4, &aMockArchives, &aMutations);

                        //This fails pretty quickly as soon as count is 4.
                        FakeMutation::AttemptGenerateRandom(3, &aMockArchives, &aMutations);
                        
                        
                        
                        
                        
                        //aMutations.push_back(aMutation);
                        
                        
                        if (![RoundTripTests run_CorruptUnbundle:aJob withMutations:&aMutations]) {
                            [self logRegression:aJob withMutations:&aMutations];
                            XCTFail(@"Round trip medium failed on corrupt unbundle.");
                            return;
                        }
                        
                        if (![RoundTripTests run_CorruptRecover:aJob withMutations:&aMutations]) {
                            [self logRegression:aJob withMutations:&aMutations];
                            XCTFail(@"Round trip medium failed on corrupt recover.");
                            return;
                        }
                        
                        
                        
                        
                        // auto aArchiveFiles = aHardDrive.ListFilesRecursive(aJob.mArchived.ToString());
                        // string aArchiveName = aArchiveFiles[aArchiveIndex];
                        
                        
                    }
                }
            }
        }
    }
}

- (void)test_100_full_spectrum {
    
    for (int aPayloadBytesPerBlock=1;aPayloadBytesPerBlock<=100;aPayloadBytesPerBlock+=6) {
        printf("aPayloadBytesPerBlock = %d\n", aPayloadBytesPerBlock);
        
        for (int aBlocksPerArchive=10;aBlocksPerArchive<=12;aBlocksPerArchive+=2) {
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
                        
                        
                        
                        int aNameLo = Random::Get(1, 180);
                        int aLameHi = aNameLo + Random::Get(0, 100);
                        //vector <ByteString> aNames = Words::GetRandomFolderNames(Random::Get(1, 4), 2, 4);
                        vector <ByteString> aNames = Words::GetRandomFolderNames(Random::Get(1, 12), aNameLo, aLameHi);
                        
                        vector<FakeFile> aFiles;
                        
                        for (auto aName: aNames) {
                            
                            int aDupeCount = Random::Get(1, 4);
                            
                            
                            for (int aDupeIndex=0;aDupeIndex<aDupeCount;aDupeIndex++) {
                                
                                ByteString aModifiedName;
                                if (aDupeIndex == 0) {
                                    aModifiedName.Set(aName);
                                } else if (aDupeIndex == 1) {
                                    aModifiedName.Set(ByteString("$PARTIAL_") + aName);
                                } else {
                                    aModifiedName.Set(ByteString("$PARTIAL_") + aName + ByteString("_") + ByteString(aDupeIndex - 1));
                                }
                                
                                int aIsFolder = Random::Get(2);
                                
                                if (aIsFolder) {
                                    FakeFile aFile;
                                    aFile.mName.Set(aModifiedName);
                                    aFile.mIsFolder = true;
                                    aFiles.push_back(aFile);
                                } else {
                                    FakeFile aFile;
                                    aFile.mName.Set(aModifiedName);
                                    aFile.mContent.Set(Words::GetRandomFileContent(0, 100));
                                    aFile.mIsFolder = false;
                                    aFiles.push_back(aFile);
                                }
                                
                            }
                            
                            
                            
                        }
                        
                        
                        /*
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
                        */
                        
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
                        
                        
                        //vector<FakeArchive> aMockArchives;
                        
                        vector<FakeArchive> aMockArchives;
                        if (![RoundTripTests run_HappyFlow: aJob withArchives: &aMockArchives]) {
                            [self logRegression:aJob withMutations:NULL];
                            XCTFail(@"Round trip medium failed on happy flow.");
                            return;
                        }
                        
                        //int aArchiveIndex = Random::Get((int)aMockArchives.size());
                        //int aBlockIndex = Random::Get((int)aMockArchives[aArchiveIndex].mBlocks.size());
                        //run_CorruptUnbundle
                        
                        
                        //MockHardDrive aHardDrive;
                        //vector<FakeFileBlockSpan> aBlockSpans;
                        
                        //int aArchiveIndex = Random::Get((int)aMockArchives.size());
                        //int aBlockIndex = Random::Get((int)aMockArchives[aArchiveIndex].mBlocks.size());
                        //string aArchiveName = aMockArchives[aArchiveIndex].mFilePath.ToString();
                        
                        //FakeMutation aMutation;
                        //aMutation.SetMangleBlock(aArchiveName, aArchiveIndex, aBlockIndex);
                        
                        vector<FakeMutation> aMutations;
                        
                        FakeMutation::AttemptGenerateRandom(8, &aMockArchives, &aMutations);
                        
                        /*
                        FakeMutation::AttemptGenerateRandomBlockDestruction(Random::Get(3),
                                                                          &aMockArchives,
                                                                          &aMutations);
                        */
                        
                        //aMutations.push_back(aMutation);
                        
                        
                        if (![RoundTripTests run_CorruptUnbundle:aJob withMutations:&aMutations]) {
                            [self logRegression:aJob withMutations:&aMutations];
                            XCTFail(@"Round trip medium failed on corrupt unbundle.");
                            return;
                        }
                        
                        if (![RoundTripTests run_CorruptRecover:aJob withMutations:&aMutations]) {
                            [self logRegression:aJob withMutations:&aMutations];
                            XCTFail(@"Round trip medium failed on corrupt recover.");
                            return;
                        }
                        
                        
                        
                        
                        // auto aArchiveFiles = aHardDrive.ListFilesRecursive(aJob.mArchived.ToString());
                        // string aArchiveName = aArchiveFiles[aArchiveIndex];
                        
                        
                    }
                }
            }
        }
    }
}



@end
