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
        pFileSystem.WriteFile(aPath, aFile.mContent.mData, aFile.mContent.mLength);
        
        printf("%.*s\n", (int)(aPath.size()), (char*)aPath.c_str());
    }
    
    //for (int aFileIndex=0;aFileIndex<pJob.mFileList)
    
    //pFileSystem.ClearDirectory(<#const std::string &pPath#>)
    
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
    
    string aSourceFolder = pJob.mSource.ToString();
    string aDestinationFolder = pJob.mDestination.ToString();
    
    vector<ByteString> aPayloadList;
    for (int aFileIndex=0; aFileIndex<((int)pJob.mFileList.size()); aFileIndex++) {
        FakeFile aFile = pJob.mFileList[aFileIndex];
        ByteString aPayload;
        if (!aFile.ToPayload(&aPayload, pError)) {
            return false;
        }
        aPayloadList.push_back(aPayload);
    }
    
    vector<int> aSkipDistances;
    vector<ByteString> aPayloadChunks;

    int aChunkLength = pJob.mPayloadBytesPerBlock;
    
    // Total stream size
    int aTotalSize = 0;
    for (int aPayloadIndex = 0; aPayloadIndex < ((int)aPayloadList.size()); aPayloadIndex++) {
        aTotalSize += aPayloadList[aPayloadIndex].mLength;
    }

    int aChunkCount = (aTotalSize + aChunkLength - 1) / aChunkLength;
    for (int i = 0; i < aChunkCount; i++) {
        ByteString aChunk;
        aPayloadChunks.push_back(aChunk);
    }
    
    for (int i = 0; i < aChunkCount; i++) {
        aPayloadChunks[i].Size(aChunkLength);
        memset(aPayloadChunks[i].mData, 0, aChunkLength);
        aPayloadChunks[i].mLength = aChunkLength;
    }
    
    aSkipDistances.resize(aChunkCount, -1);
    
    int aPayloadIndex = 0;
    int aPayloadOffset = 0;
    int aPayloadListCount = (int)aPayloadList.size();
    
    int aChunkIndex = 0;
    int aChunkOffset = 0;
    
    int aSkipDistance = 0;
    if (aPayloadListCount <= 1) {
        aSkipDistance = -1;
    }
    
    while ((aChunkIndex < aChunkCount) && (aPayloadIndex < aPayloadListCount)) {
        int aPayloadLength = aPayloadList[aPayloadIndex].mLength;
        int aCopyAmountChoiceA = (aChunkLength - aChunkOffset);
        int aCopyAmountChoiceB = (aPayloadLength - aPayloadOffset);
        int aAmount = min(aCopyAmountChoiceA, aCopyAmountChoiceB);

        memcpy(&(aPayloadChunks[aChunkIndex].mData[aChunkOffset]),
               &(aPayloadList[aPayloadIndex].mData[aPayloadOffset]),
               aAmount);
        
        aChunkOffset += aAmount;
        if (aChunkOffset >= aChunkLength) {
            aChunkOffset = 0;
            aSkipDistances[aChunkIndex] = aSkipDistance;
            ++aChunkIndex;
        }
        
        aPayloadOffset += aAmount;
        if (aPayloadOffset >= aPayloadLength) {
            aPayloadOffset = 0;
            aSkipDistance = 0;
            ++aPayloadIndex;
        } else {
            aSkipDistance = aPayloadLength - aPayloadOffset;
        }
        
        if (aPayloadIndex >= (aPayloadListCount - 1)) {
            if (aPayloadOffset == 0) {
                aSkipDistance = 0;
            } else {
                aSkipDistance = -1;
            }
        }
    }
    
    if (aPayloadListCount <= 0) {
        if (pError != NULL) {
            pError->Set("There were no payload chunks.");
        }
        return false;
    }
    
    int aArchiveCount = (aChunkCount + (pJob.mBlocksPerArchive - 1)) / pJob.mBlocksPerArchive;
    
    int aFirstDataArchiveIndex = 0;
    int aFirstDataBlockIndex = 0;
    
    int aBlockIndex = 0;
    for (int aArchiveIndex=0;aArchiveIndex<aArchiveCount;aArchiveIndex++) {
        int aBlockCount = (aChunkCount - aBlockIndex);
        if (aBlockCount > pJob.mBlocksPerArchive) {
            aBlockCount = pJob.mBlocksPerArchive;
        }
        int aBlockCeiling = aBlockIndex + aBlockCount;
        
        FakeArchive aArchive;
        
        int aLocalBlockIndex = aBlockIndex;
        while (aLocalBlockIndex < aBlockCeiling) {
            
            FakeArchiveBlock aBlock;
            
            if ((aArchiveIndex == 0) && (aLocalBlockIndex == 0)) {
                if (!aBlock.mHeader.mSkipRecord.SetInvalid()) {
                    return false;
                }
            } else if (aSkipDistances[aLocalBlockIndex] == -1) {
                if (!aBlock.mHeader.mSkipRecord.SetInvalid()) {
                    return false;
                }
            } else if (!aBlock.mHeader.mSkipRecord.SetValid(aSkipDistances[aLocalBlockIndex],
                                                pJob.mBlocksPerArchive,
                                                pJob.mPayloadBytesPerBlock,
                                                aFirstDataArchiveIndex,
                                                aFirstDataBlockIndex,
                                                pError)) {
                return false;
            }
            
            aBlock.mPayload.Set(aPayloadChunks[aLocalBlockIndex]);
            aLocalBlockIndex++;
            
            aArchive.mBlocks.push_back(aBlock);
        }
        
        if (pResult != NULL) {
            pResult->push_back(aArchive);
        }
        
        aBlockIndex += pJob.mBlocksPerArchive;
    }
    
    for (int aIndex=0;aIndex<((int)aSkipDistances.size());aIndex++) {
        //printf("skip[%d] = %d\n", aIndex, aSkipDistances[aIndex]);
    }
    
    return true;
}
