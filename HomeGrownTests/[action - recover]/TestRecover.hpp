//
//  TestRecover.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/6/26.
//

#ifndef TestRecover_hpp
#define TestRecover_hpp

#include "JobBundle.hpp"
#include "MockFileSystem.hpp"
#include "FakeArchive.hpp"
#include "ByteMap.hpp"
#include <vector>

using namespace std;

class TestRecover {
    
public:
    
    static bool                         PerformMock(JobBundle &pJob,
                                                    vector<FakeArchive> *pArchiveList,
                                                    vector<FakeFile> *pResult,
                                                    ByteString *pError);
    
    
    
};

#endif /* TestRecover_hpp */
