//
//  FakeFile.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "FakeFile.hpp"
#include "knobs.hpp"
#include "namespaces.hpp"
#include "FormatUtilities.hpp"

FakeFile::FakeFile() {
    mIsFolder = false;
    
    mIsRecoverDeleted = false;
    mIsRecoverPartial = false;
    
    mIsPartial = false;
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
            ByteString aErrorString =
                ByteString("Error: file name length of ") +
                ByteString(mName.mLength) +
                ByteString(" exceeds the max (") +
                ByteString((int)(knobs::kMaxPathLengthV2)) +
                ByteString(")");
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
    
    pPayload->Clear();
    
    // 1. Name Length (2 bytes, Little Endian)
    unsigned short aNameLength = (unsigned short)mName.mLength;
    unsigned char aNameLengthBytes[2];
    aNameLengthBytes[0] = (unsigned char)(aNameLength & 0xFF);
    aNameLengthBytes[1] = (unsigned char)((aNameLength >> 8) & 0xFF);
    pPayload->Append(aNameLengthBytes, 2);
    
    // 2. Name Bytes
    pPayload->Append(mName);

    // 3. Type
    if (mIsFolder) {
        unsigned char aType = (unsigned char)TypedRecordTypeV2::kDataFolder;
        pPayload->Append(&aType, 1);
        return true;
    }

    unsigned char aType = (unsigned char)TypedRecordTypeV2::kDataFile;
    pPayload->Append(&aType, 1);
    
    // 4. Content Length (8 bytes, Little Endian)
    unsigned long long aContentLength = (unsigned long long)mContent.mLength;
    unsigned char aContentLengthBytes[8];

    aContentLengthBytes[0] = (unsigned char)((aContentLength >> 0)  & 0xFF);
    aContentLengthBytes[1] = (unsigned char)((aContentLength >> 8)  & 0xFF);
    aContentLengthBytes[2] = (unsigned char)((aContentLength >> 16) & 0xFF);
    aContentLengthBytes[3] = (unsigned char)((aContentLength >> 24) & 0xFF);
    aContentLengthBytes[4] = (unsigned char)((aContentLength >> 32) & 0xFF);
    aContentLengthBytes[5] = (unsigned char)((aContentLength >> 40) & 0xFF);
    aContentLengthBytes[6] = (unsigned char)((aContentLength >> 48) & 0xFF);
    aContentLengthBytes[7] = (unsigned char)((aContentLength >> 56) & 0xFF);

    pPayload->Append(aContentLengthBytes, 8);
    
    // 5. Content Bytes
    pPayload->Append(mContent);
    
    return true;
}

bool FakeFile::FromPayload(ByteString &pPayload, ByteString *pError) {
    
    mName.Free();
    mContent.Free();
    
    int aReadPosition = 0;

    // 1. Name Length (2 bytes, Little Endian)
    if (pPayload.mLength < aReadPosition + 2) {
        if (pError != NULL) {
            pError->Set("Error: Payload too short for name length.");
        }
        return false;
    }
    
    unsigned short aNameLength = 0;
    aNameLength |= (unsigned short)pPayload.mData[aReadPosition++];
    aNameLength |= (unsigned short)pPayload.mData[aReadPosition++] << 8;
    
    if (aNameLength > knobs::kMaxPathLengthV2) {
        if (pError != NULL) {
            ByteString aErrorString =
                ByteString("Error: file name length of ") +
                ByteString(aNameLength) +
                ByteString(" exceeds the max (") +
                ByteString((int)(knobs::kMaxPathLengthV2)) +
                ByteString(")");
            pError->Set(aErrorString);
        }
        return false;
    }

    // 2. Name Bytes
    if (pPayload.mLength < aReadPosition + aNameLength) {
        if (pError != NULL) {
            pError->Set("Error: Payload too short for name data.");
        }
        return false;
    }
    
    mName.Set(&pPayload.mData[aReadPosition], aNameLength);
    aReadPosition += aNameLength;

    // 3. Type
    if (pPayload.mLength < aReadPosition + 1) {
        if (pError != NULL) {
            pError->Set("Error: Payload too short for type.");
        }
        return false;
    }
    
    unsigned char aType = pPayload.mData[aReadPosition++];

    if (aType == (unsigned char)TypedRecordTypeV2::kDataFolder) {
        mIsFolder = true;
        return true;
    }
    
    if (aType != (unsigned char)TypedRecordTypeV2::kDataFile) {
        if (pError != NULL) {
            pError->Set("Error: Unknown record type.");
        }
        return false;
    }
    
    mIsFolder = false;

    // 4. Content Length (8 bytes, Little Endian)
    if (pPayload.mLength < aReadPosition + 8) {
        if (pError != NULL) {
            pError->Set("Error: Payload too short for content length.");
        }
        return false;
    }

    unsigned long long aContentLength = 0;

    aContentLength |= ((unsigned long long)pPayload.mData[aReadPosition++]) << 0;
    aContentLength |= ((unsigned long long)pPayload.mData[aReadPosition++]) << 8;
    aContentLength |= ((unsigned long long)pPayload.mData[aReadPosition++]) << 16;
    aContentLength |= ((unsigned long long)pPayload.mData[aReadPosition++]) << 24;
    aContentLength |= ((unsigned long long)pPayload.mData[aReadPosition++]) << 32;
    aContentLength |= ((unsigned long long)pPayload.mData[aReadPosition++]) << 40;
    aContentLength |= ((unsigned long long)pPayload.mData[aReadPosition++]) << 48;
    aContentLength |= ((unsigned long long)pPayload.mData[aReadPosition++]) << 56;

    // 5. Content Bytes
    if (pPayload.mLength < aReadPosition + (int)aContentLength) {
        if (pError != NULL) {
            pError->Set("Error: Payload too short for content data.");
        }
        return false;
    }

    mContent.Set(&pPayload.mData[aReadPosition], (int)aContentLength);
    
    return true;
}

bool FakeFile::ToPreviewPayload(ByteString *pPayload, ByteString *pError) {
    if (mName.mLength > knobs::kMaxPathLengthV2) {
        if (pError != NULL) {
            ByteString aErrorString =
            ByteString("Error: file name length of ") +
            ByteString(mName.mLength) +
            ByteString(" exceeds the max (") +
            ByteString((int)(knobs::kMaxPathLengthV2)) +
            ByteString(")");
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
    
    pPayload->Clear();
    
    // 1. Name Length (2 bytes, Little Endian)
    unsigned short aNameLength = (unsigned short)mName.mLength;
    unsigned char aNameLengthBytes[2];
    aNameLengthBytes[0] = (unsigned char)(aNameLength & 0xFF);
    aNameLengthBytes[1] = (unsigned char)((aNameLength >> 8) & 0xFF);
    pPayload->Append(aNameLengthBytes, 2);
    
    // 2. Name Bytes
    pPayload->Append(mName);
    
    // 3. Type
    if (mIsFolder) {
        unsigned char aType = (unsigned char)TypedRecordTypeV2::kManifestFolder;
        pPayload->Append(&aType, 1);
        
        unsigned char aExtra = memory_layout::specs_verified::kPreviewRecordPlaceholderValueV2;
        pPayload->Append(&aExtra, 1);
        
        return true;
    }
    
    unsigned char aType = (unsigned char)TypedRecordTypeV2::kManifestFile;
    pPayload->Append(&aType, 1);
    
    unsigned char aExtra = memory_layout::specs_verified::kPreviewRecordPlaceholderValueV2;
    pPayload->Append(&aExtra, 1);
    
    // 4. Content Length (8 bytes, Little Endian)
    unsigned long long aContentLength = (unsigned long long)mContent.mLength;
    unsigned char aContentLengthBytes[8];
    
    aContentLengthBytes[0] = (unsigned char)((aContentLength >> 0)  & 0xFF);
    aContentLengthBytes[1] = (unsigned char)((aContentLength >> 8)  & 0xFF);
    aContentLengthBytes[2] = (unsigned char)((aContentLength >> 16) & 0xFF);
    aContentLengthBytes[3] = (unsigned char)((aContentLength >> 24) & 0xFF);
    aContentLengthBytes[4] = (unsigned char)((aContentLength >> 32) & 0xFF);
    aContentLengthBytes[5] = (unsigned char)((aContentLength >> 40) & 0xFF);
    aContentLengthBytes[6] = (unsigned char)((aContentLength >> 48) & 0xFF);
    aContentLengthBytes[7] = (unsigned char)((aContentLength >> 56) & 0xFF);
    
    pPayload->Append(aContentLengthBytes, 8);
    
    return true;
}

void FakeFile::RecoverTruncate(int pSize) {
    if (pSize < mContent.mLength) {
        mIsRecoverPartial = true;
        mContent.Truncate(pSize);
    }
}

void FakeFile::RecoverDelete() {
    mIsRecoverDeleted = true;
}


