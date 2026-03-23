#ifndef PEANUT_BUTTER_ULTIMA_ARCHIVE_ARCHIVE_SKIP_HPP_
#define PEANUT_BUTTER_ULTIMA_ARCHIVE_ARCHIVE_SKIP_HPP_

#include <cstdint>

#include "PeanutButter.hpp"

namespace peanutbutter {

bool TryBuildSkipRecord(std::uint64_t pDistanceBytes,
                        std::uint32_t pBlocksPerArchive,
                        SkipRecord& pOutSkip);

}  // namespace peanutbutter

#endif  // PEANUT_BUTTER_ULTIMA_ARCHIVE_ARCHIVE_SKIP_HPP_
