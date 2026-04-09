//
//  FakeArchive.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/4/26.
//

#ifndef FakeArchive_hpp
#define FakeArchive_hpp


#include "FakeArchiveHeader.hpp"
#include "FakeArchiveBlock.hpp"
#include "namespaces.hpp"
#include <vector>

class FakeArchive {
    
public:
    FakeArchive();
    ~FakeArchive();
    
    FakeArchiveHeader                       mHeader;
    vector<FakeArchiveBlock>                mBlocks;
    
    ByteString                              mFilePath;
    
    int                                     mArchiveUUID;
    
    int                                     mTemp;
    
};

#endif /* FakeArchive_hpp */
