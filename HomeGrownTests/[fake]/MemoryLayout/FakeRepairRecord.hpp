//
//  FakeRepairRecord.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/2/26.
//

#ifndef FakeRepairRecord_hpp
#define FakeRepairRecord_hpp

#include "ByteString.hpp"

class FakeRepairRecord {
public:
    
    FakeRepairRecord();
    ~FakeRepairRecord();
    
    void                    SetValid(int pArchiveIndex,
                                     int pBlockIndex);
    void                    SetInvalid();
    
    
    int                     mArchiveIndex;
    int                     mBlockIndex;
    
    bool                    mExpectInvalid;
    
    int                     mMainBlockIndex;
};

#endif /* FakeRepairRecord_hpp */
