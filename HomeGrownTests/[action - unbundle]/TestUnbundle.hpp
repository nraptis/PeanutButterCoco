//
//  TestUnbundle.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/6/26.
//

#ifndef TestUnbundle_hpp
#define TestUnbundle_hpp

#include "JobBundle.hpp"
#include "MockFileSystem.hpp"
#include "FakeArchive.hpp"
#include "ByteMap.hpp"
#include <vector>

using namespace std;

class TestUnbundle {
    
public:
    
    static bool                         PerformMock(JobBundle &pJob,
                                                    vector<FakeArchive> *pArchiveList,
                                                    vector<FakeFile> *pResult,
                                                    ByteString *pError);
    
    static bool                         ExecuteConsecutive(vector<FakeArchiveBlock> *pConsecutiveBlockList,
                                                           int pBlocksPerArchive,
                                                           int pPayloadOffset,
                                                           int pPayloadBytesPerBlock,
                                                           int pMaxPathLength,
                                                           ByteMap *pNameMap,
                                                           vector<FakeFile> *pResult,
                                                           ByteString *pError);
    
};

#endif /* TestUnbundle_hpp */
