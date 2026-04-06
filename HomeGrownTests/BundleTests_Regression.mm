//
//  BundleTests_Regression.m
//  HomeGrownTests
//
//  Created by Magneto on 4/4/26.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#include "TestBundle.hpp"
#include "JobBundle.hpp"
#include "WrappedArchiveAssembler.hpp"
#include "BundleVerify.hpp"
#include "FakeFile.hpp"


@interface BundleTests_Regression : XCTestCase
@end

@implementation BundleTests_Regression

- (BOOL) run: (JobBundle &)pJob {
    
    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;
    
    if (!TestBundle::PerformReal(pJob, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    vector<WrappedArchive> aRealArchives = WrappedArchiveAssembler::Get(pJob.mDestination.ToString(),
                                                                        aFileSystem,
                                                                        pJob.mBlocksPerArchive,
                                                                        pJob.mPayloadBytesPerBlock + Layout::SectionHeaderSize());
    
    vector<FakeArchive> aMockArchives;
    if (!TestBundle::PerformMock(pJob, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!BundleVerify::Execute(pJob, aRealArchives, aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    return YES;
}

- (void)test_regression_small_window_a {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("CQ", "b");
    aJob.AddFile("V.s", "龙PA");
    aJob.AddFile("c.g", "Z永g");
    aJob.AddFile("rI", "");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_b {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("Ω.S", "");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_c {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFolder("Yxba");
    aJob.AddFolder("اi");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_d {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.AddFile("a", "a");
    aJob.AddFile("b", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_e {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4096;
    aJob.mPreviewEnabled = true;
    aJob.AddFile("a.txt", "a");
    aJob.AddFile("b.dat", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_f {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 2;
    aJob.mBlocksPerArchive = 4096;
    aJob.mPreviewEnabled = true;
    aJob.SetRepair40();
    aJob.AddFile("a.txt", "a");
    aJob.AddFile("b.dat", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_g {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = true;

    // Files
    aJob.AddFile("GK", "E力W");
    aJob.AddFile("I.PD", "c");
    aJob.AddFile("Q", "WyX");
    aJob.AddFile("bj", "Z");
    aJob.AddFile("q", "qd");
    aJob.AddFile("sk.G", "z");
    aJob.AddFile("xh.J", "f");
    aJob.AddFile("б.I", "Q");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_h {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    aJob.SetRepair40();
    aJob.AddFile("a.txt", "a");
    aJob.AddFile("b.dat", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_i {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = true;
    aJob.SetRepair40();
    aJob.AddFile("a.txt", "a");
    aJob.AddFile("b.dat", "b");
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_j {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = true;
    aJob.SetRepair80();

    // Files
    aJob.AddFile("D", "OQ");
    aJob.AddFile("D.I", "明zE");
    aJob.AddFile("I", "zhzいXm");
    aJob.AddFile("I.P", "");
    aJob.AddFile("O.R", "ぷv");
    aJob.AddFile("OJ", "⭐bw");
    aJob.AddFile("fC", "K大lfy");
    aJob.AddFile("t.Hw", "冰wΩdi");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_k {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    aJob.SetRepair80();

    // Files
    aJob.AddFile("D", "OQ");
    aJob.AddFile("D.I", "明zE");
    aJob.AddFile("I", "zhzいXm");
    aJob.AddFile("I.P", "");
    aJob.AddFile("O.R", "ぷv");
    aJob.AddFile("OJ", "⭐bw");
    aJob.AddFile("fC", "K大lfy");
    aJob.AddFile("t.Hw", "冰wΩdi");
    
    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_l {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mPreviewEnabled = true;
    
    aJob.AddFolder("Talf");
    aJob.AddFolder("aбb");
    aJob.AddFolder("oCACM🛸zhQ");
    aJob.AddFile("бtв.VGod", "sAぽa🏠Mbpz");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_m {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 4;
    aJob.mBlocksPerArchive = 4;
    aJob.mPreviewEnabled = true;
    
    
    // Files
    aJob.AddFile("D.S", "Δجい🏠M");
    aJob.AddFolder("جSвbгKoka");
    aJob.AddFolder("ゑبDeゑJ");
    aJob.AddFolder("🛸jyt");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_n {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 5;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    
    // Files
    aJob.AddFile("H.CBewCXzDmw", "Cw");
    aJob.AddFolder("K");
    aJob.AddFolder("TI");
    aJob.AddFolder("いU永Jb");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_o {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = 0;
    aJob.mRepairCoverage = 0;

    // Files
    aJob.AddFile("T", "冰ΩfUvAΞBW");
    aJob.AddFile("W.fjQo", "🚀BаWh");
    aJob.AddFile("Xi.AqB", "");
    aJob.AddFile("u", "бZf冰m");
    aJob.AddFile("y.تD", "C⭐z力H");
    aJob.AddFile("yJm.gkA", "⚙️NsO");
    aJob.AddFile("zO.POQU", "zΞo明تTFZmJ");
    aJob.AddFile("вΨゑ.гC", "⚙️ΩzvRf");
    aJob.AddFile("⭐Ω.m", "Fyゑ🧬ゐCh");
    aJob.AddFile("おz.めk", "S龙NMc🤖تXd");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_p {
    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 1;
    aJob.mBlocksPerArchive = 1;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 0;

    // Minimal boundary case:
    // "a" folder record = 4 bytes total (2 len + 1 char + 1 type)
    // "b" folder record starts at global data byte 4 (exact block boundary).
    aJob.AddFolder("a");
    aJob.AddFolder("b");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

- (void)test_regression_small_window_q {
    
    // Data record bytes (preview disabled):
    // 1) File "ab" + content "cd" (15 bytes total):
    //    [02 00] [61 62] [03] [02 00 00 00 00 00 00 00] [63 64]
    //    global bytes: 0..14
    //
    // 2) Folder "e" (4 bytes total):
    //    [01 00] [65] [04]
    //    global bytes: 15..18
    //
    // 3) Folder "f" (4 bytes total):
    //    [01 00] [66] [04]
    //    global bytes: 19..22
    //
    // Payload blocks (5 bytes each):
    // block 0 (bytes 0..4):   02 00 61 62 03
    // block 1 (bytes 5..9):   02 00 00 00 00
    // block 2 (bytes 10..14): 00 00 00 63 64
    // block 3 (bytes 15..19): 01 00 65 04 01
    // block 4 (bytes 20..24): 00 66 04 00 00   // last 2 bytes are zero padding
    //
    // Key boundary: block 3 starts exactly at record-start byte 15.

    JobBundle aJob;
    aJob.mPayloadBytesPerBlock = 5;
    aJob.mBlocksPerArchive = 1000;
    aJob.mPreviewEnabled = false;
    aJob.mRepairCoverage = 0;

    aJob.AddFile("ab", "cd");
    aJob.AddFolder("e");
    aJob.AddFolder("f");

    if ([self run:aJob] == NO) {
        XCTFail(@"Regression test failed.");
    }
}

@end
