//
//  JobBundle.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "JobBundle.hpp"
#include "ByteMap.hpp"
#include <algorithm>

JobBundle::JobBundle() {
    mInput = "/root/input";
    mArchived = "/root/archived";
    mUnarchived = "/root/unarchived";
    mFilePrefix = "bdl_";
    mPayloadBytesPerBlock = 18;
    mMaxPathLength = 1024;
    mMaxArchiveCount = 64000;
    mBatchSize = 4;
    mBlocksPerArchive = 4;
    mEncryptionEnabled = false;
    mRepairCoverage = 0;
    mPreviewEnabled = false;
    mClearDestination = true;
}

JobBundle::~JobBundle() {
    
}

void JobBundle::AddFile(string pName, string pContent) {
    FakeFile aFile;
    aFile.mName.Set(pName);
    aFile.mContent.Set(pContent);
    aFile.mIsFolder = false;
    mFileList.push_back(aFile);
}

void JobBundle::AddFile(ByteString pName, ByteString pContent) {
    FakeFile aFile;
    aFile.mName.Set(pName);
    aFile.mContent.Set(pContent);
    aFile.mIsFolder = false;
    mFileList.push_back(aFile);
}

void JobBundle::AddFolder(string pName) {
    FakeFile aFile;
    aFile.mName.Set(pName);
    aFile.mIsFolder = true;
    mFileList.push_back(aFile);
}

void JobBundle::AddFolder(ByteString pName) {
    FakeFile aFile;
    aFile.mName.Set(pName);
    aFile.mIsFolder = true;
    mFileList.push_back(aFile);
}

void JobBundle::SortFiles() {
    std::sort(mFileList.begin(), mFileList.end(), [](const FakeFile &pLeft, const FakeFile &pRight) {
        return pLeft.mName.Compare(pRight.mName) < 0;
    });
}

bool JobBundle::ContainsDuplicateFiles() const {
    ByteMap aMap;
    for (int i=0; i<((int)mFileList.size()); i++) {
        const ByteString &aName = mFileList[i].mName;
        if (aMap.Exists(aName)) {
            return true;
        }
        aMap.Add(aName);
    }
    return false;
}

void JobBundle::SetRepairOff() {
    mRepairCoverage = 0;
}

void JobBundle::SetRepair20() {
    mRepairCoverage = (unsigned char)(RepairCoveragePresetV2::k20);
}

void JobBundle::SetRepair40() {
    mRepairCoverage = (unsigned char)(RepairCoveragePresetV2::k40);
}

void JobBundle::SetRepair60() {
    mRepairCoverage = (unsigned char)(RepairCoveragePresetV2::k60);
}

void JobBundle::SetRepair80() {
    mRepairCoverage = (unsigned char)(RepairCoveragePresetV2::k80);
}
