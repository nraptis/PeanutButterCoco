//
//  WrappedArchiveBlock.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/4/26.
//

#ifndef WrappedArchiveBlock_hpp
#define WrappedArchiveBlock_hpp

#include "SectionHeader.hpp"
#include "ByteString.hpp"

using namespace peanutbutter::memory_layout;

class WrappedArchiveBlock {
public:
    WrappedArchiveBlock();
    ~WrappedArchiveBlock();
    
    SectionHeaderV2 mHeader;
    ByteString mPayload;
    
};

#endif /* WrappedArchiveBlock_hpp */
