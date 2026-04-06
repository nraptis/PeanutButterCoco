//
//  FakeMangleTool.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#include "FakeMangleTool.hpp"

bool FakeMangleTool::MangleBlock(int pArchiveIndex,
                                 int pBlockIndex,
                                 vector<FakeFile> *pFileList,
                                 vector<FakeArchive> *pArchiveList,
                                 vector<FakeFileBlockSpan> *pSpanList,
                                 ByteString *pError) {
    
    if (pFileList == NULL) {
        if (pError != NULL) {
            pError->Set("Mangle block, null file list.");
        }
        return false;
    }
    if (pArchiveList == NULL) {
        if (pError != NULL) {
            pError->Set("Mangle block, null archive list.");
        }
        return false;
    }
    if (pSpanList == NULL) {
        if (pError != NULL) {
            pError->Set("Mangle block, null span list.");
        }
        return false;
    }
    if ((pArchiveIndex < 0) || (pArchiveIndex >= pArchiveList->size())) {
        if (pError != NULL) {
            ByteString aError = ByteString("Mangle block, archive index ") + ByteString(pArchiveIndex) + ByteString(", out of bounds.");
            pError->Set(aError);
        }
        return false;
    }
    
    FakeArchive aArchive = (*pArchiveList)[pArchiveIndex];
    
    if ((pBlockIndex < 0) || (pBlockIndex >= aArchive.mBlocks.size())) {
        if (pError != NULL) {
            ByteString aError = ByteString("Mangle block, archive at index ") + ByteString(pArchiveIndex) + ByteString(" cannot support block index of ")  + ByteString(pBlockIndex) + ByteString(", out of bounds.");
            pError->Set(aError);
        }
        return false;
    }
    
    FakeArchiveBlock aBlock = aArchive.mBlocks[pBlockIndex];
    
    for (int aFileIndex=0; aFileIndex<(*pFileList).size(); aFileIndex++) {
        FakeFile &aFile = (*pFileList)[aFileIndex];
        if (aFile.mIsRecoverDeleted) {
            continue;
        }
        
        int aFirstByteInBlock = FileFirstByteInBlock(aFile,
                                                     aArchive.mArchiveUUID,
                                                     aBlock.mBlockUUID,
                                                     pSpanList);
        if (aFirstByteInBlock < 0) {
            continue;
        }
        int aLastByteInBlock = FileLastByteInBlock(aFile,
                                                   aArchive.mArchiveUUID,
                                                   aBlock.mBlockUUID,
                                                   pSpanList);
        if (aLastByteInBlock < aFirstByteInBlock) {
            if (pError != NULL) {
                ByteString aError = ByteString("Mangle block, invalid file span range for file {") +
                                    aFile.mName +
                                    ByteString("}.");
                pError->Set(aError);
            }
            return false;
        }
        
        if (aFile.mIsFolder) {
            aFile.RecoverDelete();
            continue;
        }
        
        const int aHeaderBytesBeforeContent =
            FakeFileBlockSpan::PartialRecoverMinimumLength(aFile.mName.mLength);
        const int aRecoverableContentBytes = (aFirstByteInBlock - aHeaderBytesBeforeContent);
        if (aRecoverableContentBytes <= 0) {
            aFile.RecoverDelete();
            continue;
        }
        
        // This block is fully mangled, so bytes from first-in-block through last-in-block are lost.
        // Keep only the safe prefix before the damaged range.
        aFile.RecoverTruncate(aRecoverableContentBytes);
    }
    
    return true;
}

bool FakeMangleTool::DeleteBlock(int pArchiveIndex,
                                 int pBlockIndex,
                                 vector<FakeFile> *pFileList,
                                 vector<FakeArchive> *pArchiveList,
                                 vector<FakeFileBlockSpan> *pSpanList,
                                 ByteString *pError) {
    if (pArchiveList == NULL) {
        if (pError != NULL) {
            pError->Set("Delete block, null archive list.");
        }
        return false;
    }
    if ((pArchiveIndex < 0) || (pArchiveIndex >= pArchiveList->size())) {
        if (pError != NULL) {
            ByteString aError = ByteString("Delete block, archive index ") +
                                ByteString(pArchiveIndex) +
                                ByteString(", out of bounds.");
            pError->Set(aError);
        }
        return false;
    }
    
    const int aBlockCount = (int)(*pArchiveList)[pArchiveIndex].mBlocks.size();
    if ((pBlockIndex < 0) || (pBlockIndex >= aBlockCount)) {
        if (pError != NULL) {
            ByteString aError = ByteString("Delete block, archive at index ") +
                                ByteString(pArchiveIndex) +
                                ByteString(" cannot support block index of ") +
                                ByteString(pBlockIndex) +
                                ByteString(", out of bounds.");
            pError->Set(aError);
        }
        return false;
    }
    
    // Real DeleteBlock physically removes a block, so every later block in the
    // same archive shifts left and fails header-index validation. Model that by
    // applying full-block loss for the deleted block and the entire archive tail.
    for (int aLoopBlockIndex=pBlockIndex; aLoopBlockIndex<aBlockCount; aLoopBlockIndex++) {
        if (!MangleBlock(pArchiveIndex,
                         aLoopBlockIndex,
                         pFileList,
                         pArchiveList,
                         pSpanList,
                         pError)) {
            return false;
        }
    }
    
    return true;
}

bool FakeMangleTool::FileByteRangeInBlock(FakeFile pFile,
                                          int pArchiveUUID,
                                          int pBlockUUID,
                                          vector<FakeFileBlockSpan> *pSpanList,
                                          int *pStartIndex,
                                          int *pEndIndex) {
    if ((pSpanList == NULL) || (pStartIndex == NULL) || (pEndIndex == NULL)) {
        return false;
    }
    
    for (const FakeFileBlockSpan &aSpan : *pSpanList) {
        if (aSpan.mName.Compare(pFile.mName) != 0) {
            continue;
        }
        
        const int aEntryCount = (int)aSpan.mBlockIdentifiers.size();
        if (((int)aSpan.mArchiveIdentifiers.size() != aEntryCount) ||
            ((int)aSpan.mStartIndex.size() != aEntryCount) ||
            ((int)aSpan.mEndIndex.size() != aEntryCount)) {
            return false;
        }
        
        for (int aIndex=0; aIndex<aEntryCount; aIndex++) {
            if ((aSpan.mArchiveIdentifiers[aIndex] == pArchiveUUID) &&
                (aSpan.mBlockIdentifiers[aIndex] == pBlockUUID)) {
                *pStartIndex = aSpan.mStartIndex[aIndex];
                *pEndIndex = aSpan.mEndIndex[aIndex];
                return true;
            }
        }
        
        return false;
    }
    
    return false;
}

int FakeMangleTool::FileFirstByteInBlock(FakeFile pFile,
                                         int pArchiveUUID,
                                         int pBlockUUID,
                                         vector<FakeFileBlockSpan> *pSpanList) {
    int aStartIndex = -1;
    int aEndIndex = -1;
    if (!FileByteRangeInBlock(pFile,
                              pArchiveUUID,
                              pBlockUUID,
                              pSpanList,
                              &aStartIndex,
                              &aEndIndex)) {
        return -1;
    }
    return aStartIndex;
}

int FakeMangleTool::FileLastByteInBlock(FakeFile pFile,
                                        int pArchiveUUID,
                                        int pBlockUUID,
                                        vector<FakeFileBlockSpan> *pSpanList) {
    int aStartIndex = -1;
    int aEndIndex = -1;
    if (!FileByteRangeInBlock(pFile,
                              pArchiveUUID,
                              pBlockUUID,
                              pSpanList,
                              &aStartIndex,
                              &aEndIndex)) {
        return -1;
    }
    if (aEndIndex <= aStartIndex) {
        return -1;
    }
    return (aEndIndex - 1);
}
