//
//  FakeSkipRecord.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
//

#include "FakeSkipRecord.hpp"

FakeSkipRecord::FakeSkipRecord() {
    mPayloadDistance = 0;
    mArchiveIndex = 0;
    mBlockIndex = 0;
    mByteIndex = 0;
    mExpectInvalid = false;
    
}

FakeSkipRecord::~FakeSkipRecord() {
    
}

bool FakeSkipRecord::SetInvalid() {
    
    mPayloadDistance = -1;
    mArchiveIndex = -1;
    mBlockIndex = -1;
    mByteIndex = -1;
    
    mExpectInvalid = true;
    
    return true;
}

bool FakeSkipRecord::SetValid(int pPayloadDistance,
                         int pBlocksPerArchive,
                         int pPayloadBytesPerBlock,
                         ByteString *pError) {
    
    if (pBlocksPerArchive <= 0) {
        if (pError != NULL) pError->Set("Invalid blocks per archive.");
        return false;
    }
    
    if (pPayloadBytesPerBlock <= 0) {
        if (pError != NULL) pError->Set("Invalid payload bytes per block.");
        return false;
    }
    
    if (pPayloadDistance < 0) {
        if (pError != NULL) pError->Set("Invalid payload distance.");
        return false;
    }
    
    // Store raw
    mPayloadDistance = pPayloadDistance;
    
    // Decompose
    int aBytesPerArchive = pBlocksPerArchive * pPayloadBytesPerBlock;
    
    mArchiveIndex = pPayloadDistance / aBytesPerArchive;
    
    int aRemainder = pPayloadDistance % aBytesPerArchive;
    
    mBlockIndex = aRemainder / pPayloadBytesPerBlock;
    mByteIndex  = aRemainder % pPayloadBytesPerBlock;
    
    mExpectInvalid = false;
    
    return true;
}
