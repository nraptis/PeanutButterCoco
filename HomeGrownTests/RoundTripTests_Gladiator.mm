//
//  RoundTripTests_Gladiator.m
//  HomeGrownTests
//
//  Created by Magneto on 4/8/26.
//

#import <Foundation/Foundation.h>
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

@interface RoundTripTests_Gladiator : XCTestCase
@end

@implementation RoundTripTests_Gladiator

- (void)test_regression_gladiator_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 60;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("$PARTIAL_QGan🚀جVRyrAbv", "WV");
    aJob.AddFile("$PARTIAL_QGan🚀جVRyrAbv_1", "⚡Δt");
    aJob.AddFolder("$PARTIAL_QGan🚀جVRyrAbv_2");
    aJob.AddFolder("$PARTIAL_s冰MpぷめXID⚙️gr");
    aJob.AddFolder("$PARTIAL_s冰MpぷめXID⚙️gr_1");
    aJob.AddFolder("QGan🚀جVRyrAbv");
    aJob.AddFolder("s冰MpぷめXID⚙️gr");
    aJob.AddFolder("ぷ山ب🌈⭐龙KdlX");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetMangleArchive("/root/archived/bdl_07.PBTR", 7);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_06.PBTR", 6, 0);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteArchive("/root/archived/bdl_04.PBTR", 4);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_06.PBTR", 6, 0);
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

- (void)test_regression_gladiator_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 40;
    aJob.mBlocksPerArchive = 10;
    aJob.mPreviewEnabled = true;
    aJob.mRepairCoverage = 20;
    aJob.AddFolder("$PARTIAL_wl⭐Ξあ永ZثثRryfD");
    aJob.AddFile("$PARTIAL_wl⭐Ξあ永ZثثRryfD_1", "");
    aJob.AddFile("$PARTIAL_wl⭐Ξあ永ZثثRryfD_2", "电o");
    aJob.AddFolder("$PARTIAL_めzвmSk⚙️力ぽx龙lcぽGT💡a");
    aJob.AddFile("$PARTIAL_めzвmSk⚙️力ぽx龙lcぽGT💡a_1", "⭐DkW");
    aJob.AddFile("$PARTIAL_めzвmSk⚙️力ぽx龙lcぽGT💡a_2", "hаIm");
    aJob.AddFile("$PARTIAL_和のぷ力a", "大K");
    aJob.AddFile("$PARTIAL_和のぷ力a_1", "TsGv");
    aJob.AddFile("wl⭐Ξあ永ZثثRryfD", "аpd");
    aJob.AddFolder("めzвmSk⚙️力ぽx龙lcぽGT💡a");
    aJob.AddFile("和のぷ力a", "Y");

    vector<FakeArchive> aMockArchives;
    if (![RoundTripTests run_HappyFlow:aJob withArchives:&aMockArchives]) {
        XCTFail(@"Round trip regression failed on happy flow.");
        return;
    }
    vector<FakeMutation> aMutations;
    {
        FakeMutation aMutation;
        aMutation.SetMangleBlock("/root/archived/bdl_1.PBTR", 1, 4);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteArchive("/root/archived/bdl_2.PBTR", 2);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteArchive("/root/archived/bdl_0.PBTR", 0);
        aMutations.push_back(aMutation);
    }
    {
        FakeMutation aMutation;
        aMutation.SetDeleteBlock("/root/archived/bdl_1.PBTR", 1, 5);
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

- (void)test_regression_gladiator_c {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 32;
    aJob.mBlocksPerArchive = 10;
    aJob.mPreviewEnabled = true;
    aJob.mRepairCoverage = 60;
    aJob.AddFolder("$PARTIAL_C🚀lWаr和mb");
    aJob.AddFile("$PARTIAL_C🚀lWаr和mb_1", "StD");
    aJob.AddFolder("$PARTIAL_C🚀lWаr和mb_2");
    aJob.AddFolder("$PARTIAL_HsrゑCXu明NfjB");
    aJob.AddFolder("$PARTIAL_HsrゑCXu明NfjB_1");
    aJob.AddFile("$PARTIAL_HsrゑCXu明NfjB_2", "VبP");
    aJob.AddFolder("$PARTIAL_🚀ぷ⚙️pV");
    aJob.AddFile("$PARTIAL_🚀ぷ⚙️pV_1", "bA");
    aJob.AddFile("$PARTIAL_🚀ぷ⚙️pV_2", "qD");
    aJob.AddFile("C🚀lWаr和mb", "r");
    aJob.AddFile("HsrゑCXu明NfjB", "LX⭐T");
    aJob.AddFolder("🚀ぷ⚙️pV");

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
        aMutation.SetDeleteArchive("/root/archived/bdl_2.PBTR", 2);
        aMutations.push_back(aMutation);
    }
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

- (void)test_regression_gladiator_d {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 32;
    aJob.mBlocksPerArchive = 10;
    aJob.mPreviewEnabled = true;
    aJob.AddFolder("$PARTIAL_VUEaぷO");
    aJob.AddFile("$PARTIAL_VUEaぷO_1", "");
    aJob.AddFolder("$PARTIAL_VUEaぷO_2");
    aJob.AddFolder("$PARTIAL_вあ山rXxXFB电Sd");
    aJob.AddFile("$PARTIAL_вあ山rXxXFB电Sd_1", "lتF");
    aJob.AddFolder("$PARTIAL_人冰jGhlbh");
    aJob.AddFolder("$PARTIAL_🛸HZXаyぷ电tlت永u人اnJB");
    aJob.AddFile("$PARTIAL_🛸HZXаyぷ电tlت永u人اnJB_1", "");
    aJob.AddFolder("$PARTIAL_🛸HZXаyぷ电tlت永u人اnJB_2");
    aJob.AddFolder("VUEaぷO");
    aJob.AddFolder("вあ山rXxXFB电Sd");
    aJob.AddFile("人冰jGhlbh", "Rq");
    aJob.AddFolder("🛸HZXаyぷ电tlت永u人اnJB");

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

@end
