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

- (void)test_100_full_spectrum {
    
    for (int aTestIndex=0;aTestIndex<50000000;aTestIndex++) {
        
        int aPayloadBytesPerBlock = Random::Get(1, 256);
        int aBlocksPerArchive = Random::Get(1, 256);
        
        vector<FakeFile> aFiles;
        if (Random::Get(2) == 0) {
            
            int aNameLo = Random::Get(1, 72);
            int aLameHi = aNameLo + Random::Get(0, 116);
            if (aPayloadBytesPerBlock < 8) {
                aNameLo = Random::Get(1, 8);
                aLameHi = aNameLo + Random::Get(0, 8);
            }
            
            vector <ByteString> aNames;
            if (aPayloadBytesPerBlock < 8) {
                aNames = Words::GetRandomFolderNames(Random::Get(1, 8), aNameLo, aLameHi);
            } else {
                aNames = Words::GetRandomFolderNames(Random::Get(1, 22), aNameLo, aLameHi);
            }
            
            for (auto aName: aNames) {
                int aDupeCount = Random::Get(1, 8);
                if (aPayloadBytesPerBlock < 8) {
                    aDupeCount = Random::Get(1, 4);
                }
                
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
                        
                        if (aPayloadBytesPerBlock < 8) {
                            aFile.mContent.Set(Words::GetRandomFileContent(0, 12));
                        } else {
                            aFile.mContent.Set(Words::GetRandomFileContent(0, 96));
                        }
                        
                        aFile.mIsFolder = false;
                        aFiles.push_back(aFile);
                    }
                }
            }
            
        } else {
            
            int aFileCount = Random::Get(0, 24);
            int aFolderCount = Random::Get(0, 24);
            if (aPayloadBytesPerBlock < 8) {
                aFileCount = Random::Get(0, 6);
                aFolderCount = Random::Get(0, 6);
            }
            
            if ((aFileCount == 0) && (aFolderCount == 0)) {
                if (Random::Get(2) == 0) {
                    aFileCount = 1;
                } else {
                    aFolderCount = 1;
                }
            }
            
            int aFileNameLo = Random::Get(1, 26);
            int aFileNameHi = aFileNameLo + Random::Get(0, 90);
            
            int aFileContentLo = Random::Get(0, 128);
            int aFileContentHi = aFileContentLo + Random::Get(0, 180);
            
            int aFolderNameLo = Random::Get(1, 24);
            int aFolderNameHi = aFolderNameLo + Random::Get(0, 110);
            
            if (aPayloadBytesPerBlock < 8) {
                aFileNameLo = Random::Get(1, 4);
                aFileNameHi = aFileNameLo + Random::Get(0, 12);
                
                aFileContentLo = Random::Get(0, 4);
                aFileContentHi = aFileContentLo + Random::Get(0, 16);
                
                aFolderNameLo = Random::Get(1, 6);
                aFolderNameHi = aFolderNameLo + Random::Get(0, 10);
            }
            
            aFiles = Words::GetRandomFilesAndFolders(aFileCount, aFileNameLo, aFileNameHi,
                                                                      aFileContentLo, aFileContentHi,
                                                                      aFolderCount, aFolderNameLo, aFolderNameHi);
        }
        
        JobBundle aJob;
        for (auto aFile: aFiles) {
            aJob.mFileList.push_back(aFile);
        }
        aJob.mPayloadBytesPerBlock = aPayloadBytesPerBlock;
        aJob.mBlocksPerArchive = aBlocksPerArchive;
        
        if (Random::Get(2) == 1) {
            aJob.mPreviewEnabled = false;
        } else {
            aJob.mPreviewEnabled = true;
        }
        
        if (Random::Get(2) == 0) {
            int aWhich = Random::Get(4);
            if (aWhich == 0) { aJob.SetRepair20(); }
            if (aWhich == 1) { aJob.SetRepair40(); }
            if (aWhich == 2) { aJob.SetRepair60(); }
            if (aWhich == 3) { aJob.SetRepair80(); }
        }
        
        vector<FakeArchive> aMockArchives;
        if (![RoundTripTests run_HappyFlow: aJob withArchives: &aMockArchives]) {
            [self logRegression:aJob withMutations:NULL];
            XCTFail(@"Round trip medium failed on happy flow.");
            return;
        }
        
        vector<FakeMutation> aMutations;
        
        int aWhichMutation = Random::Get(5);
        if (aWhichMutation == 0) {
            FakeMutation::AttemptGenerateRandom(Random::Get(8), &aMockArchives, &aMutations);
        }
        if (aWhichMutation == 1) {
            FakeMutation::AttemptGenerateRandomBlockDeletions(Random::Get(8), &aMockArchives, &aMutations);
        }
        if (aWhichMutation == 2) {
            FakeMutation::AttemptGenerateRandomArchiveDestruction(Random::Get(8), &aMockArchives, &aMutations);
        }
        if (aWhichMutation == 3) {
            FakeMutation::AttemptGenerateRandomBlockSwaps(Random::Get(8), &aMockArchives, &aMutations);
        }
        
        if ((aTestIndex % 1000) == 0) {
            printf("Test #%d, (%d|%d)\n", aTestIndex, aPayloadBytesPerBlock, aBlocksPerArchive);
        }
        
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
    }
}

- (void)test_regression_medium_recover_preserves_partial_outputs_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 51;
    aJob.mBlocksPerArchive = 91;
    aJob.mRepairCoverage = 40;
    aJob.AddFolder("$PARTIAL_GDrVUw");
    aJob.AddFile("$PARTIAL_GDrVUw_1", "Hjl永电JgxfHM永🚀ب电ov人キI永l");
    aJob.AddFile("$PARTIAL_QeاMy", "Xsth");
    aJob.AddFile("$PARTIAL_аBWb", "av🌈ب🚀uaIث⚙️e永MتSA");
    aJob.AddFolder("$PARTIAL_аBWb_1");
    aJob.AddFolder("$PARTIAL_аBWb_2");
    aJob.AddFolder("$PARTIAL_のedm");
    aJob.AddFolder("$PARTIAL_のedm_1");
    aJob.AddFile("$PARTIAL_のedm_2", "力人sثあبぽY明jHRUNRnS⭐jg");
    aJob.AddFolder("$PARTIAL_キgi");
    aJob.AddFolder("GDrVUw");
    aJob.AddFile("QeاMy", "chnD🧬めzjI");
    aJob.AddFile("xキK", "和x🚀wg💡rCキΨبRNLm电Edv");
    aJob.AddFolder("аBWb");
    aJob.AddFile("のedm", "X⭐🤖💡ECΦqA");
    aJob.AddFolder("キgi");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 2);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 11);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 0);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 3);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 4);
        aMutations.push_back(aMutation);
    }

    if (![RoundTripTests run_CorruptUnbundle:aJob withMutations:&aMutations]) {
        XCTFail(@"Round trip regression failed on corrupt unbundle.");
        return;
    }

    if (![RoundTripTests run_CorruptRecover:aJob withMutations:&aMutations]) {
        XCTFail(@"Round trip regression failed on corrupt recover.");
        return;
    }
}

- (void)test_regression_medium_recover_preserves_partial_outputs_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 155;
    aJob.mPreviewEnabled = true;
    aJob.mRepairCoverage = 20;
    aJob.AddFolder("$PARTIAL_aK🤖NjO🚀Xik");
    aJob.AddFile("$PARTIAL_aK🤖NjO🚀Xik_1", "p山جHnOrΔ龙キzblTq");
    aJob.AddFile("$PARTIAL_ySE力z⚙️山c", "MjبCвjΩ人UぽΦgHNz");
    aJob.AddFile("$PARTIAL_ySE力z⚙️山c_1", "大ClN");
    aJob.AddFile("$PARTIAL_yoabS🧬ΞCбYw山L", "дdゑ明山UTTZFeIhNбгxjW🚀ΦQZΞO");
    aJob.AddFolder("$PARTIAL_yoabS🧬ΞCбYw山L_1");
    aJob.AddFolder("aK🤖NjO🚀Xik");
    aJob.AddFolder("ySE力z⚙️山c");
    aJob.AddFile("yoabS🧬ΞCбYw山L", "电Yk力Pct🚀⚙️BMeゑa🏠v");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetMangleArchive("/root/archived/bdl_1.PBTR", 1);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_2.PBTR", 2, 138);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteArchive("/root/archived/bdl_3.PBTR", 3);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_2.PBTR", 2, 132);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteArchive("/root/archived/bdl_2.PBTR", 2);
        aMutations.push_back(aMutation);
    }

    if (![RoundTripTests run_CorruptUnbundle:aJob withMutations:&aMutations]) {
        XCTFail(@"Round trip regression failed on corrupt unbundle.");
        return;
    }

    if (![RoundTripTests run_CorruptRecover:aJob withMutations:&aMutations]) {
        XCTFail(@"Round trip regression failed on corrupt recover.");
        return;
    }
}

@end
