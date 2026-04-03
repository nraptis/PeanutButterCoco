//
//  TestBundle.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "TestBundle.hpp"
#include "BundleRequest.hpp"
#include "Bundle_Workflow.hpp"
#include "namespaces.hpp"

void TestBundle::PerformReal(JobBundle &pJob, MockFileSystem &pFileSystem) {
    
    
    string aCWD = pFileSystem.CurrentWorkingDirectory();
    
    //for (int aFileIndex=0;aFileIndex<pJob.mFileList)
    
    //pFileSystem.ClearDirectory(<#const std::string &pPath#>)
    
    BundleRequestV2 aRequest;
    aRequest.mSourceDirectory = pJob.mSource.ToString();
    aRequest.mDestinationDirectory = pJob.mDestination.ToString();
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
    aRequest.mBlockCount = pJob.mBlockCount;
    aRequest.mFilePrefix = pJob.mFilePrefix.ToString();
    aRequest.mPassword = "password";
    
    ArchiveLayoutConfigV2 aLayout;
    aLayout.mArchiveBlockBytes = pJob.mBlockCount;
    aLayout.mMaxPathLength = pJob.mMaxPathLength;
    aLayout.mMaxArchiveCount = kDefaultMaxArchiveCountV2;
    aLayout.mMaxBlocksPerArchive = pJob.mBlockCount;
    
    SimpleBundleRuntime aRuntime;
    BundleStageContextV2 aContext(aRequest, &aRuntime, &pFileSystem, &aLayout);
    
    const std::vector<BundlePhaseEntryV2> aPhases = BuildBundlePhaseListV2(aRequest);
    
    std::size_t aCurrentPhaseIndex = 0u;
    while (aCurrentPhaseIndex < aPhases.size()) {
        const bundle_workflow::BundlePhaseEntryV2 &aPhase = aPhases[aCurrentPhaseIndex];
        
        if (aContext.IsCancelRequested()) {
            if (ShouldDeferBundleCancelForPhaseV2(aContext.State(), aPhase.mStage) == false) {
                // Cancelled...
                return;
            }
        }
        
        if (RunBundlePhaseV2(aContext, aPhase, aCurrentPhaseIndex, aPhases.size()) == false) {
            if (aContext.IsCancelRequested()) {
                if (ShouldDeferBundleCancelForPhaseV2(aContext.State(), aPhase.mStage) == false) {
                    // Cancelled...
                    return;
                }
            }
            
            // Failed
            return;
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
            return;
        }
        
        ++aCurrentPhaseIndex;
    }
}

void TestBundle::PerformMock(JobBundle &pJob) {
    
}
