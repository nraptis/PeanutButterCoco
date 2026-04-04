//
//  BundleVerify.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/4/26.
//

#ifndef BundleVerify_hpp
#define BundleVerify_hpp

#include "FakeArchive.hpp"
#include "WrappedArchive.hpp"
#include "JobBundle.hpp"
#include "ByteString.hpp"

class BundleVerify {
public:
    
    static bool                         Execute(JobBundle &pJob, vector<WrappedArchive> pReal, vector<FakeArchive> pMock, ByteString *pError);
    
};

#endif /* BundleVerify_hpp */
