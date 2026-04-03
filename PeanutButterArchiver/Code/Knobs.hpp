#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace peanutbutter::knobs {

inline constexpr std::uint32_t kDefaultBlocksPerArchiveV2 = 100u;
inline constexpr char kDefaultBundleBlockCountTitleV2[] = "100 blocks";

inline constexpr std::size_t kArchiveHeaderBytesV2 = 64u;
inline constexpr std::size_t kSectionHeaderBytesV2 = 96u;
inline constexpr std::size_t kDefaultArchiveBlockBytesV2 = 1044480u;
inline constexpr std::size_t kDefaultSectionPayloadBytesV2 =
    kDefaultArchiveBlockBytesV2 - kSectionHeaderBytesV2;
inline std::size_t kArchiveBlockBytesV2 = kDefaultArchiveBlockBytesV2;
inline std::size_t kSectionPayloadBytesV2 = kDefaultSectionPayloadBytesV2;
inline constexpr std::size_t kMaxPathLengthV2 = 16384u;

// Batch slicing budgets are intentionally expressed in payload bytes so tests can
// tune throughput by setting values relative to section payload size.
inline constexpr std::size_t kBatchBudgetBytesBundleV2 = kDefaultSectionPayloadBytesV2;
inline constexpr std::size_t kBatchBudgetBytesDecodeV2 = kDefaultSectionPayloadBytesV2;
inline constexpr std::size_t kBatchBudgetBytesBundleRepairV2 =
    kDefaultSectionPayloadBytesV2;
inline constexpr std::size_t kBatchBudgetBytesRepairV2 = kDefaultSectionPayloadBytesV2;
inline constexpr std::size_t kBatchBudgetBytesSanityCompareV2 =
    kDefaultArchiveBlockBytesV2 * 8u;

constexpr std::uint32_t BatchBlocksFromBudgetBytesV2(
    std::size_t pBudgetBytes,
    std::size_t pPayloadBytesPerBlock) {
  return pBudgetBytes == 0u || pPayloadBytesPerBlock == 0u
             ? 1u
             : static_cast<std::uint32_t>(
                   (pBudgetBytes + pPayloadBytesPerBlock - 1u) /
                   pPayloadBytesPerBlock);
}

inline constexpr std::uint32_t kDefaultBatchSizeBundleV2 =
    BatchBlocksFromBudgetBytesV2(kBatchBudgetBytesBundleV2,
                                 kDefaultSectionPayloadBytesV2);
inline constexpr std::uint32_t kDefaultBatchSizeDecodeV2 =
    BatchBlocksFromBudgetBytesV2(kBatchBudgetBytesDecodeV2,
                                 kDefaultSectionPayloadBytesV2);
inline constexpr std::uint32_t kDefaultBatchSizeBundleRepairV2 =
    BatchBlocksFromBudgetBytesV2(kBatchBudgetBytesBundleRepairV2,
                                 kDefaultSectionPayloadBytesV2);
inline constexpr std::uint32_t kDefaultBatchSizeRepairV2 =
    BatchBlocksFromBudgetBytesV2(kBatchBudgetBytesRepairV2,
                                 kDefaultSectionPayloadBytesV2);
inline constexpr std::uint32_t kDefaultBatchSizeSanityCompareV2 =
    BatchBlocksFromBudgetBytesV2(kBatchBudgetBytesSanityCompareV2,
                                 kDefaultArchiveBlockBytesV2);
inline constexpr std::uint32_t kDefaultBatchSizeSanityDiscoveryV2 = 512u;

// Mutable batch sizes so tests can override pacing per run.
inline std::uint32_t kBatchSizeBundleV2 = kDefaultBatchSizeBundleV2;
inline std::uint32_t kBatchSizeDecodeV2 = kDefaultBatchSizeDecodeV2;
inline std::uint32_t kBatchSizeBundleRepairV2 = kDefaultBatchSizeBundleRepairV2;
inline std::uint32_t kBatchSizeRepairV2 = kDefaultBatchSizeRepairV2;
inline std::uint32_t kBatchSizeSanityCompareV2 =
    kDefaultBatchSizeSanityCompareV2;
inline std::uint32_t kBatchSizeSanityDiscoveryV2 =
    kDefaultBatchSizeSanityDiscoveryV2;

inline void ResetBatchSizesToDefaultV2() {
  kBatchSizeBundleV2 = kDefaultBatchSizeBundleV2;
  kBatchSizeDecodeV2 = kDefaultBatchSizeDecodeV2;
  kBatchSizeBundleRepairV2 = kDefaultBatchSizeBundleRepairV2;
  kBatchSizeRepairV2 = kDefaultBatchSizeRepairV2;
  kBatchSizeSanityCompareV2 = kDefaultBatchSizeSanityCompareV2;
  kBatchSizeSanityDiscoveryV2 = kDefaultBatchSizeSanityDiscoveryV2;
}

inline void SetAllBatchSizesForTestingV2(std::uint32_t pBatchSize) {
  const std::uint32_t aSafe = std::max<std::uint32_t>(1u, pBatchSize);
  kBatchSizeBundleV2 = aSafe;
  kBatchSizeDecodeV2 = aSafe;
  kBatchSizeBundleRepairV2 = aSafe;
  kBatchSizeRepairV2 = aSafe;
  kBatchSizeSanityCompareV2 = aSafe;
  kBatchSizeSanityDiscoveryV2 = aSafe;
}

inline void SetBatchSizesForTestingV2(std::uint32_t pBundleBatchSize,
                                      std::uint32_t pDecodeBatchSize,
                                      std::uint32_t pBundleRepairBatchSize,
                                      std::uint32_t pRepairBatchSize) {
  kBatchSizeBundleV2 = std::max<std::uint32_t>(1u, pBundleBatchSize);
  kBatchSizeDecodeV2 = std::max<std::uint32_t>(1u, pDecodeBatchSize);
  kBatchSizeBundleRepairV2 =
      std::max<std::uint32_t>(1u, pBundleRepairBatchSize);
  kBatchSizeRepairV2 = std::max<std::uint32_t>(1u, pRepairBatchSize);
}

inline void SetSanityBatchSizesForTestingV2(
    std::uint32_t pSanityDiscoveryBatchSize,
    std::uint32_t pSanityCompareBatchSize) {
  kBatchSizeSanityDiscoveryV2 =
      std::max<std::uint32_t>(1u, pSanityDiscoveryBatchSize);
  kBatchSizeSanityCompareV2 =
      std::max<std::uint32_t>(1u, pSanityCompareBatchSize);
}

inline void ResetPayloadSizePerBlockV2() {
  kArchiveBlockBytesV2 = kDefaultArchiveBlockBytesV2;
  kSectionPayloadBytesV2 = kDefaultSectionPayloadBytesV2;
}

inline void SetPayloadSizePerBlockV2(std::size_t pPayloadBytesPerBlock) {
  const std::size_t aSafePayload = std::max<std::size_t>(1u, pPayloadBytesPerBlock);
  kSectionPayloadBytesV2 = aSafePayload;
  kArchiveBlockBytesV2 = kSectionHeaderBytesV2 + aSafePayload;
}

inline constexpr std::uint32_t kBatchSizeBundleDiscoveryV2 = 512u;
inline constexpr std::uint32_t kBundleDiscoveryProgressItemIntervalV2 = 64u;
inline constexpr std::uint32_t kDecodeDiscoveryProgressItemIntervalV2 = 2048u;

inline constexpr std::uint64_t kBundleArchiveProgressArchiveLogIntervalV2 = 64u;
inline constexpr std::uint64_t kBundleArchiveProgressFileLogIntervalV2 = 1000u;
inline constexpr std::uint64_t kBundleArchiveProgressByteLogIntervalV2 =
    250u * 1024u * 1024u;

inline constexpr std::uint64_t kDecodeArchiveProgressArchiveLogIntervalV2 = 64u;
inline constexpr std::uint64_t kDecodeArchiveProgressFileLogIntervalV2 = 1000u;
inline constexpr std::uint64_t kDecodeArchiveProgressFolderLogIntervalV2 = 256u;
inline constexpr std::uint64_t kDecodeArchiveProgressByteLogIntervalV2 =
    250u * 1024u * 1024u;

inline constexpr std::uint64_t kSanityDiscoveryFileLogIntervalV2 = 256u;
inline constexpr std::uint64_t kSanityDiscoveryFolderLogIntervalV2 = 256u;

inline constexpr std::uint64_t kSanityCompareFileLogIntervalV2 = 256u;
inline constexpr std::uint64_t kSanityCompareFolderLogIntervalV2 = 256u;
inline constexpr std::uint64_t kSanityCompareByteLogIntervalV2 =
    64u * 1024u * 1024u;

inline constexpr bool kEngineEmitRuntimeEventsByDefaultV2 = false;
inline constexpr bool kLogShowTimestampsV2 = true;

}  // namespace peanutbutter::knobs
