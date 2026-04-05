//
//  FakeArchiveHeader.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
//

#include "FakeArchiveHeader.hpp"


FakeArchiveHeader::FakeArchiveHeader() {
    
    mDirtyState = ((unsigned char)ArchiveDirtyStateV2::kInvalid);
    mIsEncrypted = true;
    
    mArchiveIndex = 0;
    mArchiveCount = 0;
    
    //mEmptyFolderBlockCount = 0;
    
    mBlockCountPreview = 0;
    mBlockCountMain = 0;
    mBlockCountRepair = 0;
    
    mArchiveFamilyId = 0;
    
}

FakeArchiveHeader::~FakeArchiveHeader() {
    
}
