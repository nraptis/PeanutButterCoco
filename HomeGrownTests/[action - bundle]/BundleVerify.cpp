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
                        ByteString(") was not equal to mock byte (") + ByteString(aMockByte) + ByteString(")\n");
                        
                        /*
                        int aStart = aRealPayloadIndex - 8;
                        int aEnd = aRealPayloadIndex + 16;
                        if (aStart < 0) { aStart = 0; }
                        if (aEnd >= aRealPayloadCount) { aEnd = aRealPayloadCount - 1; }
                        
                        printf("Real: ");
                        for (int aByte=aStart;aByte<=aEnd;aByte++) {
                            printf("%d, ", aRealBlock->mPayload.mData[aByte]);
                        }
                        
                        printf("\nMock: ");
                        for (int aByte=aStart;aByte<=aEnd;aByte++) {
                            printf("%d, ", aMockBlock->mPayload.mData[aByte]);
                        }
                        
                        printf("\n...");
                        */
                        
                        pError->Set(aError);
                    }
                    return false;
                }
                
                ++aRealPayloadIndex;
                ++aMockPayloadIndex;
            }
            
            if (Layout::ToInt(aRealBlock->mHeader.mBlockCountPreview) != aMockBlock->mHeader.mBlockCountPreview) {
                if (pError != NULL) {
                    ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                    ByteString(", block ") + ByteString(aRealBlockIndex) +
                    ByteString(", real section header preview block count (") + ByteString(Layout::ToInt(aRealBlock->mHeader.mBlockCountPreview)) +
                    ByteString(") was not equal to mock section header preview block count (") + ByteString((int)aMockBlock->mHeader.mBlockCountPreview)
                    + ByteString(")");
                    pError->Set(aError);
                }
                return false;
            }
            
            if (Layout::ToInt(aRealBlock->mHeader.mBlockCountMain) != aMockBlock->mHeader.mBlockCountMain) {
                if (pError != NULL) {
                    ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                    ByteString(", block ") + ByteString(aRealBlockIndex) +
                    ByteString(", real section header main block count (") + ByteString(Layout::ToInt(aRealBlock->mHeader.mBlockCountMain)) +
                    ByteString(") was not equal to mock section header main block count (") + ByteString((int)aMockBlock->mHeader.mBlockCountMain)
                    + ByteString(")");
                    pError->Set(aError);
                }
                return false;
            }
            
            if (Layout::ToInt(aRealBlock->mHeader.mBlockCountRepair) != aMockBlock->mHeader.mBlockCountRepair) {
                if (pError != NULL) {
                    ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                    ByteString(", block ") + ByteString(aRealBlockIndex) +
                    ByteString(", real section header repair block count (") + ByteString(Layout::ToInt(aRealBlock->mHeader.mBlockCountRepair)) +
                    ByteString(") was not equal to mock section header repair block count (") + ByteString((int)aMockBlock->mHeader.mBlockCountRepair)
                    + ByteString(")");
                    pError->Set(aError);
                }
                return false;
            }
            
            if (aRealBlock->mHeader.mSectionType != ((unsigned char)aMockBlock->mHeader.mSectionType)) {
                if (pError != NULL) {
                    
                    ByteString aType1 = string("???");
                    if (aRealBlock->mHeader.mSectionType == ((unsigned char)SectionTypeV2::kPreviewManifest)) {
                        aType1 = "PreviewManifest";
                    }
                    if (aRealBlock->mHeader.mSectionType == ((unsigned char)SectionTypeV2::kArchiveData)) {
                        aType1 = "ArchiveData";
                    }
                    if (aRealBlock->mHeader.mSectionType == ((unsigned char)SectionTypeV2::kRepairData)) {
                        aType1 = "RepairData";
                    }
                    
                    ByteString aType2 = string("???");
                    if (aMockBlock->mHeader.mSectionType == ((unsigned char)SectionTypeV2::kPreviewManifest)) {
                        aType2 = "PreviewManifest";
                    }
                    if (aMockBlock->mHeader.mSectionType == ((unsigned char)SectionTypeV2::kArchiveData)) {
                        aType2 = "ArchiveData";
                    }
                    if (aMockBlock->mHeader.mSectionType == ((unsigned char)SectionTypeV2::kRepairData)) {
                        aType2 = "RepairData";
                    }
                    
                    ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                    ByteString(", block ") + ByteString(aRealBlockIndex) +
                    ByteString(", real section header type (") + aType1 +
                    ByteString(") was not equal to mock section header type (") + aType2
                    + ByteString(")");
                    pError->Set(aError);
                }
                return false;
            }
            
            
            
            int aRealRepairIndex1 = aRealBlock->mHeader.mRepairRecord.mArchiveIndex;
            int aRealRepairIndex2 = aRealBlock->mHeader.mRepairRecord.mBlockIndex;
            int aMockRepairIndex1 = aMockBlock->mHeader.mRepairRecord.mArchiveIndex;
            int aMockRepairIndex2 = aMockBlock->mHeader.mRepairRecord.mBlockIndex;
            
            if (aMockBlock->mHeader.mRepairRecord.mExpectInvalid) {
                if (aRealRepairIndex1 < (int)pReal.size()) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real repair record archive (") + ByteString(aRealRepairIndex1) +
                        ByteString(") was expected to be invalid, so larger than (") + ByteString((int)pReal.size())
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
                if (aRealRepairIndex2 < pJob.mBlocksPerArchive) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real repair record block (") + ByteString(aRealRepairIndex1) +
                        ByteString(") was expected to be invalid, so larger than (") + ByteString(pJob.mBlocksPerArchive)
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
            } else {
                if (aRealRepairIndex1 != aMockRepairIndex1) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real repair record archive (") + ByteString(aRealRepairIndex1) +
                        ByteString(") was not equal to mock repair record archive (") + ByteString(aMockRepairIndex1)
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
                if (aRealRepairIndex2 != aMockRepairIndex2) {
                    if (pError != NULL) {
                        ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                        ByteString(", block ") + ByteString(aRealBlockIndex) +
                        ByteString(", real repair record block (") + ByteString(aRealRepairIndex2) +
                        ByteString(") was not equal to mock repair record block (") + ByteString(aMockRepairIndex2)
                        + ByteString(")");
                        pError->Set(aError);
                    }
                    return false;
                }
            }
            
            
            
            
            int aRealSkipIndex1 = Layout::ToInt(aRealBlock->mHeader.mSkipRecord.mArchiveIndex);
            int aRealSkipIndex2 = aRealBlock->mHeader.mSkipRecord.mBlockIndex;
            int aRealSkipIndex3 = Layout::ToInt(aRealBlock->mHeader.mSkipRecord.mByteIndex);
            
            int aMockSkipIndex1 = aMockBlock->mHeader.mSkipRecord.mArchiveIndex;
            int aMockSkipIndex2 = aMockBlock->mHeader.mSkipRecord.mBlockIndex;
            int aMockSkipIndex3 = aMockBlock->mHeader.mSkipRecord.mByteIndex;
            
            if (aMockBlock->mHeader.mSkipRecord.mExpectInvalid) {
                
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
                
            } else {
                
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
        
        if (Layout::ToInt(aReal->mHeader.mBlockCountPreview) != aMock->mHeader.mBlockCountPreview) {
            if (pError != NULL) {
                ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                ByteString(", real archive header preview block count (") + ByteString(Layout::ToInt(aReal->mHeader.mBlockCountPreview)) +
                ByteString(") was not equal to mock archive header preview block count (") + ByteString((int)aMock->mHeader.mBlockCountPreview)
                + ByteString(")");
                pError->Set(aError);
            }
            return false;
        }
        
        if (Layout::ToInt(aReal->mHeader.mBlockCountMain) != aMock->mHeader.mBlockCountMain) {
            if (pError != NULL) {
                ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                ByteString(", real archive header main block count (") + ByteString(Layout::ToInt(aReal->mHeader.mBlockCountMain)) +
                ByteString(") was not equal to mock archive header main block count (") + ByteString((int)aMock->mHeader.mBlockCountMain)
                + ByteString(")");
                pError->Set(aError);
            }
            return false;
        }
        
        if (Layout::ToInt(aReal->mHeader.mBlockCountRepair) != aMock->mHeader.mBlockCountRepair) {
            if (pError != NULL) {
                ByteString aError = ByteString("At archive ") + ByteString(aRealIndex) +
                ByteString(", real archive header repair block count (") + ByteString(Layout::ToInt(aReal->mHeader.mBlockCountRepair)) +
                ByteString(") was not equal to mock archive header repair block count (") + ByteString((int)aMock->mHeader.mBlockCountRepair)
                + ByteString(")");
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

