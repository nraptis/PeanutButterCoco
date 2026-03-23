#pragma once

#include <cstdint>
#include <string>

namespace peanutbutter {

enum class ProgressStageV2 {
  kIdle = 0,
  kPreflight = 1,
  kHeaderBootstrap = 2,
  kDiscovery = 3,
  kInspection = 4,
  kMemoryPlanning = 5,
  kDeriveCipherMaterial = 6,
  kAssembleCipherStack = 7,
  kArchiveManifest = 8,
  kFolderPacking = 9,
  kManifestDiscovery = 10,
  kArchivePacking = 11,
  kArchiveDecode = 12,
  kRepairPacking = 13,
  kFinalizingHeaders = 14,
  kFinalize = 15,
  kCompare = 16,
  kRepairApply = 17,
};

struct ProgressSnapshotV2 {
  ProgressStageV2 mStage = ProgressStageV2::kIdle;
  double mLocalFraction = 0.0;
  double mOverallFraction = 0.0;
  std::uint64_t mEstimatedMillis = 0u;
  std::string mLabel;
};

}  // namespace peanutbutter
