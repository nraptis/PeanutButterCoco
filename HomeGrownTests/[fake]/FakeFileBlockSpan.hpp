//
//  FakeFileBlockSpan.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/5/26.
//

#ifndef FakeFileBlockSpan_hpp
#define FakeFileBlockSpan_hpp

#include "ByteString.hpp"
#include <vector>

using namespace std;

class FakeFileBlockSpan {
public:
    FakeFileBlockSpan();
    ~FakeFileBlockSpan();
    
    ByteString                  mName;
    
    vector<int>                 mArchiveIdentifiers;
    vector<int>                 mBlockIdentifiers;
    
};

#endif /* FakeFileBlockSpan_hpp */
