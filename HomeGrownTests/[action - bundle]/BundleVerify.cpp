//
//  BundleVerify.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/4/26.
//

#include "BundleVerify.hpp"
#include "Layout.hpp"

bool BundleVerify::Execute(JobBundle &pJob, vector<WrappedArchive> pReal, vector<FakeArchive> pMock, ByteString *pError) {
    
    int aRealIndex = 0;
    int aRealCount = (int)pReal.size();
    
    int aMockIndex = 0;
    int aMockCount = (int)pMock.size();
    
    
    while ((aRealIndex < aRealCount) && (aMockIndex < aMockCount)) {
        
        WrappedArchive *aReal = &(pReal[aRealIndex]);
        FakeArchive *aMock = &(pMock[aMockIndex]);
        
        int aRealBlockIndex = 0;
        int aRealBlockCount = (int)(aReal->mBlocks.size());
        
        int aMockBlockIndex = 0;
        int aMockBlockCount = (int)(aMock->mBlocks.size());
        
        while ((aRealBlockIndex < aRealBlockCount) && (aMockBlockIndex < aMockBlockCount)) {
            
            WrappedArchiveBlock *aRealBlock = &(aReal->mBlocks[aRealBlockIndex]);
            FakeArchiveBlock *aMockBlock = &(aMock->mBlocks[aMockBlockIndex]);
            
            int aRealPayloadIndex = 0;
            int aRealPayloadCount = aRealBlock->mPayload.mLength;
            
            int aMockPayloadIndex = 0;
            int aMockPayloadCount = aMockBlock->mPayload.mLength;
            
            while ((aRealPayloadIndex < aRealPayloadCount) && (aMockPayloadIndex < aMockPayloadCount)) {
                unsigned char aRealByte = aRealBlock->mPayload.mData[aRealPayloadIndex];
                unsigned char aMockByte = aMockBlock->mPayload.mData[aMockPayloadIndex];
                if (aRealByte != aMockByte) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", byte ") + ByteString(aRealPayloadIndex) +
                        ByteString(", real byte (") + ByteString(aRealByte) +
                        ByteString(") was not equal to mock byte (") + ByteString(aMockByte) + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
                
                ++aRealPayloadIndex;
                ++aMockPayloadIndex;
            }
            
            // The clincher: which blocks must be invalid.
            
            int aRealSkipIndex1 = aRealBlock->mHeader.mSkipRecord.mArchiveDistance;
            int aRealSkipIndex2 = aRealBlock->mHeader.mSkipRecord.mBlockDistance;
            int aRealSkipIndex3 = Layout::ToInt(aRealBlock->mHeader.mSkipRecord.mByteDistance);
            
            int aMockSkipIndex1 = aMockBlock->mHeader.mSkipRecord.mArchiveIndex;
            int aMockSkipIndex2 = aMockBlock->mHeader.mSkipRecord.mBlockIndex;
            int aMockSkipIndex3 = aMockBlock->mHeader.mSkipRecord.mByteIndex;
            
            if (aMockBlock->mHeader.mSkipRecord.mExpectInvalid) {
                
                /*
                if (aRealSkipIndex1 < (int)pReal.size()) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real skip record archive (") + ByteString(aRealSkipIndex1) +
                        ByteString(") was expected to be invalid, so larger than (") + ByteString((int)pReal.size())
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
                if (aRealSkipIndex2 < pJob.mBlocksPerArchive) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real skip record block (") + ByteString(aRealSkipIndex1) +
                        ByteString(") was expected to be invalid, so larger than (") + ByteString(pJob.mBlocksPerArchive)
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
                if (aRealSkipIndex3 < pJob.mPayloadBytesPerBlock) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real skip record byte (") + ByteString(aRealSkipIndex1) +
                        ByteString(") was expected to be invalid, so larger than (") + ByteString(pJob.mPayloadBytesPerBlock)
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
                */
                
            } else {
                
                /*
                if (aRealSkipIndex1 != aMockSkipIndex1) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real skip record archive (") + ByteString(aRealSkipIndex1) +
                        ByteString(") was not equal to mock skip record archive (") + ByteString(aMockSkipIndex1)
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
                if (aRealSkipIndex2 != aMockSkipIndex2) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real skip record block (") + ByteString(aRealSkipIndex2) +
                        ByteString(") was not equal to mock skip record block (") + ByteString(aMockSkipIndex2)
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
                if (aRealSkipIndex3 != aMockSkipIndex3) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real skip record byte (") + ByteString(aRealSkipIndex3) +
                        ByteString(") was not equal to mock skip record byte (") + ByteString(aMockSkipIndex3)
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
                */
                
            }
            
            
            if (aRealPayloadCount != aMockPayloadCount) {
                if (pError != NULL) {
                    ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                    ByteString(", block ") + ByteString(aRealBlockIndex) +
                    ByteString(", real payload count (") + ByteString(aRealPayloadCount) +
                    ByteString(") was not equal to mock payload count (") + ByteString(aMockPayloadCount)
                    + ByteString(")");
                    
                    pError->Set(aError);
                }
                return false;
            }
            
            ++aRealBlockIndex;
            ++aMockBlockIndex;
        }
        
        if (aRealBlockCount != aMockBlockCount) {
            if (pError != NULL) {
                ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                ByteString(", real block count (") + ByteString(aRealBlockCount) +
                ByteString(") was not equal to mock count (") + ByteString(aMockBlockCount) + ByteString(")");
                pError->Set(aError);
            }
            return false;
        }
        
        ++aRealIndex;
        ++aMockIndex;
    }
    
    if (aRealCount != aMockCount) {
        if (pError != NULL) {
            ByteString aError = ByteString("Real count (") + ByteString(aRealCount) +
            ByteString(") was not equal to mock count (") + ByteString(aMockCount) + ByteString(")");
            pError->Set(aError);
        }
        return false;
    }
    
    return true;
}
