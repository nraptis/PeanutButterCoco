//
//  RoundTripTests_Nonfatal.m
//  HomeGrownTests
//
//  Created by Magneto on 4/7/26.
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

@interface RoundTripTests_Nonfatal : XCTestCase
@end

@implementation RoundTripTests_Nonfatal

- (void)test_regression_nonfatal_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 12;
    aJob.mBlocksPerArchive = 2;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFolder("电r💡IL");
    aJob.AddFolder("龙ثبahj");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteArchive("/root/archived/bdl_0.PBTR", 0);
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

- (void)test_regression_nonfatal_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 12;
    aJob.mBlocksPerArchive = 2;
    aJob.AddFile("H.xAU", "qΞwMi");
    aJob.AddFolder("dаゐぷJ冰U");
    aJob.AddFolder("ΨeV永ΔぷD");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
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

- (void)test_regression_nonfatal_c {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 12;
    aJob.mBlocksPerArchive = 2;
    aJob.AddFile("m", "🧬бfvhΦpTZ");
    aJob.AddFile("o", "のGjoΨQаeHp");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    vector<FakeMutation> aMutations;
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

- (void)test_regression_nonfatal_d {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 12;
    aJob.mBlocksPerArchive = 2;
    aJob.mPreviewEnabled = 1;
    aJob.mRepairCoverage = 20;

    // Files
    aJob.AddFile("Y.力Z", "i");
    aJob.AddFile("大.ja", "");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    if (aMockArchives.size() <= 0) {
        XCTFail(@"Round trip regression produced no archives.");
        return;
    }
    
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteArchive("/root/archived/bdl_1.PBTR", 1);
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

- (void)test_regression_nonfatal_e {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 20;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = 1;
    aJob.mRepairCoverage = 20;

    // Files
    aJob.AddFile("Dc.d", "和电oe");
    aJob.AddFile("W🚀.n", "s🌈eIs");
    aJob.AddFile("キ.hOh", "zSmゑBثP");
    
    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    if (aMockArchives.size() <= 0) {
        XCTFail(@"Round trip regression produced no archives.");
        return;
    }

    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteArchive("/root/archived/bdl_0.PBTR", 0);
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

- (void)test_regression_nonfatal_g {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 20;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("E.aZ", "ICbΨвf");
    aJob.AddFile("ye.F", "бдΩuq");
    aJob.AddFile("ج.C", "山KLxny");
    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    if (aMockArchives.size() <= 0) {
        XCTFail(@"Round trip regression produced no archives.");
        return;
    }
    
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetMangleArchive("/root/archived/bdl_1.PBTR", 1);
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

- (void)test_regression_nonfatal_h {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 2;

    aJob.AddFolder("TΦIPa🏠Pv");
    aJob.AddFolder("VTج明Ωug");
    aJob.AddFolder("ぷ电🧬BK");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    
    vector<FakeMutation> aMutations;

    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 1, "/root/archived/bdl_2.PBTR", 2, 1);
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

- (void)test_regression_nonfatal_i {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 2;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("Y.r", "bI");
    aJob.AddFile("z.c", "Ye");
    
    /*
     Error: Unbundle, mock file {Y.r} is missing from real files.
     file {Y.r} memberhsip in [a: 2, b: 1] (6 / 6)
     file {Y.r} memberhsip in [a: 3, b: 0] (6 / 6)
     file {Y.r} memberhsip in [a: 3, b: 1] (4 / 6)
     file {z.c} memberhsip in [a: 3, b: 1] (2 / 6)
     file {z.c} memberhsip in [a: 4, b: 0] (6 / 6)
     file {z.c} memberhsip in [a: 4, b: 1] (6 / 6)
     file {z.c} memberhsip in [a: 5, b: 0] (2 / 6)
     */

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 0, "/root/archived/bdl_2.PBTR", 2, 0);
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
 
- (void)test_regression_nonfatal_j {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 2;
    aJob.AddFile("Uj", "sتAu");
    aJob.AddFolder("lgдo");
    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 0, "/root/archived/bdl_2.PBTR", 2, 0);
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

- (void)test_regression_nonfatal_k {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 2;
    aJob.AddFile("d", "ثu龙力wRZ");
    aJob.AddFolder("山аثгΔAc");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_2.PBTR", 2, 1, "/root/archived/bdl_0.PBTR", 0, 1);
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
