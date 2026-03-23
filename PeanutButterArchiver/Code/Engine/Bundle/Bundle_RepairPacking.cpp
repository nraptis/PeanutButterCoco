#include "Bundle_RepairPacking.hpp"

#include <algorithm>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/ArchiveHeader.hpp"

namespace peanutbutter {
namespace {

struct RepairCopyRefV2 {
  std::size_t mArchiveIndex = 0u;
  std::uint32_t mLocalBlockIndex = 0u;
};

std::vector<RepairCopyRefV2> BuildRepairCopyOrder(
    const std::vector<std::uint32_t>& pRepairCopyBlockCounts) {
  std::vector<RepairCopyRefV2> aCopies;
  std::uint32_t aMaxLayer = 0u;
  for (std::uint32_t aCount : pRepairCopyBlockCounts) {
    aMaxLayer = std::max(aMaxLayer, aCount);
  }

  for (std::uint32_t aLayer = 0u; aLayer < aMaxLayer; ++aLayer) {
    for (std::size_t aArchiveIndex = 0u;
         aArchiveIndex < pRepairCopyBlockCounts.size();
         ++aArchiveIndex) {
      if (aLayer >= pRepairCopyBlockCounts[aArchiveIndex]) {
        continue;
      }
      aCopies.push_back({aArchiveIndex, aLayer});
    }
  }

  return aCopies;
}

bool ReadExactArchiveBlock(FileSystemV2& pFileSystem,
                           const std::string& pArchivePath,
                           std::uint32_t pLocalBlockIndex,
                           std::size_t pArchiveBlockBytes,
                           ByteBufferV2& pOutBlock,
                           std::string& pOutError) {
  pOutError.clear();

  std::unique_ptr<FileReadStreamV2> aRead = pFileSystem.OpenReadStream(pArchivePath);
  if (aRead == nullptr || !aRead->IsReady()) {
    pOutError = "failed opening repair source archive for read";
    return false;
  }

  const std::size_t aOffset = static_cast<std::size_t>(
      memory_layout::kArchiveHeaderBytesV2 +
      (static_cast<std::uint64_t>(pLocalBlockIndex) * pArchiveBlockBytes));
  if (!aRead->Read(aOffset, pOutBlock.Data(), pArchiveBlockBytes)) {
    pOutError = "failed reading repair source block";
    return false;
  }
  return true;
}

std::string BuildRepairPackingProgressLabel(std::uint64_t pPacked,
                                            std::uint64_t pTotal) {
  return "Packing repair copies: " + std::to_string(pPacked) + "/" +
         std::to_string(pTotal) + " blocks";
}

}  // namespace

bool BundleRepairPackingV2::Run(BundleStageContextV2& pContext) {
  BundlePackingStateV2& aPacking = pContext.State().mPacking;
  const BundleMemoryPlanV2& aMemoryPlan = pContext.State().mMemoryPlan;

  aPacking.mRepairPackedBlockCount = 0u;

  if (!pContext.Request().mRepairEnabled || aMemoryPlan.mRepairSectorBlockCount == 0u) {
    pContext.EmitLog(LogLevelV2::kInfo,
                     LogPhaseSkippedV2(LogActionV2::kBundle, ProgressStageV2::kRepairPacking,
                                       "repair copies are disabled"));
    pContext.EmitPhaseProgress(1.0, "Repair packing complete");
    return !pContext.IsCancelRequested();
  }

  if (aPacking.mArchivePaths.size() != aMemoryPlan.mArchives.size()) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionV2::kBundle,
                         ProgressStageV2::kRepairPacking,
                         "archive packing did not leave a complete archive path list"));
    return false;
  }

  const std::vector<RepairCopyRefV2> aRepairCopies =
      BuildRepairCopyOrder(aMemoryPlan.mRepairCopyBlockCounts);
  if (aRepairCopies.size() != aMemoryPlan.mRepairSectorBlockCount) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionV2::kBundle,
                         ProgressStageV2::kRepairPacking,
                         "repair copy planning disagreed with repair-sector block count"));
    return false;
  }

  const std::size_t aArchiveBlockBytes = pContext.Layout().mArchiveBlockBytes;
  ByteBufferV2 aBlockBytes(aArchiveBlockBytes);
  if (aBlockBytes.Empty()) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionV2::kBundle,
                         ProgressStageV2::kRepairPacking,
                         "failed allocating repair copy buffer"));
    return false;
  }

  std::size_t aCopyCursor = 0u;
  for (std::size_t aArchiveIndex = 0u;
       aArchiveIndex < aMemoryPlan.mArchives.size();
       ++aArchiveIndex) {
    const PlannedArchiveFileV2& aArchive = aMemoryPlan.mArchives[aArchiveIndex];
    const std::uint32_t aSourceBlocksInArchive =
        aArchiveIndex < aMemoryPlan.mSourceArchiveBlockCounts.size()
            ? aMemoryPlan.mSourceArchiveBlockCounts[aArchiveIndex]
            : 0u;
    const std::uint32_t aRepairBlocksInArchive =
        aArchive.mBlockCount > aSourceBlocksInArchive
            ? (aArchive.mBlockCount - aSourceBlocksInArchive)
            : 0u;
    if (aRepairBlocksInArchive == 0u) {
      continue;
    }

    for (std::uint32_t aRepairIndex = 0u;
         aRepairIndex < aRepairBlocksInArchive;
         ++aRepairIndex) {
      if (aCopyCursor >= aRepairCopies.size()) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(LogActionV2::kBundle,
                             ProgressStageV2::kRepairPacking,
                             "repair copy generation ran out of source blocks early"));
        return false;
      }

      const RepairCopyRefV2& aCopy = aRepairCopies[aCopyCursor];
      if (aCopy.mArchiveIndex >= aMemoryPlan.mArchives.size()) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(LogActionV2::kBundle,
                             ProgressStageV2::kRepairPacking,
                             "repair copy referenced an invalid source archive"));
        return false;
      }

      std::string aReadError;
      if (!ReadExactArchiveBlock(pContext.FileSystem(),
                                 aMemoryPlan.mArchives[aCopy.mArchiveIndex].mPath,
                                 aCopy.mLocalBlockIndex,
                                 aArchiveBlockBytes,
                                 aBlockBytes,
                                 aReadError)) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(LogActionV2::kBundle,
                             ProgressStageV2::kRepairPacking,
                             aReadError));
        return false;
      }

      if (!pContext.FileSystem().AppendFile(
              aArchive.mPath, aBlockBytes.Data(), aArchiveBlockBytes)) {
        pContext.EmitLog(
            LogLevelV2::kError,
            LogPhaseFailedV2(LogActionV2::kBundle,
                             ProgressStageV2::kRepairPacking,
                             "failed appending a repair copy block"));
        return false;
      }

      ++aCopyCursor;
      ++aPacking.mRepairPackedBlockCount;
      pContext.EmitPhaseProgress(
          aMemoryPlan.mRepairSectorBlockCount == 0u
              ? 1.0
              : static_cast<double>(aPacking.mRepairPackedBlockCount) /
                    static_cast<double>(aMemoryPlan.mRepairSectorBlockCount),
          BuildRepairPackingProgressLabel(aPacking.mRepairPackedBlockCount,
                                          aMemoryPlan.mRepairSectorBlockCount));
    }
  }

  if (aCopyCursor != aRepairCopies.size()) {
    pContext.EmitLog(
        LogLevelV2::kError,
        LogPhaseFailedV2(LogActionV2::kBundle,
                         ProgressStageV2::kRepairPacking,
                         "repair copy generation left unplaced repair blocks"));
    return false;
  }

  pContext.EmitLog(
      LogLevelV2::kInfo,
      "[Bundle][Repair Packing] Packed " +
          std::to_string(aPacking.mRepairPackedBlockCount) + " exact repair copies (" +
          std::to_string(pContext.Request().mRepairPercent) + "% front-of-archive coverage).");
  pContext.EmitPhaseProgress(1.0, "Repair packing complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
