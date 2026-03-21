#pragma once

#include <cstdint>
#include <string>

namespace peanutbutter {

enum class ProgressStageV2 {
  kIdle = 0,
  kPreflight = 1,
  kHeaderBootstrap = 2,
  kDiscovery = 3,
  kMemoryPlanning = 4,
  kDeriveCipherMaterial = 5,
  kAssembleCipherStack = 6,
  kArchiveManifest = 7,
  kFolderPacking = 8,
  kManifestDiscovery = 9,
  kArchivePacking = 10,
  kArchiveDecode = 11,
  kRepairPacking = 12,
  kFinalizingHeaders = 13,
  kFinalize = 14,
  kCompare = 15,
};

struct ProgressSnapshotV2 {
  ProgressStageV2 mStage = ProgressStageV2::kIdle;
  double mLocalFraction = 0.0;
  double mOverallFraction = 0.0;
  std::uint64_t mEstimatedMillis = 0u;
  std::string mLabel;
};

}  // namespace peanutbutter
