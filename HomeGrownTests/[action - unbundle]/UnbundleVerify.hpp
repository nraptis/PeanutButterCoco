//
//  UnbundleVerify.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/6/26.
//

#ifndef UnbundleVerify_hpp
#define UnbundleVerify_hpp

#include "FakeArchive.hpp"
#include "WrappedArchive.hpp"
#include "JobBundle.hpp"
#include "ByteString.hpp"

class UnbundleVerify {
public:
    
    static bool Execute(vector<FakeFile> *pFilesReal, vector<FakeFile> *pFilesMock, ByteString *pError);
    static bool Execute_Damaged(vector<FakeFile> &pFilesReal, vector<FakeFile> &pFilesMock, ByteString *pError);
    
};


#endif /* UnbundleVerify_hpp */
