#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "FileSystem.hpp"

namespace peanutbutter {

inline std::string BuildConflictLeafNameV2(const FileSystemV2& pFileSystem,
                                           const std::string& pPreferredLeafName,
                                           std::uint32_t pOrdinal) {
  const std::string aExtension = pFileSystem.Extension(pPreferredLeafName);
  std::string aStem = pFileSystem.StemName(pPreferredLeafName);
  if (aStem.empty()) {
    aStem = "output";
  }

  if (pOrdinal == 0u) {
    return aStem + aExtension;
  }
  return aStem + "_" + std::to_string(pOrdinal) + aExtension;
}

inline bool PathIsReservedV2(
    const std::string& pPath,
    const std::vector<std::string>* pReservedPaths) {
  if (pReservedPaths == nullptr) {
    return false;
  }
  for (const std::string& aReservedPath : *pReservedPaths) {
    if (aReservedPath == pPath) {
      return true;
    }
  }
  return false;
}

inline bool ResolveNoOverwritePathV2(
    const FileSystemV2& pFileSystem,
    const std::string& pParentPath,
    const std::string& pPreferredLeafName,
    std::string& pOutResolvedPath,
    const std::vector<std::string>* pReservedPaths = nullptr,
    std::uint32_t pMaxOrdinalsToTry = 1000000u) {
  pOutResolvedPath.clear();
  for (std::uint32_t aOrdinal = 0u; aOrdinal < pMaxOrdinalsToTry; ++aOrdinal) {
    const std::string aLeaf =
        BuildConflictLeafNameV2(pFileSystem, pPreferredLeafName, aOrdinal);
    const std::string aPath = pFileSystem.JoinPath(pParentPath, aLeaf);
    if (!pFileSystem.Exists(aPath) && !PathIsReservedV2(aPath, pReservedPaths)) {
      pOutResolvedPath = aPath;
      return true;
    }
  }
  return false;
}

inline bool ResolveNoOverwritePathTripletV2(
    const FileSystemV2& pFileSystem,
    const std::string& pParentPath,
    const std::string& pPreferredLeafName,
    const std::string& pWritingPrefix,
    const std::string& pPartialPrefix,
    std::string& pOutWritingPath,
    std::string& pOutFinalPath,
    std::string& pOutPartialPath,
    const std::vector<std::string>* pReservedPaths = nullptr,
    std::uint32_t pMaxOrdinalsToTry = 1000000u) {
  pOutWritingPath.clear();
  pOutFinalPath.clear();
  pOutPartialPath.clear();

  for (std::uint32_t aOrdinal = 0u; aOrdinal < pMaxOrdinalsToTry; ++aOrdinal) {
    const std::string aLeaf =
        BuildConflictLeafNameV2(pFileSystem, pPreferredLeafName, aOrdinal);
    const std::string aWritingLeaf = pWritingPrefix + aLeaf;
    const std::string aPartialLeaf = pPartialPrefix + aLeaf;
    const std::string aFinalPath = pFileSystem.JoinPath(pParentPath, aLeaf);
    const std::string aWritingPath = pFileSystem.JoinPath(pParentPath, aWritingLeaf);
    const std::string aPartialPath = pFileSystem.JoinPath(pParentPath, aPartialLeaf);
    if (pFileSystem.Exists(aFinalPath) ||
        pFileSystem.Exists(aWritingPath) ||
        pFileSystem.Exists(aPartialPath) ||
        PathIsReservedV2(aFinalPath, pReservedPaths) ||
        PathIsReservedV2(aWritingPath, pReservedPaths) ||
        PathIsReservedV2(aPartialPath, pReservedPaths)) {
      continue;
    }

    pOutFinalPath = aFinalPath;
    pOutWritingPath = aWritingPath;
    pOutPartialPath = aPartialPath;
    return true;
  }
  return false;
}

}  // namespace peanutbutter

