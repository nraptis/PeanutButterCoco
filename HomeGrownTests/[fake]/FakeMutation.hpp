//
//  FakeMutation.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/6/26.
//

#ifndef FakeMutation_hpp
#define FakeMutation_hpp

#include "ByteString.hpp"
#include "FakeArchive.hpp"
#include "BlockReference.hpp"

class FakeMutation {
public:
    FakeMutation();
    ~FakeMutation();
    
    enum class MutationKind {
        kNone = -1,
        kMangleBlock = 1,
        kDeleteBlock = 2,
        kSwapBlocks = 3,
        kDeleteArchive = 4,
        kMangleArchive = 5
    };
    
    MutationKind                    mMutationKind;
    
    int                             mArchiveIndex; // mPrimaryArchiveIndex
    int                             mBlockIndex; // // mPrimaryBlockIndex
    int                             mArchiveIndexB;  // mSecondaryArchiveIndex
    int                             mBlockIndexB; // mSecondaryBlockndex
    
    ByteString                      mPrimaryArchiveFileName;
    ByteString                      mSecondaryArchiveFileName;
    
    void                            SetDeleteBlock(string pArchiveFileName, int pArchiveIndex, int pBlockIndex);
    void                            SetMangleBlock(string pArchiveFileName, int pArchiveIndex, int pBlockIndex);
    
    void                            SetDeleteArchive(string pArchiveFileName, int pArchiveIndex);
    void                            SetMangleArchive(string pArchiveFileName, int pArchiveIndex);
    
    void                            SetSwapBlocks(string pArchiveFileNameA, int pArchiveIndexA, int pBlockIndexA, string pArchiveFileNameB, int pArchiveIndexB, int pBlockIndexB);
    
    static void                     AttemptGenerateRandomBlockMangles(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult);
    
    static void                     AttemptGenerateRandomBlockDeletions(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult);
    
    static void                     AttemptGenerateRandomBlockDestruction(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult);
    
    static void                     AttemptGenerateRandomArchiveMangles(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult);
    static void                     AttemptGenerateRandomArchiveDeletions(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult);
    
    
    static void                     AttemptGenerateRandomArchiveDestruction(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult);
    
    static void                     AttemptGenerateRandomBlockSwaps(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult);
    
    static void                     AttemptGenerateRandom(int pTargetCount, vector<FakeArchive> *pMockArchives, vector<FakeMutation> *pResult);
    
    
    static BlockReferencePair       GetRandomSwap(vector<FakeArchive> *pWorkingList);
    
};

#endif /* FakeMutation_hpp */
