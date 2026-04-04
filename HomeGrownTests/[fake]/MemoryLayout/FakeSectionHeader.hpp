//
//  FakeSectionHeader.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
//

#ifndef FakeSectionHeader_hpp
#define FakeSectionHeader_hpp

#include "FakeSkipRecord.hpp"

class FakeSectionHeader {
public:
    
    FakeSectionHeader();
    ~FakeSectionHeader();
    
    FakeSkipRecord                  mSkipRecord;
    
};

#endif
