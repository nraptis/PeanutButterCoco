#include "SectionHeader.hpp"

namespace peanutbutter::memory_layout {
namespace {

bool IsKnownSectionType(std::uint8_t pValue) {
  switch (static_cast<SectionTypeV2>(pValue)) {
    case SectionTypeV2::kArchiveData:
    case SectionTypeV2::kPreviewManifest:
    case SectionTypeV2::kEmptyFolderManifest:
    case SectionTypeV2::kRepairData:
      return true;
  }
  return false;
}

}  // namespace

bool ValidateSectionHeader(const SectionHeaderV2& pHeader,
                           MemoryLayoutErrorInfo* pOutError) {
  ClearMemoryLayoutError(pOutError);

  if (!IsKnownSectionType(pHeader.mSectionType)) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kInvalidSectionType,
                                            "ValidateSectionHeader",
                                            "SectionType",
                                            39u,
                                            kSectionHeaderBytesV2,
                                            kSectionHeaderBytesV2,
                                            pHeader.mSectionType,
                                            static_cast<std::uint64_t>(SectionTypeV2::kRepairData));
    return false;
  }
  if ((pHeader.mSectionFlags & ~kSectionFlagSparsePaddingHasNextRecordV2) != 0u) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kInvalidBooleanByte,
                                            "ValidateSectionHeader",
                                            "SectionFlags",
                                            40u,
                                            kSectionHeaderBytesV2,
                                            kSectionHeaderBytesV2,
                                            pHeader.mSectionFlags,
                                            kSectionFlagSparsePaddingHasNextRecordV2);
    return false;
  }

  return true;
}

bool ReadSectionHeader(const unsigned char* pBytes,
                       std::size_t pByteCount,
                       SectionHeaderV2& pOutHeader,
                       MemoryLayoutErrorInfo* pOutError) {
  pOutHeader = SectionHeaderV2{};
  ClearMemoryLayoutError(pOutError);

  if (pBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "ReadSectionHeader",
                            "bytes",
                            0u,
                            0u,
                            kSectionHeaderBytesV2);
    return false;
  }
  if (pByteCount < kSectionHeaderBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "ReadSectionHeader",
                            "bytes",
                            0u,
                            pByteCount,
                            kSectionHeaderBytesV2);
    return false;
  }

  MemoryLayoutErrorInfo aNestedError;
  if (!ReadCheckSum(pBytes + 0u, kCheckSumBytesV2, pOutHeader.mCheckSum, &aNestedError)) {
    if (pOutError != nullptr) {
      *pOutError = aNestedError;
    }
    return false;
  }
  if (!ReadSkipRecord(pBytes + kCheckSumBytesV2,
                      kSkipRecordBytesV2,
                      pOutHeader.mSkipRecord,
                      &aNestedError)) {
    if (pOutError != nullptr) {
      *pOutError = aNestedError;
      pOutError->mOperation = "ReadSectionHeader";
      pOutError->mOffset += kCheckSumBytesV2;
    }
    return false;
  }

  pOutHeader.mSectionType = pBytes[kCheckSumBytesV2 + kSkipRecordBytesV2];
  pOutHeader.mSectionFlags = pBytes[kCheckSumBytesV2 + kSkipRecordBytesV2 + 1u];
  for (std::size_t aIndex = 0u; aIndex < sizeof(pOutHeader.mReserved); ++aIndex) {
    pOutHeader.mReserved[aIndex] =
        pBytes[kCheckSumBytesV2 + kSkipRecordBytesV2 + 2u + aIndex];
  }

  if (!ReadRepairRecord(pBytes + kCheckSumBytesV2 + kSkipRecordBytesV2 + 17u,
                        kRepairRecordBytesV2,
                        pOutHeader.mRepairRecord,
                        &aNestedError)) {
    if (pOutError != nullptr) {
      *pOutError = aNestedError;
      pOutError->mOperation = "ReadSectionHeader";
      pOutError->mOffset += kCheckSumBytesV2 + kSkipRecordBytesV2 + 17u;
    }
    return false;
  }

  return ValidateSectionHeader(pOutHeader, pOutError);
}

bool WriteSectionHeader(const SectionHeaderV2& pHeader,
                        unsigned char* pOutBytes,
                        std::size_t pByteCount,
                        MemoryLayoutErrorInfo* pOutError) {
  ClearMemoryLayoutError(pOutError);

  if (pOutBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "WriteSectionHeader",
                            "bytes",
                            0u,
                            0u,
                            kSectionHeaderBytesV2);
    return false;
  }
  if (pByteCount < kSectionHeaderBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "WriteSectionHeader",
                            "bytes",
                            0u,
                            pByteCount,
                            kSectionHeaderBytesV2);
    return false;
  }
  if (!ValidateSectionHeader(pHeader, pOutError)) {
    if (pOutError != nullptr) {
      pOutError->mOperation = "WriteSectionHeader";
    }
    return false;
  }

  MemoryLayoutErrorInfo aNestedError;
  if (!WriteCheckSum(pHeader.mCheckSum, pOutBytes + 0u, kCheckSumBytesV2, &aNestedError)) {
    if (pOutError != nullptr) {
      *pOutError = aNestedError;
    }
    return false;
  }
  if (!WriteSkipRecord(pHeader.mSkipRecord,
                       pOutBytes + kCheckSumBytesV2,
                       kSkipRecordBytesV2,
                       &aNestedError)) {
    if (pOutError != nullptr) {
      *pOutError = aNestedError;
      pOutError->mOperation = "WriteSectionHeader";
      pOutError->mOffset += kCheckSumBytesV2;
    }
    return false;
  }

  pOutBytes[kCheckSumBytesV2 + kSkipRecordBytesV2] =
      static_cast<unsigned char>(pHeader.mSectionType);
  pOutBytes[kCheckSumBytesV2 + kSkipRecordBytesV2 + 1u] =
      static_cast<unsigned char>(pHeader.mSectionFlags);
  for (std::size_t aIndex = 0u; aIndex < sizeof(pHeader.mReserved); ++aIndex) {
    pOutBytes[kCheckSumBytesV2 + kSkipRecordBytesV2 + 2u + aIndex] =
        static_cast<unsigned char>(pHeader.mReserved[aIndex]);
  }

  if (!WriteRepairRecord(pHeader.mRepairRecord,
                         pOutBytes + kCheckSumBytesV2 + kSkipRecordBytesV2 + 17u,
                         kRepairRecordBytesV2,
                         &aNestedError)) {
    if (pOutError != nullptr) {
      *pOutError = aNestedError;
      pOutError->mOperation = "WriteSectionHeader";
      pOutError->mOffset += kCheckSumBytesV2 + kSkipRecordBytesV2 + 17u;
    }
    return false;
  }

  return true;
}

}  // namespace peanutbutter::memory_layout
