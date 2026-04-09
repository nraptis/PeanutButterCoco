//
//  TestRecover.cpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/6/26.
//

#include "TestRecover.hpp"
#include "ByteMap.hpp"
#include "namespaces.hpp"
#include "Layout.hpp"
#include "BlockReference.hpp"
#include "TestUnbundle.hpp"
#include <algorithm>



/*
bool IsMainBlock(const FakeArchiveBlock *pBlock, int pBlocksPerArchive) {
    int aFlatIndex = FlattenedMainIndex(pBlock, pBlocksPerArchive);
    if ((aFlatIndex >= pBlock->mHeader.mBlockCountPreview) &&
        (aFlatIndex < (pBlock->mHeader.mBlockCountPreview + pBlock->mHeader.mBlockCountMain))) {
        return true;
    } else {
        return false;
    }
}
*/

bool IsValidSkip(const FakeArchiveBlock *pBlock,
                  int pBlocksPerArchive,
                  int pPayloadBytesPerBlock) {
    if (pBlock->mHeader.mSkipRecord.mByteIndex < 0) { return false; }
    if (pBlock->mHeader.mSkipRecord.mByteIndex >= pPayloadBytesPerBlock) { return false; }
    if (pBlock->mHeader.mSkipRecord.mBlockIndex < 0) { return false; }
    if (pBlock->mHeader.mSkipRecord.mBlockIndex >= pBlocksPerArchive) { return false; }
    if (pBlock->mHeader.mSkipRecord.mArchiveIndex < 0) { return false; }
    if (pBlock->mHeader.mSkipRecord.mArchiveIndex >= pBlock->mHeader.mArchiveCount) { return false; }
    return true;
}



bool IsNeighbor(const FakeArchiveBlock *pBlock1, const FakeArchiveBlock *pBlock2, int pBlocksPerArchive) {
    int aFlatIndex1 = FlattenedMainIndex(pBlock1, pBlocksPerArchive);
    int aFlatIndex2 = FlattenedMainIndex(pBlock2, pBlocksPerArchive);
    return ((aFlatIndex1 + 1) == aFlatIndex2);
}

bool TestRecover::PerformMock(JobBundle &pJob,
                               vector<FakeArchive> *pArchiveList,
                               vector<FakeFile> *pResult,
                              ByteString *pError) {
    
    if (pArchiveList == NULL) {
        if (pError != NULL) {
            pError->Set("Test recover perform mock received null archive vector.");
        }
        return false;
    }
    
    if (pResult == NULL) {
        if (pError != NULL) {
            pError->Set("Test recover perform mock received null result file vector.");
        }
        return false;
    }
    
    if (pJob.mBlocksPerArchive <= 0) {
        if (pError != NULL) {
            pError->Set("Test recover perform mock received invalid blocks per archive.");
        }
        return false;
    }
    
    if (pJob.mPayloadBytesPerBlock <= 0) {
        if (pError != NULL) {
            pError->Set("Test recover perform mock received invalid payload bytes per block.");
        }
        return false;
    }
    
    pResult->clear();
    
    if (pArchiveList->size() <= 0) {
        printf("Warning: Test recover perform mock received empty archive list.");
        return true;
    }
    
    
    vector<FakeArchiveBlock *> aRepairBlocks;
    unordered_map<BlockReference, FakeArchiveBlock *, BlockHasher> aMainBlockMap;
    
    for (int aArchiveIndex=0; aArchiveIndex<pArchiveList->size(); aArchiveIndex++) {
        
        FakeArchive aArchive = (*pArchiveList)[aArchiveIndex];
        
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
            
            int aItemIndex = (int)aBlock.mHeader.mArchiveIndex * pJob.mBlocksPerArchive + aBlock.mHeader.mBlockIndex;
            if (aItemIndex >= aBlock.mHeader.mBlockCountPreview) {
                if (aItemIndex < (aBlock.mHeader.mBlockCountPreview + aBlock.mHeader.mBlockCountMain)) {
                    BlockReference aBlockReference;
                    aBlockReference.mArchiveIndex = (int)aBlock.mHeader.mArchiveIndex;
                    aBlockReference.mBlockIndex = aBlock.mHeader.mBlockIndex;
                    aMainBlockMap[aBlockReference] = &((*pArchiveList)[aArchiveIndex].mBlocks[aBlockIndex]);
                    
                } else {
                    aRepairBlocks.push_back(&((*pArchiveList)[aArchiveIndex].mBlocks[aBlockIndex]));
                }
            }
        }
    }
    
    for (int aRepairIndex=0; aRepairIndex<aRepairBlocks.size(); aRepairIndex++) {
        FakeArchiveBlock *aRepairBlock = aRepairBlocks[aRepairIndex];
        if (aRepairBlock->mIsInvalid) { continue; }
        
        BlockReference aBlockReference;
        aBlockReference.mArchiveIndex = (int)aRepairBlock->mHeader.mRepairRecord.mArchiveIndex;
        aBlockReference.mBlockIndex = aRepairBlock->mHeader.mRepairRecord.mBlockIndex;
        
        auto aIterator = aMainBlockMap.find(aBlockReference);
        if (aIterator == aMainBlockMap.end()) { continue; }
        
        FakeArchiveBlock *aMainBlock = aMainBlockMap[aBlockReference];
        if (aMainBlock == NULL) { continue; }
        
        aMainBlock->mIsInvalid = false;
        
        /*
        printf("Recover healed block [%d|%d] with repair block {%d}\n",
               (int)aRepairBlock->mHeader.mRepairRecord.mArchiveIndex,
               aRepairBlock->mHeader.mRepairRecord.mBlockIndex,
               aRepairBlock->mHeader.mBlockIndex);
        */
    }
    
    
    // Fill with all where (mIsInvalid == false)
    vector<FakeArchiveBlock *> aBlockListSorted;
    for (auto &aMainPair : aMainBlockMap) {
        FakeArchiveBlock *aBlock = aMainPair.second;
        if (aBlock == NULL) {
            continue;
        }
        if (aBlock->mIsInvalid) {
            continue;
        }
        aBlockListSorted.push_back(aBlock);
    }
    
    // Sort in ascending order by
    // 1.) aRepairBlock->mHeader.mArchiveIndex
    // 2.) aRepairBlock->mHeader.mBlockIndex
    std::sort(aBlockListSorted.begin(),
              aBlockListSorted.end(),
              [](FakeArchiveBlock *pLeft, FakeArchiveBlock *pRight) {
        if (pLeft == NULL) {
            return true;
        }
        if (pRight == NULL) {
            return false;
        }
        
        if (pLeft->mHeader.mArchiveIndex != pRight->mHeader.mArchiveIndex) {
            return pLeft->mHeader.mArchiveIndex < pRight->mHeader.mArchiveIndex;
        }
        return pLeft->mHeader.mBlockIndex < pRight->mHeader.mBlockIndex;
    });
    
    
    int aBlocksPerArchive = pJob.mBlocksPerArchive;
    (void)aBlocksPerArchive;
    vector<vector<FakeArchiveBlock *>> aRuns;
    
    int aStartIndex = 0;
    while (aStartIndex < aBlockListSorted.size()) {
        vector<FakeArchiveBlock *> aRun;
        FakeArchiveBlock *aPrevious = aBlockListSorted[aStartIndex];
        aRun.push_back(aPrevious);
        int aEndIndex = aStartIndex + 1;
        while (aEndIndex < aBlockListSorted.size()) {
            FakeArchiveBlock *aCurrent = aBlockListSorted[aEndIndex];
            if (IsNeighbor(aPrevious, aCurrent, aBlocksPerArchive)) {
                aRun.push_back(aCurrent);
                ++aEndIndex;
            } else {
                break;
            }
            aPrevious = aCurrent;
        }
        aRuns.push_back(aRun);
        aStartIndex = aEndIndex;
    }
    
    ByteMap aNameMap;
    
    for (auto aRun: aRuns) {
        if (aRun.size() <= 0) {
            if (pError != NULL) {
                pError->Set("Run logic did not work.");
            }
            return false;
        }
        
        int aTravelIndex = 0;
        while (aTravelIndex < aRun.size()) {
            FakeArchiveBlock *aBlock = aRun[aTravelIndex];
            if (IsFirstBlock(aBlock, pJob.mBlocksPerArchive)) { break; }
            if (IsValidSkip(aBlock,
                            pJob.mBlocksPerArchive,
                            pJob.mPayloadBytesPerBlock)) {
                break;
            } else {
                aTravelIndex++;
            }
        }
        
        if (aTravelIndex < aRun.size()) {
            int aSkipByte = 0;
            while (aTravelIndex < aRun.size()) {
                FakeArchiveBlock *aCheckBlock = aRun[aTravelIndex];
                if (IsFirstBlock(aCheckBlock, aBlocksPerArchive)) {
                    break;
                } else {
                    if (aCheckBlock->mHeader.mArchiveIndex == aCheckBlock->mHeader.mSkipRecord.mArchiveIndex) {
                        if (aCheckBlock->mHeader.mBlockIndex == aCheckBlock->mHeader.mSkipRecord.mBlockIndex) {
                            if (aCheckBlock->mHeader.mSkipRecord.mByteIndex >= 0 && aCheckBlock->mHeader.mSkipRecord.mByteIndex < pJob.mPayloadBytesPerBlock) {
                                aSkipByte = aCheckBlock->mHeader.mSkipRecord.mByteIndex;
                                break;
                            }
                        }
                    } else if (aCheckBlock->mHeader.mArchiveIndex > aCheckBlock->mHeader.mSkipRecord.mArchiveIndex) {
                        if (aCheckBlock->mHeader.mSkipRecord.mByteIndex >= 0 && aCheckBlock->mHeader.mSkipRecord.mByteIndex < pJob.mPayloadBytesPerBlock) {
                            aSkipByte = aCheckBlock->mHeader.mSkipRecord.mByteIndex;
                            break;
                        }
                    }
                }
                ++aTravelIndex;
            }
            
            vector<FakeArchiveBlock> aBlockList;
            while (aTravelIndex < aRun.size()) {
                aBlockList.push_back(*aRun[aTravelIndex]);
                aTravelIndex++;
            }
            
            if (!TestUnbundle::ExecuteConsecutive(&aBlockList,
                                                  pJob.mBlocksPerArchive,
                                                  aSkipByte,
                                                  pJob.mPayloadBytesPerBlock,
                                                  pJob.mMaxPathLength,
                                                  &aNameMap,
                                                  pResult,
                                                  pError)) {
                return false;
            }
        }
    }
    
    return true;
}
