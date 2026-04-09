//
//  SimpleDecodeRuntime.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/5/26.
//

#ifndef SimpleDecodeRuntime_hpp
#define SimpleDecodeRuntime_hpp

#include "DecodeRequest.hpp"
#include "Decode_Execution.hpp"
#include "ArchiveLayoutConfig.hpp"
#include "SimpleDecodeRuntime.hpp"
#include "namespaces.hpp"

class SimpleDecodeRuntime: public DecodeRuntimeV2 {
public:
    
    SimpleDecodeRuntime();
    ~SimpleDecodeRuntime();
    
    bool IsCancelRequested() const override;
    
    void EmitLog(peanutbutter::LogLevelV2 pLevel, const std::string &pMessage) override;
    
    void EmitProgress(peanutbutter::ProgressStageV2 pStage,
                      double pLocalFraction,
                      double pOverallFraction,
                      const std::string& pLabel) override;
    
    bool WantsRuntimeEvent(RuntimeEventKindV2 pKind) const override;
    bool EmitRuntimeEvent(const RuntimeEventV2 &pEvent) override;
    bool SawEvent(RuntimeEventKindV2 pKind) const;
    
    bool                                mCancelled;
    void                                Cancel();
    
    std::vector<RuntimeEventV2>         mEvents;
    std::vector<std::string>            mLogs;
};

#endif /* SimpleDecodeRuntime_hpp */
