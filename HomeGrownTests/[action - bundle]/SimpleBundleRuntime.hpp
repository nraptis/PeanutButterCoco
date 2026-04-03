//
//  SimpleBundleRuntime.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/3/26.
//

#ifndef SimpleBundleRuntime_hpp
#define SimpleBundleRuntime_hpp

#include "BundleRequest.hpp"
#include "Bundle_Execution.hpp"
#include "ArchiveLayoutConfig.hpp"
#include "namespaces.hpp"

class SimpleBundleRuntime: public BundleRuntimeV2 {
public:
    
    SimpleBundleRuntime();
    ~SimpleBundleRuntime();
    
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

#endif /* SimpleBundleRuntime_hpp */
