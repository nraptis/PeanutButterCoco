//
//  Layout.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "Layout.hpp"
#include "knobs.hpp"
#include "namespaces.hpp"

int Layout::ArchiveHeaderSize() {
    int aResult = (int)(peanutbutter::memory_layout::kArchiveHeaderBytesV2);
    return aResult;
}

int Layout::SectionHeaderSize() {
    int aResult = (int)(peanutbutter::memory_layout::kSectionHeaderBytesV2);
    return aResult;
}
