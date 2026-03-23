#include "Archive/ArchiveSkip.hpp"

#include <limits>

namespace peanutbutter {

bool TryBuildSkipRecord(std::uint64_t pDistanceBytes,
                        std::uint32_t pBlocksPerArchive,
                        SkipRecord& pOutSkip) {
  pOutSkip = SkipRecord{};
  if (pBlocksPerArchive == 0u || pBlocksPerArchive > kMaxBlocksPerArchive) {
    return false;
  }
  const std::uint64_t aPayloadBytesPerArchive =
      static_cast<std::uint64_t>(pBlocksPerArchive) *
      static_cast<std::uint64_t>(kPayloadBytesPerL3);
  if (aPayloadBytesPerArchive == 0u) {
    return false;
  }

  const std::uint64_t aArchiveDistance = pDistanceBytes / aPayloadBytesPerArchive;
  const std::uint64_t aArchiveRemainder = pDistanceBytes % aPayloadBytesPerArchive;
  const std::uint64_t aBlockDistance =
      aArchiveRemainder / static_cast<std::uint64_t>(kPayloadBytesPerL3);
  const std::uint64_t aByteDistance =
      aArchiveRemainder % static_cast<std::uint64_t>(kPayloadBytesPerL3);

  if (aArchiveDistance >
      static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max())) {
    return false;
  }
  if (aBlockDistance >
      static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max())) {
    return false;
  }
  if (aByteDistance >
      static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
    return false;
  }

  pOutSkip.mArchiveDistance = static_cast<std::uint16_t>(aArchiveDistance);
  pOutSkip.mBlockDistance = static_cast<std::uint16_t>(aBlockDistance);
  pOutSkip.mByteDistance = static_cast<std::uint32_t>(aByteDistance);
  return true;
}

}  // namespace peanutbutter
