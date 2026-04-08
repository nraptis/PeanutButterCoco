//
//  BlockReference.cpp
//  PeanutButterArchiver
//
//  Created by Magneto on 4/7/26.
//

#include "BlockReference.hpp"
#include "FakeArchiveBlock.hpp"

int FlattenedMainIndex(const FakeArchiveBlock *pBlock, int pBlocksPerArchive) {
    return (int)(pBlock->mHeader.mArchiveIndex * pBlocksPerArchive) + (pBlock->mHeader.mBlockIndex);
}

bool IsFirstBlock(const FakeArchiveBlock *pBlock, int pBlocksPerArchive) {
    int aFlatIndex = FlattenedMainIndex(pBlock, pBlocksPerArchive);
    return (aFlatIndex == pBlock->mHeader.mBlockCountPreview);
}
