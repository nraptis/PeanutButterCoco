//
//  Layout.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#ifndef Layout_hpp
#define Layout_hpp

#include "Primatives.hpp"
#include "ByteString.hpp"

using namespace std;
using namespace peanutbutter::memory_layout;

class Layout {
    
public:
    
    static int              ArchiveHeaderSize();
    static int              SectionHeaderSize();
    
    static uint64_t         ToLong(const PackedUint24V2& pValue);
    static uint64_t         ToLong(const PackedUint48V2& pValue);
    
    static int              ToInt(const PackedUint24V2& pValue);
    static int              ToInt(const PackedUint48V2& pValue);
    
    static int              GetZeroCount(int pMaximum);
    static ByteString       GetZeroPadded(int pNumber, int pZeroCount);
    

};

#endif /* Layout_hpp */
