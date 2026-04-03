#pragma once

#include <string>

#include "../../Common/DecodeRequest.hpp"
#include "../../Common/RepairRequest.hpp"
#include "Decode_Context.hpp"

namespace peanutbutter {

enum class DecodeExecutionStatusV2 {
  kCompleted = 0,
  kFailed = 1,
  kCanceled = 2,
};

struct DecodeExecutionResultV2 {
  DecodeExecutionStatusV2 mStatus = DecodeExecutionStatusV2::kFailed;
  DecodeWorkStateV2 mState{};
  std::string mFailureMessage;

  bool Succeeded() const;
  bool Failed() const;
  bool Canceled() const;
};

DecodeExecutionResultV2 ExecuteDecodeV2(
    const DecodeRequestV2& pRequest,
    DecodeRuntimeV2* pRuntime = nullptr,
    FileSystemV2* pFileSystem = nullptr,
    const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

DecodeExecutionResultV2 ExecuteManifestV2(
    const DecodeRequestV2& pRequest,
    DecodeRuntimeV2* pRuntime = nullptr,
    FileSystemV2* pFileSystem = nullptr,
    const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

DecodeExecutionResultV2 ExecuteRepairV2(
    const RepairRequestV2& pRequest,
    DecodeRuntimeV2* pRuntime = nullptr,
    FileSystemV2* pFileSystem = nullptr,
    const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

}  // namespace peanutbutter
