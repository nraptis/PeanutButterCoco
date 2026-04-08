//
//  RoundTripTests_Dragon.m
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

@interface RoundTripTests_Dragon : XCTestCase
@end

@implementation RoundTripTests_Dragon

- (void)test_regression_dragon_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 2;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("Hв.t", "FP明ac");
    aJob.AddFile("ب.eM", "明おΦOnJC");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_2.PBTR", 2, 0, "/root/archived/bdl_1.PBTR", 1, 1);
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

- (void)test_regression_dragon_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 6;
    aJob.mBlocksPerArchive = 2;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("N.UKRIбHXZ", "J");
    aJob.AddFile("nt.JsYlm", "u");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_3.PBTR", 3, 1, "/root/archived/bdl_6.PBTR", 6, 1);
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

- (void)test_regression_dragon_c {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("W⭐.S", "あNF");
    aJob.AddFile("e.あD", "mJXKZ");
    aJob.AddFile("в.sqq", "mH🤖q人ZK");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 0, "/root/archived/bdl_0.PBTR", 0, 2);
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

- (void)test_regression_dragon_d {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("AR永V.DのبD", "exIkr");
    aJob.AddFile("Xc力اU.аΔE", "zΩu");
    aJob.AddFile("⚙️.NIS", "⭐ir");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 1, "/root/archived/bdl_0.PBTR", 0, 0);
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

- (void)test_regression_dragon_e {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.mRepairCoverage = 60;
    aJob.AddFile("P.d", "W");
    aJob.AddFile("k.M", "w");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 1, "/root/archived/bdl_0.PBTR", 0, 0);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_0.PBTR", 0, 1);
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

- (void)test_regression_dragon_f {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 14;
    aJob.mPreviewEnabled = true;
    aJob.mRepairCoverage = 20;
    aJob.AddFolder("HahT");
    aJob.AddFolder("pwzG");
    aJob.AddFile("y🚀ث.R", "hmgA");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 0, "/root/archived/bdl_0.PBTR", 0, 1);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 1, "/root/archived/bdl_0.PBTR", 0, 3);
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

- (void)test_regression_dragon_g {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("tiゐ.🛸B", "wCNQ");
    aJob.AddFile("Φあ.xV", "ぷP");

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
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 1, "/root/archived/bdl_0.PBTR", 0, 0);
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

- (void)test_regression_dragon_h {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.mRepairCoverage = 40;
    aJob.AddFile("Iгu🏠.jH", "");
    aJob.AddFolder("P力JEiJ");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 0, "/root/archived/bdl_0.PBTR", 0, 1);
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

- (void)test_regression_dragon_i {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.AddFolder("дkP");
    aJob.AddFile("ゐLX.L", "ゑyU");
    aJob.AddFile("🧬.QMupK", "🤖k");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_0.PBTR", 0, 0);
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


- (void)test_regression_dragon_j {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.AddFolder("LYIb");
    aJob.AddFolder("QUゐPL");
    aJob.AddFile("gMF.U", "");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_0.PBTR", 0, 1);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_0.PBTR", 0, 1);
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

- (void)test_regression_dragon_k {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("Af.⚙️p", "E");
    aJob.AddFolder("Me");
    aJob.AddFolder("vG");

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
        aMutation.SetDeleteBlock("/root/archived/bdl_0.PBTR", 0, 2);
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


- (void)test_regression_dragon_l {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 24;
    aJob.mBlocksPerArchive = 6;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("Eめ.k", "ob");
    aJob.AddFile("o⚡v.W", "cs");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetSwapBlocks("/root/archived/bdl_0.PBTR", 0, 1, "/root/archived/bdl_0.PBTR", 0, 3);
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

@end
