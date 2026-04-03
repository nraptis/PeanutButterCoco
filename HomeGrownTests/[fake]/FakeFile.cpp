//
//  FakeFile.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "FakeFile.hpp"
#include "knobs.hpp"
#include "namespaces.hpp"

FakeFile::

FakeFile::FakeFile() {
    mIsFolder = false;
}

FakeFile::~FakeFile() {
    
}

bool FakeFile::HasName(ByteString pName) {
    int aCompareResult = mName.Compare(pName);
    return (aCompareResult == 0);
}

bool FakeFile::HasContent(ByteString pContent) {
    int aCompareResult = mContent.Compare(pContent);
    return (aCompareResult == 0);
    
}

bool FakeFile::ToPayload(ByteString *pPayload, ByteString *pError) {
    if (mName.mLength > knobs::kMaxPathLengthV2) {
        if (pError != NULL) {
            ByteString aErrorString = ByteString("Error: file name length of ") + ByteString(mName.mLength) + ByteString(" exceeds the max (") + ByteString((int)(knobs::kMaxPathLengthV2)) + ByteString(")");
            pError->Set(aErrorString);
        }
        return false;
    }
    
    if (pPayload == NULL) {
        if (pError != NULL) {
            pError->Set("Error: The payload ByteString was null.");
        }
        return false;
    }
    
    unsigned short aNameLength = (unsigned short)(mName.mLength);
    unsigned long long aContentLength = (unsigned long long)(mContent.mLength);
    
    // 1. Name Length (2 bytes, Little Endian)
    unsigned short aNameLen = (unsigned short)mName.mLength;
    unsigned char aNameLenBytes[2];
    aNameLenBytes[0] = (unsigned char)(aNameLen & 0xFF);         // Lower byte
    aNameLenBytes[1] = (unsigned char)((aNameLen >> 8) & 0xFF);  // Upper byte
    pPayload->Append(aNameLenBytes, 2);
    
    // 2. Name Bytes
    pPayload->Append(mName);
    
    // 3. Content Length (8 bytes, Little Endian)
    unsigned long long aContLen = (unsigned long long)mContent.mLength;
    unsigned char aContLenBytes[8];
    for (int i = 0; i < 8; i++) {
        aContLenBytes[i] = (unsigned char)((aContLen >> (8 * i)) & 0xFF);
    }
    pPayload->Append(aContLenBytes, 8);
    
    // 4. Content Bytes
    pPayload->Append(mContent);
    
    return true;
}

bool FakeFile::FromPayload(ByteString &pPayload, ByteString *pError) {
    
    mName.Free();
    mContent.Free();
    
    int aReadPos = 0;

    // 1. Read Name Length (2 bytes, Little Endian)
    if (pPayload.mLength < aReadPos + 2) {
        if (pError) pError->Set("Error: Payload too short for name length.");
        return false;
    }
    
    unsigned short aNameLength = 0;
    aNameLength |= (unsigned short)pPayload.mData[aReadPos++];
    aNameLength |= (unsigned short)pPayload.mData[aReadPos++] << 8;
    
    if (aNameLength > knobs::kMaxPathLengthV2) {
        if (pError != NULL) {
            ByteString aErrorString = ByteString("Error: file name length of ") + ByteString(aNameLength) + ByteString(" exceeds the max (") + ByteString((int)(knobs::kMaxPathLengthV2)) + ByteString(")");
            pError->Set(aErrorString);
            
        }
        return false;
    }

    // 2. Read Name Bytes
    if (pPayload.mLength < aReadPos + aNameLength) {
        if (pError != NULL) {
            pError->Set("Error: Payload too short for name data.");
        }
        return false;
    }
    mName.Set(&pPayload.mData[aReadPos], aNameLength);
    aReadPos += aNameLength;

    // 3. Read Content Length (8 bytes, Little Endian)
    if (pPayload.mLength < aReadPos + 8) {
        if (pError != NULL) {
            pError->Set("Error: Payload too short for content length.");
        }
        return false;
    }

    unsigned long long aContLength = 0;
    for (int i = 0; i < 8; i++) {
        aContLength |= (unsigned long long)(pPayload.mData[aReadPos++] << (8 * i));
    }

    // 4. Read Content Bytes
    if (pPayload.mLength < aReadPos + (int)aContLength) {
        if (pError != NULL) {
            pError->Set("Error: Payload too short for content data.");
        }
        return false;
    }
    mContent.Set(&pPayload.mData[aReadPos], (int)aContLength);
    
    return true;
}
