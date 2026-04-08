//
//  FakeMutation.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#include "FakeMutation.hpp"
#include "Random.hpp"

FakeMutation::FakeMutation() {
    mMutationKind = FakeMutation::MutationKind::kNone;
    mArchiveIndex = -1;
    mBlockIndex = -1;
    mArchiveIndexB = -1;
    mBlockIndexB = -1;
}
 
FakeMutation::~FakeMutation() {
    
}

void FakeMutation::SetDeleteArchive(string pArchiveFileName, int pArchiveIndex) {
    mMutationKind = MutationKind::kDeleteArchive;
    mPrimaryArchiveFileName.Set(pArchiveFileName);
    mSecondaryArchiveFileName.Clear();
    mArchiveIndex = pArchiveIndex;
    mBlockIndex = -1;
    mArchiveIndexB = -1;
    mBlockIndexB = -1;
}

void FakeMutation::SetMangleArchive(string pArchiveFileName, int pArchiveIndex) {
    mMutationKind = MutationKind::kMangleArchive;
    mPrimaryArchiveFileName.Set(pArchiveFileName);
    mSecondaryArchiveFileName.Clear();
    mArchiveIndex = pArchiveIndex;
    mBlockIndex = -1;
    mArchiveIndexB = -1;
    mBlockIndexB = -1;
}

void FakeMutation::SetDeleteBlock(string pArchiveFileName, int pArchiveIndex, int pBlockIndex) {
    mMutationKind = MutationKind::kDeleteBlock;
    mPrimaryArchiveFileName.Set(pArchiveFileName);
    mSecondaryArchiveFileName.Clear();
    mArchiveIndex = pArchiveIndex;
    mBlockIndex = pBlockIndex;
    mArchiveIndexB = -1;
    mBlockIndexB = -1;
}

void FakeMutation::SetMangleBlock(string pArchiveFileName, int pArchiveIndex, int pBlockIndex) {
    mMutationKind = MutationKind::kMangleBlock;
    mPrimaryArchiveFileName.Set(pArchiveFileName);
    mSecondaryArchiveFileName.Clear();
    mArchiveIndex = pArchiveIndex;
    mBlockIndex = pBlockIndex;
    mArchiveIndexB = -1;
    mBlockIndexB = -1;
}

void FakeMutation::SetSwapBlocks(string pArchiveFileNameA, int pArchiveIndexA, int pBlockIndexA, string pArchiveFileNameB, int pArchiveIndexB, int pBlockIndexB) {
    mMutationKind = MutationKind::kSwapBlocks;
    mPrimaryArchiveFileName.Set(pArchiveFileNameA);
    mSecondaryArchiveFileName.Set(pArchiveFileNameB);
    mArchiveIndex = pArchiveIndexA;
    mBlockIndex = pBlockIndexA;
    mArchiveIndexB = pArchiveIndexB;
    mBlockIndexB = pBlockIndexB;
}


void FakeMutation::AttemptGenerateRandomBlockMangles(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult) {
   
    vector<FakeArchive> aWorkingList;
    for (auto aArchive: *pMockArchives) {
        if (aArchive.mBlocks.size() > 0) {
            aArchive.mTemp = (int)aArchive.mBlocks.size();
            aWorkingList.push_back(aArchive);
        }
    }
    
    for (int aMutationIndex=0; aMutationIndex<pTargetCount; aMutationIndex++) {
        if (aWorkingList.size() <= 0) { break; }

        int aArchiveIndex = Random::Get((int)aWorkingList.size());
        int aBlockIndex = Random::Get((int)aWorkingList[aArchiveIndex].mTemp);
        
        FakeMutation aMutation;
        aMutation.SetMangleBlock(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                 (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex,
                                 aBlockIndex);
        pResult->push_back(aMutation);
    }
    
}

void FakeMutation::AttemptGenerateRandomBlockDeletions(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult) {
    
    vector<FakeArchive> aWorkingList;
    for (auto aArchive: *pMockArchives) {
        if (aArchive.mBlocks.size() > 0) {
            aArchive.mTemp = (int)aArchive.mBlocks.size();
            aWorkingList.push_back(aArchive);
        }
    }
    
    for (int aMutationIndex=0; aMutationIndex<pTargetCount; aMutationIndex++) {
        if (aWorkingList.size() <= 0) { break; }
        
        if (aWorkingList.size() == 1) {
            if (aWorkingList[0].mTemp <= 1) {
                // We don't delete the one last block.
                break;
            }
        }
        
        int aArchiveIndex = Random::Get((int)aWorkingList.size());
        int aBlockIndex = Random::Get((int)aWorkingList[aArchiveIndex].mTemp);
        
        FakeMutation aMutation;
        aMutation.SetDeleteBlock(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                 (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex,
                                 aBlockIndex);
        pResult->push_back(aMutation);
        
        aWorkingList[aArchiveIndex].mTemp--;
        if (aWorkingList[aArchiveIndex].mTemp <= 0) {
            aWorkingList.erase(aWorkingList.begin() + aArchiveIndex);
        }
    }
    
}


void FakeMutation::AttemptGenerateRandomBlockDestruction(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult) {
    vector<FakeArchive> aWorkingList;
    for (auto aArchive: *pMockArchives) {
        if (aArchive.mBlocks.size() > 0) {
            aArchive.mTemp = (int)aArchive.mBlocks.size();
            aWorkingList.push_back(aArchive);
        }
    }
    
    for (int aMutationIndex=0; aMutationIndex<pTargetCount; aMutationIndex++) {
        if (aWorkingList.size() <= 0) { break; }
        
        if (aWorkingList.size() == 1) {
            if (aWorkingList[0].mTemp <= 1) {
                // We don't delete the one last block.
                break;
            }
        }
        
        int aArchiveIndex = Random::Get((int)aWorkingList.size());
        int aBlockIndex = Random::Get((int)aWorkingList[aArchiveIndex].mTemp);
        
        FakeMutation aMutation;
        int aWhich = Random::Get(2);
        if (aWhich == 0) {
            // Delete
            
            aMutation.SetDeleteBlock(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                     (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex,
                                     aBlockIndex);
            aWorkingList[aArchiveIndex].mTemp--;
            if (aWorkingList[aArchiveIndex].mTemp <= 0) {
                aWorkingList.erase(aWorkingList.begin() + aArchiveIndex);
            }
        } else {
            // Mangle
            aMutation.SetMangleBlock(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                     (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex,
                                     aBlockIndex);
        }
        pResult->push_back(aMutation);
        
    }
    
}

void FakeMutation::AttemptGenerateRandomArchiveMangles(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult) {
    vector<FakeArchive> aWorkingList;
    for (auto aArchive: *pMockArchives) {
        if (aArchive.mBlocks.size() > 0) {
            aArchive.mTemp = (int)aArchive.mBlocks.size();
            aWorkingList.push_back(aArchive);
        }
    }
    
    for (int aMutationIndex=0; aMutationIndex<pTargetCount; aMutationIndex++) {
        if (aWorkingList.size() <= 0) { break; }
        int aArchiveIndex = Random::Get((int)aWorkingList.size());
        
        FakeMutation aMutation;
        aMutation.SetMangleArchive(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                   (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex);
        pResult->push_back(aMutation);
    }
    
}


void FakeMutation::AttemptGenerateRandomArchiveDeletions(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult) {
    
    vector<FakeArchive> aWorkingList;
    for (auto aArchive: *pMockArchives) {
        if (aArchive.mBlocks.size() > 0) {
            aArchive.mTemp = (int)aArchive.mBlocks.size();
            aWorkingList.push_back(aArchive);
        }
    }
    
    for (int aMutationIndex=0; aMutationIndex<pTargetCount; aMutationIndex++) {
        if (aWorkingList.size() <= 1) { break; }

        int aArchiveIndex = Random::Get((int)aWorkingList.size());
        
        FakeMutation aMutation;
        aMutation.SetDeleteArchive(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                   (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex);
        aWorkingList.erase(aWorkingList.begin() + aArchiveIndex);
        
        pResult->push_back(aMutation);
    }
    
}

void FakeMutation::AttemptGenerateRandomArchiveDestruction(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult) {
    
    vector<FakeArchive> aWorkingList;
    for (auto aArchive: *pMockArchives) {
        if (aArchive.mBlocks.size() > 0) {
            aArchive.mTemp = (int)aArchive.mBlocks.size();
            aWorkingList.push_back(aArchive);
        }
    }
    
    for (int aMutationIndex=0; aMutationIndex<pTargetCount; aMutationIndex++) {
        if (aWorkingList.size() <= 0) { break; }

        int aArchiveIndex = Random::Get((int)aWorkingList.size());
        
        FakeMutation aMutation;
        
        int aWhich = Random::Get(2);
        if (aWorkingList.size() == 1) {
            aWhich = 0;
        }
        
        if (aWhich == 0) {
            aMutation.SetMangleArchive(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                       (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex);
        } else {
            aMutation.SetDeleteArchive(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                       (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex);
            aWorkingList.erase(aWorkingList.begin() + aArchiveIndex);
        }
        
        pResult->push_back(aMutation);
    }
}

void FakeMutation::AttemptGenerateRandomBlockSwaps(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult) {
    
    vector<FakeArchive> aWorkingList;
    for (auto aArchive: *pMockArchives) {
        if (aArchive.mBlocks.size() > 0) {
            aArchive.mTemp = (int)aArchive.mBlocks.size();
            aWorkingList.push_back(aArchive);
        }
    }
    
    for (int aMutationIndex=0; aMutationIndex<pTargetCount; aMutationIndex++) {
        if (aWorkingList.size() <= 0) { break; }
        
        BlockReferencePair aSwap = GetRandomSwap(&aWorkingList);
        if (aSwap.mBlockA.mArchiveIndex == -1) { return; }
        if (aSwap.mBlockA.mBlockIndex == -1) { return; }
        
        if (aSwap.mBlockB.mArchiveIndex == -1) { return; }
        if (aSwap.mBlockB.mBlockIndex == -1) { return; }
        
        int aArchiveA = aSwap.mBlockA.mArchiveIndex;
        int aBlockA = aSwap.mBlockA.mBlockIndex;
        
        int aArchiveB = aSwap.mBlockB.mArchiveIndex;
        int aBlockB = aSwap.mBlockB.mBlockIndex;
        
        FakeMutation aMutation;
        aMutation.SetSwapBlocks(aWorkingList[aArchiveA].mFilePath.ToString(),
                                (int)aWorkingList[aArchiveA].mHeader.mArchiveIndex,
                                aBlockA,
                                aWorkingList[aArchiveB].mFilePath.ToString(),
                                (int)aWorkingList[aArchiveB].mHeader.mArchiveIndex,
                                aBlockB);
        pResult->push_back(aMutation);
    }
}

BlockReferencePair FakeMutation::GetRandomSwap(vector<FakeArchive> *pWorkingList) {
    BlockReferencePair aResult;
    aResult.mBlockA.mArchiveIndex = -1;
    aResult.mBlockA.mBlockIndex = -1;
    aResult.mBlockB.mArchiveIndex = -1;
    aResult.mBlockB.mBlockIndex = -1;
    
    vector <BlockReference> aBlockReferenceList;
    
    for (int aArchiveIndex=0; aArchiveIndex<pWorkingList->size();aArchiveIndex++) {
        const FakeArchive &aArchive = (*pWorkingList)[aArchiveIndex];
        int aLiveBlockCount = (int)aArchive.mBlocks.size();
        if ((aArchive.mTemp > 0) && (aArchive.mTemp < aLiveBlockCount)) {
            aLiveBlockCount = aArchive.mTemp;
        }
        for (int aBlockIndex=0; aBlockIndex<aLiveBlockCount; aBlockIndex++) {
            BlockReference aBlockReference;
            aBlockReference.mArchiveIndex = aArchiveIndex;
            aBlockReference.mBlockIndex = aBlockIndex;
            aBlockReferenceList.push_back(aBlockReference);
        }
    }
    
    if (aBlockReferenceList.size() < 2) {
        return aResult;
    }
    
    for (int aBlockReferenceIndex=0; aBlockReferenceIndex<aBlockReferenceList.size(); aBlockReferenceIndex++) {
        int aSwapIndex = Random::Get((int)aBlockReferenceList.size());
        BlockReference aBlockReferenceA = aBlockReferenceList[aBlockReferenceIndex];
        BlockReference aBlockReferenceB = aBlockReferenceList[aSwapIndex];
        aBlockReferenceList[aBlockReferenceIndex] = aBlockReferenceB;
        aBlockReferenceList[aSwapIndex] = aBlockReferenceA;
    }
    
    aResult.mBlockA = aBlockReferenceList[0];
    aResult.mBlockB = aBlockReferenceList[((int)aBlockReferenceList.size()) - 1];
    
    return aResult;
}

void FakeMutation::AttemptGenerateRandom(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult) {
    vector<FakeArchive> aWorkingList;
    for (auto aArchive: *pMockArchives) {
        if (aArchive.mBlocks.size() > 0) {
            aArchive.mTemp = (int)aArchive.mBlocks.size();
            aWorkingList.push_back(aArchive);
        }
    }
    
    for (int aMutationIndex=0; aMutationIndex<pTargetCount; aMutationIndex++) {
        if (aWorkingList.size() <= 0) { return; }
        
        // 0 = mangle block
        // 1 = delete block
        // 2 = swap blocks
        // 3 = mangle archive
        // 4 = delete archive
        bool aAllowed[5];
        aAllowed[0] = true;
        aAllowed[1] = true;
        aAllowed[2] = true;
        aAllowed[3] = true;
        aAllowed[4] = true;
        
        if (aWorkingList.size() == 1) {
            aAllowed[4] = false;
            if (aWorkingList[0].mTemp <= 1) {
                aAllowed[1] = false;
            }
        }
        
        BlockReferencePair aSwap;
        aSwap.mBlockA.mArchiveIndex = -1;
        aSwap.mBlockA.mBlockIndex = -1;
        aSwap.mBlockB.mArchiveIndex = -1;
        aSwap.mBlockB.mBlockIndex = -1;
        
        // The clincher
        //aAllowed[2] = false;
        
        for (int i=0;i<10;i++) {
            
            BlockReferencePair aTestSwap = GetRandomSwap(&aWorkingList);
            
            bool aInvalid = false;
            if (aTestSwap.mBlockA.mArchiveIndex == -1) { aInvalid = true; }
            if (aTestSwap.mBlockA.mBlockIndex == -1) { aInvalid = true; }
            if (aTestSwap.mBlockB.mArchiveIndex == -1) { aInvalid = true; }
            if (aTestSwap.mBlockB.mBlockIndex == -1) { aInvalid = true; }
            if (aInvalid == false) {
                aSwap.mBlockA.mArchiveIndex = aTestSwap.mBlockA.mArchiveIndex;
                aSwap.mBlockA.mBlockIndex = aTestSwap.mBlockA.mBlockIndex;
                aSwap.mBlockB.mArchiveIndex = aTestSwap.mBlockB.mArchiveIndex;
                aSwap.mBlockB.mBlockIndex = aTestSwap.mBlockB.mBlockIndex;
                break;
            }
        }
        
        if (aSwap.mBlockA.mArchiveIndex == -1) { aAllowed[2] = false; }
        if (aSwap.mBlockA.mBlockIndex == -1) { aAllowed[2] = false; }
        if (aSwap.mBlockB.mArchiveIndex == -1) { aAllowed[2] = false; }
        if (aSwap.mBlockB.mBlockIndex == -1) { aAllowed[2] = false; }
        
        aAllowed[2] = false;
        
        vector<int> aChoices;
        if (aAllowed[0] == true) { aChoices.push_back(0); }
        if (aAllowed[1] == true) { aChoices.push_back(1); }
        if (aAllowed[2] == true) { aChoices.push_back(2); }
        if (aAllowed[3] == true) { aChoices.push_back(3); }
        if (aAllowed[4] == true) { aChoices.push_back(4); }
        
        if (aChoices.size() <= 0) { break; }
        int aChoiceIndex = Random::Get((int)aChoices.size());
        int aChoice = aChoices[aChoiceIndex];
        
        if (aChoice == 0) { // mangle block
            int aArchiveIndex = Random::Get((int)aWorkingList.size());
            int aBlockIndex = Random::Get((int)aWorkingList[aArchiveIndex].mTemp);
            FakeMutation aMutation;
            aMutation.SetMangleBlock(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                     (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex,
                                     aBlockIndex);
            pResult->push_back(aMutation);
            
        } else if (aChoice == 1) { // delete block
            int aArchiveIndex = Random::Get((int)aWorkingList.size());
            int aBlockIndex = Random::Get((int)aWorkingList[aArchiveIndex].mTemp);
            FakeMutation aMutation;
            aMutation.SetDeleteBlock(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                     (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex,
                                     aBlockIndex);
            pResult->push_back(aMutation);
            
            aWorkingList[aArchiveIndex].mTemp--;
            if (aWorkingList[aArchiveIndex].mTemp <= 0) {
                aWorkingList.erase(aWorkingList.begin() + aArchiveIndex);
            }
            
        }  else if (aChoice == 2) { // swap blocks
            int aArchiveA = aSwap.mBlockA.mArchiveIndex;
            int aBlockA = aSwap.mBlockA.mBlockIndex;
            
            int aArchiveB = aSwap.mBlockB.mArchiveIndex;
            int aBlockB = aSwap.mBlockB.mBlockIndex;
            
            FakeMutation aMutation;
            aMutation.SetSwapBlocks(aWorkingList[aArchiveA].mFilePath.ToString(),
                                    (int)aWorkingList[aArchiveA].mHeader.mArchiveIndex,
                                    aBlockA,
                                    aWorkingList[aArchiveB].mFilePath.ToString(),
                                    (int)aWorkingList[aArchiveB].mHeader.mArchiveIndex,
                                    aBlockB);
            pResult->push_back(aMutation);
            
        }  else if (aChoice == 3) { // mangle archive
            int aArchiveIndex = Random::Get((int)aWorkingList.size());
            
            FakeMutation aMutation;
            aMutation.SetMangleArchive(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                       (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex);
            pResult->push_back(aMutation);
            
        }  else if (aChoice == 4) { // delete archive
            int aArchiveIndex = Random::Get((int)aWorkingList.size());
            
            FakeMutation aMutation;
            aMutation.SetDeleteArchive(aWorkingList[aArchiveIndex].mFilePath.ToString(),
                                       (int)aWorkingList[aArchiveIndex].mHeader.mArchiveIndex);
            pResult->push_back(aMutation);
            
            aWorkingList.erase(aWorkingList.begin() + aArchiveIndex);
            
        } else { // unknown
            return;
        }
        
        
    }
}
