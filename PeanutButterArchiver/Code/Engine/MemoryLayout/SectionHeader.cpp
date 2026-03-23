#include "SectionHeader.hpp"

#include "FormatUtilities.hpp"

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
                                            47u,
                                            kSectionHeaderBytesV2,
                                            kSectionHeaderBytesV2,
                                            pHeader.mSectionType,
                                            static_cast<std::uint64_t>(SectionTypeV2::kRepairData));
    return false;
  }
  if (pHeader.mSectionFlags != 0u) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kInvalidBooleanByte,
                                            "ValidateSectionHeader",
                                            "SectionFlags",
                                            48u,
                                            kSectionHeaderBytesV2,
                                            kSectionHeaderBytesV2,
                                            pHeader.mSectionFlags,
                                            0u);
    return false;
  }
  if (pHeader.mPayloadBytesUsed > 0u &&
      pHeader.mPayloadBytesUsed > static_cast<std::uint32_t>(kSectionPayloadBytesV2)) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kIntegerOutOfRange,
                                            "ValidateSectionHeader",
                                            "PayloadBytesUsed",
                                            49u,
                                            kSectionHeaderBytesV2,
                                            kSectionHeaderBytesV2,
                                            pHeader.mPayloadBytesUsed,
                                            kSectionPayloadBytesV2);
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

  if (!ReadRepairRecord(pBytes + kCheckSumBytesV2 + kSkipRecordBytesV2,
                        kRepairRecordBytesV2,
                        pOutHeader.mRepairRecord,
                        &aNestedError)) {
    if (pOutError != nullptr) {
      *pOutError = aNestedError;
      pOutError->mOperation = "ReadSectionHeader";
      pOutError->mOffset += kCheckSumBytesV2 + kSkipRecordBytesV2;
    }
    return false;
  }

  constexpr std::size_t kMetadataOffset =
      kCheckSumBytesV2 + kSkipRecordBytesV2 + kRepairRecordBytesV2;
  pOutHeader.mSectionType = pBytes[kMetadataOffset + 0u];
  pOutHeader.mSectionFlags = pBytes[kMetadataOffset + 1u];
  pOutHeader.mPayloadBytesUsed = ReadUint32LE(pBytes + kMetadataOffset + 2u);
  pOutHeader.mArchiveFileCount = ReadUint32LE(pBytes + kMetadataOffset + 6u);
  pOutHeader.mArchiveBlockCount = ReadUint32LE(pBytes + kMetadataOffset + 10u);
  pOutHeader.mArchiveIndex = ReadUint32LE(pBytes + kMetadataOffset + 14u);
  pOutHeader.mBlockIndex = ReadUint32LE(pBytes + kMetadataOffset + 18u);
  pOutHeader.mArchiveDataBlockCount = ReadUint32LE(pBytes + kMetadataOffset + 22u);
  pOutHeader.mPreviewManifestBlockCount = ReadUint32LE(pBytes + kMetadataOffset + 26u);
  pOutHeader.mFolderManifestBlockCount = ReadUint32LE(pBytes + kMetadataOffset + 30u);
  pOutHeader.mRepairDataBlockCount = ReadUint32LE(pBytes + kMetadataOffset + 34u);
  pOutHeader.mArchiveFamilyId = ReadUint64LE(pBytes + kMetadataOffset + 38u);
  for (std::size_t aIndex = 0u; aIndex < sizeof(pOutHeader.mReserved); ++aIndex) {
    pOutHeader.mReserved[aIndex] = pBytes[kMetadataOffset + 46u + aIndex];
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

  if (!WriteRepairRecord(pHeader.mRepairRecord,
                         pOutBytes + kCheckSumBytesV2 + kSkipRecordBytesV2,
                         kRepairRecordBytesV2,
                         &aNestedError)) {
    if (pOutError != nullptr) {
      *pOutError = aNestedError;
      pOutError->mOperation = "WriteSectionHeader";
      pOutError->mOffset += kCheckSumBytesV2 + kSkipRecordBytesV2;
    }
    return false;
  }

  constexpr std::size_t kMetadataOffset =
      kCheckSumBytesV2 + kSkipRecordBytesV2 + kRepairRecordBytesV2;
  pOutBytes[kMetadataOffset + 0u] = static_cast<unsigned char>(pHeader.mSectionType);
  pOutBytes[kMetadataOffset + 1u] = static_cast<unsigned char>(pHeader.mSectionFlags);
  WriteUint32LE(pHeader.mPayloadBytesUsed, pOutBytes + kMetadataOffset + 2u);
  WriteUint32LE(pHeader.mArchiveFileCount, pOutBytes + kMetadataOffset + 6u);
  WriteUint32LE(pHeader.mArchiveBlockCount, pOutBytes + kMetadataOffset + 10u);
  WriteUint32LE(pHeader.mArchiveIndex, pOutBytes + kMetadataOffset + 14u);
  WriteUint32LE(pHeader.mBlockIndex, pOutBytes + kMetadataOffset + 18u);
  WriteUint32LE(pHeader.mArchiveDataBlockCount, pOutBytes + kMetadataOffset + 22u);
  WriteUint32LE(pHeader.mPreviewManifestBlockCount, pOutBytes + kMetadataOffset + 26u);
  WriteUint32LE(pHeader.mFolderManifestBlockCount, pOutBytes + kMetadataOffset + 30u);
  WriteUint32LE(pHeader.mRepairDataBlockCount, pOutBytes + kMetadataOffset + 34u);
  WriteUint64LE(pHeader.mArchiveFamilyId, pOutBytes + kMetadataOffset + 38u);
  for (std::size_t aIndex = 0u; aIndex < sizeof(pHeader.mReserved); ++aIndex) {
    pOutBytes[kMetadataOffset + 46u + aIndex] =
        static_cast<unsigned char>(pHeader.mReserved[aIndex]);
  }

  return true;
}

}  // namespace peanutbutter::memory_layout
