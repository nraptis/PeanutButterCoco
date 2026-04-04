//
//  WrappedArchive.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/4/26.
//

#ifndef WrappedArchive_hpp
#define WrappedArchive_hpp

#include "ArchiveHeader.hpp"
#include "WrappedArchiveBlock.hpp"
#include <vector>

using namespace peanutbutter::memory_layout;
using namespace std;

class WrappedArchive {
public:
    
    WrappedArchive();
    ~WrappedArchive();
    
    ArchiveHeaderV2                         mHeader;
    vector<WrappedArchiveBlock>             mBlocks;
    
    ByteString                              mFilePath;
    ByteString                              mFileStem;
    ByteString                              mFileNumberString;
    int                                     mFileNumber;
    
};

#endif /* WrappedArchive_hpp */
