#include "ArchiveHeader.hpp"

#include <cstring>

namespace peanutbutter::memory_layout {
namespace {

bool IsKnownDirtyState(std::uint8_t pValue) {
  switch (static_cast<ArchiveDirtyStateV2>(pValue)) {
    case ArchiveDirtyStateV2::kInvalid:
    case ArchiveDirtyStateV2::kFinishedWithCancel:
    case ArchiveDirtyStateV2::kFinishedWithError:
    case ArchiveDirtyStateV2::kFinishedWithCancelAndError:
    case ArchiveDirtyStateV2::kFinished:
      return true;
  }
  return false;
}

}  // namespace

bool ValidateArchiveHeader(const ArchiveHeaderV2& pHeader,
                           MemoryLayoutErrorInfo* pOutError) {
  ClearMemoryLayoutError(pOutError);

  if (pHeader.mMagic != kArchiveMagicV2) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kMagicMismatch,
                                            "ValidateArchiveHeader",
                                            "Magic",
                                            0u,
                                            kArchiveHeaderBytesV2,
                                            kArchiveHeaderBytesV2,
                                            pHeader.mMagic,
                                            kArchiveMagicV2);
    return false;
  }
  if (pHeader.mArchiveFormatVersion == 0u) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kUnsupportedVersion,
                                            "ValidateArchiveHeader",
                                            "ArchiveFormatVersion",
                                            8u,
                                            kArchiveHeaderBytesV2,
                                            kArchiveHeaderBytesV2,
                                            pHeader.mArchiveFormatVersion,
                                            2u);
    return false;
  }
  if (pHeader.mIsEncrypted > 1u) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kInvalidBooleanByte,
                                            "ValidateArchiveHeader",
                                            "IsEncrypted",
                                            12u,
                                            kArchiveHeaderBytesV2,
                                            kArchiveHeaderBytesV2,
                                            pHeader.mIsEncrypted,
                                            1u);
    return false;
  }
  if (!IsKnownDirtyState(pHeader.mDirtyState)) {
    AssignMemoryLayoutObservedExpectedError(pOutError,
                                            MemoryLayoutErrorCode::kInvalidDirtyState,
                                            "ValidateArchiveHeader",
                                            "DirtyState",
                                            11u,
                                            kArchiveHeaderBytesV2,
                                            kArchiveHeaderBytesV2,
                                            pHeader.mDirtyState,
                                            static_cast<std::uint64_t>(ArchiveDirtyStateV2::kFinished));
    return false;
  }

  return true;
}

bool ReadArchiveHeader(const unsigned char* pBytes,
                       std::size_t pByteCount,
                       ArchiveHeaderV2& pOutHeader,
                       MemoryLayoutErrorInfo* pOutError) {
  pOutHeader = ArchiveHeaderV2{};
  ClearMemoryLayoutError(pOutError);

  if (pBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "ReadArchiveHeader",
                            "bytes",
                            0u,
                            0u,
                            kArchiveHeaderBytesV2);
    return false;
  }
  if (pByteCount < kArchiveHeaderBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "ReadArchiveHeader",
                            "bytes",
                            0u,
                            pByteCount,
                            kArchiveHeaderBytesV2);
    return false;
  }

  pOutHeader.mMagic = ReadUint64LE(pBytes + 0u);
  pOutHeader.mArchiveFormatVersion = pBytes[8u];
  pOutHeader.mCipherVersion = pBytes[9u];
  pOutHeader.mExpanderVersion = pBytes[10u];
  pOutHeader.mDirtyState = pBytes[11u];
  pOutHeader.mIsEncrypted = pBytes[12u];
  pOutHeader.mCipherProfile = pBytes[13u];
  pOutHeader.mExpanderProfile = pBytes[14u];
  pOutHeader.mReserved0 = pBytes[15u];

  std::memcpy(pOutHeader.mArchiveIndex.mBytes.data(), pBytes + 16u, kPackedUint48BytesV2);
  std::memcpy(pOutHeader.mArchiveCount.mBytes.data(), pBytes + 22u, kPackedUint48BytesV2);
  std::memcpy(pOutHeader.mBlockCountMain.mBytes.data(), pBytes + 28u, kPackedUint48BytesV2);
  std::memcpy(pOutHeader.mReservedCount0.mBytes.data(), pBytes + 34u, kPackedUint48BytesV2);
  std::memcpy(
      pOutHeader.mBlockCountPreview.mBytes.data(), pBytes + 40u, kPackedUint48BytesV2);
  std::memcpy(
      pOutHeader.mBlockCountRepair.mBytes.data(), pBytes + 46u, kPackedUint48BytesV2);

  pOutHeader.mArchiveFamilyId = ReadUint64LE(pBytes + 52u);
  pOutHeader.mReserved1 = ReadUint32LE(pBytes + 60u);

  return ValidateArchiveHeader(pOutHeader, pOutError);
}

bool WriteArchiveHeader(const ArchiveHeaderV2& pHeader,
                        unsigned char* pOutBytes,
                        std::size_t pByteCount,
                        MemoryLayoutErrorInfo* pOutError) {
  ClearMemoryLayoutError(pOutError);

  if (pOutBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "WriteArchiveHeader",
                            "bytes",
                            0u,
                            0u,
                            kArchiveHeaderBytesV2);
    return false;
  }
  if (pByteCount < kArchiveHeaderBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "WriteArchiveHeader",
                            "bytes",
                            0u,
                            pByteCount,
                            kArchiveHeaderBytesV2);
    return false;
  }
  if (!ValidateArchiveHeader(pHeader, pOutError)) {
    if (pOutError != nullptr) {
      pOutError->mOperation = "WriteArchiveHeader";
    }
    return false;
  }

  WriteUint64LE(pHeader.mMagic, pOutBytes + 0u);
  pOutBytes[8u] = static_cast<unsigned char>(pHeader.mArchiveFormatVersion);
  pOutBytes[9u] = static_cast<unsigned char>(pHeader.mCipherVersion);
  pOutBytes[10u] = static_cast<unsigned char>(pHeader.mExpanderVersion);
  pOutBytes[11u] = static_cast<unsigned char>(pHeader.mDirtyState);
  pOutBytes[12u] = static_cast<unsigned char>(pHeader.mIsEncrypted);
  pOutBytes[13u] = static_cast<unsigned char>(pHeader.mCipherProfile);
  pOutBytes[14u] = static_cast<unsigned char>(pHeader.mExpanderProfile);
  pOutBytes[15u] = static_cast<unsigned char>(pHeader.mReserved0);

  std::memcpy(pOutBytes + 16u, pHeader.mArchiveIndex.mBytes.data(), kPackedUint48BytesV2);
  std::memcpy(pOutBytes + 22u, pHeader.mArchiveCount.mBytes.data(), kPackedUint48BytesV2);
  std::memcpy(pOutBytes + 28u, pHeader.mBlockCountMain.mBytes.data(), kPackedUint48BytesV2);
  std::memcpy(pOutBytes + 34u, pHeader.mReservedCount0.mBytes.data(), kPackedUint48BytesV2);
  std::memcpy(
      pOutBytes + 40u, pHeader.mBlockCountPreview.mBytes.data(), kPackedUint48BytesV2);
  std::memcpy(
      pOutBytes + 46u, pHeader.mBlockCountRepair.mBytes.data(), kPackedUint48BytesV2);

  WriteUint64LE(pHeader.mArchiveFamilyId, pOutBytes + 52u);
  WriteUint32LE(pHeader.mReserved1, pOutBytes + 60u);
  return true;
}

}  // namespace peanutbutter::memory_layout
