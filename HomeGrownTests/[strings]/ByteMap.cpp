//
//  ByteMap.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "ByteMap.hpp"

ByteMap::ByteMap() {
    
}

ByteMap::~ByteMap() {
    
}

void ByteMap::Add(const string &pKey) {
    Add(ByteString(pKey));
}

void ByteMap::Add(const ByteString &pKey) {
    mSet.insert(pKey);
}

bool ByteMap::Exists(const string &pKey) {
    return Exists(ByteString(pKey));
}

bool ByteMap::Exists(const ByteString &pKey) {
#if __cplusplus >= 202002L
    return mSet.contains(pKey);
#else
    return mSet.find(pKey) != mSet.end();
#endif
}

void ByteMap::Clear() {
    mSet.clear();
}
