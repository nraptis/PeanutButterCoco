#include "SectionHeader.hpp"

#include <cstring>

#include "FormatUtilities.hpp"

namespace peanutbutter::memory_layout {
namespace {

bool IsKnownSectionType(std::uint8_t pValue) {
  switch (static_cast<SectionTypeV2>(pValue)) {
    case SectionTypeV2::kArchiveData:
    case SectionTypeV2::kPreviewManifest:
    case SectionTypeV2::kRepairData:
      return true;
  }
  return false;
}

}  // namespace

bool ValidateSectionHeader(const SectionHeaderV2& pHeader,
                           MemoryLayoutErrorInfo* pOutError) {
  ClearMemoryLayoutError(pOutError);

  constexpr std::uint64_t kMetadataOffset = static_cast<std::uint64_t>(
      kCheckSumBytesV2 + kSkipRecordBytesV2 + kRepairRecordBytesV2);
  if (pHeader.mCheckSumKind != specs_verified::kSectionCheckSumKindSha256V2) {
    AssignMemoryLayoutObservedExpectedError(
        pOutError,
        MemoryLayoutErrorCode::kIntegerOutOfRange,
        "ValidateSectionHeader",
        "CheckSumKind",
        kMetadataOffset + 0u,
        kSectionHeaderBytesV2,
        kSectionHeaderBytesV2,
        pHeader.mCheckSumKind,
        specs_verified::kSectionCheckSumKindSha256V2);
    return false;
  }

  if (!IsKnownSectionType(pHeader.mSectionType)) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kInvalidSectionType,
                                            "ValidateSectionHeader",
                                            "SectionType",
                                            kMetadataOffset + 1u,
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
                                            kMetadataOffset + 2u,
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
                                            kMetadataOffset + 3u,
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
  pOutHeader.mCheckSumKind = pBytes[kMetadataOffset + 0u];
  pOutHeader.mSectionType = pBytes[kMetadataOffset + 1u];
  pOutHeader.mSectionFlags = pBytes[kMetadataOffset + 2u];
  pOutHeader.mPayloadBytesUsed = ReadUint32LE(pBytes + kMetadataOffset + 3u);
  pOutHeader.mArchiveFileCount = ReadUint32LE(pBytes + kMetadataOffset + 7u);
  pOutHeader.mArchiveBlockCount = ReadUint32LE(pBytes + kMetadataOffset + 11u);
  pOutHeader.mArchiveIndex = ReadUint32LE(pBytes + kMetadataOffset + 15u);
  pOutHeader.mBlockIndex = ReadUint32LE(pBytes + kMetadataOffset + 19u);
  std::memcpy(
      pOutHeader.mBlockCountMain.mBytes.data(), pBytes + kMetadataOffset + 23u, kPackedUint48BytesV2);
  std::memcpy(
      pOutHeader.mBlockCountPreview.mBytes.data(), pBytes + kMetadataOffset + 29u, kPackedUint48BytesV2);
  std::memcpy(
      pOutHeader.mBlockCountRepair.mBytes.data(), pBytes + kMetadataOffset + 35u, kPackedUint48BytesV2);
  pOutHeader.mArchiveFamilyId = ReadUint64LE(pBytes + kMetadataOffset + 41u);
  std::memcpy(
      pOutHeader.mReserved, pBytes + kMetadataOffset + 49u, sizeof(pOutHeader.mReserved));

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
  pOutBytes[kMetadataOffset + 0u] = static_cast<unsigned char>(pHeader.mCheckSumKind);
  pOutBytes[kMetadataOffset + 1u] = static_cast<unsigned char>(pHeader.mSectionType);
  pOutBytes[kMetadataOffset + 2u] = static_cast<unsigned char>(pHeader.mSectionFlags);
  WriteUint32LE(pHeader.mPayloadBytesUsed, pOutBytes + kMetadataOffset + 3u);
  WriteUint32LE(pHeader.mArchiveFileCount, pOutBytes + kMetadataOffset + 7u);
  WriteUint32LE(pHeader.mArchiveBlockCount, pOutBytes + kMetadataOffset + 11u);
  WriteUint32LE(pHeader.mArchiveIndex, pOutBytes + kMetadataOffset + 15u);
  WriteUint32LE(pHeader.mBlockIndex, pOutBytes + kMetadataOffset + 19u);
  std::memcpy(
      pOutBytes + kMetadataOffset + 23u, pHeader.mBlockCountMain.mBytes.data(), kPackedUint48BytesV2);
  std::memcpy(
      pOutBytes + kMetadataOffset + 29u, pHeader.mBlockCountPreview.mBytes.data(), kPackedUint48BytesV2);
  std::memcpy(
      pOutBytes + kMetadataOffset + 35u, pHeader.mBlockCountRepair.mBytes.data(), kPackedUint48BytesV2);
  WriteUint64LE(pHeader.mArchiveFamilyId, pOutBytes + kMetadataOffset + 41u);
  std::memcpy(pOutBytes + kMetadataOffset + 49u,
              pHeader.mReserved,
              sizeof(pHeader.mReserved));

  return true;
}

}  // namespace peanutbutter::memory_layout
