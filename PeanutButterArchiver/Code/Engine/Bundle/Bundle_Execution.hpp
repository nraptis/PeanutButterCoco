#pragma once

#include <string>
#include <vector>

#include "Bundle_Context.hpp"

namespace peanutbutter {

enum class BundleExecutionStatusV2 {
  kCompleted = 0,
  kFailed = 1,
  kCanceled = 2,
};

struct BundleExecutionResultV2 {
  BundleExecutionStatusV2 mStatus = BundleExecutionStatusV2::kFailed;
  BundleWorkStateV2 mState{};
  std::string mFailureMessage;

  bool Succeeded() const;
  bool Failed() const;
  bool Canceled() const;
  const std::vector<std::string>& ArchivePaths() const;
};

BundleExecutionResultV2 ExecuteBundleV2(BundleStageContextV2& pContext);

BundleExecutionResultV2 ExecuteBundleV2(
    const BundleRequestV2& pRequest,
    BundleRuntimeV2* pRuntime = nullptr,
    FileSystemV2* pFileSystem = nullptr,
    const memory_layout::ArchiveLayoutConfigV2* pLayout = nullptr);

}  // namespace peanutbutter
