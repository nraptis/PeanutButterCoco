#include "RepairRecord.hpp"

#include "Primatives.hpp"

namespace peanutbutter::memory_layout {

bool ReadRepairRecord(const unsigned char* pBytes,
                      std::size_t pByteCount,
                      RepairRecordV2& pOutRepairRecord,
                      MemoryLayoutErrorInfo* pOutError) {
  pOutRepairRecord = RepairRecordV2{};
  ClearMemoryLayoutError(pOutError);

  if (pBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "ReadRepairRecord",
                            "bytes",
                            0u,
                            0u,
                            kRepairRecordBytesV2);
    return false;
  }
  if (pByteCount < kRepairRecordBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "ReadRepairRecord",
                            "bytes",
                            0u,
                            pByteCount,
                            kRepairRecordBytesV2);
    return false;
  }

  pOutRepairRecord.mArchiveIndex = ReadUint16LE(pBytes + 0u);
  pOutRepairRecord.mBlockIndex = ReadUint16LE(pBytes + 2u);
  return true;
}

bool WriteRepairRecord(const RepairRecordV2& pRepairRecord,
                       unsigned char* pOutBytes,
                       std::size_t pByteCount,
                       MemoryLayoutErrorInfo* pOutError) {
  ClearMemoryLayoutError(pOutError);

  if (pOutBytes == nullptr) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kNullBuffer,
                            "WriteRepairRecord",
                            "bytes",
                            0u,
                            0u,
                            kRepairRecordBytesV2);
    return false;
  }
  if (pByteCount < kRepairRecordBytesV2) {
    AssignMemoryLayoutError(pOutError,
                            MemoryLayoutErrorCode::kBufferTooSmall,
                            "WriteRepairRecord",
                            "bytes",
                            0u,
                            pByteCount,
                            kRepairRecordBytesV2);
    return false;
  }

  WriteUint16LE(pRepairRecord.mArchiveIndex, pOutBytes + 0u);
  WriteUint16LE(pRepairRecord.mBlockIndex, pOutBytes + 2u);
  return true;
}

}  // namespace peanutbutter::memory_layout
