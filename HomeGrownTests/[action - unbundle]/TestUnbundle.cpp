//
//  TestUnbundle.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#include "TestUnbundle.hpp"
#include "ByteMap.hpp"
#include "namespaces.hpp"
#include "Layout.hpp"
#include "BlockReference.hpp"

bool TestUnbundle::ExecuteConsecutive(vector<FakeArchiveBlock> *pConsecutiveBlockList,
                                      int pBlocksPerArchive,
                                      int pPayloadOffset,
                                      int pPayloadBytesPerBlock,
                                      int pMaxPathLength,
                                      ByteMap *pNameMap,
                                      vector<FakeFile> *pResult,
                                      ByteString *pError) {
    
    if (pConsecutiveBlockList == NULL) {
        if (pError != NULL) {
            pError->Set("Test bundle execute consecutive received null block vector.");
        }
        return false;
    }
    
    if (pResult == NULL) {
        if (pError != NULL) {
            pError->Set("Test bundle execute consecutive received null result file vector.");
        }
        return false;
    }
    
    if (pConsecutiveBlockList->size() <= 0) {
        return true;
    }
    
    
    
    
    ByteString aPayload;
    aPayload.Size(((int)pConsecutiveBlockList->size()) * pPayloadBytesPerBlock);
    
    FakeArchiveBlock *aPreviousBlock = NULL;
    
    /*
    for (int aBlockIndex=0; aBlockIndex<pConsecutiveBlockList->size(); aBlockIndex++) {
        FakeArchiveBlock *aCurrentBlock = &((*pConsecutiveBlockList)[aBlockIndex]);
        printf("doin blox [%d] = (%d | %d)\n", aBlockIndex, (int)aCurrentBlock->mHeader.mArchiveIndex, (int)aCurrentBlock->mHeader.mBlockIndex);
    }
    */
    
    for (int aBlockIndex=0; aBlockIndex<pConsecutiveBlockList->size(); aBlockIndex++) {
        FakeArchiveBlock *aCurrentBlock = &((*pConsecutiveBlockList)[aBlockIndex]);
        
        if (aCurrentBlock->mHeader.mSectionType != ((unsigned char)SectionTypeV2::kArchiveData)) {
            printf("FATAL: This should not happen on consecutive groomed block\n");
            printf("Non-archive data block %d.\n", aBlockIndex);
            break;
        }
            
        if (aPreviousBlock != NULL) {
            int aPreviousFlatIndex = FlattenedMainIndex(aPreviousBlock, pBlocksPerArchive);
            int aCurrentFlatIndex = FlattenedMainIndex(aCurrentBlock, pBlocksPerArchive);
            
            if ((aCurrentFlatIndex - aPreviousFlatIndex) != 1) {
                printf("FATAL: This should not happen on consecutive groomed block\n");
                printf("Non-consecutive flat indices %d to %d.\n", aPreviousFlatIndex, aCurrentFlatIndex);
                break;
            }
        }
        
        aPayload.Append(aCurrentBlock->mPayload);
        
        aPreviousBlock = aCurrentBlock;
    }
    
    //for (auto aBlock : *pConsecutiveBlockList) {
    //    aPayload.Append(aBlock.mPayload);
    //}
    
    
    int aPayloadIndex = pPayloadOffset;
    int aPayloadLength = aPayload.mLength;
    
    //FlattenedMainIndex
    
    
    while (aPayloadIndex < aPayloadLength) {
        
        int aRemainingBytes = (aPayloadLength - aPayloadIndex);
        
        if (aRemainingBytes < 2) { break; }
        
        unsigned short aNameLength = 0;
        aNameLength |= (unsigned short)aPayload.mData[aPayloadIndex + 0];
        aNameLength |= (unsigned short)aPayload.mData[aPayloadIndex + 1] << 8;
        
        aPayloadIndex += 2;
        aRemainingBytes -= 2;
        
        if (aNameLength > pMaxPathLength) {
            if (pError != NULL) {
                ByteString aErrorString =
                ByteString("Error: file name length of ") +
                ByteString(aNameLength) +
                ByteString(" exceeds the max (") +
                ByteString((int)(pMaxPathLength)) +
                ByteString(")");
                pError->Set(aErrorString);
            }
            return false;
        }
        
        if (aRemainingBytes < aNameLength) { break; }
        
        FakeFile aFile;
        
        aFile.mName.Set(&(aPayload.mData[aPayloadIndex]), aNameLength);
        
        aPayloadIndex += aNameLength;
        aRemainingBytes -= aNameLength;
        
        if (aRemainingBytes < 1) { break; }
        
        unsigned char aType = aPayload.mData[aPayloadIndex];
        
        // The flag byte.
        aPayloadIndex += 1;
        aRemainingBytes -= 1;
        
        if (aType == (unsigned char)TypedRecordTypeV2::kDataFolder) {
            if (pNameMap->Exists(aFile.mName)) {
                ByteString aFixedName = Layout::GetUniqueName(aFile.mName, pNameMap);
                aFile.mName.Set(aFixedName);
            }
            pNameMap->Add(aFile.mName);
            aFile.mIsFolder = true;
            pResult->push_back(aFile);
            continue;
        }
        
        if (aType != (unsigned char)TypedRecordTypeV2::kDataFile) { break; }
        
        if (aRemainingBytes < 8) {
            aFile.mName = ByteString("$PARTIAL_") + ByteString(aFile.mName);
            if (pNameMap->Exists(aFile.mName)) {
                ByteString aFixedName = Layout::GetUniqueName(aFile.mName, pNameMap);
                aFile.mName.Set(aFixedName);
            }
            pNameMap->Add(aFile.mName);
            aFile.mContent.Set("");
            aFile.mIsPartial = true;
            pResult->push_back(aFile);
            break;
        }
        
        unsigned long long aContentLength = 0;
        aContentLength |= ((unsigned long long)aPayload.mData[aPayloadIndex + 0]) << 0;
        aContentLength |= ((unsigned long long)aPayload.mData[aPayloadIndex + 1]) << 8;
        aContentLength |= ((unsigned long long)aPayload.mData[aPayloadIndex + 2]) << 16;
        aContentLength |= ((unsigned long long)aPayload.mData[aPayloadIndex + 3]) << 24;
        aContentLength |= ((unsigned long long)aPayload.mData[aPayloadIndex + 4]) << 32;
        aContentLength |= ((unsigned long long)aPayload.mData[aPayloadIndex + 5]) << 40;
        aContentLength |= ((unsigned long long)aPayload.mData[aPayloadIndex + 6]) << 48;
        aContentLength |= ((unsigned long long)aPayload.mData[aPayloadIndex + 7]) << 56;
        
        aPayloadIndex += 8;
        aRemainingBytes -= 8;
        
        if (aContentLength > aRemainingBytes) {
            aFile.mContent.Set(&aPayload.mData[aPayloadIndex], aRemainingBytes);
            aFile.mIsPartial = true;
            
            aFile.mName = ByteString("$PARTIAL_") + ByteString(aFile.mName);
            if (pNameMap->Exists(aFile.mName)) {
                ByteString aFixedName = Layout::GetUniqueName(aFile.mName, pNameMap);
                aFile.mName.Set(aFixedName);
            }
            pNameMap->Add(aFile.mName);
            
            pResult->push_back(aFile);
            
            aPayloadIndex += aRemainingBytes;
            aRemainingBytes = 0;
        } else {
            aFile.mContent.Set(&aPayload.mData[aPayloadIndex], (int)aContentLength);
            aFile.mIsPartial = false;
            
            if (pNameMap->Exists(aFile.mName)) {
                ByteString aFixedName = Layout::GetUniqueName(aFile.mName, pNameMap);
                aFile.mName.Set(aFixedName);
            }
            pNameMap->Add(aFile.mName);
            pResult->push_back(aFile);
            aPayloadIndex += aContentLength;
            aRemainingBytes -= aContentLength;
        }
    }
    
    return true;
}


bool TestUnbundle::PerformMock(JobBundle &pJob,
                               vector<FakeArchive> *pArchiveList,
                               vector<FakeFile> *pResult,
                               ByteString *pError) {
    
    if (pArchiveList == NULL) {
        if (pError != NULL) {
            pError->Set("Test bundle perform mock received null archive vector.");
        }
        return false;
    }
    
    if (pResult == NULL) {
        if (pError != NULL) {
            pError->Set("Test bundle perform mock received null result file vector.");
        }
        return false;
    }
    
    if (pJob.mBlocksPerArchive <= 0) {
        if (pError != NULL) {
            pError->Set("Test bundle perform mock received invalid blocks per archive.");
        }
        return false;
    }
    
    if (pJob.mPayloadBytesPerBlock <= 0) {
        if (pError != NULL) {
            pError->Set("Test bundle perform mock received invalid payload bytes per block.");
        }
        return false;
    }
    
    pResult->clear();
    
    if (pArchiveList->size() <= 0) {
        printf("Warning: Test bundle perform mock received empty archive list.");
        return true;
    }
    
    
    vector<FakeArchiveBlock> aBlockListFlat;
    bool aDidHitFailure = false;
    int aItemIndex = 0;
    int aPreviewBlockCount = (int)((*pArchiveList)[0].mHeader.mBlockCountPreview);
    int aMainBlockCount = (int)((*pArchiveList)[0].mHeader.mBlockCountMain);
    
    for (int aArchiveIndex=0; aArchiveIndex<pArchiveList->size(); aArchiveIndex++) {
        
        FakeArchive aArchive = (*pArchiveList)[aArchiveIndex];
        
        if (aArchive.mHeader.mArchiveIndex != aArchiveIndex) {
            aDidHitFailure = true;
            break;
        }
        
        int aBlockIndexCeiling = (int)aArchive.mBlocks.size();
        if (aBlockIndexCeiling > pJob.mBlocksPerArchive) { aBlockIndexCeiling = pJob.mBlocksPerArchive; }
        
        for (int aBlockIndex=0; aBlockIndex<aBlockIndexCeiling; aBlockIndex++) {
            FakeArchiveBlock aBlock = aArchive.mBlocks[aBlockIndex];
            if (aBlock.mPayload.mLength != pJob.mPayloadBytesPerBlock) {
                if (pError != NULL) {
                    ByteString aError = ByteString("Test bundle perform mock received invalid block payload length, got ") + ByteString(aBlock.mPayload.mLength) +
                    ByteString(" but expected ") + ByteString(pJob.mPayloadBytesPerBlock);
                    pError->Set(aError);
                }
                return false;
            }
            
            //int aItemIndex = (int)aBlock.mHeader.mArchiveIndex * pJob.mBlocksPerArchive + aBlock.mHeader.mBlockIndex;
            if (aItemIndex < aPreviewBlockCount) {
                ++aItemIndex;
                continue;
            }
            
            if (aItemIndex >= aPreviewBlockCount) {
                if (aBlock.mIsInvalid) {
                    aDidHitFailure = true;
                    break;
                }
                if (aBlock.mHeader.mBlockIndex != aBlockIndex) {
                    aDidHitFailure = true;
                    break;
                }
                if (aItemIndex < (aPreviewBlockCount + aMainBlockCount)) {
                    
                    if (aItemIndex == aPreviewBlockCount) {
                        int aFirstBlockIndex = FlattenedMainIndex(&aBlock, pJob.mBlocksPerArchive);
                        if (aFirstBlockIndex != aPreviewBlockCount) {
                            printf("the first block was out of order, whole 'regular unbundle' is broken.\n");
                            return true;
                        }
                    }
                    aBlockListFlat.push_back(aBlock);
                }
            }
            ++aItemIndex;
        }
        
        if (aDidHitFailure == true) {
            break;
        }
    }
    
    
    ByteMap aNameMap;
    return ExecuteConsecutive(&aBlockListFlat,
                              pJob.mBlocksPerArchive,
                              0,
                              pJob.mPayloadBytesPerBlock,
                              pJob.mMaxPathLength,
                              &aNameMap,
                              pResult,
                              pError);
    
}
