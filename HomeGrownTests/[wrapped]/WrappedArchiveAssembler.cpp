//
//  WrappedArchiveAssembler.cpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/4/26.
//

#include "WrappedArchiveAssembler.hpp"
#include "Layout.hpp"
#include "ArchiveHeader.hpp"
#include "WrappedArchiveBlock.hpp"
#include "TestBundle.hpp"

vector<WrappedArchive> WrappedArchiveAssembler::Get(string pDirectory,
                                                    MockFileSystem &pFileSystem,
                                                    int pBlocksPerArchive,
                                                    int pBytesPerBlock) {
    
    if (pBytesPerBlock <= Layout::SectionHeaderSize()) {
        printf("Fatal, pBytesPerBlock (%d) <= SectionHeaderSize (%d)\n", pBytesPerBlock, Layout::SectionHeaderSize());
        exit(0);
    }
    
    if (pBlocksPerArchive <= 0) {
        printf("Fatal, pBlocksPerArchive (%d) <= 0\n", pBlocksPerArchive);
        exit(0);
    }
    
    vector<WrappedArchive> aResult;
    vector<string> aArchiveFiles = pFileSystem.mDrive->ListFilesRecursive(pDirectory);
    
    for (int aArchiveFileIndex=0; aArchiveFileIndex<((int)aArchiveFiles.size()); aArchiveFileIndex++) {
        string aArchiveFileName = aArchiveFiles[aArchiveFileIndex];
        
        ByteString aRead = pFileSystem.Load(aArchiveFileName);
        int aArchiveHeaderLength = min(Layout::ArchiveHeaderSize(), aRead.mLength);
        
        if (aArchiveHeaderLength >= Layout::ArchiveHeaderSize()) {
            WrappedArchive aArchive;
            if (ReadArchiveHeader(aRead.mData, aArchiveHeaderLength, aArchive.mHeader)) {
                
                int aCeilingA = pBytesPerBlock * pBlocksPerArchive + Layout::ArchiveHeaderSize();
                int aCeilingB = aRead.mLength;
                if (aCeilingB > aCeilingA) {
                    printf("Warn: Archive file (%d) is larger than expected (%d)\n", aCeilingB, aCeilingA);
                }
                
                int aCeiling = min(aCeilingA, aCeilingB);
                int aOffset = Layout::ArchiveHeaderSize();
                int aBlockNumber = 1;
                while (aOffset < aCeiling) {
                    if ((aOffset + pBytesPerBlock) <= aCeiling) {
                        WrappedArchiveBlock aBlock;
                        unsigned char *aBlockData = aRead.mData + aOffset;
                        int aBlockLength = pBytesPerBlock;
                        int aSectionHeaderSize = Layout::SectionHeaderSize();
                        if (aBlockLength >= aSectionHeaderSize) {
                            if (!ReadSectionHeader(aBlockData, aBlockLength, aBlock.mHeader)) {
                                printf("Warn: Failed to read section header for block %d.\n", aBlockNumber);
                            } else {
                                int aPayloadLength = aBlockLength - aSectionHeaderSize;
                                aBlock.mPayload.Size(aPayloadLength);
                                for (int i = 0; i < aPayloadLength; i++) {
                                    aBlock.mPayload.mData[i] = aBlockData[i + aSectionHeaderSize];
                                }
                                aBlock.mPayload.mLength = aPayloadLength;
                                aArchive.mBlocks.push_back(aBlock);
                            }
                        } else {
                            printf("Warn: Section header for block %d was too small (%d).\n", aBlockNumber, aBlockLength);
                        }
                    }
                    aBlockNumber++;
                    aOffset += pBytesPerBlock;
                }
            }
            
            aArchive.mFilePath = aArchiveFileName;
            
            string aPath = aArchiveFileName;
            
            size_t aSlash = aPath.find_last_of("/\\");
            string aFileName = (aSlash == string::npos) ? aPath : aPath.substr(aSlash + 1);
            
            size_t aDot = aFileName.find_last_of('.');
            string aStem = (aDot == string::npos) ? aFileName : aFileName.substr(0, aDot);

            aArchive.mFileStem = aStem;
            
            int aEnd = (int)aStem.length() - 1;
            int aStart = aEnd;

            while (aStart >= 0 && isdigit((unsigned char)aStem[aStart])) {
                aStart--;
            }

            aStart++; // move to first digit
            
            string aNumberString;
            if (aStart <= aEnd && aStart < (int)aStem.length()) {
                aNumberString = aStem.substr(aStart, aEnd - aStart + 1);
            } else {
                aNumberString = "0";
            }

            aArchive.mFileNumberString = aNumberString;
            int aNumber = 0;
            for (int i = 0; i < (int)aNumberString.length(); i++) {
                aNumber = aNumber * 10 + (aNumberString[i] - '0');
            }
            
            aArchive.mFileNumber = aNumber;
            aResult.push_back(aArchive);
        }
    }
    
    std::sort(aResult.begin(), aResult.end(),
        [](const WrappedArchive &pLeft, const WrappedArchive &pRight) {
            return pLeft.mFilePath.Compare(pRight.mFilePath) < 0;
        }
    );
    
    return aResult;
}
