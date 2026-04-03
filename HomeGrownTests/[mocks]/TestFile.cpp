//
//  TestFile.cpp
//  HomeGrownTests
//
//  Created by Magneto on 3/24/26.
//

#include "TestFile.hpp"

TestFile::TestFile() {
    mPath = "";
    mContent = "";
}

TestFile::TestFile(std::string pPath) {
    mPath = pPath;
    mContent = "";
}

TestFile::TestFile(std::string pPath, std::string pContent) {
    mPath = pPath;
    mContent = pContent;
}

bool TestFile::ContentsEqual(TestFile *pFile) {
    if (pFile == NULL) { return false; }
    if (pFile->mContent != mContent) { return false; }
    return true;
}
