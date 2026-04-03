//
//  TestArchive.hpp
//  HomeGrownTests
//
//  Created by Magneto on 3/24/26.
//

#ifndef TestArchive_hpp
#define TestArchive_hpp

#include <stdio.h>
#include <string>
#include <vector>

#include "ArchiveHeader.hpp"
#include "SectionHeader.hpp"
#include "MockFileSystem.hpp"

using namespace std;
using namespace peanutbutter;
using namespace peanutbutter::memory_layout;

class TestBlock {
    
    SectionHeaderV2 mHeader;
    ByteBufferV2 mData;
    
};

class TestArchive {
    
public:
    
    TestArchive();
    explicit TestArchive(std::string pPath);
    //TestArchive(vec);
    
    
    //TestFile(std::string pPath);
    //TestFile(std::string pPath, std::string pContent);
    //bool ContentsEqual(TestFile *pFile);
    
    std::string mPath;
    ArchiveHeaderV2 mHeader;
    vector<TestBlock> mBlockList;
    
};

#endif /* TestArchive_hpp */
