//
//  FakeArchiveHeader.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
//

#include "FakeArchiveHeader.hpp"


FakeArchiveHeader::FakeArchiveHeader() {
    mMagic = kArchiveMagicV2;
    mArchiveFormatVersion = 2;
    mCipherVersion = 1;
    mExpanderVersion = 1;
    mDirtyState = ((unsigned char)ArchiveDirtyStateV2::kInvalid);
    mIsEncrypted = true;
    mCipherProfile = 0;
    mExpanderProfile = 0;
    mArchiveIndex = 0;
    
    mArchiveCount = 0;
    mArchiveDataBlockCount = 0;
    
    //mEmptyFolderBlockCount = 0;
    
    mPreviewManifestBlockCount = 0;
    mRepairSectorBlockCount = 0;
    mArchiveFamilyId = 0;
    
}

FakeArchiveHeader::~FakeArchiveHeader() {
    
}
