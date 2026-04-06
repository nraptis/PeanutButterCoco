//
//  FakeFileBlockSpan.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/5/26.
//

#include "FakeFileBlockSpan.hpp"

FakeFileBlockSpan::FakeFileBlockSpan() {
    
}

FakeFileBlockSpan::~FakeFileBlockSpan() {
    
}

int FakeFileBlockSpan::PartialRecoverMinimumLength(int pNameLength) {
    int aNameLength = pNameLength;
    if (aNameLength < 0) {
        aNameLength = 0;
    }
    // [name_len_2][name_bytes][type_1][file_size_8]
    return 2 + aNameLength + 1 + 8;
}
