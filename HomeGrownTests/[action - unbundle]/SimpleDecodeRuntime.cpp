//
//  SimpleDecodeRuntime.cpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/5/26.
//

#include "SimpleDecodeRuntime.hpp"

SimpleDecodeRuntime::SimpleDecodeRuntime() {
    mCancelled = false;
}

SimpleDecodeRuntime::~SimpleDecodeRuntime() {
    
}

bool SimpleDecodeRuntime::IsCancelRequested() const {
    return (mCancelled == true);
}

void SimpleDecodeRuntime::EmitLog(peanutbutter::LogLevelV2 pLevel, const std::string& pMessage) {
    mLogs.push_back(pMessage);
    (void)pLevel;
}

void SimpleDecodeRuntime::EmitProgress(peanutbutter::ProgressStageV2 pStage,
                                       double pLocalFraction,
                                       double pOverallFraction,
                                       const std::string &pLabel) {
    
}

bool SimpleDecodeRuntime::WantsRuntimeEvent(RuntimeEventKindV2 pKind) const {
    return true;
}

bool SimpleDecodeRuntime::EmitRuntimeEvent(const RuntimeEventV2& pEvent) {
    mEvents.push_back(pEvent);
    return true;
}

bool SimpleDecodeRuntime::SawEvent(RuntimeEventKindV2 pKind) const {
    for (const RuntimeEventV2& aEvent : mEvents) {
        if (aEvent.mKind == pKind) {
            return true;
        }
    }
    return false;
}

void SimpleDecodeRuntime::Cancel() {
    mCancelled = true;
}
