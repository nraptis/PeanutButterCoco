//
//  JobBundle.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "JobBundle.hpp"

JobBundle::JobBundle() {
    mSource = "input";
    mDestination = "archived";
    mFilePrefix = "bdl_";
    mPayloadByteCount = 18;
    mMaxPathLength = 128;
    mMaxArchiveCount = 64000;
    mBatchSize = 4;
    mBlockCount = 4;
    mEncryptionEnabled = false;
    mRepairCoverage = 0;
    mPreviewEnabled = false;
    mClearDestination = true;
}

JobBundle::~JobBundle() {
    
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
