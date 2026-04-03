//
//  SimpleBundleRuntime.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#include "SimpleBundleRuntime.hpp"

SimpleBundleRuntime::SimpleBundleRuntime() {
    mCancelled = false;
}

SimpleBundleRuntime::~SimpleBundleRuntime() {
    
}

bool SimpleBundleRuntime::IsCancelRequested() const {
    return (mCancelled == true);
}

void SimpleBundleRuntime::EmitLog(peanutbutter::LogLevelV2 pLevel, const std::string& pMessage) {
    mLogs.push_back(pMessage);
}

void SimpleBundleRuntime::EmitProgress(peanutbutter::ProgressStageV2 pStage,
                                       double pLocalFraction,
                                       double pOverallFraction,
                                       const std::string &pLabel) {
    
}

bool SimpleBundleRuntime::WantsRuntimeEvent(RuntimeEventKindV2 pKind) const {
    return true;
}

bool SimpleBundleRuntime::EmitRuntimeEvent(const RuntimeEventV2& pEvent) {
    mEvents.push_back(pEvent);
    return true;
}

bool SimpleBundleRuntime::SawEvent(RuntimeEventKindV2 pKind) const {
    for (const RuntimeEventV2& aEvent : mEvents) {
        if (aEvent.mKind == pKind) {
            return true;
        }
    }
    return false;
}

void SimpleBundleRuntime::Cancel() {
    mCancelled = true;
}

