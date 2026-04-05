#pragma once

#include <cstddef>
#include <cstdint>

namespace peanutbutter::memory_layout::specs_verified {

// Gold-seal specs: keep this list intentionally short and high confidence.
inline constexpr std::size_t kSectionCheckSumBytesV2 = 32u;
inline constexpr std::uint8_t kSectionCheckSumKindSha256V2 = 0u;
inline constexpr std::size_t kSectionReservedBytesV2 = 3u;
inline constexpr std::size_t kPreviewRecordPlaceholderBytesV2 = 1u;
inline constexpr std::uint8_t kPreviewRecordPlaceholderValueV2 = 0u;
// Fixed-width typed-record scalars are stream bytes and may cross block boundaries.
inline constexpr bool kRecordScalarsMayCrossBlockBoundariesV2 = true;
inline constexpr std::size_t kMinimumSectionPayloadBytesV2 = 1u;

}  // namespace peanutbutter::memory_layout::specs_verified
