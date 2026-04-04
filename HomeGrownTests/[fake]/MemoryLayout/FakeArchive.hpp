//
//  FakeArchive.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/4/26.
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
    ByteString                              mFileStem;
    ByteString                              mFileNumberString;
    int                                     mFileNumber;
    
};

#endif /* FakeArchive_hpp */
