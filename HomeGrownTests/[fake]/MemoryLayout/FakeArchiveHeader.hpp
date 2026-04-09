//
//  FakeArchiveHeader.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/2/26.
//

#ifndef FakeArchiveHeader_hpp
#define FakeArchiveHeader_hpp

#include "ArchiveHeader.hpp"
#include "namespaces.hpp"

//namespace peanutbutter::memory_layout {
//inline constexpr std::size_t kArchiveHeaderBytesV2 = knobs::kArchiveHeaderBytesV2;
//inline constexpr std::uint64_t kArchiveMagicV2 = 0x5045414E55544254ULL;


class FakeArchiveHeader {
public:
    
    FakeArchiveHeader();
    ~FakeArchiveHeader();
    
    unsigned char                   mDirtyState;
    bool                            mIsEncrypted;
    
    unsigned long long              mArchiveIndex;
    unsigned long long              mArchiveCount;
    
    unsigned long long              mBlockCountPreview;
    unsigned long long              mBlockCountMain;
    unsigned long long              mBlockCountRepair;
    
    unsigned long long              mArchiveFamilyId = 0u;
    
};


#endif /* FakeArchiveHeader_hpp */
