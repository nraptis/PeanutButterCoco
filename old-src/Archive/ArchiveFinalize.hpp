#ifndef PEANUT_BUTTER_ULTIMA_ARCHIVE_ARCHIVE_FINALIZE_HPP_
#define PEANUT_BUTTER_ULTIMA_ARCHIVE_ARCHIVE_FINALIZE_HPP_

#include <cstddef>
#include <cstdint>

#include "AppShell_Bundle.hpp"

namespace peanutbutter {

ArchiveHeader BuildArchiveHeaderForPlan(const BundleRequest& pRequest,
                                        const BundleDiscovery& pDiscovery,
                                        const BundleArchivePlan& pPlan,
                                        std::uint32_t pArchiveCount,
                                        std::uint32_t pPayloadLength,
                                        DirtyType pDirtyType);

OperationResult FinalizeArchiveHeaders(const BundleDiscovery& pDiscovery,
                                       std::size_t pArchiveCountToFinalize,
                                       DirtyType pDirtyType,
                                       FileSystem& pFileSystem,
                                       Logger* pLogger = nullptr,
                                       CancelCoordinator* pCancelCoordinator = nullptr);

}  // namespace peanutbutter

#endif  // PEANUT_BUTTER_ULTIMA_ARCHIVE_ARCHIVE_FINALIZE_HPP_
