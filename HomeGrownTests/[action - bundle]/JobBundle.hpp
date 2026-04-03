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

// mMaxArchiveCount is same as mBlockCount, 2 sources
// mCancelFinishBlocks is defunct
// mSafeModeEnabled is defunct

class JobBundle {
public:
    JobBundle();
    ~JobBundle();
    
    vector<FakeFile>                mFileList;
    
    ByteString                      mSource;
    ByteString                      mDestination;
    
    ByteString                      mFilePrefix;
    
    int                             mPayloadByteCount;
    int                             mBatchSize;
    int                             mBlockCount;
    
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
