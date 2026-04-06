//
//  RecoverTests_Regression.m
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

@interface RecoverTests_Regression : XCTestCase
@end

@implementation RecoverTests_Regression

- (BOOL) run_stepA: (JobBundle &)pJob
     withHardDrive: (MockHardDrive &)pHardDrive
  withFakeArchives: (vector<FakeArchive> *) pFakeArchives
    withBlockSpans: (vector<FakeFileBlockSpan> *) pBlockSpans {
    
    if (pFakeArchives == NULL) {
        printf("Error: fake archive vector missing...\n");
        return NO;
    }
    
    
    if (pBlockSpans == NULL) {
        printf("Error: block span vector missing...\n");
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
        return NO;
    }
    
    if (!BundleVerify::Execute(pJob, aRealArchives, *pFakeArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!TestUnbundleWithHooks::PerformRealUnbundle(
                                                    pJob,
                                                    aFileSystem,
                                                    [](const TestUnbundleWithHooks::PhaseBatchFeedback &pFeedback,
                                                       peanutbutter::DecodeStageContextV2 &pContext,
                                                       SimpleDecodeRuntime &pRuntime) {
                                                           (void)pRuntime;
                                                           if (!pFeedback.mRunSucceeded) {
                                                               const string aError = pContext.LastErrorLog();
                                                               printf("Unbundle failed in phase '%s' (stage=%d): %s\n",
                                                                      pFeedback.mPhase,
                                                                      (int)pFeedback.mStage,
                                                                      aError.c_str());
                                                           }
                                                       },
                                                    &aErrorString)) {
                                                        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
                                                        return NO;
                                                    }
    
    vector <FakeFile> aFiles;
    if (!TestUnbundleWithHooks::CollectFiles(pJob, aFiles, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!UnbundleVerify::Execute(aFiles, pJob.mFileList, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!TestBundle::GetBlockSpans(pJob, pBlockSpans, pFakeArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
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
                                                          (void)pRuntime;
                                                          if (!pFeedback.mRunSucceeded) {
                                                              const string aError = pContext.LastErrorLog();
                                                              printf("Recover failed in phase '%s' (stage=%d): %s\n",
                                                                     pFeedback.mPhase,
                                                                     (int)pFeedback.mStage,
                                                                     aError.c_str());
                                                          }
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
    
    vector <FakeFile> aFilesA;
    if (!TestUnbundleWithHooks::CollectFiles(pJob, aFilesA, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!UnbundleVerify::Execute_Damaged(aFilesA, pJob.mFileList, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    return YES;
}

- (BOOL) mangleSingleBlockAndRun_stepC: (JobBundle &)pJob
                          withHardDrive: (MockHardDrive &)pHardDrive
                       withFakeArchives: (vector<FakeArchive> *) pFakeArchives
                         withBlockSpans: (vector<FakeFileBlockSpan> *) pBlockSpans
                           archiveIndex: (int)pArchiveIndex
                             blockIndex: (int)pBlockIndex {
    
    if (pFakeArchives == NULL || pBlockSpans == NULL) {
        printf("Error: mangle helper missing fake-archive/span inputs.\n");
        return NO;
    }
    
    if (pArchiveIndex < 0 || pBlockIndex < 0) {
        printf("Error: mangle helper received negative archive/block index.\n");
        return NO;
    }
    
    auto aArchiveFiles = pHardDrive.ListFilesRecursive(pJob.mArchived.ToString());
    if (pArchiveIndex >= static_cast<int>(aArchiveFiles.size())) {
        printf("Error: mangle helper archive index %d is out of range (%d archives).\n",
               pArchiveIndex,
               static_cast<int>(aArchiveFiles.size()));
        return NO;
    }
    
    const string aArchiveName = aArchiveFiles[pArchiveIndex];
    
    ByteString aErrorString;
    if (!pHardDrive.MangleBlock(aArchiveName, pBlockIndex, pJob, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!FakeMangleTool::MangleBlock(pArchiveIndex,
                                     pBlockIndex,
                                     &pJob.mFileList,
                                     pFakeArchives,
                                     pBlockSpans,
                                     &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    return [self run_stepC:pJob withHardDrive:pHardDrive];
}

- (void)test_recover_regression_small_window_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 5;
    aJob.mBlocksPerArchive = 100;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 20;

    // [len][len]['a']['b'][typ]
    // [siz][siz][siz][siz][siz]
    // [siz][siz][siz]['c']['d']
    aJob.AddFile("ab", "cc");
    
    // [len][len]
    aJob.AddFile("c", "d");
    
    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }
    
    printf("Span dump begin:\n");
    for (const FakeFileBlockSpan &aSpan : aBlockSpans) {
        for (int aSpanIndex=0; aSpanIndex<(int)aSpan.mArchiveIdentifiers.size(); aSpanIndex++) {
            const int aArchiveUUID = aSpan.mArchiveIdentifiers[aSpanIndex];
            const int aBlockUUID = aSpan.mBlockIdentifiers[aSpanIndex];
            int aArchiveIndex = -1;
            int aBlockIndex = -1;
            for (int aArchiveLoop=0; aArchiveLoop<(int)aMockArchives.size(); aArchiveLoop++) {
                if (aMockArchives[aArchiveLoop].mArchiveUUID != aArchiveUUID) {
                    continue;
                }
                aArchiveIndex = aArchiveLoop;
                for (int aBlockLoop=0; aBlockLoop<(int)aMockArchives[aArchiveLoop].mBlocks.size(); aBlockLoop++) {
                    if (aMockArchives[aArchiveLoop].mBlocks[aBlockLoop].mBlockUUID == aBlockUUID) {
                        aBlockIndex = aBlockLoop;
                        break;
                    }
                }
                break;
            }
            printf("Span {%s}: archive=%d block=%d start=%d end=%d\n",
                   aSpan.mName.ToString().c_str(),
                   aArchiveIndex,
                   aBlockIndex,
                   aSpan.mStartIndex[aSpanIndex],
                   aSpan.mEndIndex[aSpanIndex]);
        }
    }
    printf("Span dump end.\n");

    int aArchiveIndex = 0;
    int aBlockIndex = 3;
    if ([self mangleSingleBlockAndRun_stepC:aJob
                               withHardDrive:aHardDrive
                            withFakeArchives:&aMockArchives
                              withBlockSpans:&aBlockSpans
                                archiveIndex:aArchiveIndex
                                  blockIndex:aBlockIndex] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }
}

- (void)test_recover_regression_small_window_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 20;

    // Files
    aJob.AddFile("K.永ぷQ", "");
    aJob.AddFolder("SYoK");
    aJob.AddFolder("eoおEUz");
    aJob.AddFolder("gAkPmB");
    aJob.AddFolder("⚡pwx");
    aJob.AddFolder("いxcHD");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }

    int aArchiveIndex = 1;
    int aBlockIndex = 0;
    if ([self mangleSingleBlockAndRun_stepC:aJob
                               withHardDrive:aHardDrive
                            withFakeArchives:&aMockArchives
                              withBlockSpans:&aBlockSpans
                                archiveIndex:aArchiveIndex
                                  blockIndex:aBlockIndex] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }
}

- (void)test_recover_regression_small_window_c {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 20;

    // Files
    aJob.AddFile("V.P", "Z⚙️V");
    aJob.AddFile("XH.Hup", "CEkSc");
    aJob.AddFile("mFW.rD", "W💡Q");
    aJob.AddFile("x.⭐uのn", "ج⚡YWK");
    aJob.AddFile("ΞEおΨ.M", "SΩrRLoF");
    aJob.AddFile("вD.T", "бd");
    aJob.AddFile("🧬.بJp", "JxTrI");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }

    int aArchiveIndex = 123;
    int aBlockIndex = 0;
    if ([self mangleSingleBlockAndRun_stepC:aJob
                               withHardDrive:aHardDrive
                            withFakeArchives:&aMockArchives
                              withBlockSpans:&aBlockSpans
                                archiveIndex:aArchiveIndex
                                  blockIndex:aBlockIndex] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }

    // Optional: replace placeholders with concrete values from the failing run.

    NSLog(@"Regression case reproduced.");
}

- (void)test_recover_regression_small_window_d {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Minimal repro for window_c behavior:
    // Keep the same global byte positions around archive block 123 by using
    // one padding file (96 bytes total payload) before the three critical files.
    aJob.AddFile("p", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    aJob.AddFile("ΞEおΨ.M", "SΩrRLoF");
    aJob.AddFile("вD.T", "бd");
    aJob.AddFile("🧬.بJp", "JxTrI");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }

    int aArchiveIndex = 123;
    int aBlockIndex = 0;
    if ([self mangleSingleBlockAndRun_stepC:aJob
                               withHardDrive:aHardDrive
                            withFakeArchives:&aMockArchives
                              withBlockSpans:&aBlockSpans
                                archiveIndex:aArchiveIndex
                                  blockIndex:aBlockIndex] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }
}

- (void)test_recover_regression_small_window_e {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFile("fp", "дoA");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }

    int aArchiveIndex = 0;
    int aBlockIndex = 2;
    if ([self mangleSingleBlockAndRun_stepC:aJob
                               withHardDrive:aHardDrive
                            withFakeArchives:&aMockArchives
                              withBlockSpans:&aBlockSpans
                                archiveIndex:aArchiveIndex
                                  blockIndex:aBlockIndex] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }

    // Optional: replace placeholders with concrete values from the failing run.

    NSLog(@"Regression case reproduced.");
}

- (void)test_recover_regression_small_window_f {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFile("ΨN.l", "y");
    aJob.AddFile("和Q.f", "G");
    aJob.AddFile("永.R", "fl");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }

    int aArchiveIndex = 0;
    int aBlockIndex = 5;
    if ([self mangleSingleBlockAndRun_stepC:aJob
                               withHardDrive:aHardDrive
                            withFakeArchives:&aMockArchives
                              withBlockSpans:&aBlockSpans
                                archiveIndex:aArchiveIndex
                                  blockIndex:aBlockIndex] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }

    // Optional: replace placeholders with concrete values from the failing run.

    NSLog(@"Regression case reproduced.");
}

- (void)test_recover_regression_small_window_g {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // ASCII-only mimic of small_window_f: block 5 lands in file-size bytes.
    aJob.AddFile("a", "abcdefgh");
    aJob.AddFile("b", "G");
    aJob.AddFile("c", "fl");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }

    int aArchiveIndex = 0;
    int aBlockIndex = 5;
    if ([self mangleSingleBlockAndRun_stepC:aJob
                               withHardDrive:aHardDrive
                            withFakeArchives:&aMockArchives
                              withBlockSpans:&aBlockSpans
                                archiveIndex:aArchiveIndex
                                  blockIndex:aBlockIndex] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }
}


- (void)test_recover_regression_small_window_h {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 10;
    aJob.mBlocksPerArchive = 2;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 20;

    // Files
    aJob.AddFolder("B力E");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }

    int aArchiveIndex = 0;
    int aBlockIndex = 1;
    if ([self mangleSingleBlockAndRun_stepC:aJob
                               withHardDrive:aHardDrive
                            withFakeArchives:&aMockArchives
                              withBlockSpans:&aBlockSpans
                                archiveIndex:aArchiveIndex
                                  blockIndex:aBlockIndex] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }
}

- (void)test_recover_regression_small_window_i {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 10;
    aJob.mBlocksPerArchive = 2;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFolder("B力E");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }

    int aArchiveIndex = 0;
    int aBlockIndex = 0;
    if ([self mangleSingleBlockAndRun_stepC:aJob
                               withHardDrive:aHardDrive
                            withFakeArchives:&aMockArchives
                              withBlockSpans:&aBlockSpans
                                archiveIndex:aArchiveIndex
                                  blockIndex:aBlockIndex] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }
}

- (void)test_recover_regression_small_window_j {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFolder("R");
    aJob.AddFile("ag.E", "Mb大");
    aJob.AddFolder("c");
    aJob.AddFolder("m");
    aJob.AddFile("pp", "MвhzcvwZ");
    aJob.AddFile("wF", "i⚙️P");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        return;
    }

    int aArchiveIndex1 = 4;
    int aBlockIndex1 = 1;
    string aArchiveName1 = "/root/archived/bdl_4.PBTR";
    
    int aArchiveIndex2 = 1;
    int aBlockIndex2 = 2;
    string aArchiveName2 = "/root/archived/bdl_1.PBTR";
    

    ByteString aErrorString;
    if (!aHardDrive.DeleteBlock(aArchiveName1, aBlockIndex1, aJob, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"Hard-drive mangle failed.");
        return;
    }
    if (!aHardDrive.DeleteBlock(aArchiveName2, aBlockIndex2, aJob, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"Hard-drive mangle failed.");
        return;
    }

    if (!FakeMangleTool::DeleteBlock(aArchiveIndex1,
                                     aBlockIndex1,
                                     &aJob.mFileList,
                                     &aMockArchives,
                                     &aBlockSpans,
                                     &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"Fake mangle failed.");
        return;
    }
    
    if (!FakeMangleTool::DeleteBlock(aArchiveIndex2,
                                     aBlockIndex2,
                                     &aJob.mFileList,
                                     &aMockArchives,
                                     &aBlockSpans,
                                     &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"Fake mangle failed.");
        return;
    }

    if ([self run_stepC:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step C failed.");
        return;
    }

    // Optional: replace placeholders with concrete values from the failing run.

    NSLog(@"Regression case reproduced.");
}

- (void)test_recover_regression_small_window_k {
    FILE *aTrace = fopen("/tmp/small_window_k_trace.txt", "w");
    if (aTrace != NULL) {
        fprintf(aTrace, "trace begin\n");
    }
    
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFile("jx.KجRS", "Vث山IiB");
    aJob.AddFile("lIm.ΔFdME", "M永Sl");
    aJob.AddFile("p.C", "キDHRTB");
    aJob.AddFile("ا.L💡thk", "fاKGe");
    aJob.AddFile("اk🏠.MCnt", "ثY🧬ΔB");
    aJob.AddFile("⭐山.Qqx", "い大kTuS");
    aJob.AddFile("⭐🤖F.w", "人cpJR");
    aJob.AddFile("人xبr.DRG", "RqJQvR");
    aJob.AddFile("🌈.Ii", "ΞkICFmeaE");
    aJob.AddFile("💡I.s", "oвNキR");

    MockHardDrive aHardDrive;
    vector<FakeFileBlockSpan> aBlockSpans;
    vector<FakeArchive> aMockArchives;

    if ([self run_stepA:aJob
          withHardDrive:aHardDrive
         withFakeArchives:&aMockArchives
         withBlockSpans:&aBlockSpans] == NO) {
        XCTFail(@"Step A failed.");
        return;
    }

    if ([self run_stepB:aJob withHardDrive:aHardDrive] == NO) {
        XCTFail(@"Step B failed.");
        if (aTrace != NULL) {
            fprintf(aTrace, "stepB failed\n");
            fclose(aTrace);
        }
        return;
    }
    
    if (aTrace != NULL) {
        fprintf(aTrace, "span dump begin\n");
        for (const FakeFileBlockSpan &aSpan : aBlockSpans) {
            for (int aSpanIndex=0; aSpanIndex<(int)aSpan.mArchiveIdentifiers.size(); aSpanIndex++) {
                const int aArchiveUUID = aSpan.mArchiveIdentifiers[aSpanIndex];
                const int aBlockUUID = aSpan.mBlockIdentifiers[aSpanIndex];
                int aArchiveIndex = -1;
                int aBlockIndex = -1;
                for (int aArchiveLoop=0; aArchiveLoop<(int)aMockArchives.size(); aArchiveLoop++) {
                    if (aMockArchives[aArchiveLoop].mArchiveUUID != aArchiveUUID) {
                        continue;
                    }
                    aArchiveIndex = aArchiveLoop;
                    for (int aBlockLoop=0; aBlockLoop<(int)aMockArchives[aArchiveLoop].mBlocks.size(); aBlockLoop++) {
                        if (aMockArchives[aArchiveLoop].mBlocks[aBlockLoop].mBlockUUID == aBlockUUID) {
                            aBlockIndex = aBlockLoop;
                            break;
                        }
                    }
                    break;
                }
                fprintf(aTrace,
                        "span {%s}: archive=%d block=%d start=%d end=%d\n",
                        aSpan.mName.ToString().c_str(),
                        aArchiveIndex,
                        aBlockIndex,
                        aSpan.mStartIndex[aSpanIndex],
                        aSpan.mEndIndex[aSpanIndex]);
            }
        }
        fprintf(aTrace, "span dump end\n");
    }

    int aArchiveIndex1 = 14;
    int aBlockIndex1 = 3;
    string aArchiveName1 = "/root/archived/bdl_14.PBTR";
    
    int aArchiveIndex2 = 11;
    int aBlockIndex2 = 3;
    string aArchiveName2 = "/root/archived/bdl_11.PBTR";
    
    ByteString aErrorString;
    if (!aHardDrive.DeleteBlock(aArchiveName1, aBlockIndex1, aJob, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"Hard-drive mangle failed.");
        return;
    }
    if (!aHardDrive.DeleteBlock(aArchiveName2, aBlockIndex2, aJob, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"Hard-drive mangle failed.");
        return;
    }

    if (!FakeMangleTool::MangleBlock(aArchiveIndex1,
                                     aBlockIndex1,
                                     &aJob.mFileList,
                                     &aMockArchives,
                                     &aBlockSpans,
                                     &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"Fake mangle failed.");
        return;
    }
    
    if (!FakeMangleTool::MangleBlock(aArchiveIndex2,
                                     aBlockIndex2,
                                     &aJob.mFileList,
                                     &aMockArchives,
                                     &aBlockSpans,
                                     &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"Fake mangle failed.");
        return;
    }
    
    if (aTrace != NULL) {
        for (const FakeFile &aFile : aJob.mFileList) {
            if (aFile.mName.ToString() != "人xبr.DRG") {
                continue;
            }
            fprintf(aTrace,
                    "mock target: deleted=%d partial=%d content_len=%d\n",
                    aFile.mIsRecoverDeleted ? 1 : 0,
                    aFile.mIsRecoverPartial ? 1 : 0,
                    aFile.mContent.mLength);
        }
    }

    if ([self run_stepC:aJob withHardDrive:aHardDrive] == NO) {
        if (aTrace != NULL) {
            MockFileSystem aFileSystem(&aHardDrive);
            vector <FakeFile> aRealFiles;
            if (TestUnbundleWithHooks::CollectFiles(aJob, aRealFiles, aFileSystem, &aErrorString)) {
                fprintf(aTrace, "real damaged file count=%d\n", (int)aRealFiles.size());
                for (const FakeFile &aFile : aRealFiles) {
                    fprintf(aTrace,
                            "real {%s}: folder=%d content_len=%d\n",
                            aFile.mName.ToString().c_str(),
                            aFile.mIsFolder ? 1 : 0,
                            aFile.mContent.mLength);
                }
            } else {
                fprintf(aTrace, "collect real files failed in trace path\n");
            }
            fprintf(aTrace, "stepC failed\n");
            fclose(aTrace);
        }
        XCTFail(@"Step C failed.");
        return;
    }

    // Optional: replace placeholders with concrete values from the failing run.

    NSLog(@"Regression case reproduced.");
    if (aTrace != NULL) {
        fprintf(aTrace, "stepC passed\n");
        fclose(aTrace);
    }
}



@end
