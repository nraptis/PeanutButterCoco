#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace peanutbutter::memory_layout {

enum class MemoryLayoutErrorCode : std::uint8_t {
  kNone = 0u,
  kNullBuffer = 1u,
  kBufferTooSmall = 2u,
  kMagicMismatch = 3u,
  kUnsupportedVersion = 4u,
  kInvalidBooleanByte = 5u,
  kInvalidDirtyState = 6u,
  kInvalidSectionType = 7u,
  kIntegerOutOfRange = 8u,
  kInvalidArgument = 9u,
};

struct MemoryLayoutErrorInfo {
  MemoryLayoutErrorCode mCode = MemoryLayoutErrorCode::kNone;
  std::string mOperation;
  std::string mFieldName;
  std::size_t mOffset = 0u;
  std::size_t mAvailableBytes = 0u;
  std::size_t mRequiredBytes = 0u;
  std::uint64_t mObservedValue = 0u;
  std::uint64_t mExpectedValue = 0u;
  bool mHasObservedValue = false;
  bool mHasExpectedValue = false;
};

void ClearMemoryLayoutError(MemoryLayoutErrorInfo* pOutError);

void AssignMemoryLayoutError(MemoryLayoutErrorInfo* pOutError,
                             MemoryLayoutErrorCode pCode,
                             const std::string& pOperation,
                             const std::string& pFieldName,
                             std::size_t pOffset,
                             std::size_t pAvailableBytes,
                             std::size_t pRequiredBytes);

void AssignMemoryLayoutObservedExpectedError(
    MemoryLayoutErrorInfo* pOutError,
    MemoryLayoutErrorCode pCode,
    const std::string& pOperation,
    const std::string& pFieldName,
    std::size_t pOffset,
    std::size_t pAvailableBytes,
    std::size_t pRequiredBytes,
    std::uint64_t pObservedValue,
    std::uint64_t pExpectedValue);

std::string DescribeMemoryLayoutError(const MemoryLayoutErrorInfo& pError);

}  // namespace peanutbutter::memory_layout
