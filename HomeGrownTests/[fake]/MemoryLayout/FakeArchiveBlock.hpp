//
//  FakeArchiveBlock.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/4/26.
//

#ifndef FakeArchiveBlock_hpp
#define FakeArchiveBlock_hpp

#include "FakeSectionHeader.hpp"
#include "ByteString.hpp"
#include "namespaces.hpp"

class FakeArchiveBlock {
    
public:
    FakeArchiveBlock();
    ~FakeArchiveBlock();
    
    FakeSectionHeader                       mHeader;
    ByteString                              mPayload;
    
    int                                     mBlockUUID;
    
    bool                                    mIsInvalid;
    
};

#endif /* FakeArchiveBlock_hpp */
