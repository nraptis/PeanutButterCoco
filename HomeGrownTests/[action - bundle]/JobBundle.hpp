//
//  JobBundle.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#ifndef JobBundle_hpp
#define JobBundle_hpp

#include <vector>
#include "FakeFile.hpp"
#include "ByteString.hpp"
#include "Layout.hpp"
#include "knobs.hpp"
#include "BundleRequest.hpp"
#include "Bundle_Workflow.hpp"
#include "ArchiveHeader.hpp"

// mMaxArchiveCount is same as mBlockCount, 2 sources
// mCancelFinishBlocks is defunct
// mSafeModeEnabled is defunct

class JobBundle {
public:
    JobBundle();
    ~JobBundle();
    
    vector<FakeFile>                mFileList;
    
    void                            SortFiles();
    bool                            ContainsDuplicateFiles() const;
    
    void                            AddFile(string pName, string pContent);
    void                            AddFile(ByteString pName, ByteString pContent);
    
    void                            AddFolder(string pName);
    void                            AddFolder(ByteString pName);
    
    ByteString                      mInput;
    ByteString                      mArchived;
    ByteString                      mUnarchived;
    
    
    ByteString                      mFilePrefix;
    
    int                             mPayloadBytesPerBlock;
    int                             mBatchSize;
    int                             mBlocksPerArchive;
    
    int                             mMaxPathLength;
    int                             mMaxArchiveCount;
    
    bool                            mEncryptionEnabled;
    
    unsigned char                   mRepairCoverage;
    
    bool                            mPreviewEnabled;
    
    bool                            mClearDestination;
    
    void                            SetRepairOff();
    void                            SetRepair20();
    void                            SetRepair40();
    void                            SetRepair60();
    void                            SetRepair80();
    
};

#endif /* JobBundle_hpp */
