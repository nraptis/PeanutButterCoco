#pragma once

#include <string>

namespace peanutbutter {

enum class HomeTabV2 {
  kBundle = 0,
  kUnbundle = 1,
  kRepair = 2,
  kSanity = 3,
};

struct AppConfigStateV2 {
  HomeTabV2 mHomeTab = HomeTabV2::kBundle;

  std::string mBundleDirectorySource;
  std::string mBundleDirectoryDestination;
  std::string mBundleFilePrefix = "archive";
  bool mBundleRepair = true;
  bool mBundleSafe = true;
  bool mBundleEncrypt = true;
  bool mBundleIncludePreview = true;
  std::string mBundleBlockCount = "4 blocks";
  std::string mBundleEncryptionStrength = "Encryption: High";
  std::string mBundleTableStrength = "Tables: High";
  std::string mBundleRepairSize = "25%";
  std::string mBundlePassword;

  std::string mUnbundleDirectorySource;
  std::string mUnbundleDirectoryDestination;
  bool mUnbundleEncrypt = true;
  std::string mUnbundlePassword;

  std::string mRepairDirectorySource;
  std::string mRepairDirectoryDestination;
  bool mRepairEncrypt = true;
  std::string mRepairPassword;

  std::string mSanityDirectorySource;
  std::string mSanityDirectoryDestination;
};

}  // namespace peanutbutter
