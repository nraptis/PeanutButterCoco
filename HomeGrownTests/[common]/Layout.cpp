//
//  Layout.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "Layout.hpp"
#include "knobs.hpp"
#include "namespaces.hpp"
#include <limits>

int Layout::ArchiveHeaderSize() {
    int aResult = (int)(peanutbutter::memory_layout::kArchiveHeaderBytesV2);
    return aResult;
}

int Layout::SectionHeaderSize() {
    int aResult = (int)(peanutbutter::memory_layout::kSectionHeaderBytesV2);
    return aResult;
}


uint64_t Layout::ToLong(const PackedUint24V2 &pValue) {
    return (static_cast<std::uint64_t>(pValue.mBytes[0])      ) |
           (static_cast<std::uint64_t>(pValue.mBytes[1]) <<  8) |
           (static_cast<std::uint64_t>(pValue.mBytes[2]) << 16);
}

uint64_t Layout::ToLong(const PackedUint48V2 &pValue) {
    return (static_cast<std::uint64_t>(pValue.mBytes[0])      ) |
           (static_cast<std::uint64_t>(pValue.mBytes[1]) <<  8) |
           (static_cast<std::uint64_t>(pValue.mBytes[2]) << 16) |
           (static_cast<std::uint64_t>(pValue.mBytes[3]) << 24) |
           (static_cast<std::uint64_t>(pValue.mBytes[4]) << 32) |
           (static_cast<std::uint64_t>(pValue.mBytes[5]) << 40);
}

int Layout::ToInt(const PackedUint24V2& pValue) {
    std::uint64_t value = ToLong(pValue);
    if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
}

int Layout::ToInt(const PackedUint48V2& pValue) {
    std::uint64_t value = ToLong(pValue);
    if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
}

int Layout::GetZeroCount(int pMaximum) {
    if (pMaximum == 0) {
        return 1;
    }
    
    int aValue = pMaximum;
    int aResult = 0;
    while (aValue > 0) {
        aValue /= 10;
        aResult++;
    }
    return aResult;
}

ByteString Layout::GetZeroPadded(int pNumber, int pZeroCount) {
    ByteString aNumberString = ByteString(pNumber);
    if (aNumberString.mLength < pZeroCount) {
        ByteString aResult;
        aResult.Append(string(pZeroCount - aNumberString.mLength, '0'));
        aResult.Append(aNumberString);
        
        
        //= ByteString(string(pZeroCount - aNumberString.mLength, '0'));
        return aResult;
    } else {
        return aNumberString;
    }
}
