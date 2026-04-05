//
//  FakeRepairRecord.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
//

#include "FakeRepairRecord.hpp"

FakeRepairRecord::FakeRepairRecord() {
    mArchiveIndex = -1;
    mBlockIndex = -1;
    mMainBlockIndex = -1;
}

FakeRepairRecord::~FakeRepairRecord() {
    
}

void FakeRepairRecord::SetValid(int pArchiveIndex,
                                 int pBlockIndex) {
    mArchiveIndex = pArchiveIndex;
    mBlockIndex = pBlockIndex;
    mExpectInvalid = false;
}

void FakeRepairRecord::SetInvalid() {
    mArchiveIndex = -1;
    mBlockIndex = -1;
    mExpectInvalid = true;
}
