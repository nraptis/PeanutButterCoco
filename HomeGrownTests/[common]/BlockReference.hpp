//
//  BlockReference.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/6/26.
//

#ifndef BlockReference_hpp
#define BlockReference_hpp

#include <vector>

using namespace std;
class FakeArchiveBlock;

struct BlockReference {
    int mArchiveIndex;
    int mBlockIndex;
    
    bool operator==(const BlockReference& other) const {
        return mArchiveIndex == other.mArchiveIndex &&
               mBlockIndex == other.mBlockIndex;
    }
};

struct BlockReferencePair {
    BlockReference      mBlockA;
    BlockReference      mBlockB;
};

struct BlockHasher {
    size_t operator()(const BlockReference& b) const {
        // Pack two 32-bit ints into one 64-bit size_t
        // Shifting by 32 ensures NO collisions between the two indices
        return (static_cast<uint64_t>(b.mArchiveIndex) << 32) |
                static_cast<uint32_t>(b.mBlockIndex);
    }
};

int FlattenedMainIndex(const FakeArchiveBlock *pBlock, int pBlocksPerArchive);
bool IsFirstBlock(const FakeArchiveBlock *pBlock, int pBlocksPerArchive);

#endif /* BlockReference_hpp */
