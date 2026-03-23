#include "Bundle_FinalizingHeaders.hpp"

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {
namespace {

bool BuildArchiveHeader(const BundleStageContextV2& pContext,
                        std::uint64_t pArchiveIndex,
                        std::uint8_t pDirtyState,
                        memory_layout::ArchiveHeaderV2& pOutHeader) {
  using namespace memory_layout;

  pOutHeader = ArchiveHeaderV2{};
  pOutHeader.mDirtyState = pDirtyState;
  pOutHeader.mIsEncrypted = pContext.Request().mEncryptionEnabled ? 1u : 0u;
  pOutHeader.mCipherProfile =
      static_cast<std::uint8_t>(pContext.Request().mEncryptionStrength);
  pOutHeader.mExpanderProfile =
      static_cast<std::uint8_t>(pContext.Request().mTableStrength);
  pOutHeader.mArchiveFamilyId = pContext.State().mMemoryPlan.mArchiveFamilyId;

  return TrySetPackedUint48(pOutHeader.mArchiveIndex,
                            pArchiveIndex,
                            nullptr,
                            "ArchiveIndex") &&
         TrySetPackedUint48(pOutHeader.mArchiveCount,
                            pContext.State().mMemoryPlan.mArchiveCount,
                            nullptr,
                            "ArchiveCount") &&
         TrySetPackedUint48(pOutHeader.mArchiveDataBlockCount,
                            pContext.State().mMemoryPlan.mArchiveDataBlockCount,
                            nullptr,
                            "ArchiveDataBlockCount") &&
         TrySetPackedUint48(pOutHeader.mEmptyFolderBlockCount,
                            pContext.State().mMemoryPlan.mEmptyFolderBlockCount,
                            nullptr,
                            "EmptyFolderBlockCount") &&
         TrySetPackedUint48(pOutHeader.mPreviewManifestBlockCount,
                            pContext.State().mMemoryPlan.mPreviewManifestBlockCount,
                            nullptr,
                            "PreviewManifestBlockCount") &&
         TrySetPackedUint48(pOutHeader.mRepairSectorBlockCount,
                            pContext.State().mMemoryPlan.mRepairSectorBlockCount,
                            nullptr,
                            "RepairSectorBlockCount");
}

}  // namespace

bool BundleFinalizingHeadersV2::Run(BundleStageContextV2& pContext) {
  using namespace memory_layout;

  std::array<unsigned char, kArchiveHeaderBytesV2> aHeaderBytes{};
  const std::uint8_t aDirtyState = static_cast<std::uint8_t>(
      pContext.State().mCancel.mShouldFinalizeAfterCancel
          ? ArchiveDirtyStateV2::kFinishedWithCancel
          : ArchiveDirtyStateV2::kFinished);
  const std::size_t aArchiveCountToFinalize = pContext.State().mPacking.mArchivePaths.size();
  for (std::size_t aIndex = 0u; aIndex < aArchiveCountToFinalize; ++aIndex) {
    const PlannedArchiveFileV2& aArchive = pContext.State().mMemoryPlan.mArchives[aIndex];
    ArchiveHeaderV2 aHeader;
    if (!BuildArchiveHeader(
            pContext,
            aArchive.mArchiveIndex,
            aDirtyState,
            aHeader)) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kFinalizingHeaders,
                                        "archive header values were out of range"));
      return false;
    }
    if (!WriteArchiveHeader(aHeader, aHeaderBytes.data(), aHeaderBytes.size(), nullptr)) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kFinalizingHeaders,
                                        "archive header bytes could not be written"));
      return false;
    }
    if (!pContext.FileSystem().OverwriteFileRegion(
            aArchive.mPath, 0u, aHeaderBytes.data(), aHeaderBytes.size())) {
      pContext.EmitLog(LogLevelV2::kError,
                       LogPhaseFailedV2(LogActionV2::kBundle, ProgressStageV2::kFinalizingHeaders,
                                        "archive header patch write failed"));
      return false;
    }
  }

  pContext.State().mFinalize.mHeadersFinalized = true;
  pContext.EmitLog(LogLevelV2::kInfo,
                   LogPhaseCompletedV2(LogActionV2::kBundle, ProgressStageV2::kFinalizingHeaders));
  pContext.EmitPhaseProgress(1.0, "Headers finalized");
  return true;
}

}  // namespace peanutbutter
