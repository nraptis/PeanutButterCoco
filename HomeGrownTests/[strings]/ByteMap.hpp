//
//  ByteMap.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#ifndef BYTE_MAP_H
#define BYTE_MAP_H

#include "namespaces.hpp"
#include "ByteString.hpp"
#include <unordered_set>

// --- Hash ---
struct ByteStringHash {
    size_t operator()(const ByteString& s) const {
        unsigned long aHash = 5381;
        for (int i = 0; i < s.mLength; i++) {
            int aVal = s.mData[i];
            aHash = ((aHash << 5) + aHash) ^ aVal;
        }
        return (size_t)aHash;
    }
};

// --- Equality ---
struct ByteStringEqual {
    bool operator()(const ByteString& a, const ByteString& b) const {
        return a.Compare(b) == 0;
    }
};

class ByteMap {
public:
    
    ByteMap();
    ~ByteMap();
    
    void                                                                Add(const ByteString &pKey);
    void                                                                Add(const string &pKey);
    
    bool                                                                Exists(const ByteString &pKey);
    bool                                                                Exists(const string &pKey);
    
    void                                                                Clear();
    
private:
    
    std::unordered_set<ByteString, ByteStringHash, ByteStringEqual>     mSet;
};

#endif
