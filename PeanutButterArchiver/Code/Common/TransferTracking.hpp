#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

#include "RuntimeEvent.hpp"

namespace peanutbutter {

struct TransferTrackingStateV2 {
  std::vector<std::string> mArchiveListStarted;
  std::vector<std::string> mArchiveListCompleted;
  std::vector<std::string> mFileListStarted;
  std::vector<std::string> mFileListCompleted;
  std::unordered_set<std::string> mArchiveSetStarted;
  std::unordered_set<std::string> mArchiveSetCompleted;
  std::unordered_set<std::string> mFileSetStarted;
  std::unordered_set<std::string> mFileSetCompleted;
};

inline void AppendTransferListUniqueV2(std::vector<std::string>& pList,
                                       std::unordered_set<std::string>& pSet,
                                       const std::string& pValue) {
  if (pValue.empty()) {
    return;
  }

  const auto aInserted = pSet.insert(pValue);
  if (!aInserted.second) {
    return;
  }
  pList.push_back(*aInserted.first);
}

inline std::string RuntimeEventArchiveReferenceV2(const RuntimeEventV2& pEvent) {
  std::string aReference = pEvent.FetchInfo("archive_path");
  if (aReference.empty()) {
    aReference = pEvent.FetchInfo("path");
  }
  if (aReference.empty()) {
    const std::string aArchiveIndex = pEvent.FetchInfo("archive_index");
    if (!aArchiveIndex.empty()) {
      aReference = "archive#" + aArchiveIndex;
    }
  }
  return aReference;
}

inline std::string RuntimeEventFileReferenceV2(const RuntimeEventV2& pEvent) {
  std::string aReference = pEvent.FetchInfo("output_path");
  if (aReference.empty()) {
    aReference = pEvent.FetchInfo("relative_path");
  }
  if (aReference.empty()) {
    aReference = pEvent.FetchInfo("file_name");
  }
  if (aReference.empty()) {
    aReference = pEvent.FetchInfo("path");
  }
  return aReference;
}

inline void TrackRuntimeEventTransferV2(const RuntimeEventV2& pEvent,
                                        TransferTrackingStateV2& pTransfers) {
  switch (pEvent.mKind) {
    case RuntimeEventKindV2::kBundleArchiveStarted:
    case RuntimeEventKindV2::kDecodeArchiveStarted:
    case RuntimeEventKindV2::kRepairArchiveStarted:
      AppendTransferListUniqueV2(pTransfers.mArchiveListStarted,
                                 pTransfers.mArchiveSetStarted,
                                 RuntimeEventArchiveReferenceV2(pEvent));
      return;
    case RuntimeEventKindV2::kBundleArchiveFinished:
    case RuntimeEventKindV2::kDecodeArchiveFinished:
    case RuntimeEventKindV2::kRepairArchiveFinished:
      AppendTransferListUniqueV2(pTransfers.mArchiveListCompleted,
                                 pTransfers.mArchiveSetCompleted,
                                 RuntimeEventArchiveReferenceV2(pEvent));
      return;
    case RuntimeEventKindV2::kBundleFileStarted:
    case RuntimeEventKindV2::kDecodeFileStarted:
      AppendTransferListUniqueV2(pTransfers.mFileListStarted,
                                 pTransfers.mFileSetStarted,
                                 RuntimeEventFileReferenceV2(pEvent));
      return;
    case RuntimeEventKindV2::kBundleFileFinished:
    case RuntimeEventKindV2::kDecodeFileFinished:
      AppendTransferListUniqueV2(pTransfers.mFileListCompleted,
                                 pTransfers.mFileSetCompleted,
                                 RuntimeEventFileReferenceV2(pEvent));
      return;
    default:
      return;
  }
}

inline bool RuntimeEventInfoIsSensitiveKeyV2(const std::string& pKey) {
  std::string aLower;
  aLower.reserve(pKey.size());
  for (const char aChar : pKey) {
    aLower.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(aChar))));
  }
  return aLower.find("path") != std::string::npos ||
         aLower.find("file") != std::string::npos ||
         aLower.find("name") != std::string::npos;
}

inline void ScrubRuntimeEventFileInfoV2(RuntimeEventV2& pEvent) {
  pEvent.mInfo.erase(
      std::remove_if(
          pEvent.mInfo.begin(),
          pEvent.mInfo.end(),
          [](const RuntimeEventInfoV2& pPair) {
            return RuntimeEventInfoIsSensitiveKeyV2(pPair.mKey);
          }),
      pEvent.mInfo.end());
  pEvent.mLabel = RuntimeEventKindLabelV2(pEvent.mKind);
}

}  // namespace peanutbutter
