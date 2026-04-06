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




FileSpanMap::FileSpanMap() {
    
}

FileSpanMap::~FileSpanMap() {
    
}

void FileSpanMap::Add(const string &pKey, FakeFileBlockSpan &pFileBlockSpan) {
    Add(ByteString(pKey), pFileBlockSpan);
}

void FileSpanMap::Add(const ByteString &pKey, FakeFileBlockSpan &pFileBlockSpan) {
    mMap[pKey] = pFileBlockSpan;
}

bool FileSpanMap::TryGet(const ByteString &pKey, FakeFileBlockSpan *pBlockSpan) {
    if (pBlockSpan == NULL) {
        return false;
    }
    auto aIterator = mMap.find(pKey);
    if (aIterator == mMap.end()) {
        return false;
    }
    *pBlockSpan = aIterator->second;
    return true;
}

bool FileSpanMap::TryGet(const string &pKey, FakeFileBlockSpan *pBlockSpan) {
    return TryGet(ByteString(pKey), pBlockSpan);
}

bool FileSpanMap::Get(const ByteString &pKey, FakeFileBlockSpan *pBlockSpan, ByteString *pError) {
    if (pBlockSpan == NULL) {
        if (pError != NULL) {
            pError->Set("FileSpanMap::Get received null block span pointer.");
        }
        return false;
    }
    if (!TryGet(pKey, pBlockSpan)) {
        if (pError != NULL) {
            pError->Set("File span key was not found: " + pKey.ToString());
        }
        return false;
    }
    return true;
}

bool FileSpanMap::Get(const string &pKey, FakeFileBlockSpan *pBlockSpan, ByteString *pError) {
    return Get(ByteString(pKey), pBlockSpan, pError);
}

bool FileSpanMap::Exists(const string &pKey) {
    return Exists(ByteString(pKey));
}

bool FileSpanMap::Exists(const ByteString &pKey) {
#if __cplusplus >= 202002L
    return mMap.contains(pKey);
#else
    return mMap.find(pKey) != mMap.end();
#endif
}
    
void FileSpanMap::Clear() {
    mMap.clear();
}


FileMap::FileMap() {
    
}

FileMap::~FileMap() {
    
}

void FileMap::Add(const string &pKey, FakeFile &pFile) {
    Add(ByteString(pKey), pFile);
}

void FileMap::Add(const ByteString &pKey, FakeFile &pFile) {
    mMap[pKey] = pFile;
}

bool FileMap::TryGet(const ByteString &pKey, FakeFile *pFile) {
    if (pFile == NULL) {
        return false;
    }
    auto aIterator = mMap.find(pKey);
    if (aIterator == mMap.end()) {
        return false;
    }
    *pFile = aIterator->second;
    return true;
}

bool FileMap::TryGet(const string &pKey, FakeFile *pFile) {
    return TryGet(ByteString(pKey), pFile);
}

bool FileMap::Get(const ByteString &pKey, FakeFile *pFile, ByteString *pError) {
    if (pFile == NULL) {
        if (pError != NULL) {
            pError->Set("FileMap::Get received null block span pointer.");
        }
        return false;
    }
    if (!TryGet(pKey, pFile)) {
        if (pError != NULL) {
            pError->Set("File span key was not found: " + pKey.ToString());
        }
        return false;
    }
    return true;
}

bool FileMap::Get(const string &pKey, FakeFile *pFile, ByteString *pError) {
    return Get(ByteString(pKey), pFile, pError);
}

bool FileMap::Exists(const string &pKey) {
    return Exists(ByteString(pKey));
}

bool FileMap::Exists(const ByteString &pKey) {
#if __cplusplus >= 202002L
    return mMap.contains(pKey);
#else
    return mMap.find(pKey) != mMap.end();
#endif
}
    
void FileMap::Clear() {
    mMap.clear();
}
