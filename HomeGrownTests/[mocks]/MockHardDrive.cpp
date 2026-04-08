//
//  MockHardDrive.cpp
//  PeanutButterArchiver
//
//  Created by Magneto on 3/24/26.
//

#include "MockHardDrive.hpp"
#include "JobBundle.hpp"
#include <algorithm>

namespace {

bool FailBlockMutation(ByteString *pError, const std::string &pMessage) {
    if (pError != nullptr) {
        pError->Set(pMessage);
    }
    return false;
}

bool ResolveBlockRange(MockHardDrive &pDrive,
                       const std::string &pPath,
                       int pBlockIndex,
                       const JobBundle &pJob,
                       ByteString *pError,
                       std::string *pOutPath,
                       std::size_t *pOutBlockOffset,
                       std::size_t *pOutBlockSize) {
    if (pBlockIndex < 0) {
        return FailBlockMutation(pError, "Block mutation failed: block index must be >= 0.");
    }
    if (pJob.mBlocksPerArchive <= 0) {
        return FailBlockMutation(pError, "Block mutation failed: job block count per archive must be positive.");
    }
    if (pJob.mPayloadBytesPerBlock <= 0) {
        return FailBlockMutation(pError, "Block mutation failed: job payload bytes per block must be positive.");
    }
    
    const int aDerivedArchiveHeaderSize = Layout::ArchiveHeaderSize();
    const int aDerivedSectionHeaderSize = Layout::SectionHeaderSize();
    if (aDerivedArchiveHeaderSize <= 0 || aDerivedSectionHeaderSize <= 0) {
        return FailBlockMutation(pError, "Block mutation failed: invalid layout header sizes.");
    }
    
    const std::size_t aArchiveHeaderSize =
        static_cast<std::size_t>(aDerivedArchiveHeaderSize);
    const std::size_t aBlockSize = static_cast<std::size_t>(
        aDerivedSectionHeaderSize + pJob.mPayloadBytesPerBlock);
    
    const std::string aPath = pDrive.Normalize(pPath);
    if (!pDrive.HasFile(aPath)) {
        return FailBlockMutation(pError, "Block mutation failed: target path is not a file.");
    }
    
    const std::size_t aFileSize = pDrive.GetFileLength(aPath);
    if (aFileSize < (aArchiveHeaderSize + aBlockSize)) {
        return FailBlockMutation(pError, "Block mutation failed: file is too small to contain any full blocks.");
    }
    
    const std::size_t aDataBytes = aFileSize - aArchiveHeaderSize;
    const std::size_t aWholeBlockCount = aDataBytes / aBlockSize;
    if (aWholeBlockCount == 0u) {
        return FailBlockMutation(pError, "Block mutation failed: no complete blocks in target file.");
    }
    if (static_cast<std::size_t>(pBlockIndex) >= aWholeBlockCount) {
        return FailBlockMutation(
            pError,
            "Block mutation failed: block index is out of range for the target file.");
    }
    
    const std::size_t aBlockOffset =
        aArchiveHeaderSize + static_cast<std::size_t>(pBlockIndex) * aBlockSize;
    if (aBlockOffset > aFileSize || aBlockSize > (aFileSize - aBlockOffset)) {
        return FailBlockMutation(
            pError,
            "Block mutation failed: block index is out of range for the target file.");
    }
    
    if (pOutPath != nullptr) {
        *pOutPath = aPath;
    }
    if (pOutBlockOffset != nullptr) {
        *pOutBlockOffset = aBlockOffset;
    }
    if (pOutBlockSize != nullptr) {
        *pOutBlockSize = aBlockSize;
    }
    return true;
}

} // namespace

MockHardDrive::MockHardDrive() {
  mDirectories.insert("/");
}

void MockHardDrive::Clear() {
    mFiles.clear();
    mDirectories.clear();
    mDirectories.insert("/");
}
    
std::string MockHardDrive::Normalize(const std::string& pPath) const {
    
  if (pPath.empty()) {
    return "/";
  }
  std::vector<std::string> aStack;
  for (const std::string& aToken : SplitPathTokens(pPath)) {
    if (aToken.empty() || aToken == ".") {
      continue;
    }
    if (aToken == "..") {
      if (!aStack.empty()) {
        aStack.pop_back();
      }
      continue;
    }
    aStack.push_back(aToken);
  }

  if (aStack.empty()) {
    return "/";
  }

  std::string aNormalized = "/";
  for (std::size_t aIndex = 0; aIndex < aStack.size(); ++aIndex) {
    if (aIndex > 0) {
      aNormalized.push_back('/');
    }
    aNormalized += aStack[aIndex];
  }
  return aNormalized;
}

std::vector<std::string> MockHardDrive::SplitPathTokens(const std::string& pPath) const {
    std::vector<std::string> aTokens;
    std::string aToken;
    for (char aCharacter : pPath) {
        const char aNormalized = (aCharacter == '\\') ? '/' : aCharacter;
        if (aNormalized == '/') {
            if (!aToken.empty()) {
                aTokens.push_back(aToken);
                aToken.clear();
            }
            continue;
        }
        aToken.push_back(aNormalized);
    }
    if (!aToken.empty()) {
        aTokens.push_back(aToken);
    }
    return aTokens;
}

bool MockHardDrive::IsPathInSubtree(const std::string& pRootPath,
                                    const std::string& pPath,
                                    bool pIncludeRoot) const {
    if (pRootPath == "/") {
        return pIncludeRoot ? !pPath.empty() && pPath[0] == '/' : pPath != "/";
    }
    if (pPath == pRootPath) {
        return pIncludeRoot;
    }
    if (pPath.size() <= pRootPath.size()) {
        return false;
    }
    return pPath.compare(0, pRootPath.size(), pRootPath) == 0 &&
    pPath[pRootPath.size()] == '/';
}

bool MockHardDrive::RenamePath(const std::string& pOldPath,
                               const std::string& pNewPath) {
    const std::string aOldPath = Normalize(pOldPath);
    const std::string aNewPath = Normalize(pNewPath);
    if (aOldPath.empty() || aNewPath.empty()) {
        return false;
    }
    if (aOldPath == aNewPath) {
        return HasPath(aOldPath);
    }
    if (!HasPath(aOldPath)) {
        return false;
    }
    if (IsPathInSubtree(aOldPath, aNewPath, true)) {
        return false;
    }

    EnsureParents(aNewPath);

    const bool aOldIsDirectory = HasDirectory(aOldPath);
    const bool aOldIsFile = HasFile(aOldPath);
    const bool aNewIsDirectory = HasDirectory(aNewPath);
    const bool aNewIsFile = HasFile(aNewPath);

    if (aOldIsFile) {
        if (aNewIsDirectory) {
            return false;
        }
        const auto aSource = mFiles.find(aOldPath);
        if (aSource == mFiles.end()) {
            return false;
        }
        std::vector<unsigned char> aBytes = aSource->second;
        mFiles.erase(aSource);
        mFiles[aNewPath] = std::move(aBytes);
        return true;
    }

    if (!aOldIsDirectory) {
        return false;
    }
    if (aNewIsFile) {
        return false;
    }
    if (aNewIsDirectory && DirectoryHasEntries(aNewPath)) {
        return false;
    }

    std::vector<std::string> aDirectoriesToMove;
    for (const std::string& aDirectory : mDirectories) {
        if (aDirectory == aOldPath || IsPathInSubtree(aOldPath, aDirectory, false)) {
            aDirectoriesToMove.push_back(aDirectory);
        }
    }

    std::vector<std::pair<std::string, std::vector<unsigned char>>> aFilesToMove;
    for (const auto& aFile : mFiles) {
        if (aFile.first == aOldPath || IsPathInSubtree(aOldPath, aFile.first, false)) {
            aFilesToMove.push_back(aFile);
        }
    }

    if (aNewIsDirectory) {
        mDirectories.erase(aNewPath);
    }

    for (const std::string& aDirectory : aDirectoriesToMove) {
        mDirectories.erase(aDirectory);
    }
    for (const auto& aFile : aFilesToMove) {
        mFiles.erase(aFile.first);
    }

    for (const std::string& aDirectory : aDirectoriesToMove) {
        const std::string aSuffix =
            (aDirectory == aOldPath) ? std::string() : aDirectory.substr(aOldPath.size());
        mDirectories.insert(aNewPath + aSuffix);
    }
    for (const auto& aFile : aFilesToMove) {
        const std::string aSuffix =
            (aFile.first == aOldPath) ? std::string() : aFile.first.substr(aOldPath.size());
        mFiles[aNewPath + aSuffix] = aFile.second;
    }
    return true;
}

bool MockHardDrive::OverwriteFileRegion(const std::string& pPath,
                                            std::size_t pOffset,
                                            const unsigned char* pContents,
                                        std::size_t pLength) {
    
    if (!HasFile(pPath)) { return false; }
    if (pOffset < 0) {
        pLength += pOffset;
        pOffset = 0;
    }
    if (pLength <= 0) {
        return true;
    }
    
    const std::string aPath = Normalize(pPath);
    std::size_t aCeiling = pOffset + pLength;
    mFiles[aPath].reserve(aCeiling);
    while(mFiles[aPath].size() < aCeiling) {
        mFiles[aPath].push_back(0);
    }
    
    std::size_t aWriteIndex = pOffset;
    std::size_t aReadIndex = 0;
    while (aReadIndex < pLength) {
        mFiles[aPath][aWriteIndex] = pContents[aReadIndex];
        aWriteIndex++;
        aReadIndex++;
    }
    return true;
}

std::string MockHardDrive::JoinPath(const std::string& pLeft,
                                    const std::string& pRight) const {
    if (pRight.empty()) {
        return Normalize(pLeft);
    }
    if (pRight[0] == '/' || pRight[0] == '\\') {
        return Normalize(pRight);
    }
    const std::string aLeft = pLeft.empty() ? "/" : Normalize(pLeft);
    return Normalize(aLeft + "/" + pRight);
}

std::string MockHardDrive::JoinPath(const std::string& pLeft, const std::string& pMiddle, const std::string& pRight) const {
    string aLeftAndMiddle = JoinPath(pLeft, pMiddle);
    return JoinPath(aLeftAndMiddle, pRight);
}

std::string MockHardDrive::ParentPath(const std::string& pPath) const {
  const std::string aPath = Normalize(pPath);
  if (aPath == "/") {
    return "/";
  }
  const std::size_t aLastSlash = aPath.find_last_of('/');
  if (aLastSlash == 0) {
    return "/";
  }
  return aPath.substr(0, aLastSlash);
}

std::string MockHardDrive::FileName(const std::string& pPath) const {
  const std::string aPath = Normalize(pPath);
  if (aPath == "/") {
    return {};
  }
  const std::size_t aLastSlash = aPath.find_last_of('/');
  if (aLastSlash == std::string::npos) {
    return aPath;
  }
  return aPath.substr(aLastSlash + 1);
}

std::string MockHardDrive::StemName(const std::string& pPath) const {
  const std::string aName = FileName(pPath);
  if (aName.empty()) {
    return "archive";
  }
  const std::size_t aLastDot = aName.find_last_of('.');
  if (aLastDot == std::string::npos || aLastDot == 0) {
    return aName;
  }
  const std::string aStem = aName.substr(0, aLastDot);
  return aStem.empty() ? "archive" : aStem;
}

std::string MockHardDrive::Extension(const std::string& pPath) const {
  const std::string aName = FileName(pPath);
  const std::size_t aLastDot = aName.find_last_of('.');
  if (aLastDot == std::string::npos || aLastDot == 0) {
    return {};
  }
  return aName.substr(aLastDot);
}

bool MockHardDrive::HasPath(const std::string& pPath) const {
  return HasDirectory(pPath) || HasFile(pPath);
}

bool MockHardDrive::HasDirectory(const std::string& pPath) const {
  return mDirectories.find(Normalize(pPath)) != mDirectories.end();
}

bool MockHardDrive::HasFile(const std::string& pPath) const {
  return mFiles.find(Normalize(pPath)) != mFiles.end();
}

bool MockHardDrive::RemovePath(const std::string& pPath) {
  const std::string aPath = Normalize(pPath);
  if (aPath == "/") {
    Clear();
    return true;
  }

  bool aRemoved = false;
  const auto aFileIterator = mFiles.find(aPath);
  if (aFileIterator != mFiles.end()) {
    mFiles.erase(aFileIterator);
    return true;
  }

  if (!HasDirectory(aPath)) {
    return false;
  }

  for (auto aIterator = mFiles.begin(); aIterator != mFiles.end();) {
    if (aIterator->first == aPath || IsPathInSubtree(aPath, aIterator->first, false)) {
      aIterator = mFiles.erase(aIterator);
      aRemoved = true;
      continue;
    }
    ++aIterator;
  }

  for (auto aIterator = mDirectories.begin(); aIterator != mDirectories.end();) {
    if (*aIterator == aPath || IsPathInSubtree(aPath, *aIterator, false)) {
      aIterator = mDirectories.erase(aIterator);
      aRemoved = true;
      continue;
    }
    ++aIterator;
  }

  return aRemoved;
}

bool MockHardDrive::DeleteFile(const std::string& pPath) {
  const std::string aPath = Normalize(pPath);
  const auto aIterator = mFiles.find(aPath);
  if (aIterator == mFiles.end()) {
    return false;
  }
  mFiles.erase(aIterator);
  return true;
}

bool MockHardDrive::EnsureDirectory(const std::string& pPath) {
  const std::string aPath = Normalize(pPath);
  if (HasFile(aPath)) {
    return false;
  }
  EnsureParents(aPath);
  mDirectories.insert(aPath);
  return true;
}

bool MockHardDrive::ClearDirectory(const std::string& pPath) {
  const std::string aPath = Normalize(pPath);
  EnsureDirectory(aPath);

  if (aPath == "/") {
    mFiles.clear();
    mDirectories.clear();
    mDirectories.insert("/");
    return true;
  }

  for (auto aIterator = mFiles.begin(); aIterator != mFiles.end();) {
    if (IsPathInSubtree(aPath, aIterator->first, false)) {
      aIterator = mFiles.erase(aIterator);
      continue;
    }
    ++aIterator;
  }

  for (auto aIterator = mDirectories.begin(); aIterator != mDirectories.end();) {
    if (IsPathInSubtree(aPath, *aIterator, false)) {
      aIterator = mDirectories.erase(aIterator);
      continue;
    }
    ++aIterator;
  }

  return true;
}

bool MockHardDrive::DirectoryHasEntries(const std::string& pPath) const {
  const std::string aPath = Normalize(pPath);
  if (!HasDirectory(aPath)) {
    return false;
  }

  for (const std::string& aDirectory : mDirectories) {
    if (aDirectory == aPath) {
      continue;
    }
    if (ParentPath(aDirectory) == aPath) {
      return true;
    }
  }
  for (const auto& aFile : mFiles) {
    if (ParentPath(aFile.first) == aPath) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> MockHardDrive::ListFilesRecursive(const std::string& pRootPath) const {
  const std::string aRoot = Normalize(pRootPath);
  std::vector<std::string> aResult;
  if (!HasDirectory(aRoot)) {
    return aResult;
  }
  for (const auto& aFile : mFiles) {
    if (IsPathInSubtree(aRoot, aFile.first, false)) {
      aResult.push_back(aFile.first);
    }
  }
  std::sort(aResult.begin(), aResult.end());
  return aResult;
}

std::vector<std::string> MockHardDrive::ListDirectoriesRecursive(const std::string& pRootPath) const {
  const std::string aRoot = Normalize(pRootPath);
  std::vector<std::string> aResult;
  if (!HasDirectory(aRoot)) {
    return aResult;
  }
  for (const std::string& aDirectory : mDirectories) {
    if (IsPathInSubtree(aRoot, aDirectory, false)) {
      aResult.push_back(aDirectory);
    }
  }
  std::sort(aResult.begin(), aResult.end());
  return aResult;
}

std::vector<std::string> MockHardDrive::ListFiles(const std::string& pRootPath) const {
  const std::string aRoot = Normalize(pRootPath);
  std::vector<std::string> aResult;
  if (!HasDirectory(aRoot)) {
    return aResult;
  }
  for (const auto& aFile : mFiles) {
    if (ParentPath(aFile.first) == aRoot) {
      aResult.push_back(aFile.first);
    }
  }
  std::sort(aResult.begin(), aResult.end());
  return aResult;
}

std::vector<std::string> MockHardDrive::ListDirectoryEntries(const std::string& pRootPath) const {
  const std::string aRoot = Normalize(pRootPath);
  std::vector<std::string> aResult;
  if (!HasDirectory(aRoot)) {
    return aResult;
  }

  for (const std::string& aDirectory : mDirectories) {
    if (aDirectory == aRoot) {
      continue;
    }
    if (ParentPath(aDirectory) == aRoot) {
      aResult.push_back(aDirectory);
    }
  }
  for (const auto& aFile : mFiles) {
    if (ParentPath(aFile.first) == aRoot) {
      aResult.push_back(aFile.first);
    }
  }

  std::sort(aResult.begin(), aResult.end());
  return aResult;
}

std::size_t MockHardDrive::GetFileLength(const std::string& pPath) const {
  const auto aIterator = mFiles.find(Normalize(pPath));
  if (aIterator == mFiles.end()) {
    return 0;
  }
  return aIterator->second.size();
}

bool MockHardDrive::ReadFileBytes(const std::string& pPath,
                                  std::size_t pOffset,
                                  unsigned char* pDestination,
                                  std::size_t pLength) const {
  const auto aIterator = mFiles.find(Normalize(pPath));
  if (aIterator == mFiles.end()) {
    return false;
  }
  const std::vector<unsigned char>& aBytes = aIterator->second;
  if (pOffset > aBytes.size() || pLength > (aBytes.size() - pOffset)) {
    return false;
  }
  if (pLength == 0) {
    return true;
  }
  if (pDestination == nullptr) {
    return false;
  }
  std::copy(aBytes.begin() + static_cast<std::ptrdiff_t>(pOffset),
            aBytes.begin() + static_cast<std::ptrdiff_t>(pOffset + pLength),
            pDestination);
  return true;
}

bool MockHardDrive::ClearFileBytes(const std::string& pPath) {
  const std::string aPath = Normalize(pPath);
  if (HasDirectory(aPath)) {
    return false;
  }
  EnsureParents(aPath);
  mFiles[aPath].clear();
  return true;
}

bool MockHardDrive::AppendFileBytes(const std::string& pPath,
                                    const unsigned char* pData,
                                    std::size_t pLength) {
  const std::string aPath = Normalize(pPath);
  if (HasDirectory(aPath)) {
    return false;
  }
  EnsureParents(aPath);
  std::vector<unsigned char>& aBytes = mFiles[aPath];
  if (pLength == 0) {
    return true;
  }
  if (pData == nullptr) {
    return false;
  }
  aBytes.insert(aBytes.end(), pData, pData + pLength);
  return true;
}

void MockHardDrive::EnsureParents(const std::string& pPath) {
  std::string aParent = ParentPath(pPath);
  while (!aParent.empty()) {
    mDirectories.insert(aParent);
    if (aParent == "/") {
      break;
    }
    aParent = ParentPath(aParent);
  }
}

bool MockHardDrive::MangleBlock(const std::string& pPath,
                                int pBlockIndex,
                                const JobBundle &pJob,
                                ByteString *pError) {
    std::string aPath;
    std::size_t aBlockOffset = 0u;
    std::size_t aBlockSize = 0u;
    if (!ResolveBlockRange(*this,
                           pPath,
                           pBlockIndex,
                           pJob,
                           pError,
                           &aPath,
                           &aBlockOffset,
                           &aBlockSize)) {
        return false;
    }
    
    std::vector<unsigned char> &aBytes = mFiles[aPath];
    std::fill(aBytes.begin() + static_cast<std::ptrdiff_t>(aBlockOffset),
              aBytes.begin() + static_cast<std::ptrdiff_t>(aBlockOffset + aBlockSize),
              static_cast<unsigned char>(0xFF));
    return true;
}

bool MockHardDrive::DeleteBlock(const std::string& pPath,
                                int pBlockIndex,
                                const JobBundle &pJob,
                                ByteString *pError) {
    std::string aPath;
    std::size_t aBlockOffset = 0u;
    std::size_t aBlockSize = 0u;
    if (!ResolveBlockRange(*this,
                           pPath,
                           pBlockIndex,
                           pJob,
                           pError,
                           &aPath,
                           &aBlockOffset,
                           &aBlockSize)) {
        return false;
    }
    
    std::vector<unsigned char> &aBytes = mFiles[aPath];
    aBytes.erase(aBytes.begin() + static_cast<std::ptrdiff_t>(aBlockOffset),
                 aBytes.begin() + static_cast<std::ptrdiff_t>(aBlockOffset + aBlockSize));
    return true;
}

bool MockHardDrive::DeleteBlockGhost(const std::string& pPath,
                                     int pBlockIndex,
                                     const JobBundle &pJob,
                                     ByteString *pError) {
    std::string aPath;
    std::size_t aBlockOffset = 0u;
    std::size_t aBlockSize = 0u;
    if (!ResolveBlockRange(*this,
                           pPath,
                           pBlockIndex,
                           pJob,
                           pError,
                           &aPath,
                           &aBlockOffset,
                           &aBlockSize)) {
        return false;
    }
    
    std::vector<unsigned char> &aBytes = mFiles[aPath];
    if (aBytes.size() < aBlockSize) {
        return FailBlockMutation(pError, "DeleteBlockGhost failed: file too small.");
    }
    
    const std::vector<unsigned char> aTailBlock(
        aBytes.end() - static_cast<std::ptrdiff_t>(aBlockSize),
        aBytes.end());
    
    aBytes.erase(aBytes.begin() + static_cast<std::ptrdiff_t>(aBlockOffset),
                 aBytes.begin() + static_cast<std::ptrdiff_t>(aBlockOffset + aBlockSize));
    aBytes.insert(aBytes.end(), aTailBlock.begin(), aTailBlock.end());
    return true;
}

bool MockHardDrive::MangleFile(const std::string& pPath,
                               ByteString *pError) {
    const std::string aPath = Normalize(pPath);
    if (!HasFile(aPath)) {
        return FailBlockMutation(pError, "MangleFile failed: target path is not a file.");
    }
    
    std::vector<unsigned char> &aBytes = mFiles[aPath];
    std::fill(aBytes.begin(), aBytes.end(), static_cast<unsigned char>(0xFF));
    return true;
}

bool MockHardDrive::SwapBlocks(const std::string& pPath,
                               int pBlockIndexA,
                               int pBlockIndexB,
                               const JobBundle &pJob,
                               ByteString *pError) {
    return SwapBlocksByPath(pPath, pBlockIndexA, pPath, pBlockIndexB, pJob, pError);
}

bool MockHardDrive::SwapBlocksByPath(const std::string& pPathA,
                                     int pBlockIndexA,
                                     const std::string& pPathB,
                                     int pBlockIndexB,
                                     const JobBundle &pJob,
                                     ByteString *pError) {
    const std::string aPathA = Normalize(pPathA);
    const std::string aPathB = Normalize(pPathB);
    
    if ((aPathA == aPathB) && (pBlockIndexA == pBlockIndexB)) {
        return true;
    }
    
    std::string aResolvedPathA;
    std::size_t aOffsetA = 0u;
    std::size_t aBlockSizeA = 0u;
    if (!ResolveBlockRange(*this,
                           aPathA,
                           pBlockIndexA,
                           pJob,
                           pError,
                           &aResolvedPathA,
                           &aOffsetA,
                           &aBlockSizeA)) {
        return false;
    }
    
    std::string aResolvedPathB;
    std::size_t aOffsetB = 0u;
    std::size_t aBlockSizeB = 0u;
    if (!ResolveBlockRange(*this,
                           aPathB,
                           pBlockIndexB,
                           pJob,
                           pError,
                           &aResolvedPathB,
                           &aOffsetB,
                           &aBlockSizeB)) {
        return false;
    }
    
    if (aBlockSizeA != aBlockSizeB) {
        return FailBlockMutation(pError, "SwapBlocksByPath failed: block sizes differ.");
    }
    if (aBlockSizeA == 0u) {
        return FailBlockMutation(pError, "SwapBlocksByPath failed: block size was zero.");
    }
    
    if (aResolvedPathA == aResolvedPathB) {
        std::vector<unsigned char> &aBytes = mFiles[aResolvedPathA];
        const std::ptrdiff_t aBeginA = static_cast<std::ptrdiff_t>(aOffsetA);
        const std::ptrdiff_t aBeginB = static_cast<std::ptrdiff_t>(aOffsetB);
        const std::ptrdiff_t aBlockSize = static_cast<std::ptrdiff_t>(aBlockSizeA);
        
        if (aBeginA < 0 || aBeginB < 0 || aBlockSize <= 0) {
            return FailBlockMutation(pError, "SwapBlocksByPath failed: invalid swap range.");
        }
        if (static_cast<std::size_t>(aBeginA + aBlockSize) > aBytes.size() ||
            static_cast<std::size_t>(aBeginB + aBlockSize) > aBytes.size()) {
            return FailBlockMutation(pError, "SwapBlocksByPath failed: swap range out of bounds.");
        }
        
        for (std::ptrdiff_t i = 0; i < aBlockSize; ++i) {
            std::swap(aBytes[static_cast<std::size_t>(aBeginA + i)],
                      aBytes[static_cast<std::size_t>(aBeginB + i)]);
        }
        return true;
    }
    
    std::vector<unsigned char> &aBytesA = mFiles[aResolvedPathA];
    std::vector<unsigned char> &aBytesB = mFiles[aResolvedPathB];
    if ((aOffsetA + aBlockSizeA) > aBytesA.size() ||
        (aOffsetB + aBlockSizeB) > aBytesB.size()) {
        return FailBlockMutation(pError, "SwapBlocksByPath failed: swap range out of bounds.");
    }
    
    std::vector<unsigned char> aBufferA(aBytesA.begin() + static_cast<std::ptrdiff_t>(aOffsetA),
                                        aBytesA.begin() + static_cast<std::ptrdiff_t>(aOffsetA + aBlockSizeA));
    std::vector<unsigned char> aBufferB(aBytesB.begin() + static_cast<std::ptrdiff_t>(aOffsetB),
                                        aBytesB.begin() + static_cast<std::ptrdiff_t>(aOffsetB + aBlockSizeB));
    
    std::copy(aBufferB.begin(),
              aBufferB.end(),
              aBytesA.begin() + static_cast<std::ptrdiff_t>(aOffsetA));
    std::copy(aBufferA.begin(),
              aBufferA.end(),
              aBytesB.begin() + static_cast<std::ptrdiff_t>(aOffsetB));
    
    return true;
}
