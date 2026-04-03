#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "Progress.hpp"

namespace peanutbutter {

enum class FailureFamilyV2 : std::uint8_t {
  kNone = 0u,
  kCanceled = 1u,
  kInput = 2u,
  kStructural = 3u,
  kConflict = 4u,
  kRuntimeIO = 5u,
  kCrypto = 6u,
  kInternal = 7u,
};

inline const char* FailureFamilyLabelV2(FailureFamilyV2 pFamily) {
  switch (pFamily) {
    case FailureFamilyV2::kCanceled:
      return "Canceled";
    case FailureFamilyV2::kInput:
      return "Input";
    case FailureFamilyV2::kStructural:
      return "Structural";
    case FailureFamilyV2::kConflict:
      return "Conflict";
    case FailureFamilyV2::kRuntimeIO:
      return "RuntimeIO";
    case FailureFamilyV2::kCrypto:
      return "Crypto";
    case FailureFamilyV2::kInternal:
      return "Internal";
    case FailureFamilyV2::kNone:
      return "None";
  }
  return "None";
}

struct FailureInfoV2 {
  FailureFamilyV2 mFamily = FailureFamilyV2::kNone;
  ProgressStageV2 mStage = ProgressStageV2::kIdle;
  std::uint64_t mWorkUnit = 0u;
  std::string mMessage;

  bool HasFailure() const {
    return mFamily != FailureFamilyV2::kNone;
  }
};

inline std::string LowerCaseCopyV2(const std::string& pInput) {
  std::string aOut = pInput;
  std::transform(aOut.begin(),
                 aOut.end(),
                 aOut.begin(),
                 [](unsigned char pChar) {
                   return static_cast<char>(std::tolower(pChar));
                 });
  return aOut;
}

inline bool ContainsAnySubstringV2(
    const std::string& pHaystackLower,
    const std::vector<const char*>& pNeedles) {
  for (const char* aNeedle : pNeedles) {
    if (aNeedle == nullptr) {
      continue;
    }
    if (pHaystackLower.find(aNeedle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

inline FailureFamilyV2 InferFailureFamilyV2(ProgressStageV2 pStage,
                                            const std::string& pMessage) {
  const std::string aMessageLower = LowerCaseCopyV2(pMessage);
  if (ContainsAnySubstringV2(aMessageLower,
                             {"cancel", "canceled", "cancelled"})) {
    return FailureFamilyV2::kCanceled;
  }

  if (ContainsAnySubstringV2(aMessageLower,
                             {"decrypt",
                              "decryption",
                              "encrypt",
                              "encryption",
                              "cipher",
                              "password",
                              "seal",
                              "unseal"})) {
    return FailureFamilyV2::kCrypto;
  }

  if (ContainsAnySubstringV2(aMessageLower,
                             {"collision",
                              "conflict",
                              "already exists",
                              "overwrite",
                              "rename",
                              "would collide"})) {
    return FailureFamilyV2::kConflict;
  }

  if (ContainsAnySubstringV2(aMessageLower,
                             {"checksum",
                              "header",
                              "pointer",
                              "section",
                              "archive format",
                              "family id",
                              "out of range",
                              "validation failed"})) {
    return FailureFamilyV2::kStructural;
  }

  if (ContainsAnySubstringV2(aMessageLower,
                             {"open",
                              "read",
                              "write",
                              "close",
                              "stream",
                              "directory",
                              "file",
                              "filesystem",
                              "seek",
                              "create"})) {
    return FailureFamilyV2::kRuntimeIO;
  }

  if (pStage == ProgressStageV2::kPreflight ||
      ContainsAnySubstringV2(aMessageLower,
                             {"required",
                              "path is empty",
                              "destination directory is empty",
                              "source path is empty",
                              "invalid request"})) {
    return FailureFamilyV2::kInput;
  }

  return FailureFamilyV2::kInternal;
}

}  // namespace peanutbutter

