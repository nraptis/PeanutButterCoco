#include "../PeanutButterArchiver/Code/Common/BundleRequest.hpp"
#include "../PeanutButterArchiver/Code/Common/DecodeRequest.hpp"
#include "../PeanutButterArchiver/Code/Engine/Bundle/Bundle_Context.hpp"
#include "../PeanutButterArchiver/Code/Engine/Bundle/Bundle_Director.hpp"
#include "../PeanutButterArchiver/Code/Engine/Decode/Decode_Context.hpp"
#include "../PeanutButterArchiver/Code/Engine/Decode/Decode_Director.hpp"
#include "../PeanutButterArchiver/Code/Engine/FileAccess/LocalFileSystem.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool WriteFile(const std::filesystem::path& pPath, const std::string& pContents) {
  std::ofstream aStream(pPath, std::ios::binary);
  if (!aStream.is_open()) {
    return false;
  }
  aStream.write(pContents.data(), static_cast<std::streamsize>(pContents.size()));
  return aStream.good();
}

}  // namespace

int main() {
  namespace fs = std::filesystem;
  using peanutbutter::BundleContextV2;
  using peanutbutter::BundleDirector;
  using peanutbutter::BundleRequestV2;
  using peanutbutter::DecodeContextV2;
  using peanutbutter::DecodeDirector;
  using peanutbutter::DecodeRequestV2;
  using peanutbutter::LocalFileSystemV2;

  const fs::path aBundleSource = "/tmp/pb_v2_bundle_source";
  const fs::path aBundleOutput = "/tmp/pb_v2_bundle_output";
  const fs::path aDecodeOutput = "/tmp/pb_v2_decode_output";

  fs::remove_all(aBundleSource);
  fs::remove_all(aBundleOutput);
  fs::remove_all(aDecodeOutput);
  fs::create_directories(aBundleSource / "folder");
  fs::create_directories(aBundleOutput);
  fs::create_directories(aDecodeOutput);

  if (!WriteFile(aBundleSource / "alpha.txt", "alpha-data") ||
      !WriteFile(aBundleSource / "folder" / "beta.txt", "beta-data")) {
    return 1;
  }

  LocalFileSystemV2 aFileSystem;

  BundleRequestV2 aBundleRequest;
  aBundleRequest.mSourceDirectory = aBundleSource.string();
  aBundleRequest.mDestinationDirectory = aBundleOutput.string();
  aBundleRequest.mFilePrefix = "archive";
  aBundleRequest.mEncryptionEnabled = true;
  aBundleRequest.mSafeMode = true;
  aBundleRequest.mRepairEnabled = false;
  aBundleRequest.mBlockCountLabel = "4 blocks";
  aBundleRequest.mPassword = "peanut";

  BundleContextV2 aBundleContext(aBundleRequest, aFileSystem);
  BundleDirector aBundleDirector;
  if (!aBundleDirector.Run(aBundleContext)) {
    return 2;
  }

  DecodeRequestV2 aDecodeRequest;
  aDecodeRequest.mSourceDirectory = aBundleOutput.string();
  aDecodeRequest.mDestinationDirectory = aDecodeOutput.string();
  aDecodeRequest.mEncryptionEnabled = true;
  aDecodeRequest.mPassword = "peanut";

  DecodeContextV2 aDecodeContext(aDecodeRequest, aFileSystem);
  DecodeDirector aDecodeDirector;
  if (!aDecodeDirector.Run(aDecodeContext)) {
    return 3;
  }

  const fs::path aAlphaOut = aDecodeOutput / "alpha.txt";
  const fs::path aBetaOut = aDecodeOutput / "folder" / "beta.txt";
  if (!fs::exists(aAlphaOut) || !fs::exists(aBetaOut)) {
    return 4;
  }

  return 0;
}
