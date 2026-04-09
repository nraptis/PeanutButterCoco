//
//  Layout.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "Layout.hpp"
#include "knobs.hpp"
#include "namespaces.hpp"
#include "TestBundle.hpp"
#include <limits>

int Layout::ArchiveHeaderSize() {
    int aResult = (int)(peanutbutter::memory_layout::kArchiveHeaderBytesV2);
    return aResult;
}

int Layout::SectionHeaderSize() {
    int aResult = (int)(peanutbutter::memory_layout::kSectionHeaderBytesV2);
    return aResult;
}


uint64_t Layout::ToLong(const PackedUint24V2 &pValue) {
    return (static_cast<std::uint64_t>(pValue.mBytes[0])      ) |
           (static_cast<std::uint64_t>(pValue.mBytes[1]) <<  8) |
           (static_cast<std::uint64_t>(pValue.mBytes[2]) << 16);
}

uint64_t Layout::ToLong(const PackedUint48V2 &pValue) {
    return (static_cast<std::uint64_t>(pValue.mBytes[0])      ) |
           (static_cast<std::uint64_t>(pValue.mBytes[1]) <<  8) |
           (static_cast<std::uint64_t>(pValue.mBytes[2]) << 16) |
           (static_cast<std::uint64_t>(pValue.mBytes[3]) << 24) |
           (static_cast<std::uint64_t>(pValue.mBytes[4]) << 32) |
           (static_cast<std::uint64_t>(pValue.mBytes[5]) << 40);
}

int Layout::ToInt(const PackedUint24V2& pValue) {
    std::uint64_t value = ToLong(pValue);
    if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
}

int Layout::ToInt(const PackedUint48V2& pValue) {
    std::uint64_t value = ToLong(pValue);
    if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
}

int Layout::GetZeroCount(int pMaximum) {
    if (pMaximum == 0) {
        return 1;
    }
    
    int aValue = pMaximum;
    int aResult = 0;
    while (aValue > 0) {
        aValue /= 10;
        aResult++;
    }
    return aResult;
}

ByteString Layout::GetZeroPadded(int pNumber, int pZeroCount) {
    ByteString aNumberString = ByteString(pNumber);
    if (aNumberString.mLength < pZeroCount) {
        ByteString aResult;
        aResult.Append(string(pZeroCount - aNumberString.mLength, '0'));
        aResult.Append(aNumberString);
        
        
        //= ByteString(string(pZeroCount - aNumberString.mLength, '0'));
        return aResult;
    } else {
        return aNumberString;
    }
}

ByteString Layout::GetUniqueName(ByteString pPreferredName, ByteMap *pMap) {
    
    ByteString aResult;
    aResult.Set(pPreferredName);
    if (pMap == NULL) {
        return aResult;
    }
    for (int aTryNumber=1;aTryNumber<1000;aTryNumber++) {
        ByteString aCheck = aResult + ByteString("_") + ByteString(aTryNumber);
        if (pMap->Exists(aCheck) == false) {
            return aCheck;
        }
    }
    return aResult;
}

void Layout::PrintPackedMembership(JobBundle &pJob) {

    pJob.SortFiles();
    
    int aBlockLength = pJob.mPayloadBytesPerBlock;
    
    string aSourceFolder = pJob.mInput.ToString();
    string aDestinationFolder = pJob.mArchived.ToString();
    
    
    vector<ByteString> aPreviewPayloadBlocks;
    int aPreviewBlockCount = 0;
    
    ByteString aErrorString;
    
    if (pJob.mPreviewEnabled) {
        vector<ByteString> aPreviewPayloadList;
        for (int aFileIndex=0; aFileIndex<((int)pJob.mFileList.size()); aFileIndex++) {
            FakeFile aFile = pJob.mFileList[aFileIndex];
            ByteString aPayload;
            if (!aFile.ToPreviewPayload(&aPayload, &aErrorString)) {
                return;
            }
            aPreviewPayloadList.push_back(aPayload);
        }
        if (!TestBundle::PackBlocks(&aPreviewPayloadList, &aPreviewPayloadBlocks, aBlockLength, &aErrorString)) {
            return;
        }
        aPreviewBlockCount = (int)aPreviewPayloadBlocks.size();
    }
    
    vector<ByteString> aMainPayloadList;
    vector<FakeFile> aMainPayloadFiles;
    for (int aFileIndex=0; aFileIndex<((int)pJob.mFileList.size()); aFileIndex++) {
        FakeFile aFile = pJob.mFileList[aFileIndex];
        ByteString aPayload;
        if (!aFile.ToPayload(&aPayload, &aErrorString)) {
            return;
        }
        aMainPayloadFiles.push_back(aFile);
        aMainPayloadList.push_back(aPayload);
    }
    int aMainPayloadCount = (int)aMainPayloadList.size();
    if (aMainPayloadCount <= 0) {
        return;
    }

    
    int aMainTotalLength = 0;
    for (int aPayloadIndex = 0; aPayloadIndex < ((int)aMainPayloadList.size()); aPayloadIndex++) {
        aMainTotalLength += aMainPayloadList[aPayloadIndex].mLength;
    }
    
    int aMainBlockCount = (aMainTotalLength + aBlockLength - 1) / aBlockLength;
    
    int aPayloadIndex = 0;
    int aPayloadOffset = 0;
    int aBlockIndex = 0;
    int aBlockOffset = 0;
    
    vector<FakeFile> aBlockFiles;
    vector<int> aBlockArchives;
    vector<int> aBlockBlocks;
    vector<int> aBlockAmounts;
    
    
    while ((aBlockIndex < aMainBlockCount) && (aPayloadIndex < aMainPayloadCount)) {
        
        int aPayloadLength = aMainPayloadList[aPayloadIndex].mLength;
        int aCopyAmountChoiceA = (aBlockLength - aBlockOffset);
        int aCopyAmountChoiceB = (aPayloadLength - aPayloadOffset);
        int aAmount = min(aCopyAmountChoiceA, aCopyAmountChoiceB);

        FakeFile aFile = aMainPayloadFiles[aPayloadIndex];
        
        int aPackedBlock = aPreviewBlockCount + aBlockIndex;
        int aPackedArchive = (aPackedBlock / pJob.mBlocksPerArchive);
        aPackedBlock = aPackedBlock - (aPackedArchive * pJob.mBlocksPerArchive);
        
        printf("file {%s} memberhsip in [a: %d, b: %d] (%d / %d)\n",
               aFile.mName.ToString().c_str(),
               aPackedArchive,
               aPackedBlock,
               aAmount,
               aBlockLength);
        
        aBlockFiles.push_back(aFile);
        aBlockArchives.push_back(aPackedArchive);
        aBlockBlocks.push_back(aPackedBlock);
        aBlockAmounts.push_back(aAmount);
        
        // What I want to print here:
        //
        
        aBlockOffset += aAmount;
        if (aBlockOffset >= aBlockLength) {
            aBlockOffset = 0;
            ++aBlockIndex;
        }
        
        aPayloadOffset += aAmount;
        if (aPayloadOffset >= aPayloadLength) {
            aPayloadOffset = 0;
            ++aPayloadIndex;
        }
    }
    
    
    int aNonRepairBlockCount = (int)(aPreviewPayloadBlocks.size() + aMainBlockCount);
    
    vector<ByteString> aRepairPayloadBlocks;
    
    int aTargetRepairBlockCount = 0;
    aTargetRepairBlockCount = (aMainBlockCount * ((int)pJob.mRepairCoverage) + 99) / 100;
    
    if (aTargetRepairBlockCount > 0) {
        
        int aFirstMainBlockIndex = (int)aPreviewPayloadBlocks.size();
        int aConsumedRepairBlockCount = 0;
        for (int aFlatBlockIndex = aFirstMainBlockIndex;
             aFlatBlockIndex < aNonRepairBlockCount &&
             aConsumedRepairBlockCount < aTargetRepairBlockCount;
             aFlatBlockIndex++) {
            int aSourceArchiveIndex = aFlatBlockIndex / pJob.mBlocksPerArchive;
            int aSourceBlockIndex =
                (aFlatBlockIndex - aSourceArchiveIndex * pJob.mBlocksPerArchive);
            int aRepairFlatIndex = aNonRepairBlockCount + aConsumedRepairBlockCount;
            int aRepairArchiveIndex = (aRepairFlatIndex / pJob.mBlocksPerArchive);
            int aRepairBlockIndex =
                (aRepairFlatIndex - aRepairArchiveIndex * pJob.mBlocksPerArchive);

            for (int aScan=0;aScan<aBlockFiles.size();aScan++) {
                int aArchiveIndex = aBlockArchives[aScan];
                int aBlockIndex = aBlockBlocks[aScan];

                if ((aSourceArchiveIndex == aArchiveIndex) && (aSourceBlockIndex == aBlockIndex)) {
                    FakeFile aFile = aBlockFiles[aScan];
                    int aAmount = aBlockAmounts[aScan];

                    printf("file {%s} repaired by [a: %d, b: %d] => [a: %d, b: %d] (%d / %d)\n",
                           aFile.mName.ToString().c_str(),
                           aRepairArchiveIndex,
                           aRepairBlockIndex,
                           aArchiveIndex,
                           aBlockIndex,
                           aAmount,
                           aBlockLength);

                }
            }

            aConsumedRepairBlockCount++;
        }

        
    }
    
}
