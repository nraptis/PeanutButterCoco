//
//  FakeSkipRecord.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
//

#ifndef FakeSkipRecord_hpp
#define FakeSkipRecord_hpp

#include "ByteString.hpp"

class FakeSkipRecord {
    
public:
    
    FakeSkipRecord();
    ~FakeSkipRecord();
    
    bool                    SetValid(int pPayloadDistance,
                                int pBlocksPerArchive,
                                int pPayloadBytesPerBlock,
                                ByteString *pError);
    void                    SetInvalid();
    
    int                     mPayloadDistance;
    
    int                     mArchiveIndex;
    int                     mBlockIndex;
    int                     mByteIndex;
    
    bool                    mExpectInvalid;
    
};

#endif /* FakeSkipRecord_hpp */
