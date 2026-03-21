#include "SkipRecord.hpp"

namespace peanutbutter::memory_layout {

std::uint32_t GetSkipRecordByteDistance(const SkipRecordV2& pSkipRecord) {
  return PackedUint24ToUInt32(pSkipRecord.mByteDistance);
}

bool SetSkipRecordByteDistance(SkipRecordV2& pSkipRecord,
                               std::uint32_t pByteDistance,
                               MemoryLayoutErrorInfo* pOutError) {
  return TrySetPackedUint24(pSkipRecord.mByteDistance,
                            pByteDistance,
                            pOutError,
                            "SkipRecord.ByteDistance");
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

  pOutSkipRecord.mArchiveDistance = ReadUint16LE(pBytes + 0u);
  pOutSkipRecord.mBlockDistance = ReadUint16LE(pBytes + 2u);
  pOutSkipRecord.mByteDistance.mBytes[0] = static_cast<std::uint8_t>(pBytes[4u]);
  pOutSkipRecord.mByteDistance.mBytes[1] = static_cast<std::uint8_t>(pBytes[5u]);
  pOutSkipRecord.mByteDistance.mBytes[2] = static_cast<std::uint8_t>(pBytes[6u]);
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

  WriteUint16LE(pSkipRecord.mArchiveDistance, pOutBytes + 0u);
  WriteUint16LE(pSkipRecord.mBlockDistance, pOutBytes + 2u);
  pOutBytes[4u] = static_cast<unsigned char>(pSkipRecord.mByteDistance.mBytes[0]);
  pOutBytes[5u] = static_cast<unsigned char>(pSkipRecord.mByteDistance.mBytes[1]);
  pOutBytes[6u] = static_cast<unsigned char>(pSkipRecord.mByteDistance.mBytes[2]);
  return true;
}

}  // namespace peanutbutter::memory_layout
