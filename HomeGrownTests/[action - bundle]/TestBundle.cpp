//
//  TestBundle.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "TestBundle.hpp"
#include "MockHardDrive.hpp"
#include "MockFileSystem.hpp"
#include "BlockReference.hpp"
#include "namespaces.hpp"

bool TestBundle::PackBlocks(vector<ByteString> *pPayloadList, vector<ByteString> *pPayloadBlocks, int pBlockLength, ByteString *pError) {
    
    if (pPayloadList == NULL) {
        if (pError != NULL) {
            pError->Set("Pack blocks received null payload list vector.");
        }
        return false;
    }
    
    if (pPayloadBlocks == NULL) {
        if (pError != NULL) {
            pError->Set("Pack blocks received null payload chunks vector.");
        }
        return false;
    }
    
    if (pBlockLength <= 0) {
        if (pError != NULL) {
            pError->Set("Pack blocks received a chunk length of 0 or less.");
        }
        return false;
    }
    
    int aTotalLength = 0;
    for (int aPayloadIndex = 0; aPayloadIndex < ((int)pPayloadList->size()); aPayloadIndex++) {
        aTotalLength += (*pPayloadList)[aPayloadIndex].mLength;
    }
    
    int aBlockCount = (aTotalLength + pBlockLength - 1) / pBlockLength;
    for (int i = 0; i < aBlockCount; i++) {
        ByteString aBlock;
        (*pPayloadBlocks).push_back(aBlock);
    }
    
    for (int i = 0; i < aBlockCount; i++) {
        (*pPayloadBlocks)[i].Size(pBlockLength);
        memset((*pPayloadBlocks)[i].mData, 0, pBlockLength);
        (*pPayloadBlocks)[i].mLength = pBlockLength;
    }
    
    int aPayloadIndex = 0;
    int aPayloadOffset = 0;
    int aPayloadCount = (int)pPayloadList->size();
    int aBlockIndex = 0;
    int aBlockOffset = 0;
    
    while ((aBlockIndex < aBlockCount) && (aPayloadIndex < aPayloadCount)) {
        
        int aPayloadLength = (*pPayloadList)[aPayloadIndex].mLength;
        int aCopyAmountChoiceA = (pBlockLength - aBlockOffset);
        int aCopyAmountChoiceB = (aPayloadLength - aPayloadOffset);
        int aAmount = min(aCopyAmountChoiceA, aCopyAmountChoiceB);
        
        memcpy(&((*pPayloadBlocks)[aBlockIndex].mData[aBlockOffset]),
               &((*pPayloadList)[aPayloadIndex].mData[aPayloadOffset]),
               aAmount);
        
        aBlockOffset += aAmount;
        if (aBlockOffset >= pBlockLength) {
            aBlockOffset = 0;
            ++aBlockIndex;
        }
        
        aPayloadOffset += aAmount;
        if (aPayloadOffset >= aPayloadLength) {
            aPayloadOffset = 0;
            ++aPayloadIndex;
        }
    }
    
    return true;
}

bool TestBundle::PerformMock(JobBundle &pJob, vector<FakeArchive> *pResult, ByteString *pError) {
    
    if (pJob.ContainsDuplicateFiles()) {
        if (pError != NULL) {
            pError->Set("Job contains duplicate files.");
        }
        return false;
    }
    
    if (pJob.mFileList.size() == 0) {
        if (pError != NULL) {
            pError->Set("Job contains no files.");
        }
        return false;
    }
    
    if (pJob.mPayloadBytesPerBlock <= 0) {
        if (pError != NULL) {
            pError->Set("Job contains invalid payload byte count.");
        }
        return false;
    }
    
    if (pJob.mBlocksPerArchive <= 0) {
        if (pError != NULL) {
            pError->Set("Job contains invalid block count.");
        }
        return false;
    }
    
    pJob.SortFiles();
    
    int aBlockLength = pJob.mPayloadBytesPerBlock;
    
    string aSourceFolder = pJob.mInput.ToString();
    string aDestinationFolder = pJob.mArchived.ToString();
    
    vector<int> aSkipTargets;
    
    vector<ByteString> aPreviewPayloadBlocks;
    int aPreviewBlockCount = 0;
    
    if (pJob.mPreviewEnabled) {
        vector<ByteString> aPreviewPayloadList;
        for (int aFileIndex=0; aFileIndex<((int)pJob.mFileList.size()); aFileIndex++) {
            FakeFile aFile = pJob.mFileList[aFileIndex];
            ByteString aPayload;
            if (!aFile.ToPreviewPayload(&aPayload, pError)) {
                return false;
            }
            aPreviewPayloadList.push_back(aPayload);
        }
        if (!PackBlocks(&aPreviewPayloadList, &aPreviewPayloadBlocks, aBlockLength, pError)) {
            return false;
        }
        aPreviewBlockCount = (int)aPreviewPayloadBlocks.size();
    }
    
    vector<ByteString> aMainPayloadList;
    for (int aFileIndex=0; aFileIndex<((int)pJob.mFileList.size()); aFileIndex++) {
        FakeFile aFile = pJob.mFileList[aFileIndex];
        ByteString aPayload;
        if (!aFile.ToPayload(&aPayload, pError)) {
            return false;
        }
        aMainPayloadList.push_back(aPayload);
    }
    int aMainPayloadCount = (int)aMainPayloadList.size();
    if (aMainPayloadCount <= 0) {
        if (pError != NULL) {
            pError->Set("There were no main payload list items.");
        }
        return false;
    }
    
    vector<ByteString> aMainPayloadBlocks;
    if (!PackBlocks(&aMainPayloadList, &aMainPayloadBlocks, aBlockLength, pError)) {
        return false;
    }
    int aMainBlockCount = (int)aMainPayloadBlocks.size();
    if (aMainBlockCount <= 0) {
        if (pError != NULL) {
            pError->Set("There were no main payload chunks.");
        }
        return false;
    }
    
    
    
    int aPayloadByteIndex = aPreviewBlockCount * pJob.mPayloadBytesPerBlock;
    int aBlockIndex = 0;
    int aBlockOffset = 0;
    int aPayloadIndex = 0;
    int aPayloadOffset = 0;
    
    while ((aBlockIndex < aMainBlockCount) && (aPayloadIndex < aMainPayloadCount)) {
        int aPayloadLength = aMainPayloadList[aPayloadIndex].mLength;
        int aCopyAmountChoiceA = (aBlockLength - aBlockOffset);
        int aCopyAmountChoiceB = (aPayloadLength - aPayloadOffset);
        int aAmount = min(aCopyAmountChoiceA, aCopyAmountChoiceB);
        
        if (aBlockOffset == 0) {
            if (aBlockIndex == 0) {
                aSkipTargets.push_back(-1);
            } else {
                if (aPayloadOffset == 0) {
                    aSkipTargets.push_back(aPayloadByteIndex);
                } else if (aPayloadIndex >= (aMainPayloadCount - 1)) {
                    aSkipTargets.push_back(-1);
                } else {
                    aSkipTargets.push_back(aPayloadByteIndex + aPayloadLength);
                }
            }
        }
        
        aBlockOffset += aAmount;
        if (aBlockOffset >= aBlockLength) {
            aBlockOffset = 0;
            ++aBlockIndex;
        }
        
        aPayloadOffset += aAmount;
        if (aPayloadOffset >= aPayloadLength) {
            aPayloadByteIndex += aPayloadLength;
            aPayloadOffset = 0;
            ++aPayloadIndex;
        }
    }
    
    // We might need a last skip target.
    if ((int)aSkipTargets.size() < (aPreviewBlockCount + aMainBlockCount)) {
        if (aPayloadOffset == 0) {
            aSkipTargets.push_back(aPayloadByteIndex);
        } else {
            aSkipTargets.push_back(-1);
        }
    }
    
    int aNonRepairBlockCount = (int)(aPreviewPayloadBlocks.size() + aMainPayloadBlocks.size());
    
    int aRepairBlockCount = 0;
    vector<ByteString> aRepairPayloadBlocks;
    
    int aTargetRepairBlockCount = 0;
    aTargetRepairBlockCount = (aMainBlockCount * ((int)pJob.mRepairCoverage) + 99) / 100;
    
    vector<FakeRepairRecord> aRepairRecords;
    if (aTargetRepairBlockCount > 0) {
        
        vector<FakeRepairRecord> aNewRepairRecords;
        int aFirstMainBlockIndex = (int)aPreviewPayloadBlocks.size();
        for (int aFlatBlockIndex = aFirstMainBlockIndex;
             aFlatBlockIndex < aNonRepairBlockCount &&
             ((int)aNewRepairRecords.size()) < aTargetRepairBlockCount;
             aFlatBlockIndex++) {
            int aRepairArchiveIndex = aFlatBlockIndex / pJob.mBlocksPerArchive;
            int aRepairBlockIndex =
                (aFlatBlockIndex - aRepairArchiveIndex * pJob.mBlocksPerArchive);
            FakeRepairRecord aRepairRecord;
            aRepairRecord.SetValid(aRepairArchiveIndex, aRepairBlockIndex);
            aRepairRecord.mMainBlockIndex = (aFlatBlockIndex - aFirstMainBlockIndex);
            aNewRepairRecords.push_back(aRepairRecord);
        }

        vector<ByteString> aRepairPayloadList;
        int aMainPayloadCount = (int)aMainPayloadList.size();
        if (aMainPayloadCount <= 0) {
            if (pError != NULL) {
                pError->Set("There were no main payload list items.");
            }
            return false;
        }
        
        for (int aRepairIndex=0; aRepairIndex<((int)aNewRepairRecords.size()); aRepairIndex++) {
            FakeRepairRecord aRepairRecord = aNewRepairRecords[aRepairIndex];
            if (aRepairRecord.mExpectInvalid == true) {
                if (pError != NULL) {
                    pError->Set("The repair record, for real block, has invalid target.");
                }
                return false;
            }
            if ((aRepairRecord.mMainBlockIndex < 0) || (aRepairRecord.mMainBlockIndex >= aMainBlockCount)) {
                if (pError != NULL) {
                    pError->Set("The repair record, for real block, points to bad main chunk.");
                }
                return false;
            }
            
            aRepairRecords.push_back(aRepairRecord);
            aRepairPayloadList.push_back(aMainPayloadBlocks[aRepairRecord.mMainBlockIndex]);
        }
        
        if (!PackBlocks(&aRepairPayloadList, &aRepairPayloadBlocks, aBlockLength, pError)) {
            return false;
        }
        
        aRepairBlockCount = (int)aRepairPayloadBlocks.size();
        if (aRepairBlockCount <= 0) {
            if (pError != NULL) {
                pError->Set("There were no repair payload chunks.");
            }
            return false;
        }
        
        /*
        for (int aRepairIndex=0; aRepairIndex<((int)aNewRepairRecords.size()); aRepairIndex++) {
            printf("new repair record[%d] = {%d | %d}\n", aRepairIndex, aNewRepairRecords[aRepairIndex].mArchiveIndex, aNewRepairRecords[aRepairIndex].mBlockIndex);
        }
        */
    }
    
    //printf("Packed %d preview blocks, %d main blocks, and %d repair blocks\n", aPreviewBlockCount, aMainBlockCount, aRepairBlockCount);
    
    int aTotalBlockCount = (int)(aPreviewPayloadBlocks.size() + aMainPayloadBlocks.size() + aRepairPayloadBlocks.size());
    
    vector<ByteString> aPayloadBlocks;
    for (auto aBlock: aPreviewPayloadBlocks) { aPayloadBlocks.push_back(aBlock); }
    for (auto aBlock: aMainPayloadBlocks) { aPayloadBlocks.push_back(aBlock); }
    for (auto aBlock: aRepairPayloadBlocks) { aPayloadBlocks.push_back(aBlock); }
    
    int aArchiveCount = (aTotalBlockCount + (pJob.mBlocksPerArchive - 1)) / pJob.mBlocksPerArchive;
        
    int aBlockIndexOuter = 0;
    
    int aArchiveUUID =  10000;
    int aBlockUUID =    1000;
    
    unordered_map<BlockReference, FakeArchiveBlock, BlockHasher> aMainBlockMap;
    
    for (int aArchiveIndex=0;aArchiveIndex<aArchiveCount;aArchiveIndex++) {
    
        int aBlockCount = (aTotalBlockCount - aBlockIndexOuter);
        if (aBlockCount > pJob.mBlocksPerArchive) {
            aBlockCount = pJob.mBlocksPerArchive;
        }
        int aBlockCeiling = aBlockIndexOuter + aBlockCount;
        
        FakeArchive aArchive;
        aArchive.mHeader.mArchiveIndex = aArchiveIndex;
        aArchive.mHeader.mArchiveCount = aArchiveCount;
        aArchive.mHeader.mBlockCountPreview = aPreviewBlockCount;
        aArchive.mHeader.mBlockCountMain = aMainBlockCount;
        aArchive.mHeader.mBlockCountRepair = aRepairBlockCount;
        aArchive.mArchiveUUID = aArchiveUUID;
        aArchiveUUID += 1;
        
        int aLoopBlockIndex = aBlockIndexOuter;
        while (aLoopBlockIndex < aBlockCeiling) {
            
            FakeArchiveBlock aBlock;
            aBlock.mHeader.mBlockIndex = (aLoopBlockIndex - aBlockIndexOuter);
            aBlock.mHeader.mArchiveIndex = aArchiveIndex;
            aBlock.mHeader.mArchiveCount = aArchiveCount;
            aBlock.mHeader.mBlockCountPreview = aPreviewBlockCount;
            aBlock.mHeader.mBlockCountMain = aMainBlockCount;
            aBlock.mHeader.mBlockCountRepair = aRepairBlockCount;
            aBlock.mBlockUUID = aBlockUUID;
            aBlockUUID += 1;
            
            int aType; // 2 = repair, 1 = main, 0 = preview
            if (aLoopBlockIndex < aPreviewBlockCount) {
                aType = 0;
                aBlock.mHeader.mSectionType = (unsigned char)(SectionTypeV2::kPreviewManifest);
            } else if (aLoopBlockIndex < aNonRepairBlockCount) {
                aType = 1;
                aBlock.mHeader.mSectionType = (unsigned char)(SectionTypeV2::kArchiveData);
            } else {
                aType = 2;
                aBlock.mHeader.mSectionType = (unsigned char)(SectionTypeV2::kRepairData);
            }
            
            if (aType == 2) {
                FakeRepairRecord aRecord = aRepairRecords[aLoopBlockIndex - aNonRepairBlockCount];
                aBlock.mHeader.mRepairRecord.SetValid(aRecord.mArchiveIndex, aRecord.mBlockIndex);
            } else {
                aBlock.mHeader.mRepairRecord.SetInvalid();
            }
            
            if (aType == 0) {
                aBlock.mHeader.mSkipRecord.SetInvalid();
            } else if (aType == 1) {
                
                int aSkipTarget = aSkipTargets[aLoopBlockIndex - aPreviewBlockCount];
                
                if (aSkipTarget == -1) {
                    aBlock.mHeader.mSkipRecord.SetInvalid();
                } else if (!aBlock.mHeader.mSkipRecord.SetValid(aSkipTarget,
                                                                pJob.mBlocksPerArchive,
                                                                pJob.mPayloadBytesPerBlock,
                                                                pError)) {
                    if (pError != NULL) {
                        pError->Set("The valid skip record had invalid parameters.");
                    }
                    return false;
                }
            } else {
                BlockReference aTargetBlockReference;
                aTargetBlockReference.mArchiveIndex = (int)aBlock.mHeader.mRepairRecord.mArchiveIndex;
                aTargetBlockReference.mBlockIndex = aBlock.mHeader.mRepairRecord.mBlockIndex;
                
                auto aIterator = aMainBlockMap.find(aTargetBlockReference);
                if (aIterator == aMainBlockMap.end()) {
                    if (pError != NULL) {
                        pError->Set("The repair record is not pointing to a valid main block.");
                    }
                    return false;
                }
                
                FakeArchiveBlock aTargetBlock = aMainBlockMap[aTargetBlockReference];
                
                if (aTargetBlock.mHeader.mSkipRecord.mExpectInvalid) {
                    aBlock.mHeader.mSkipRecord.SetInvalid();
                } else {
                    int aRepairSkipTarget = aTargetBlock.mHeader.mSkipRecord.mPayloadDistance;
                    if (!aBlock.mHeader.mSkipRecord.SetValid( aRepairSkipTarget,
                                                                    pJob.mBlocksPerArchive,
                                                                    pJob.mPayloadBytesPerBlock,
                                                             pError)) {
                        if (pError != NULL) {
                            pError->Set("The valid skip record had invalid parameters (repair block).");
                        }
                    }
                }
            }
            
            aBlock.mPayload.Set(aPayloadBlocks[aLoopBlockIndex]);
            aArchive.mBlocks.push_back(aBlock);
            aLoopBlockIndex++;
            
            if (aType == 1) {
                BlockReference aBlockReference;
                aBlockReference.mArchiveIndex = (int)aBlock.mHeader.mArchiveIndex;
                aBlockReference.mBlockIndex = aBlock.mHeader.mBlockIndex;
                aMainBlockMap[aBlockReference] = aBlock;
            }
        }
        
        if (pResult != NULL) {
            pResult->push_back(aArchive);
        }
        
        aBlockIndexOuter += pJob.mBlocksPerArchive;
    }
    
    int aZeroCount = Layout::GetZeroCount(aArchiveCount);
    
    MockHardDrive aHardDrive;
    for (int aArchiveIndex=0;aArchiveIndex<aArchiveCount;aArchiveIndex++) {
        ByteString aZeroString = Layout::GetZeroPadded(aArchiveIndex, aZeroCount);
        ByteString aNameString = pJob.mFilePrefix + aZeroString;
        ByteString aExtensionString = ByteString(string(kArchiveFileSuffixV2));
        ByteString aFileStem = aNameString + aExtensionString;
        (*pResult)[aArchiveIndex].mFilePath = ByteString(aHardDrive.JoinPath(pJob.mArchived.ToString(), aFileStem.ToString()));
    }
    
    return true;
}
