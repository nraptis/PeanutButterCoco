//
//  FakeSectionHeader.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
//

#ifndef FakeSectionHeader_hpp
#define FakeSectionHeader_hpp

#include "FakeSkipRecord.hpp"
#include "FakeRepairRecord.hpp"

// Note: mFolderManifestBlockCount should not be needed.

class FakeSectionHeader {
public:
    
    FakeSectionHeader();
    ~FakeSectionHeader();
    
    FakeSkipRecord                  mSkipRecord;
    FakeRepairRecord                mRepairRecord;
    
    unsigned char                   mSectionType;
    
    int                             mBlockIndex;
    
    unsigned long long              mArchiveIndex;
    unsigned long long              mArchiveCount;
    
    unsigned long long              mBlockCountPreview;
    unsigned long long              mBlockCountMain;
    unsigned long long              mBlockCountRepair;
    
};

#endif
