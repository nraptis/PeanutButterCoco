#pragma once

#include <cstddef>

#include "../../Common/DecodeRequest.hpp"
#include "../../Common/RepairRequest.hpp"
#include "../Decode/Decode_Workflow.hpp"

namespace peanutbutter::repair_workflow {

using RepairPhaseEntryV2 = decode_workflow::DecodePhaseEntryV2;
using RepairPhaseListViewV2 = decode_workflow::DecodePhaseListViewV2;

inline DecodeRequestV2 MakeRepairDecodeRequestV2(const RepairRequestV2& pRequest) {
  DecodeRequestV2 aDecodeRequest;
  aDecodeRequest.mSourcePath = pRequest.mSourcePath;
  aDecodeRequest.mDestinationDirectory = pRequest.mDestinationDirectory;
  aDecodeRequest.mEncryptionEnabled = pRequest.mEncryptionEnabled;
  aDecodeRequest.mAggressive = pRequest.mAggressive;
  aDecodeRequest.mCancelFinishBlocks = pRequest.mCancelFinishBlocks;
  aDecodeRequest.mPassword = pRequest.mPassword;
  aDecodeRequest.mIntent = DecodeIntentV2::kRepair;
  return aDecodeRequest;
}

inline RepairPhaseListViewV2 BuildRepairPhaseListV2() {
  return decode_workflow::BuildDecodePhaseListV2(
      decode_workflow::DecodePhasePlanV2::kRepair);
}

inline bool RunRepairPhaseV2(DecodeStageContextV2& pContext,
                             const RepairPhaseEntryV2& pPhase,
                             std::size_t pPhaseIndex,
                             std::size_t pPhaseCount) {
  return decode_workflow::RunDecodePhaseV2(
      pContext, pPhase, pPhaseIndex, pPhaseCount, LogActionV2::kRepair);
}

inline bool ShouldDeferRepairCancelForPhaseV2(
    const DecodeWorkStateV2& pState,
    ProgressStageV2 pStage) {
  return decode_workflow::ShouldDeferDecodeCancelForPhaseV2(
      pState, pStage, ProgressStageV2::kRepairApply);
}

inline std::size_t FindRepairPhaseIndexV2(RepairPhaseListViewV2 pPhases,
                                          ProgressStageV2 pStage) {
  return decode_workflow::FindDecodePhaseIndexV2(pPhases, pStage);
}

}  // namespace peanutbutter::repair_workflow
