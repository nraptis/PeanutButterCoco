//
//  TestUnbundleWithHooks.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#ifndef TestUnbundleWithHooks_hpp
#define TestUnbundleWithHooks_hpp

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "ArchiveLayoutConfig.hpp"
#include "ByteString.hpp"
#include "DecodeRequest.hpp"
#include "Decode_Workflow.hpp"
#include "JobBundle.hpp"
#include "MockFileSystem.hpp"
#include "SimpleDecodeRuntime.hpp"
#include "ByteMap.hpp"

class TestUnbundleWithHooks {
public:
    struct PhaseBatchFeedback {
        // Lower-case callback label. Archive decode is surfaced as "flight".
        const char                      *mPhase = "idle";
        peanutbutter::ProgressStageV2    mStage = peanutbutter::ProgressStageV2::kIdle;
        std::size_t                      mPhaseIndex = 0u;
        std::size_t                      mPhaseCount = 0u;
        std::size_t                      mBatch = 0u; // 1-based batch count within the current phase.
        bool                             mRunSucceeded = false;
        bool                             mNeedsMoreHeartbeats = false;
        std::uint64_t                    mWorkUnit = 0u;
    };
    
    using BatchCallback = std::function<void(const PhaseBatchFeedback &pFeedback,
                                             peanutbutter::DecodeStageContextV2 &pContext,
                                             SimpleDecodeRuntime &pRuntime)>;
    
    static bool PerformRealUnbundle(JobBundle &pJob,
                            MockFileSystem &pFileSystem,
                            ByteString *pError) {
        return PerformRealUnbundle(pJob, pFileSystem, BatchCallback(), pError);
    }
    
    static bool PerformRealUnbundle(JobBundle &pJob,
                            MockFileSystem &pFileSystem,
                            const BatchCallback &pOnBatch,
                            ByteString *pError) {
        return PerformReal(pJob, pFileSystem, DecodeIntentV2::kUnbundle, pOnBatch, pError);
    }
    
    
    static bool PerformRealRecover(JobBundle &pJob,
                            MockFileSystem &pFileSystem,
                            ByteString *pError) {
        return PerformRealRecover(pJob, pFileSystem, BatchCallback(), pError);
    }
    
    static bool PerformRealRecover(JobBundle &pJob,
                            MockFileSystem &pFileSystem,
                            const BatchCallback &pOnBatch,
                            ByteString *pError) {
        return PerformReal(pJob, pFileSystem, DecodeIntentV2::kRecover, pOnBatch, pError);
    }
    
    
    
    static bool PerformRealRepair(JobBundle &pJob,
                            MockFileSystem &pFileSystem,
                            ByteString *pError) {
        return PerformRealRepair(pJob, pFileSystem, BatchCallback(), pError);
    }
    
    static bool PerformRealRepair(JobBundle &pJob,
                            MockFileSystem &pFileSystem,
                            const BatchCallback &pOnBatch,
                                  ByteString *pError) {
        return PerformReal(pJob, pFileSystem, DecodeIntentV2::kRepair, pOnBatch, pError);
    }
    
    static bool CollectFiles(JobBundle &pJob,
                             vector<FakeFile> &pFileList,
                             MockFileSystem &pFileSystem,
                             ByteString *pError) {
        
        vector<string> aUnbundledFiles = pFileSystem.mDrive->ListFilesRecursive(pJob.mUnarchived.ToString());
        
        ByteMap aMap;
        
        string aFileRoot = pJob.mUnarchived.ToString();
        
        for (int aUnbundledFileIndex=0; aUnbundledFileIndex<((int)aUnbundledFiles.size()); aUnbundledFileIndex++) {
            string aFileName = aUnbundledFiles[aUnbundledFileIndex];
            
            ByteString aName = ByteString(aFileName);
            
            if (aMap.Exists(aName)) {
                if (pError != NULL) {
                    ByteString aErrorString = ByteString("Collect files loaded same name twice, ") + aName;
                    pError->Set(aErrorString);
                }
                return false;
            }
            aMap.Add(aName);
            
            ByteString aContent = pFileSystem.Load(aFileName);
            
            if (pFileSystem.Exists(aFileName) == false) {
                if (pError != NULL) {
                    ByteString aErrorString = ByteString("Collect files found non-existing file, ") + aName;
                    pError->Set(aErrorString);
                }
                return false;
            }
            
            FakeFile aFile;
            aFile.mName.Set(pFileSystem.RelativeToRoot(aFileRoot, aName.ToString()));
            aFile.mContent.Set(aContent);
            aFile.mIsFolder = false;
            pFileList.push_back(aFile);
        }
        
        vector<string> aUnbundledFolders = pFileSystem.mDrive->ListDirectoriesRecursive(pJob.mUnarchived.ToString());
        for (int aUnbundledFolderIndex=0; aUnbundledFolderIndex<((int)aUnbundledFolders.size()); aUnbundledFolderIndex++) {
            string aFolderName = aUnbundledFolders[aUnbundledFolderIndex];
            
            ByteString aName = ByteString(aFolderName);
            
            if (aMap.Exists(aName)) {
                if (pError != NULL) {
                    ByteString aErrorString = ByteString("Collect Folders loaded same name twice, ") + aName;
                    pError->Set(aErrorString);
                }
                return false;
            }
            aMap.Add(aName);
            
            FakeFile aFile;
            aFile.mName.Set(pFileSystem.RelativeToRoot(aFileRoot, aName.ToString()));
            aFile.mIsFolder = true;
            pFileList.push_back(aFile);
        }
        
        return true;
    }
    
private:
    static bool PerformReal(JobBundle &pJob,
                            MockFileSystem &pFileSystem,
                            DecodeIntentV2 pIntent,
                            const BatchCallback &pOnBatch,
                            ByteString *pError) {
        
        using namespace peanutbutter;
        using namespace peanutbutter::decode_workflow;
        using namespace peanutbutter::memory_layout;
        
        const std::string aUnbundledd = pJob.mArchived.ToString();
        const std::string aUnarchived = pJob.mUnarchived.ToString();
        
        
        DecodeRequestV2 aRequest;
        aRequest.mSourcePath = aUnbundledd;
        aRequest.mDestinationDirectory = aUnarchived;
        aRequest.mClearDestinationBeforeWrite = true;
        aRequest.mEncryptionEnabled = pJob.mEncryptionEnabled;
        aRequest.mPassword = "password";
        aRequest.mIntent = pIntent;
        
        ArchiveLayoutConfigV2 aLayout;
        aLayout.mArchiveBlockBytes = pJob.mPayloadBytesPerBlock + Layout::SectionHeaderSize();
        aLayout.mMaxPathLength = pJob.mMaxPathLength;
        aLayout.mMaxArchiveCount = kDefaultMaxArchiveCountV2;
        aLayout.mMaxBlocksPerArchive = pJob.mBlocksPerArchive;
        
        SimpleDecodeRuntime aRuntime;
        DecodeStageContextV2 aContext(aRequest, &aRuntime, &pFileSystem, &aLayout);
        const DecodePhaseListViewV2 aPhases = BuildDecodePhaseListV2(DecodePhasePlanV2::kDecode);
        std::vector<std::size_t> aPhaseBatchCounts(aPhases.mCount, 0u);
        
        std::size_t aCurrentPhaseIndex = 0u;
        while (aCurrentPhaseIndex < aPhases.mCount) {
            const DecodePhaseEntryV2 &aPhase = aPhases.mEntries[aCurrentPhaseIndex];
            
            if (aContext.IsCancelRequested()) {
                if (!ShouldDeferDecodeCancelForPhaseV2(aContext.State(),
                                                       aPhase.mStage,
                                                       ProgressStageV2::kArchiveDecode)) {
                    if (pError != nullptr) {
                        pError->Set("Unbundle job was cancelled.");
                    }
                    return false;
                }
            }
            
            std::size_t aSliceBudget = 1u;
            if (aPhase.mStage == ProgressStageV2::kArchiveDecode) {
                aSliceBudget = std::max<std::size_t>(
                    1u, static_cast<std::size_t>(pJob.mBatchSize));
            }
            
            std::size_t aSliceCount = 0u;
            while (true) {
                const bool aSucceeded = RunDecodePhaseV2(
                    aContext,
                    aPhase,
                    aCurrentPhaseIndex,
                    aPhases.mCount,
                    LogActionFromDecodeIntentV2(aRequest.mIntent));
                const bool aNeedsMoreHeartbeats = aContext.ActivePhaseNeedsMoreHeartbeats();
                
                if (aCurrentPhaseIndex < aPhaseBatchCounts.size()) {
                    ++aPhaseBatchCounts[aCurrentPhaseIndex];
                }
                
                if (pOnBatch) {
                    PhaseBatchFeedback aFeedback;
                    aFeedback.mPhase = PhaseNameForHook(aPhase.mStage);
                    aFeedback.mStage = aPhase.mStage;
                    aFeedback.mPhaseIndex = aCurrentPhaseIndex;
                    aFeedback.mPhaseCount = aPhases.mCount;
                    aFeedback.mBatch = aPhaseBatchCounts[aCurrentPhaseIndex];
                    aFeedback.mRunSucceeded = aSucceeded;
                    aFeedback.mNeedsMoreHeartbeats = aNeedsMoreHeartbeats;
                    aFeedback.mWorkUnit = aContext.State().mWorkUnitsProcessed;
                    pOnBatch(aFeedback, aContext, aRuntime);
                }
                
                if (!aSucceeded) {
                    if (aContext.IsCancelRequested()) {
                        if (!ShouldDeferDecodeCancelForPhaseV2(aContext.State(),
                                                               aPhase.mStage,
                                                               ProgressStageV2::kArchiveDecode)) {
                            if (pError != nullptr) {
                                pError->Set("Unbundle job was cancelled.");
                            }
                            return false;
                        }
                    }
                    
                    if (pError != nullptr) {
                        pError->Set("Unbundle job has failed.");
                    }
                    return false;
                }
                
                ++aSliceCount;
                if (!aNeedsMoreHeartbeats) {
                    break;
                }
                if (aContext.ActivePhaseBatchYieldRequested()) {
                    break;
                }
                if (aSliceCount >= aSliceBudget) {
                    break;
                }
                if (aContext.IsCancelRequested() &&
                    !ShouldDeferDecodeCancelForPhaseV2(aContext.State(),
                                                       aPhase.mStage,
                                                       ProgressStageV2::kArchiveDecode)) {
                    if (pError != nullptr) {
                        pError->Set("Unbundle job was cancelled.");
                    }
                    return false;
                }
            }
            
            if (aContext.ActivePhaseNeedsMoreHeartbeats()) {
                continue;
            }
            
            if (aContext.IsCancelRequested()) {
                if (aContext.State().mCancel.mShouldFinalizeAfterCancel &&
                    aPhase.mStage != ProgressStageV2::kFinalize) {
                    aCurrentPhaseIndex = FindDecodePhaseIndexV2(aPhases, ProgressStageV2::kFinalize);
                    continue;
                }
                
                if (pError != nullptr) {
                    pError->Set("Unbundle job was cancelled.");
                }
                return false;
            }
            
            ++aCurrentPhaseIndex;
        }
        
        return true;
    }
    
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
                return "archive_packing";
            case ProgressStageV2::kArchiveDecode:
                return "flight";
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

#endif /* TestUnbundleWithHooks_hpp */
