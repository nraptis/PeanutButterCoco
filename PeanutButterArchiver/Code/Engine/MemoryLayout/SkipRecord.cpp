#include "SkipRecord.hpp"

namespace peanutbutter::memory_layout {

std::uint32_t GetSkipRecordArchiveIndex(const SkipRecordV2& pSkipRecord) {
  return PackedUint24ToUInt32(pSkipRecord.mArchiveIndex);
}

bool SetSkipRecordArchiveIndex(SkipRecordV2& pSkipRecord,
                               std::uint32_t pArchiveIndex,
                               MemoryLayoutErrorInfo* pOutError) {
  return TrySetPackedUint24(
      pSkipRecord.mArchiveIndex, pArchiveIndex, pOutError, "SkipRecordArchiveIndex");
}

std::uint32_t GetSkipRecordByteDistance(const SkipRecordV2& pSkipRecord) {
  return PackedUint24ToUInt32(pSkipRecord.mByteIndex);
}

bool SetSkipRecordByteDistance(SkipRecordV2& pSkipRecord,
                               std::uint32_t pByteDistance,
                               MemoryLayoutErrorInfo* pOutError) {
  return TrySetPackedUint24(
      pSkipRecord.mByteIndex, pByteDistance, pOutError, "SkipRecordByteDistance");
}

bool ReadSkipRecord(const unsigned char* pBytes,
                    std::size_t pByteCount,
                    SkipRecordV2& pOutSkipRecord,
                    MemoryLayoutErrorInfo* pOutError) {
  pOutSkipRecord = SkipRecordV2{};
  ClearMemoryLayoutError(pOutError);

  if (pBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "ReadSkipRecord",
                            "bytes",
                            0u,
                            0u,
                            kSkipRecordBytesV2);
    return false;
  }
  if (pByteCount < kSkipRecordBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "ReadSkipRecord",
                            "bytes",
                            0u,
                            pByteCount,
                            kSkipRecordBytesV2);
    return false;
  }

  if (!SetSkipRecordArchiveIndex(
          pOutSkipRecord, ReadUint24LE(pBytes + 0u), pOutError)) {
    return false;
  }
  pOutSkipRecord.mBlockIndex = ReadUint16LE(pBytes + 3u);
  if (!SetSkipRecordByteDistance(
          pOutSkipRecord, ReadUint24LE(pBytes + 5u), pOutError)) {
    return false;
  }
  return true;
}

bool WriteSkipRecord(const SkipRecordV2& pSkipRecord,
                     unsigned char* pOutBytes,
                     std::size_t pByteCount,
                     MemoryLayoutErrorInfo* pOutError) {
  ClearMemoryLayoutError(pOutError);

  if (pOutBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "WriteSkipRecord",
                            "bytes",
                            0u,
                            0u,
                            kSkipRecordBytesV2);
    return false;
  }
  if (pByteCount < kSkipRecordBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "WriteSkipRecord",
                            "bytes",
                            0u,
                            pByteCount,
                            kSkipRecordBytesV2);
    return false;
  }

  WriteUint24LE(GetSkipRecordArchiveIndex(pSkipRecord), pOutBytes + 0u);
  WriteUint16LE(pSkipRecord.mBlockIndex, pOutBytes + 3u);
  WriteUint24LE(GetSkipRecordByteDistance(pSkipRecord), pOutBytes + 5u);
  return true;
}

}  // namespace peanutbutter::memory_layout
