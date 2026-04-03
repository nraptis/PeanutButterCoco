//
//  FakeArchiveHeader.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
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
    
    unsigned long long mMagic;
    unsigned char mArchiveFormatVersion;
    unsigned char mCipherVersion;
    unsigned char mExpanderVersion;
    unsigned char mDirtyState;
    bool mIsEncrypted;
    unsigned char mCipherProfile;
    unsigned char mExpanderProfile;
    unsigned long long mArchiveIndex;
    unsigned long long mArchiveCount;
    unsigned long long mArchiveDataBlockCount;
    //unsigned long long mEmptyFolderBlockCount; // We don't need this.
    
    unsigned long long mPreviewManifestBlockCount;
    unsigned long long mRepairSectorBlockCount;
    unsigned long long mArchiveFamilyId = 0u;
    
};


#endif /* FakeArchiveHeader_hpp */
