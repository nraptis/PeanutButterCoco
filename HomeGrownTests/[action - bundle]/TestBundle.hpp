//
//  TestBundle.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#ifndef TestBundle_hpp
#define TestBundle_hpp

#include "JobBundle.hpp"
#include "MockFileSystem.hpp"
#include "FakeArchive.hpp"
#include "ByteMap.hpp"
#include <vector>

using namespace std;

class TestBundle {
    
public:
    
    static bool                         PackBlocks(vector<ByteString> *pPayloadList, vector<ByteString> *pPayloadBlocks, int pBlockLength, ByteString *pError);
    static bool                         PerformMock(JobBundle &pJob, vector<FakeArchive> *pResult, ByteString *pError);
    
};

#endif /* TestBundle_hpp */
