//
//  HelloTests.mm
//  PeanutButterArchiver
//
//  Created by Magneto on 4/3/26.
//

#import <XCTest/XCTest.h>
#include "namespaces.hpp"
#include "FakeFile.hpp"
#include "Random.hpp"
#include "Words.hpp"
#include "TestBundle.hpp"
#include "JobBundle.hpp"
#include "WrappedArchiveAssembler.hpp"
#include "BundleVerify.hpp"

@interface HelloTests : XCTestCase
@end

@implementation HelloTests

- (void)testFakeFileSimpleA {
    FakeFile aFile;
    aFile.mName = "file.txt";
    aFile.mContent = "content";
    if (!(aFile.HasName(ByteString("file.txt")))) {
        XCTFail(@"File does not have expected name...");
        return;
    }
    if (!(aFile.HasContent(ByteString("content")))) {
        XCTFail(@"File does not have expected content...");
        return;
    }
}

- (void)testFakeFileSimpleB {
    FakeFile aFile;
    aFile.mName = "file.txt";
    aFile.mContent = "content";
    if (aFile.HasName(ByteString("file.tx"))) {
        XCTFail(@"File does have unexpected name...");
        return;
    }
    if (aFile.HasContent(ByteString("conten"))) {
        XCTFail(@"File does have unexpected content...");
        return;
    }
}

- (void)testFakeFileSimpleC {
    FakeFile aFile;
    aFile.mName = "file.txt";
    aFile.mContent = "content";
    if (aFile.HasName(ByteString("file.txtt"))) {
        XCTFail(@"File does have unexpected name...");
        return;
    }
    if (aFile.HasContent(ByteString("contentt"))) {
        XCTFail(@"File does have unexpected content...");
        return;
    }
}

- (void)testSomeWords {
    for (int i=0;i<512;i++) {
        int aLength = Random::Get(1, 10);
        ByteString aWord = Words::GetRandomFileName(aLength);
        if (aWord.mLength != aLength) {
            XCTFail(@"String is not the expected count...");
            return;
        }
    }
    for (int i=0;i<512;i++) {
        int aLength = Random::Get(0, 10);
        ByteString aWord = Words::GetRandomFileContent(aLength);
        if (aWord.mLength != aLength) {
            XCTFail(@"String is not the expected count...");
            return;
        }
    }
}

- (void)testQuickBundle {
    
    FakeFile aFile1;
    aFile1.mName = "a.txt";
    aFile1.mContent = "ZZHOME content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content a content a content content";
    
    FakeFile aFile2;
    aFile2.mName = "b.txt";
    aFile2.mContent = "b content b";
    
    FakeFile aFile3;
    aFile3.mName = "folder/c.txt";
    aFile3.mContent = "c content c";
    
    FakeFile aFile4;
    aFile4.mName = "d.txt";
    aFile4.mContent = "d content d";
    
    JobBundle aJob;
    aJob.mFileList.push_back(aFile1);
    aJob.mFileList.push_back(aFile2);
    aJob.mFileList.push_back(aFile3);
    aJob.mFileList.push_back(aFile4);
    
    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;
    
    if (!TestBundle::PerformReal(aJob, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"The bundle job returned an error.");
        return;
    }
    
    auto a = WrappedArchiveAssembler::Get(aJob.mDestination.ToString(),
                                          aFileSystem,
                                          aJob.mBlocksPerArchive,
                                          aJob.mPayloadBytesPerBlock + Layout::SectionHeaderSize());

    
    vector<FakeArchive> aMocks;
    if (!TestBundle::PerformMock(aJob, &aMocks, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"The bundle job returned an error.");
        return;
    }
    
    if (!BundleVerify::Execute(aJob, a, aMocks, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        XCTFail(@"The bundle job returned an error.");
        return;
    }
    
}

@end
