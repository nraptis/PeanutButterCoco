//
//  MutationTool.cpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/6/26.
//

#include "MutationTool.hpp"
#include <unordered_set>

// Note: Deleting and mangling blocks, from the test suite's perspective, is equivalent.
// This suite makes some index adjustments to replicate 'real file system' behavior, but it is internally consistent with the desired behavior.

bool MutationTool::ApplyMutationsMock(vector<FakeMutation> *pMutations,
                                        vector<FakeArchive> *pArchiveList,
                                      ByteString *pError) {

    vector<FakeMutation> aMutationList;
    for (auto aMutation : *pMutations) {
        aMutationList.push_back(aMutation);
    }
    
    const int aArchiveCount = (int)pArchiveList->size();
    for (int aMutationIndex=0; aMutationIndex<aMutationList.size(); aMutationIndex++) {
        const FakeMutation &aMutation = aMutationList[aMutationIndex];
        
        const auto FailMutation = [&](const ByteString &pMessage) -> bool {
            if (pError != NULL) {
                pError->Set(pMessage);
            }
            return false;
        };
        
        const auto ResolveArchiveIndex = [&](int pArchiveIndex,
                                             const ByteString &pArchiveFileName) -> int {
            if ((pArchiveIndex >= 0) && (pArchiveIndex < aArchiveCount)) {
                return pArchiveIndex;
            }
            if (pArchiveFileName.mLength <= 0) {
                return -1;
            }
            for (int aArchiveIndex=0; aArchiveIndex<aArchiveCount; aArchiveIndex++) {
                if ((*pArchiveList)[aArchiveIndex].mFilePath.Compare(pArchiveFileName) == 0) {
                    return aArchiveIndex;
                }
            }
            return -1;
        };
        
        const int aArchiveIndexA =
        ResolveArchiveIndex(aMutation.mArchiveIndex, aMutation.mPrimaryArchiveFileName);
        const int aBlockIndexA = aMutation.mBlockIndex;
        
        int aArchiveIndexB = -1;
        if ((aMutation.mArchiveIndexB >= 0) || (aMutation.mSecondaryArchiveFileName.mLength > 0)) {
            aArchiveIndexB =
            ResolveArchiveIndex(aMutation.mArchiveIndexB, aMutation.mSecondaryArchiveFileName);
        } else {
            aArchiveIndexB = aArchiveIndexA;
        }
        const int aBlockIndexB = aMutation.mBlockIndexB;
        
        if (aMutation.mMutationKind == FakeMutation::MutationKind::kMangleBlock ||
            aMutation.mMutationKind == FakeMutation::MutationKind::kDeleteBlock) {
            if ((aArchiveIndexA < 0) || (aArchiveIndexA >= aArchiveCount)) {
                return FailMutation(ByteString("Mutation archive index out of range at mutation index ") + ByteString(aMutationIndex));
            }
            FakeArchive &aArchive = (*pArchiveList)[aArchiveIndexA];
            if ((aBlockIndexA < 0) || (aBlockIndexA >= (int)aArchive.mBlocks.size())) {
                return FailMutation(ByteString("Mutation block index out of range at mutation index ") + ByteString(aMutationIndex));
            }
            
            aArchive.mBlocks[aBlockIndexA].mIsInvalid = true;
            
            if (aMutation.mMutationKind == FakeMutation::MutationKind::kDeleteBlock) {
                for (int aCheckMutationIndex=(aMutationIndex + 1); aCheckMutationIndex<aMutationList.size(); aCheckMutationIndex++) {
                    if (aMutationList[aCheckMutationIndex].mArchiveIndex == aMutation.mArchiveIndex) {
                        if ((aMutationList[aCheckMutationIndex].mMutationKind == FakeMutation::MutationKind::kDeleteBlock) ||
                            (aMutationList[aCheckMutationIndex].mMutationKind == FakeMutation::MutationKind::kMangleBlock)) {
                            
                            if (aMutationList[aCheckMutationIndex].mBlockIndex > aMutation.mBlockIndex) {
                                aMutationList[aCheckMutationIndex].mBlockIndex -= 1;
                            }
                        }
                    }
                }
            }
            
            continue;
        }
        
        if (aMutation.mMutationKind == FakeMutation::MutationKind::kSwapBlocks) {
            if ((aArchiveIndexA < 0) || (aArchiveIndexA >= aArchiveCount) ||
                (aArchiveIndexB < 0) || (aArchiveIndexB >= aArchiveCount)) {
                return FailMutation(ByteString("Swap mutation archive index out of range at mutation index ") + ByteString(aMutationIndex));
            }
            
            FakeArchive &aArchiveA = (*pArchiveList)[aArchiveIndexA];
            FakeArchive &aArchiveB = (*pArchiveList)[aArchiveIndexB];
            if ((aBlockIndexA < 0) || (aBlockIndexA >= (int)aArchiveA.mBlocks.size()) ||
                (aBlockIndexB < 0) || (aBlockIndexB >= (int)aArchiveB.mBlocks.size())) {
                return FailMutation(ByteString("Swap mutation block index out of range at mutation index ") + ByteString(aMutationIndex));
            }
            
            FakeArchiveBlock aBlockA = aArchiveA.mBlocks[aBlockIndexA];
            FakeArchiveBlock aBlockB = aArchiveB.mBlocks[aBlockIndexB];
            
            (*pArchiveList)[aArchiveIndexA].mBlocks[aBlockIndexA] = aBlockB;
            (*pArchiveList)[aArchiveIndexB].mBlocks[aBlockIndexB] = aBlockA;
            
            //aArchiveA.mBlocks[aBlockIndexA].mIsInvalid = true;
            //aArchiveB.mBlocks[aBlockIndexB].mIsInvalid = true;
            
            continue;
        }
        
        // Q: Why is this consistent, even though we are not deleting an archive?
        // A: This is a simplified behavior map of how the engine should ultimately work.
        // This makes the test 'repair' flow simple.
        // So, deleting an archive and mangling all the blocks in the archive should have the same real-engine result.
        // Recovering one block of the archive should add the block to the temporary archive, from which the pessimistic walk can
        // ultimately recover the data. This is not inconsistent from a desired behavior perspective.
        if (aMutation.mMutationKind == FakeMutation::MutationKind::kDeleteArchive ||
            aMutation.mMutationKind == FakeMutation::MutationKind::kMangleArchive) {
            if ((aArchiveIndexA < 0) || (aArchiveIndexA >= aArchiveCount)) {
                return FailMutation(ByteString("Mutation archive index out of range at mutation index ") + ByteString(aMutationIndex));
            }
            
            FakeArchive &aArchive = (*pArchiveList)[aArchiveIndexA];
            
            for (int aBlockIndex=0; aBlockIndex<(int)aArchive.mBlocks.size(); aBlockIndex++) {
                aArchive.mBlocks[aBlockIndex].mIsInvalid = true;
            }
            
            /*
            if (aMutation.mMutationKind == FakeMutation::MutationKind::kDeleteArchive) {
                for (int aCheckMutationIndex=(aMutationIndex + 1); aCheckMutationIndex<aMutationList.size(); aCheckMutationIndex++) {
                    if (aMutationList[aCheckMutationIndex].mArchiveIndex == aMutation.mArchiveIndex) {
                        if ((aMutationList[aCheckMutationIndex].mMutationKind == FakeMutation::MutationKind::kDeleteArchive) ||
                            (aMutationList[aCheckMutationIndex].mMutationKind == FakeMutation::MutationKind::kMangleArchive)) {
                            if (aMutationList[aCheckMutationIndex].mArchiveIndex > aMutation.mArchiveIndex) {
                                aMutationList[aCheckMutationIndex].mArchiveIndex -= 1;
                            }
                        }
                    }
                }
            }
            */
            
            continue;
        }
        
        if (aMutation.mMutationKind != FakeMutation::MutationKind::kNone) {
            return FailMutation(ByteString("Unknown mutation kind at mutation index ") + ByteString(aMutationIndex));
        }
    }
    
    return true;
}

bool MutationTool::ApplyMutationsReal(JobBundle pJob,
                                      vector<FakeMutation> *pMutations,
                                        MockHardDrive *pDrive,
                                        ByteString *pError) {
    
    vector<FakeMutation> aMutationList;
    for (auto aMutation : *pMutations) {
        aMutationList.push_back(aMutation);
    }
    
    if (pMutations == NULL) {
        if (pError != NULL) {
            pError->Set("ApplyMutationsReal received null mutation list.");
        }
        return false;
    }
    if (pDrive == NULL) {
        if (pError != NULL) {
            pError->Set("ApplyMutationsReal received null drive.");
        }
        return false;
    }
    
    vector<string> aArchiveFiles = pDrive->ListFilesRecursive(pJob.mArchived.ToString());

    
    for (int aMutationIndex=0; aMutationIndex<(int)aMutationList.size(); aMutationIndex++) {
        const FakeMutation &aMutation = aMutationList[aMutationIndex];
        
        string aPrimaryPath = aMutation.mPrimaryArchiveFileName.ToString();
        string aSecondaryPath = aMutation.mSecondaryArchiveFileName.ToString();
        
        if (aMutation.mMutationKind == FakeMutation::MutationKind::kMangleBlock) {
            if (aPrimaryPath.empty()) {
                if (pError != NULL) {
                    pError->Set(ByteString("ApplyMutationsReal mangle block missing archive path at mutation index ") +
                                ByteString(aMutationIndex));
                }
                return false;
            }
            if (!pDrive->MangleBlock(aPrimaryPath, aMutation.mBlockIndex, pJob, pError)) {
                return false;
            }
            continue;
        }
        
        if (aMutation.mMutationKind == FakeMutation::MutationKind::kDeleteBlock) {
            if (aPrimaryPath.empty()) {
                if (pError != NULL) {
                    pError->Set(ByteString("ApplyMutationsReal delete block missing archive path at mutation index ") +
                                ByteString(aMutationIndex));
                }
                return false;
            }
            if (!pDrive->MangleBlock(aPrimaryPath, aMutation.mBlockIndex, pJob, pError)) {
                return false;
            }
            
            for (int aCheckMutationIndex=(aMutationIndex + 1); aCheckMutationIndex<aMutationList.size(); aCheckMutationIndex++) {
                if (aMutationList[aCheckMutationIndex].mArchiveIndex == aMutation.mArchiveIndex) {
                    if ((aMutationList[aCheckMutationIndex].mMutationKind == FakeMutation::MutationKind::kDeleteBlock) ||
                        (aMutationList[aCheckMutationIndex].mMutationKind == FakeMutation::MutationKind::kMangleBlock)) {
                        if (aMutationList[aCheckMutationIndex].mBlockIndex > aMutation.mBlockIndex) {
                            aMutationList[aCheckMutationIndex].mBlockIndex -= 1;
                        }
                    }
                }
            }
            continue;
        }
        
        if (aMutation.mMutationKind == FakeMutation::MutationKind::kSwapBlocks) {
            if (aPrimaryPath.empty() || aSecondaryPath.empty()) {
                if (pError != NULL) {
                    pError->Set(ByteString("ApplyMutationsReal swap blocks missing archive path at mutation index ") +
                                ByteString(aMutationIndex));
                }
                return false;
            }
            if (!pDrive->SwapBlocksByPath(aPrimaryPath,
                                          aMutation.mBlockIndex,
                                          aSecondaryPath,
                                          aMutation.mBlockIndexB,
                                          pJob,
                                          pError)) {
                return false;
            }
            continue;
        }
        
        if (aMutation.mMutationKind == FakeMutation::MutationKind::kDeleteArchive) {
            if (aPrimaryPath.empty()) {
                if (pError != NULL) {
                    pError->Set(ByteString("ApplyMutationsReal delete archive missing archive path at mutation index ") +
                                ByteString(aMutationIndex));
                }
                return false;
            }
            if (!pDrive->DeleteFile(aPrimaryPath)) {
                if (pError != NULL) {
                    pError->Set(ByteString("ApplyMutationsReal delete archive failed at mutation index ") +
                                ByteString(aMutationIndex));
                }
                return false;
            }
            continue;
        }
        
        if (aMutation.mMutationKind == FakeMutation::MutationKind::kMangleArchive) {
            if (aPrimaryPath.empty()) {
                if (pError != NULL) {
                    pError->Set(ByteString("ApplyMutationsReal mangle archive missing archive path at mutation index ") +
                                ByteString(aMutationIndex));
                }
                return false;
            }
            if (!pDrive->MangleFile(aPrimaryPath, pError)) {
                return false;
            }
            continue;
        }
        
        if (aMutation.mMutationKind != FakeMutation::MutationKind::kNone) {
            if (pError != NULL) {
                pError->Set(ByteString("ApplyMutationsReal encountered unknown mutation kind at mutation index ") +
                            ByteString(aMutationIndex));
            }
            return false;
        }
    }
    
    return true;
}
