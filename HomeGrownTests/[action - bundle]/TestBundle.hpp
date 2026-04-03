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
#include "SimpleBundleRuntime.hpp"

class TestBundle {
    
    
    void                    PerformReal(JobBundle &pJob, MockFileSystem &pFileSystem);
    
    void                    PerformMock(JobBundle &pJob);
    
    
};

#endif /* TestBundle_hpp */
