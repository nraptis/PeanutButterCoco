#pragma once

#include <string>

#include "../Knobs.hpp"

namespace peanutbutter {

enum class HomeTabV2 {
  kBundle = 0,
  kUnbundle = 1,
  kTools = 2,
};

struct AppConfigStateV2 {
  HomeTabV2 mHomeTab = HomeTabV2::kBundle;

  double mWindowFrameX = 0.0;
  double mWindowFrameY = 0.0;
  double mWindowFrameWidth = 0.0;
  double mWindowFrameHeight = 0.0;

  std::string mTextFieldInputText;
  std::string mTextFieldArchivedText;
  std::string mTextFieldUnarchivedText;
  std::string mTextFieldCompareAText;
  std::string mTextFieldCompareBText;

  std::string mBundleFilePrefix = "archive";
  bool mBundleRepair = true;
  bool mBundleSafe = true;
  bool mBundleEncrypt = true;
  bool mBundleIncludePreview = true;
  std::string mBundleBlockCount = knobs::kDefaultBundleBlockCountTitleV2;
  std::string mBundleEncryptionStrength = "Encryption: High";
  std::string mBundleTableStrength = "Tables: High";
  std::string mBundleRepairSize = "20%";
  std::string mBundlePassword;

  bool mUnbundleEncrypt = true;
  bool mUnbundleRecover = false;
  std::string mUnbundlePassword;
};

}  // namespace peanutbutter
