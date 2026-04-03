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

- (void)testSomeWords {
    
    for (int i=0;i<1000;i++) {
        int aLength = Random::Get(2, 7);
        ByteString aWord = Words::GetRandomFileName(aLength);
        if (aWord.mLength != aLength) {
            XCTFail(@"String is not the expected count...");
            return;
        }
    }
    
    for (int i=0;i<1000;i++) {
        int aLength = Random::Get(2, 7);
        ByteString aWord = Words::GetRandomFileContent(aLength);
        if (aWord.mLength != aLength) {
            XCTFail(@"String is not the expected count...");
            return;
        }
    }
}

@end
