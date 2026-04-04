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
                         int pArchiveOffset,
                         int pBlockOffset,
                         ByteString *pError) {
    
    if (pBlocksPerArchive <= 0) {
        if (pError != NULL) pError->Set("Invalid blocks per archive.");
        return false;
    }
    
    if (pPayloadBytesPerBlock <= 0) {
        if (pError != NULL) pError->Set("Invalid payload bytes per block.");
        return false;
    }
    
    if (pArchiveOffset < 0) {
        if (pError != NULL) pError->Set("Invalid archive offset.");
        return false;
    }
    
    if (pBlockOffset < 0) {
        if (pError != NULL) pError->Set("Invalid block offset.");
        return false;
    }
    
    if (pPayloadDistance < 0) {
        if (pError != NULL) pError->Set("Invalid payload distance.");
        return false;
    }
    

    // Store raw
    mPayloadDistance = pPayloadDistance;
    
    // Flatten start position
    int aStart =
        (pArchiveOffset * pBlocksPerArchive * pPayloadBytesPerBlock) +
        (pBlockOffset   * pPayloadBytesPerBlock);
    
    // Advance
    int aEnd = aStart + pPayloadDistance;
    
    // Decompose
    int aBytesPerArchive = pBlocksPerArchive * pPayloadBytesPerBlock;
    
    mArchiveIndex = aEnd / aBytesPerArchive;
    
    int aRemainder = aEnd % aBytesPerArchive;
    
    mBlockIndex = aRemainder / pPayloadBytesPerBlock;
    mByteIndex  = aRemainder % pPayloadBytesPerBlock;
    
    mExpectInvalid = false;
    
    return true;
}
