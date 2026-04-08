//
//  MutationTool.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#ifndef MutationTool_hpp
#define MutationTool_hpp

#include "ByteString.hpp"
#include "FakeArchive.hpp"
#include "FakeFile.hpp"
#include "FakeMutation.hpp"
#include "JobBundle.hpp"
#include "MockHardDrive.hpp"

#include <vector>



using namespace std;

class MutationTool {
public:
    
    
    
    static bool                         ApplyMutationsMock(vector<FakeMutation> *pMutations,
                                                           vector<FakeArchive> *pArchiveList,
                                                           ByteString *pError);
    static bool                         ApplyMutationsReal(JobBundle pJob,
                                                           vector<FakeMutation> *pMutations,
                                                           MockHardDrive *pDrive,
                                                           ByteString *pError);
    
};

#endif /* MutationTool_hpp */
