#include "MemoryLayoutError.hpp"

#include <sstream>

namespace peanutbutter::memory_layout {

void ClearMemoryLayoutError(MemoryLayoutErrorInfo* pOutError) {
  if (pOutError == nullptr) {
    return;
  }
  *pOutError = MemoryLayoutErrorInfo{};
}

void AssignMemoryLayoutError(MemoryLayoutErrorInfo* pOutError,
                             MemoryLayoutErrorCode pCode,
                             const std::string& pOperation,
                             const std::string& pFieldName,
                             std::size_t pOffset,
                             std::size_t pAvailableBytes,
                             std::size_t pRequiredBytes) {
  if (pOutError == nullptr) {
    return;
  }

  pOutError->mCode = pCode;
  pOutError->mOperation = pOperation;
  pOutError->mFieldName = pFieldName;
  pOutError->mOffset = pOffset;
  pOutError->mAvailableBytes = pAvailableBytes;
  pOutError->mRequiredBytes = pRequiredBytes;
  pOutError->mObservedValue = 0u;
  pOutError->mExpectedValue = 0u;
  pOutError->mHasObservedValue = false;
  pOutError->mHasExpectedValue = false;
}

void AssignMemoryLayoutObservedExpectedError(
    MemoryLayoutErrorInfo* pOutError,
    MemoryLayoutErrorCode pCode,
    const std::string& pOperation,
    const std::string& pFieldName,
    std::size_t pOffset,
    std::size_t pAvailableBytes,
    std::size_t pRequiredBytes,
    std::uint64_t pObservedValue,
    std::uint64_t pExpectedValue) {
  AssignMemoryLayoutError(pOutError,
                          pCode,
                          pOperation,
                          pFieldName,
                          pOffset,
                          pAvailableBytes,
                          pRequiredBytes);
  if (pOutError == nullptr) {
    return;
  }

  pOutError->mObservedValue = pObservedValue;
  pOutError->mExpectedValue = pExpectedValue;
  pOutError->mHasObservedValue = true;
  pOutError->mHasExpectedValue = true;
}

std::string DescribeMemoryLayoutError(const MemoryLayoutErrorInfo& pError) {
  if (pError.mCode == MemoryLayoutErrorCode::kNone) {
    return "No memory-layout error.";
  }

  std::ostringstream aOut;
  aOut << "[MemoryLayout]";
  if (!pError.mOperation.empty()) {
    aOut << "[" << pError.mOperation << "]";
  }
  if (!pError.mFieldName.empty()) {
    aOut << "[" << pError.mFieldName << "]";
  }
  aOut << " ";

  switch (pError.mCode) {
    case MemoryLayoutErrorCode::kNone:
      aOut << "No error.";
      break;
    case MemoryLayoutErrorCode::kNullBuffer:
      aOut << "Received a null buffer.";
      break;
    case MemoryLayoutErrorCode::kBufferTooSmall:
      aOut << "Buffer too small.";
      break;
    case MemoryLayoutErrorCode::kMagicMismatch:
      aOut << "Magic value mismatch.";
      break;
    case MemoryLayoutErrorCode::kUnsupportedVersion:
      aOut << "Unsupported version.";
      break;
    case MemoryLayoutErrorCode::kInvalidBooleanByte:
      aOut << "Invalid boolean byte.";
      break;
    case MemoryLayoutErrorCode::kInvalidDirtyState:
      aOut << "Invalid dirty state.";
      break;
    case MemoryLayoutErrorCode::kInvalidSectionType:
      aOut << "Invalid section type.";
      break;
    case MemoryLayoutErrorCode::kIntegerOutOfRange:
      aOut << "Integer value is out of range for the packed field.";
      break;
    case MemoryLayoutErrorCode::kInvalidArgument:
      aOut << "Invalid argument.";
      break;
  }

  aOut << " Offset=" << pError.mOffset
       << ", available=" << pError.mAvailableBytes
       << ", required=" << pError.mRequiredBytes;

  if (pError.mHasObservedValue) {
    aOut << ", observed=" << pError.mObservedValue;
  }
  if (pError.mHasExpectedValue) {
    aOut << ", expected=" << pError.mExpectedValue;
  }

  return aOut.str();
}

}  // namespace peanutbutter::memory_layout
