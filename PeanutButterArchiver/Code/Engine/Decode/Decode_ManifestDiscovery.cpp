#include "Decode_ManifestDiscovery.hpp"

#include <sstream>

#include "../../Common/LogCatalog.hpp"
#include "../MemoryLayout/FormatUtilities.hpp"

namespace peanutbutter {

bool DecodeManifestDiscoveryV2::Run(DecodeStageContextV2& pContext) {
  if (pContext.Request().mIntent == DecodeIntentV2::kManifest) {
    const DecodeBootstrapStateV2& aBootstrap = pContext.State().mBootstrap;
    const DecodeDiscoveryStateV2& aDiscovery = pContext.State().mDiscovery;

    std::ostringstream aOut;
    aOut << "[Read Manifest][Report] Family "
         << aBootstrap.mFirstHeader.mArchiveFamilyId
         << ", archives=" << aDiscovery.mArchives.size()
         << ", readable_blocks=" << aDiscovery.mTotalReadableBlocks
         << ", advertised_empty_folder_blocks="
         << aBootstrap.mExpectedEmptyFolderBlockCount
         << ", advertised_preview_manifest_blocks="
         << aBootstrap.mExpectedPreviewManifestBlockCount
         << ", advertised_archive_data_blocks="
         << aBootstrap.mExpectedArchiveDataBlockCount
         << ", advertised_repair_blocks="
         << aBootstrap.mExpectedRepairBlockCount
         << ", encrypted="
         << (aBootstrap.mFirstHeader.mIsEncrypted != 0u ? "true" : "false")
         << ", discovery_mode="
         << (aDiscovery.mMode == DecodeModeV2::kOptimistic ? "optimistic"
                                                           : "pessimistic")
         << ".";
    pContext.EmitLog(LogLevelV2::kInfo, aOut.str());

    if (aBootstrap.mExpectedPreviewManifestBlockCount > 0u) {
      pContext.EmitLog(
          LogLevelV2::kWarning,
          "[Read Manifest][Report] Preview-manifest block count is only an "
          "advertised header value here; this flow does not decode preview "
          "manifest payloads.");
    }
  } else {
    pContext.EmitLog(
        LogLevelV2::kInfo,
        LogDecodeManifestSummaryV2(
            pContext.State().mBootstrap.mExpectedPreviewManifestBlockCount,
            pContext.State().mBootstrap.mExpectedRepairBlockCount));
  }
  pContext.EmitPhaseProgress(1.0, "Manifest discovery complete");
  return !pContext.IsCancelRequested();
}

}  // namespace peanutbutter
