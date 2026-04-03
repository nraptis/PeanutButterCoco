#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Progress.hpp"

namespace peanutbutter {

enum class RuntimeEventKindV2 {
  kUnknown = 0,

  kBundleArchiveStarted = 1,
  kBundleBlockStarted = 2,
  kBundleBlockFinished = 3,
  kBundleFileStarted = 4,
  kBundleFileFinished = 5,
  kBundleEncryptionFinished = 6,
  kBundleFolderStarted = 7,
  kBundleFolderFinished = 8,
  kBundleManifestItemStarted = 9,
  kBundleManifestItemFinished = 10,
  kBundleRepairBlockStarted = 11,
  kBundleRepairBlockFinished = 12,

  kDecodeArchiveStarted = 13,
  kDecodeBlockStarted = 14,
  kDecodeBlockFinished = 15,
  kDecodeFileStarted = 16,
  kDecodeFileFinished = 17,
  kDecodeDecryptionFinished = 18,
  kDecodeError = 19,
  kDecodeSkipJump = 20,
  kDecodeFolderStarted = 21,
  kDecodeFolderFinished = 22,
  kDecodeManifestItemStarted = 23,
  kDecodeManifestItemFinished = 24,

  kRepairBlockStarted = 25,
  kRepairBlockFinished = 26,
  kRepairFileCreated = 27,
  kRepairFileResized = 28,
  kRepairBlockMatched = 29,
  kRepairBlockUnmatched = 30,
  kBundleArchiveFinished = 31,
  kBundleArchiveHeaderWritten = 32,
  kBundleArchiveHeaderFinalized = 33,
  kBundleBlockHeaderWritten = 34,
  kBundleDiscoveryItemScanned = 35,
  kDecodeArchiveFinished = 36,
  kDecodeArchiveHeaderRead = 37,
  kDecodeBlockHeaderRead = 38,
  kDecodeDecryptionStarted = 39,
  kDecodeDiscoveryArchiveScanned = 40,
  kDecodeInspectionBlockScanned = 41,
  kRepairArchiveStarted = 42,
  kRepairArchiveFinished = 43,
  kRepairArchiveHeaderWritten = 44,
};

struct RuntimeEventInfoV2 {
  std::string mKey;
  std::string mValue;
};

struct RuntimeEventV2 {
  RuntimeEventKindV2 mKind = RuntimeEventKindV2::kUnknown;
  ProgressStageV2 mStage = ProgressStageV2::kIdle;
  std::string mLabel;
  std::vector<RuntimeEventInfoV2> mInfo;

  std::string FetchInfo(const std::string& pKey) const {
    for (const RuntimeEventInfoV2& aPair : mInfo) {
      if (aPair.mKey == pKey) {
        return aPair.mValue;
      }
    }
    return {};
  }

  void SetInfo(const std::string& pKey, const std::string& pValue) {
    for (RuntimeEventInfoV2& aPair : mInfo) {
      if (aPair.mKey == pKey) {
        aPair.mValue = pValue;
        return;
      }
    }
    mInfo.push_back({pKey, pValue});
  }

  void SetInfo(const std::string& pKey, const char* pValue) {
    SetInfo(pKey, pValue == nullptr ? std::string() : std::string(pValue));
  }

  void SetInfo(const std::string& pKey, std::uint64_t pValue) {
    SetInfo(pKey, std::to_string(pValue));
  }

  void SetInfo(const std::string& pKey, bool pValue) {
    SetInfo(pKey, pValue ? "true" : "false");
  }
};

inline const char* RuntimeEventKindLabelV2(RuntimeEventKindV2 pKind) {
  switch (pKind) {
    case RuntimeEventKindV2::kBundleArchiveStarted:
      return "BundleArchiveStarted";
    case RuntimeEventKindV2::kBundleBlockStarted:
      return "BundleBlockStarted";
    case RuntimeEventKindV2::kBundleBlockFinished:
      return "BundleBlockFinished";
    case RuntimeEventKindV2::kBundleFileStarted:
      return "BundleFileStarted";
    case RuntimeEventKindV2::kBundleFileFinished:
      return "BundleFileFinished";
    case RuntimeEventKindV2::kBundleEncryptionFinished:
      return "BundleEncryptionFinished";
    case RuntimeEventKindV2::kBundleFolderStarted:
      return "BundleFolderStarted";
    case RuntimeEventKindV2::kBundleFolderFinished:
      return "BundleFolderFinished";
    case RuntimeEventKindV2::kBundleManifestItemStarted:
      return "BundleManifestItemStarted";
    case RuntimeEventKindV2::kBundleManifestItemFinished:
      return "BundleManifestItemFinished";
    case RuntimeEventKindV2::kBundleRepairBlockStarted:
      return "BundleRepairBlockStarted";
    case RuntimeEventKindV2::kBundleRepairBlockFinished:
      return "BundleRepairBlockFinished";
    case RuntimeEventKindV2::kDecodeArchiveStarted:
      return "DecodeArchiveStarted";
    case RuntimeEventKindV2::kDecodeBlockStarted:
      return "DecodeBlockStarted";
    case RuntimeEventKindV2::kDecodeBlockFinished:
      return "DecodeBlockFinished";
    case RuntimeEventKindV2::kDecodeFileStarted:
      return "DecodeFileStarted";
    case RuntimeEventKindV2::kDecodeFileFinished:
      return "DecodeFileFinished";
    case RuntimeEventKindV2::kDecodeDecryptionFinished:
      return "DecodeDecryptionFinished";
    case RuntimeEventKindV2::kDecodeError:
      return "DecodeError";
    case RuntimeEventKindV2::kDecodeSkipJump:
      return "DecodeSkipJump";
    case RuntimeEventKindV2::kDecodeFolderStarted:
      return "DecodeFolderStarted";
    case RuntimeEventKindV2::kDecodeFolderFinished:
      return "DecodeFolderFinished";
    case RuntimeEventKindV2::kDecodeManifestItemStarted:
      return "DecodeManifestItemStarted";
    case RuntimeEventKindV2::kDecodeManifestItemFinished:
      return "DecodeManifestItemFinished";
    case RuntimeEventKindV2::kRepairBlockStarted:
      return "RepairBlockStarted";
    case RuntimeEventKindV2::kRepairBlockFinished:
      return "RepairBlockFinished";
    case RuntimeEventKindV2::kRepairFileCreated:
      return "RepairFileCreated";
    case RuntimeEventKindV2::kRepairFileResized:
      return "RepairFileResized";
    case RuntimeEventKindV2::kRepairBlockMatched:
      return "RepairBlockMatched";
    case RuntimeEventKindV2::kRepairBlockUnmatched:
      return "RepairBlockUnmatched";
    case RuntimeEventKindV2::kBundleArchiveFinished:
      return "BundleArchiveFinished";
    case RuntimeEventKindV2::kBundleArchiveHeaderWritten:
      return "BundleArchiveHeaderWritten";
    case RuntimeEventKindV2::kBundleArchiveHeaderFinalized:
      return "BundleArchiveHeaderFinalized";
    case RuntimeEventKindV2::kBundleBlockHeaderWritten:
      return "BundleBlockHeaderWritten";
    case RuntimeEventKindV2::kBundleDiscoveryItemScanned:
      return "BundleDiscoveryItemScanned";
    case RuntimeEventKindV2::kDecodeArchiveFinished:
      return "DecodeArchiveFinished";
    case RuntimeEventKindV2::kDecodeArchiveHeaderRead:
      return "DecodeArchiveHeaderRead";
    case RuntimeEventKindV2::kDecodeBlockHeaderRead:
      return "DecodeBlockHeaderRead";
    case RuntimeEventKindV2::kDecodeDecryptionStarted:
      return "DecodeDecryptionStarted";
    case RuntimeEventKindV2::kDecodeDiscoveryArchiveScanned:
      return "DecodeDiscoveryArchiveScanned";
    case RuntimeEventKindV2::kDecodeInspectionBlockScanned:
      return "DecodeInspectionBlockScanned";
    case RuntimeEventKindV2::kRepairArchiveStarted:
      return "RepairArchiveStarted";
    case RuntimeEventKindV2::kRepairArchiveFinished:
      return "RepairArchiveFinished";
    case RuntimeEventKindV2::kRepairArchiveHeaderWritten:
      return "RepairArchiveHeaderWritten";
    case RuntimeEventKindV2::kUnknown:
      return "Unknown";
  }
  return "Unknown";
}

inline bool RuntimeEventKindIsVerboseV2(RuntimeEventKindV2 pKind) {
  switch (pKind) {
    case RuntimeEventKindV2::kBundleBlockStarted:
    case RuntimeEventKindV2::kBundleBlockFinished:
    case RuntimeEventKindV2::kBundleEncryptionFinished:
    case RuntimeEventKindV2::kBundleRepairBlockStarted:
    case RuntimeEventKindV2::kBundleRepairBlockFinished:
    case RuntimeEventKindV2::kBundleBlockHeaderWritten:
    case RuntimeEventKindV2::kBundleDiscoveryItemScanned:
    case RuntimeEventKindV2::kDecodeBlockStarted:
    case RuntimeEventKindV2::kDecodeBlockFinished:
    case RuntimeEventKindV2::kDecodeBlockHeaderRead:
    case RuntimeEventKindV2::kDecodeDecryptionStarted:
    case RuntimeEventKindV2::kDecodeDecryptionFinished:
    case RuntimeEventKindV2::kDecodeDiscoveryArchiveScanned:
    case RuntimeEventKindV2::kDecodeInspectionBlockScanned:
    case RuntimeEventKindV2::kRepairBlockStarted:
    case RuntimeEventKindV2::kRepairBlockFinished:
    case RuntimeEventKindV2::kRepairBlockMatched:
    case RuntimeEventKindV2::kRepairBlockUnmatched:
      return true;
    case RuntimeEventKindV2::kUnknown:
    case RuntimeEventKindV2::kBundleArchiveStarted:
    case RuntimeEventKindV2::kBundleFileStarted:
    case RuntimeEventKindV2::kBundleFileFinished:
    case RuntimeEventKindV2::kBundleFolderStarted:
    case RuntimeEventKindV2::kBundleFolderFinished:
    case RuntimeEventKindV2::kBundleManifestItemStarted:
    case RuntimeEventKindV2::kBundleManifestItemFinished:
    case RuntimeEventKindV2::kBundleArchiveFinished:
    case RuntimeEventKindV2::kBundleArchiveHeaderWritten:
    case RuntimeEventKindV2::kBundleArchiveHeaderFinalized:
    case RuntimeEventKindV2::kDecodeArchiveStarted:
    case RuntimeEventKindV2::kDecodeArchiveFinished:
    case RuntimeEventKindV2::kDecodeArchiveHeaderRead:
    case RuntimeEventKindV2::kDecodeFileStarted:
    case RuntimeEventKindV2::kDecodeFileFinished:
    case RuntimeEventKindV2::kDecodeError:
    case RuntimeEventKindV2::kDecodeSkipJump:
    case RuntimeEventKindV2::kDecodeFolderStarted:
    case RuntimeEventKindV2::kDecodeFolderFinished:
    case RuntimeEventKindV2::kDecodeManifestItemStarted:
    case RuntimeEventKindV2::kDecodeManifestItemFinished:
    case RuntimeEventKindV2::kRepairArchiveStarted:
    case RuntimeEventKindV2::kRepairArchiveFinished:
    case RuntimeEventKindV2::kRepairArchiveHeaderWritten:
    case RuntimeEventKindV2::kRepairFileCreated:
    case RuntimeEventKindV2::kRepairFileResized:
      return false;
  }
  return false;
}

}  // namespace peanutbutter
