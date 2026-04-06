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
    // Per-entry payload byte range for this file inside the matching block: [start, end).
    vector<int>                 mStartIndex;
    vector<int>                 mEndIndex;
    
    static int                  PartialRecoverMinimumLength(int pNameLength);
    
};

#endif /* FakeFileBlockSpan_hpp */
