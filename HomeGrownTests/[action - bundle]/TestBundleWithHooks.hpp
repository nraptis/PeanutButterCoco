//
//  TestBundleWithHooks.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/5/26.
//

#ifndef TestBundleWithHooks_hpp
#define TestBundleWithHooks_hpp

#include <functional>
#include <string>
#include <vector>

#include "ArchiveLayoutConfig.hpp"
#include "BundleRequest.hpp"
#include "Bundle_Workflow.hpp"
#include "ByteString.hpp"
#include "JobBundle.hpp"
#include "MockFileSystem.hpp"
#include "SimpleBundleRuntime.hpp"

class TestBundleWithHooks {
public:
    struct PhaseBatchFeedback {
        // Lower-case callback label. Archive packing is surfaced as "flight".
        const char                     *mPhase = "idle";
        peanutbutter::ProgressStageV2  mStage = peanutbutter::ProgressStageV2::kIdle;
        std::size_t                     mPhaseIndex = 0u;
        std::size_t                     mPhaseCount = 0u;
        std::size_t                     mBatch = 0u; // 1-based batch count within the current phase.
        bool                            mRunSucceeded = false;
        bool                            mNeedsMoreHeartbeats = false;
        std::uint64_t                   mWorkUnit = 0u;
    };
    
    using BatchCallback = std::function<void(const PhaseBatchFeedback &pFeedback,
                                             peanutbutter::BundleStageContextV2 &pContext,
                                             SimpleBundleRuntime &pRuntime)>;
    
    static bool PerformReal(JobBundle &pJob,
                            MockFileSystem &pFileSystem,
                            ByteString *pError) {
        return PerformReal(pJob, pFileSystem, BatchCallback(), pError);
    }
    
    static bool PerformReal(JobBundle &pJob,
                            MockFileSystem &pFileSystem,
                            const BatchCallback &pOnBatch,
                            ByteString *pError) {
        
        using namespace peanutbutter;
        using namespace peanutbutter::bundle_workflow;
        using namespace peanutbutter::memory_layout;
        
        if (pJob.ContainsDuplicateFiles()) {
            if (pError != nullptr) {
                pError->Set("Job contains duplicate files.");
            }
            return false;
        }
        
        if (pJob.mFileList.size() == 0) {
            if (pError != nullptr) {
                pError->Set("Job contains no files.");
            }
            return false;
        }
        
        pJob.SortFiles();
        
        std::string aSourceFolder = pJob.mInput.ToString();
        std::string aDestinationFolder = pJob.mArchived.ToString();
        
        for (int aFileIndex = 0; aFileIndex < static_cast<int>(pJob.mFileList.size()); aFileIndex++) {
            FakeFile aFile = pJob.mFileList[aFileIndex];
            std::string aPath = pFileSystem.JoinPath(aSourceFolder, aFile.mName.ToString());
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
            if (pJob.mRepairCoverage == static_cast<unsigned char>(RepairCoveragePresetV2::k80)) {
                aRequest.mRepairCoverage = RepairCoveragePresetV2::k80;
            } else if (pJob.mRepairCoverage == static_cast<unsigned char>(RepairCoveragePresetV2::k60)) {
                aRequest.mRepairCoverage = RepairCoveragePresetV2::k60;
            } else if (pJob.mRepairCoverage == static_cast<unsigned char>(RepairCoveragePresetV2::k40)) {
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
        std::vector<std::size_t> aPhaseBatchCounts(aPhases.size(), 0u);
        
        std::size_t aCurrentPhaseIndex = 0u;
        while (aCurrentPhaseIndex < aPhases.size()) {
            const BundlePhaseEntryV2 &aPhase = aPhases[aCurrentPhaseIndex];
            
            if (aContext.IsCancelRequested()) {
                if (ShouldDeferBundleCancelForPhaseV2(aContext.State(), aPhase.mStage) == false) {
                    if (pError != nullptr) {
                        pError->Set("Bundle job was cancelled.");
                    }
                    return false;
                }
            }
            
            const bool aSucceeded = RunBundlePhaseV2(aContext, aPhase, aCurrentPhaseIndex, aPhases.size());
            const bool aNeedsMoreHeartbeats = aContext.ActivePhaseNeedsMoreHeartbeats();
            
            if (aCurrentPhaseIndex < aPhaseBatchCounts.size()) {
                ++aPhaseBatchCounts[aCurrentPhaseIndex];
            }
            
            if (pOnBatch) {
                PhaseBatchFeedback aFeedback;
                aFeedback.mPhase = PhaseNameForHook(aPhase.mStage);
                aFeedback.mStage = aPhase.mStage;
                aFeedback.mPhaseIndex = aCurrentPhaseIndex;
                aFeedback.mPhaseCount = aPhases.size();
                aFeedback.mBatch = aPhaseBatchCounts[aCurrentPhaseIndex];
                aFeedback.mRunSucceeded = aSucceeded;
                aFeedback.mNeedsMoreHeartbeats = aNeedsMoreHeartbeats;
                aFeedback.mWorkUnit = aContext.State().mWorkUnitsProcessed;
                pOnBatch(aFeedback, aContext, aRuntime);
            }
            
            if (aSucceeded == false) {
                if (aContext.IsCancelRequested()) {
                    if (ShouldDeferBundleCancelForPhaseV2(aContext.State(), aPhase.mStage) == false) {
                        if (pError != nullptr) {
                            pError->Set("Bundle job was cancelled.");
                        }
                        return false;
                    }
                }
                
                if (pError != nullptr) {
                    pError->Set("Bundle job has failed.");
                }
                return false;
            }
            
            if (aNeedsMoreHeartbeats) {
                continue;
            }
            
            if (aContext.IsCancelRequested()) {
                if (aContext.State().mCancel.mShouldFinalizeAfterCancel) {
                    if (aPhase.mStage != ProgressStageV2::kFinalizingHeaders) {
                        aCurrentPhaseIndex = FindBundlePhaseIndexV2(aPhases, ProgressStageV2::kFinalizingHeaders);
                        continue;
                    }
                }
                
                if (pError != nullptr) {
                    pError->Set("Bundle job was cancelled.");
                }
                return false;
            }
            
            ++aCurrentPhaseIndex;
        }
        
        return true;
    }
    
private:
    static const char *PhaseNameForHook(peanutbutter::ProgressStageV2 pStage) {
        using peanutbutter::ProgressStageV2;
        
        switch (pStage) {
            case ProgressStageV2::kPreflight:
                return "preflight";
            case ProgressStageV2::kHeaderBootstrap:
                return "header_bootstrap";
            case ProgressStageV2::kDiscovery:
                return "discovery";
            case ProgressStageV2::kInspection:
                return "inspection";
            case ProgressStageV2::kMemoryPlanning:
                return "memory_planning";
            case ProgressStageV2::kDeriveCipherMaterial:
                return "derive_cipher_material";
            case ProgressStageV2::kAssembleCipherStack:
                return "assemble_cipher_stack";
            case ProgressStageV2::kArchiveManifest:
                return "archive_manifest";
            case ProgressStageV2::kFolderPacking:
                return "folder_packing";
            case ProgressStageV2::kManifestDiscovery:
                return "manifest_discovery";
            case ProgressStageV2::kArchivePacking:
                return "flight";
            case ProgressStageV2::kArchiveDecode:
                return "archive_decode";
            case ProgressStageV2::kRepairPacking:
                return "repair_packing";
            case ProgressStageV2::kFinalizingHeaders:
                return "finalizing_headers";
            case ProgressStageV2::kFinalize:
                return "finalize";
            case ProgressStageV2::kCompare:
                return "compare";
            case ProgressStageV2::kRepairApply:
                return "repair_apply";
            case ProgressStageV2::kIdle:
                return "idle";
        }
        
        return "idle";
    }
};

#endif /* TestBundleWithHooks_hpp */
