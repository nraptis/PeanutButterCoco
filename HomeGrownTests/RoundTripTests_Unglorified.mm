//
//  RoundTripTests_Unglorified.m
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

@interface RoundTripTests_Unglorified : XCTestCase
@end

@implementation RoundTripTests_Unglorified

- (void)test_regression_unglorified_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 8;
    aJob.mBlocksPerArchive = 2;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 40;

    // Files
    aJob.AddFile("lB.V", "oSy");
    aJob.AddFile("xW", "キWv");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_2.PBTR", 2, 0);
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

- (void)test_regression_unglorified_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 8;
    aJob.mBlocksPerArchive = 2;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFile("c.X", "");
    aJob.AddFile("o.W", "");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 0);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_0.PBTR", 0, 0);
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

- (void)test_regression_unglorified_c {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFile("Δ.ゑo", "HTre");
    aJob.AddFile("⭐.JZ", "龙m");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    vector<FakeMutation> aMutations;
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

    if (![RoundTripTests run_CorruptUnbundle:aJob withMutations:&aMutations]) {
        XCTFail(@"Round trip regression failed on corrupt unbundle.");
        return;
    }

    if (![RoundTripTests run_CorruptRecover:aJob withMutations:&aMutations]) {
        XCTFail(@"Round trip regression failed on corrupt recover.");
        return;
    }
}

- (void)test_regression_unglorified_d {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 40;

    // Files
    aJob.AddFile("K", "d🛸i");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 1);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 1);
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

- (void)test_regression_unglorified_e {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 19;
    aJob.mBlocksPerArchive = 5;
    aJob.mPreviewEnabled = 1;
    aJob.mRepairCoverage = 40;

    // Files
    aJob.AddFile("YL.H", "fث电YoM");
    aJob.AddFile("ج.C", "zجΦFвt");
    aJob.AddFile("⭐.R", "🌈のXd");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_1.PBTR", 1, 0);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_0.PBTR", 0, 4);
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

- (void)test_regression_unglorified_f {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("abcde", "tET冰Fz");
    aJob.AddFile("Ic.xy", "ot明gLv");
    aJob.AddFile("xr.A", "G人龙w");
    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }

    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteArchive("/root/archived/bdl_54.PBTR", 54);
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

- (void)test_regression_unglorified_g {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 5;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFolder("NXNu");
    aJob.AddFolder("qsبn");
    aJob.AddFolder("🌈q");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_1.PBTR", 1, 0);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_1.PBTR", 1, 2);
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

- (void)test_regression_unglorified_h {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 8;
    aJob.mBlocksPerArchive = 12;
    aJob.mPreviewEnabled = 1;
    aJob.mRepairCoverage = 20;
    aJob.AddFile("Ch.F", "N");
    aJob.AddFile("Ub", "w");
    aJob.AddFile("дv.W", "Y");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 0, "/root/archived/bdl_1.PBTR", 1, 1);
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

- (void)test_regression_unglorified_i {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 14;
    aJob.mPreviewEnabled = 1;
    aJob.mRepairCoverage = 60;
    aJob.AddFile("O.Ξb", "");
    aJob.AddFile("o", "");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 4);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_0.PBTR", 0, 1);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 1);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_0.PBTR", 0, 3);
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

- (void)test_regression_unglorified_j {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 12;
    aJob.mBlocksPerArchive = 2;
    aJob.AddFile("R.JZ", "YLc");
    aJob.AddFile("Φx.YE", "iB");
    aJob.AddFile("人.s", "جDCP");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 0);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleArchive("/root/archived/bdl_0.PBTR", 0);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleArchive("/root/archived/bdl_0.PBTR", 0);
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
