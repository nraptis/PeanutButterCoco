//
//  TestBundle.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "TestBundle.hpp"
#include "BundleRequest.hpp"
#include "Bundle_Workflow.hpp"
#include "MockHardDrive.hpp"
#include "MockFileSystem.hpp"
#include "namespaces.hpp"

bool TestBundle::PerformReal(JobBundle &pJob, MockFileSystem &pFileSystem, ByteString *pError) {
    
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
    
    pJob.SortFiles();
    
    string aSourceFolder = pJob.mSource.ToString();
    string aDestinationFolder = pJob.mDestination.ToString();
    
    for (int aFileIndex=0; aFileIndex<((int)pJob.mFileList.size()); aFileIndex++) {
        FakeFile aFile = pJob.mFileList[aFileIndex];
        string aPath = pFileSystem.JoinPath(aSourceFolder, aFile.mName.ToString());
        if (aFile.mIsFolder) {
            pFileSystem.EnsureDirectory(aPath);
        } else {
            pFileSystem.WriteFile(aPath, aFile.mContent.mData, aFile.mContent.mLength);
        }
    }
    
    BundleRequestV2 aRequest;
    aRequest.mSourceDirectory = aSourceFolder;
    aRequest.mDestinationDirectory = aDestinationFolder;
    aRequest.mClearDestinationBeforeWrite = pJob.mClearDestination;
    aRequest.mEncryptionEnabled = pJob.mEncryptionEnabled;
    aRequest.mEncryptionStrength = StrengthPresetV2::kHigh;
    aRequest.mTableStrength = StrengthPresetV2::kHigh;
    
    if (pJob.mRepairCoverage == 0) {
        aRequest.mRepairEnabled = false;
    } else {
        aRequest.mRepairEnabled = true;
        if (pJob.mRepairCoverage == (unsigned char)(RepairCoveragePresetV2::k80)) {
            aRequest.mRepairCoverage = RepairCoveragePresetV2::k80;
        } else if (pJob.mRepairCoverage == (unsigned char)(RepairCoveragePresetV2::k60)) {
            aRequest.mRepairCoverage = RepairCoveragePresetV2::k60;
        } else if (pJob.mRepairCoverage == (unsigned char)(RepairCoveragePresetV2::k40)) {
            aRequest.mRepairCoverage = RepairCoveragePresetV2::k40;
        } else {
            aRequest.mRepairCoverage = RepairCoveragePresetV2::k20;
        }
    }
    
    aRequest.mIncludePreviewManifest = pJob.mPreviewEnabled;
    aRequest.mBlockCount = pJob.mBlocksPerArchive;
    aRequest.mFilePrefix = pJob.mFilePrefix.ToString();
    aRequest.mPassword = "password";
    
    ArchiveLayoutConfigV2 aLayout;
    aLayout.mArchiveBlockBytes = pJob.mPayloadBytesPerBlock + Layout::SectionHeaderSize();
    aLayout.mMaxPathLength = pJob.mMaxPathLength;
    aLayout.mMaxArchiveCount = kDefaultMaxArchiveCountV2;
    aLayout.mMaxBlocksPerArchive = pJob.mBlocksPerArchive;
    
    SimpleBundleRuntime aRuntime;
    BundleStageContextV2 aContext(aRequest, &aRuntime, &pFileSystem, &aLayout);
    
    const std::vector<BundlePhaseEntryV2> aPhases = BuildBundlePhaseListV2(aRequest);
    
    std::size_t aCurrentPhaseIndex = 0u;
    while (aCurrentPhaseIndex < aPhases.size()) {
        const bundle_workflow::BundlePhaseEntryV2 &aPhase = aPhases[aCurrentPhaseIndex];
        
        if (aContext.IsCancelRequested()) {
            if (ShouldDeferBundleCancelForPhaseV2(aContext.State(), aPhase.mStage) == false) {
                // Cancelled...
                if (pError) {
                    
                    pError->Set("Bundle job was cancelled.");
                }
                return false;
            }
        }
        
        if (RunBundlePhaseV2(aContext, aPhase, aCurrentPhaseIndex, aPhases.size()) == false) {
            if (aContext.IsCancelRequested()) {
                if (ShouldDeferBundleCancelForPhaseV2(aContext.State(), aPhase.mStage) == false) {
                    if (pError) {
                        pError->Set("Bundle job was cancelled.");
                    }
                    return false;
                }
            }
            
            // Failed
            if (pError) {
                pError->Set("Bundle job has failed.");
            }
            return false;
        }
        
        if (aContext.ActivePhaseNeedsMoreHeartbeats()) {
            continue;
        }
        
        if (aContext.IsCancelRequested()) {
            if (aContext.State().mCancel.mShouldFinalizeAfterCancel) {
                if (aPhase.mStage != ProgressStageV2::kFinalizingHeaders) {
                    aCurrentPhaseIndex = bundle_workflow::FindBundlePhaseIndexV2(aPhases, ProgressStageV2::kFinalizingHeaders);
                    continue;
                }
            }
            
            // Cancelled
            if (pError) {
                pError->Set("Bundle job was cancelled.");
            }
            return false;
        }
        
        ++aCurrentPhaseIndex;
    }
    return true;
}

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
    
    string aSourceFolder = pJob.mSource.ToString();
    string aDestinationFolder = pJob.mDestination.ToString();
    
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
    
    
    // Now all the preview blocks point to first data block.
    
    if (aPreviewBlockCount > 0) {
        for (int aIndex=0;aIndex<aPreviewBlockCount;aIndex++) {
            
            // Option 1: All preview blocks point to real first data block:
            //aSkipTargets.push_back(aPayloadByteIndex);
            
            // Option 2: All preview blocks point to invalid blocks.
            aSkipTargets.push_back(-1);
        }
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
        
        aBlockOffset += aAmount;
        
        if (aBlockOffset >= aBlockLength) {
            if (aPayloadOffset == 0) {
                aSkipTargets.push_back(aPayloadByteIndex);
            } else if (aPayloadIndex >= (aMainPayloadCount - 1)) {
                aSkipTargets.push_back(-1);
            } else {
                aSkipTargets.push_back(aPayloadByteIndex + aPayloadLength);
            }
            
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
    
    // The count without repair blocks.
    int aPartialBlockCount = (int)(aPreviewPayloadBlocks.size() + aMainPayloadBlocks.size());
    
    int aRepairBlockCount = 0;
    vector<ByteString> aRepairPayloadBlocks;
    
    int aTargetRepairBlockCount = 0;
    aTargetRepairBlockCount = (aMainBlockCount * ((int)pJob.mRepairCoverage) + 99) / 100;
    
    vector<FakeRepairRecord> aRepairRecords;
    while (((int)aRepairRecords.size()) < aPartialBlockCount) {
        FakeRepairRecord aRepairRecord;
        aRepairRecord.SetInvalid();
        aRepairRecords.push_back(aRepairRecord);
    }
    
    if (aTargetRepairBlockCount > 0) {
        
        vector<FakeRepairRecord> aNewRepairRecords;
        
        
        int aFirstMainBlockIndex = (int)aPreviewPayloadBlocks.size();
        int aPartialArchiveCount = (aPartialBlockCount + (pJob.mBlocksPerArchive - 1)) / pJob.mBlocksPerArchive;
        vector<vector<FakeRepairRecord>> aRepairSearchStack;
        vector<int> aRepairSearchIndex;
        
        for (int aArchiveIndex=0;aArchiveIndex<aPartialArchiveCount;aArchiveIndex++) {
            vector<FakeRepairRecord> aList;
            aRepairSearchStack.push_back(aList);
            aRepairSearchIndex.push_back(0);
        }
        int aBlockIndex = 0;
        for (int aArchiveIndex=0;aArchiveIndex<aPartialArchiveCount;aArchiveIndex++) {
            int aBlockCount = (aPartialBlockCount - aBlockIndex);
            if (aBlockCount > pJob.mBlocksPerArchive) {
                aBlockCount = pJob.mBlocksPerArchive;
            }
            int aBlockCeiling = aBlockIndex + aBlockCount;
            int aLoopBlockIndex = aBlockIndex;
            while (aLoopBlockIndex < aBlockCeiling) {
                if (aLoopBlockIndex >= aFirstMainBlockIndex) {
                    int aRepairArchiveIndex = aLoopBlockIndex / pJob.mBlocksPerArchive;
                    int aRepairBlockIndex = (aLoopBlockIndex - aRepairArchiveIndex * pJob.mBlocksPerArchive);
                    FakeRepairRecord aRepairRecord;
                    aRepairRecord.SetValid(aRepairArchiveIndex, aRepairBlockIndex);
                    aRepairRecord.mMainBlockIndex = (aLoopBlockIndex - aFirstMainBlockIndex);
                    aRepairSearchStack[aArchiveIndex].push_back(aRepairRecord);
                }
                aLoopBlockIndex++;
            }
            aBlockIndex += pJob.mBlocksPerArchive;
        }
        
        while (true) {
            bool aFoundOne = false;
            for (int aIndex=0; aIndex<((int)aRepairSearchStack.size()); aIndex++) {
                if (aRepairSearchIndex[aIndex] < aRepairSearchStack[aIndex].size()) {
                    aNewRepairRecords.push_back(aRepairSearchStack[aIndex][aRepairSearchIndex[aIndex]]);
                    aRepairSearchIndex[aIndex] = aRepairSearchIndex[aIndex] + 1;
                    aFoundOne = true;
                }
                if (((int)aNewRepairRecords.size()) == aTargetRepairBlockCount) {
                    break;
                }
            }
            if (aFoundOne == false) {
                break;
            }
            if (((int)aNewRepairRecords.size()) == aTargetRepairBlockCount) {
                break;
            }
        }
        
        //aRepairRecords.push_back(aRepairSearchStack[aIndex][aRepairSearchIndex[aIndex]]);
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
    while (aSkipTargets.size() < aTotalBlockCount) {
        aSkipTargets.push_back(-1);
    }
    
    vector<ByteString> aPayloadBlocks;
    for (auto aBlock: aPreviewPayloadBlocks) { aPayloadBlocks.push_back(aBlock); }
    for (auto aBlock: aMainPayloadBlocks) { aPayloadBlocks.push_back(aBlock); }
    for (auto aBlock: aRepairPayloadBlocks) { aPayloadBlocks.push_back(aBlock); }
    
    int aFirstMainBlockIndex = (int)aPreviewPayloadBlocks.size();
    int aLastMainBlockIndexCeiling = aFirstMainBlockIndex + (aMainBlockCount);
    
    int aArchiveCount = (aTotalBlockCount + (pJob.mBlocksPerArchive - 1)) / pJob.mBlocksPerArchive;
        
    int aGLobalBlockIndex = 0;
    
    int aArchiveUUID =  100000;
    int aBlockUUID =    500000;
    
    for (int aArchiveIndex=0;aArchiveIndex<aArchiveCount;aArchiveIndex++) {
        
        int aBlockCount = (aTotalBlockCount - aGLobalBlockIndex);
        if (aBlockCount > pJob.mBlocksPerArchive) {
            aBlockCount = pJob.mBlocksPerArchive;
        }
        int aBlockCeiling = aGLobalBlockIndex + aBlockCount;
        
        FakeArchive aArchive;
        aArchive.mHeader.mArchiveIndex = aArchiveIndex;
        aArchive.mHeader.mArchiveCount = aArchiveCount;
        aArchive.mHeader.mBlockCountPreview = aPreviewBlockCount;
        aArchive.mHeader.mBlockCountMain = aMainBlockCount;
        aArchive.mHeader.mBlockCountRepair = aRepairBlockCount;
        aArchive.mArchiveUUID = aArchiveUUID;
        aArchiveUUID += 1;
        
        int aLoopBlockIndex = aGLobalBlockIndex;
        while (aLoopBlockIndex < aBlockCeiling) {
            
            FakeArchiveBlock aBlock;
            
            aBlock.mHeader.mArchiveIndex = aArchiveIndex;
            aBlock.mHeader.mArchiveCount = aArchiveCount;
            aBlock.mHeader.mBlockCountPreview = aPreviewBlockCount;
            aBlock.mHeader.mBlockCountMain = aMainBlockCount;
            aBlock.mHeader.mBlockCountRepair = aRepairBlockCount;
            aBlock.mBlockUUID = aBlockUUID;
            aBlockUUID += 1;
            
            if (aLoopBlockIndex < aFirstMainBlockIndex) {
                aBlock.mHeader.mSectionType = (unsigned char)(SectionTypeV2::kPreviewManifest);
            } else if (aLoopBlockIndex < aLastMainBlockIndexCeiling) {
                aBlock.mHeader.mSectionType = (unsigned char)(SectionTypeV2::kArchiveData);
            } else {
                aBlock.mHeader.mSectionType = (unsigned char)(SectionTypeV2::kRepairData);
            }
            
            //aBlock.mHeader.mSectionType
            
            if (aLoopBlockIndex < aRepairRecords.size()) {
                FakeRepairRecord aRecord = aRepairRecords[aLoopBlockIndex];
                if (aRecord.mExpectInvalid) {
                    aBlock.mHeader.mRepairRecord.SetInvalid();
                } else {
                    aBlock.mHeader.mRepairRecord.SetValid(aRecord.mArchiveIndex, aRecord.mBlockIndex);
                }
            } else {
                if (pError != NULL) {
                    pError->Set("There is the wrong number of repair records.");
                }
                return false;
            }
            
            if (aLoopBlockIndex == aFirstMainBlockIndex) {
                if (!aBlock.mHeader.mSkipRecord.SetInvalid()) {
                    return false;
                }
            } else if (aSkipTargets[aLoopBlockIndex] == -1) {
                if (!aBlock.mHeader.mSkipRecord.SetInvalid()) {
                    return false;
                }
            } else if (!aBlock.mHeader.mSkipRecord.SetValid( aSkipTargets[aLoopBlockIndex],
                                                            pJob.mBlocksPerArchive,
                                                            pJob.mPayloadBytesPerBlock,
                                                            pError)) {
                return false;
            }
            
            aBlock.mPayload.Set(aPayloadBlocks[aLoopBlockIndex]);
            aArchive.mBlocks.push_back(aBlock);
            aLoopBlockIndex++;
        }
        
        if (pResult != NULL) {
            pResult->push_back(aArchive);
        }
        
        aGLobalBlockIndex += pJob.mBlocksPerArchive;
    }
    
    return true;
}

bool TestBundle::GetBlockSpans(JobBundle &pJob, vector<FakeFileBlockSpan> *pBlockSpans, vector<FakeArchive> *pArchiveList, ByteString *pError) {

    if (pBlockSpans == NULL) {
        if (pError != NULL) {
            pError->Set("Job contains null block span vector.");
        }
        return false;
    }
    
    if (pArchiveList == NULL) {
        if (pError != NULL) {
            pError->Set("Job contains null archive list.");
        }
        return false;
    }
    
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
    
    if (pArchiveList->size() <= 0) {
        return true;
    }
    
    if ((*pArchiveList)[0].mBlocks.size() <= 0) {
        return true;
    }
    
    pJob.SortFiles();
    
    vector<FakeArchive> aFlatArchiveList;
    vector<FakeArchiveBlock> aFlatBlockList;
    
    for (int aArchiveIndex=0;aArchiveIndex<pArchiveList->size();aArchiveIndex++) {
        FakeArchive aArchive = ((*pArchiveList)[aArchiveIndex]);
        if (aArchiveIndex == (pArchiveList->size() - 1)) {
            if (aArchive.mBlocks.size() > pJob.mBlocksPerArchive) {
                if (pError != NULL) {
                    pError->Set("Last archive's block count is too high.");
                }
                return false;
            }
        } else {
            if (aArchive.mBlocks.size() != pJob.mBlocksPerArchive) {
                if (pError != NULL) {
                    pError->Set("Non-last archive's block count is not exactly correct.");
                }
                return false;
            }
        }
        for (int aBlockIndex=0; aBlockIndex<aArchive.mBlocks.size(); aBlockIndex++) {
            aFlatArchiveList.push_back(aArchive);
            FakeArchiveBlock aBlock = aArchive.mBlocks[aBlockIndex];
            aFlatBlockList.push_back(aBlock);
        }
    }
    
    int aBlockLength = pJob.mPayloadBytesPerBlock;
    int aBlockCountPreview = (int)(*pArchiveList)[0].mBlocks[0].mHeader.mBlockCountPreview;
    int aBlockCountMain = (int)(*pArchiveList)[0].mBlocks[0].mHeader.mBlockCountMain;
    int aBlockCountRepair = (int)(*pArchiveList)[0].mBlocks[0].mHeader.mBlockCountRepair;
    
    if (((int)aFlatBlockList.size()) != (aBlockCountPreview + aBlockCountMain + aBlockCountRepair)) {
        if (pError != NULL) {
            pError->Set("The flat block lengths are not correct.");
        }
        return false;
    }
    
    vector<ByteString> aMainPayloadList;
    vector<FakeFile> aMainFileList;
    for (int aFileIndex=0; aFileIndex<((int)pJob.mFileList.size()); aFileIndex++) {
        FakeFile aFile = pJob.mFileList[aFileIndex];
        ByteString aPayload;
        if (!aFile.ToPayload(&aPayload, pError)) {
            return false;
        }
        aMainPayloadList.push_back(aPayload);
        aMainFileList.push_back(aFile);
    }
    
    int aMainPayloadCount = (int)aMainPayloadList.size();
    if (aMainPayloadCount <= 0) {
        if (pError != NULL) {
            pError->Set("There were no main payload list items.");
        }
        return false;
    }
    
    int aTotalLength = 0;
    for (int aPayloadIndex = 0; aPayloadIndex < ((int)aMainPayloadList.size()); aPayloadIndex++) {
        aTotalLength += aMainPayloadList[aPayloadIndex].mLength;
    }
    
    int aBlockCount = (aTotalLength + aBlockLength - 1) / aBlockLength;
    
    if (aBlockCount != aBlockCountMain) {
        if (pError != NULL) {
            pError->Set("The archive's main block count does not equal our expected main block count.");
        }
        return false;
    }
    
    int aPayloadIndex = 0;
    int aPayloadOffset = 0;
    int aPayloadCount = (int)aMainPayloadList.size();
    int aBlockIndex = 0;
    int aBlockOffset = 0;
    
    FileSpanMap aMap;
    
    while ((aBlockIndex < aBlockCount) && (aPayloadIndex < aPayloadCount)) {
        
        int aPayloadLength = aMainPayloadList[aPayloadIndex].mLength;
        int aCopyAmountChoiceA = (aBlockLength - aBlockOffset);
        int aCopyAmountChoiceB = (aPayloadLength - aPayloadOffset);
        int aAmount = min(aCopyAmountChoiceA, aCopyAmountChoiceB);
        if (aAmount <= 0) {
            if (pError != NULL) {
                pError->Set("The increment amount was not a positive number, dead locked.");
            }
            return false;
        }
        
        int aFakeBlockIndex = aBlockIndex + aBlockCountPreview;
        if ((aFakeBlockIndex < 0) || (aFakeBlockIndex >= aFlatBlockList.size())) {
            if (pError != NULL) {
                pError->Set("The fake block index is not correct in the flat list.");
            }
            return false;
        }
        
        const FakeFile &aChosenFile = aMainFileList[aPayloadIndex];
        const FakeArchive &aChosenArchive = aFlatArchiveList[aFakeBlockIndex];
        const FakeArchiveBlock &aChosenBlock = aFlatBlockList[aFakeBlockIndex];
        
        FakeFileBlockSpan aSpan;
        if (!aMap.TryGet(aChosenFile.mName, &aSpan)) {
            aSpan.mName.Set(aChosenFile.mName);
        }
        if (aSpan.mArchiveIdentifiers.size() != aSpan.mBlockIdentifiers.size()) {
            if (pError != NULL) {
                pError->Set("GetBlockSpans span vectors became unbalanced.");
            }
            return false;
        }
        bool aAppend = true;
        if (aSpan.mArchiveIdentifiers.size() > 0) {
            const int aLastIndex = (int)aSpan.mArchiveIdentifiers.size() - 1;
            if ((aSpan.mArchiveIdentifiers[aLastIndex] == aChosenArchive.mArchiveUUID) &&
                (aSpan.mBlockIdentifiers[aLastIndex] == aChosenBlock.mBlockUUID)) {
                aAppend = false;
            }
        }
        if (aAppend) {
            aSpan.mArchiveIdentifiers.push_back(aChosenArchive.mArchiveUUID);
            aSpan.mBlockIdentifiers.push_back(aChosenBlock.mBlockUUID);
        }
        aMap.Add(aChosenFile.mName, aSpan);
        
        aBlockOffset += aAmount;
        if (aBlockOffset >= aBlockLength) {
            aBlockOffset = 0;
            ++aBlockIndex;
        }
        
        aPayloadOffset += aAmount;
        if (aPayloadOffset >= aPayloadLength) {
            aPayloadOffset = 0;
            ++aPayloadIndex;
        }
    }
    
    if (aPayloadIndex != aPayloadCount) {
        if (pError != NULL) {
            pError->Set("GetBlockSpans exited before consuming all payload items.");
        }
        return false;
    }
    
    pBlockSpans->clear();
    for (int aFileIndex = 0; aFileIndex < aMainFileList.size(); ++aFileIndex) {
        const FakeFile &aFile = aMainFileList[aFileIndex];
        FakeFileBlockSpan aSpan;
        if (!aMap.Get(aFile.mName, &aSpan, pError)) {
            if (pError != NULL) {
                *pError = ByteString("GetBlockSpans missing file in map: ") + aFile.mName;
            }
            return false;
        }
        pBlockSpans->push_back(aSpan);
    }
    
    return true;
}
