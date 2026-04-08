#include "LocalFileSystem.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits.h>
#include <memory>
#include <string>
#include <utility>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace peanutbutter {
namespace {

std::string JoinLocalPath(const std::string& pLeft, const std::string& pRight) {
  if (pLeft.empty()) {
    return pRight;
  }
  if (pRight.empty()) {
    return pLeft;
  }
  return (std::filesystem::path(pLeft) / std::filesystem::path(pRight))
      .lexically_normal()
      .generic_string();
}

std::string ParentLocalPath(const std::string& pPath) {
  return std::filesystem::path(pPath).parent_path().lexically_normal().generic_string();
}

std::string LocalFileName(const std::string& pPath) {
  return std::filesystem::path(pPath).filename().generic_string();
}

std::string LocalStemName(const std::string& pPath) {
  std::filesystem::path aPath(pPath);
  std::filesystem::path aStem = aPath.stem();
  if (aStem.empty()) {
    aStem = aPath.filename();
  }
  return aStem.empty() ? "archive" : aStem.generic_string();
}

std::string LocalExtension(const std::string& pPath) {
  return std::filesystem::path(pPath).extension().generic_string();
}

std::string FormatErrnoMessage(int pErrnoValue) {
  if (pErrnoValue == 0) {
    return {};
  }
  const char* aText = std::strerror(pErrnoValue);
  if (aText == nullptr || aText[0] == '\0') {
    return "errno " + std::to_string(pErrnoValue);
  }
  return std::string(aText) + " (errno " + std::to_string(pErrnoValue) + ")";
}

bool RelativePathLess(const DirectoryEntryV2& pLeft,
                      const DirectoryEntryV2& pRight) {
  if (pLeft.mRelativePath != pRight.mRelativePath) {
    return pLeft.mRelativePath < pRight.mRelativePath;
  }
  return pLeft.mPath < pRight.mPath;
}

#if defined(__APPLE__)

CFURLRef CreateFileURLForPath(const std::string& pPath) {
  return CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault,
      reinterpret_cast<const UInt8*>(pPath.data()),
      static_cast<CFIndex>(pPath.size()),
      false);
}

bool TryResolveBookmarkFileTarget(const std::string& pBookmarkPath,
                                  std::string& pOutTargetPath) {
  pOutTargetPath.clear();
  if (pBookmarkPath.empty()) {
    return false;
  }

  CFURLRef aBookmarkFileUrl = CreateFileURLForPath(pBookmarkPath);
  if (aBookmarkFileUrl == nullptr) {
    return false;
  }

  CFDataRef aBookmarkData =
      CFURLCreateBookmarkDataFromFile(kCFAllocatorDefault, aBookmarkFileUrl, nullptr);
  CFRelease(aBookmarkFileUrl);
  if (aBookmarkData == nullptr) {
    return false;
  }

  Boolean aIsStale = false;
  CFURLRef aResolvedUrl = CFURLCreateByResolvingBookmarkData(kCFAllocatorDefault,
                                                             aBookmarkData,
                                                             0,
                                                             nullptr,
                                                             nullptr,
                                                             &aIsStale,
                                                             nullptr);
  CFRelease(aBookmarkData);
  if (aResolvedUrl == nullptr) {
    return false;
  }

  char aResolvedPath[PATH_MAX];
  const bool aOk = CFURLGetFileSystemRepresentation(
      aResolvedUrl, true, reinterpret_cast<UInt8*>(aResolvedPath), sizeof(aResolvedPath));
  CFRelease(aResolvedUrl);
  if (!aOk || aResolvedPath[0] == '\0') {
    return false;
  }

  pOutTargetPath.assign(aResolvedPath);
  return true;
}

bool TryWriteBookmarkAliasFile(const std::string& pAliasPath,
                               const std::string& pTargetPath) {
  if (pAliasPath.empty() || pTargetPath.empty()) {
    return false;
  }

  const std::filesystem::path aTargetAbsolutePath =
      std::filesystem::path(pTargetPath).is_absolute()
          ? std::filesystem::path(pTargetPath)
          : (std::filesystem::path(ParentLocalPath(pAliasPath)) /
             std::filesystem::path(pTargetPath))
                .lexically_normal();
  const std::string aTargetText = aTargetAbsolutePath.generic_string();
  if (aTargetText.empty()) {
    return false;
  }

  CFURLRef aTargetUrl = CreateFileURLForPath(aTargetText);
  if (aTargetUrl == nullptr) {
    return false;
  }

  CFDataRef aBookmarkData = nullptr;
  const CFOptionFlags aBookmarkOptions[] = {
      kCFURLBookmarkCreationSuitableForBookmarkFile,
      static_cast<CFOptionFlags>(kCFURLBookmarkCreationSuitableForBookmarkFile |
                                 kCFURLBookmarkCreationMinimalBookmarkMask),
      kCFURLBookmarkCreationMinimalBookmarkMask,
  };
  for (CFOptionFlags aOption : aBookmarkOptions) {
    aBookmarkData = CFURLCreateBookmarkData(kCFAllocatorDefault,
                                            aTargetUrl,
                                            aOption,
                                            nullptr,
                                            nullptr,
                                            nullptr);
    if (aBookmarkData != nullptr) {
      break;
    }
  }
  CFRelease(aTargetUrl);
  if (aBookmarkData == nullptr) {
    return false;
  }

  CFURLRef aAliasUrl = CreateFileURLForPath(pAliasPath);
  if (aAliasUrl == nullptr) {
    CFRelease(aBookmarkData);
    return false;
  }
  const bool aWritten =
      CFURLWriteBookmarkDataToFile(aBookmarkData, aAliasUrl, 0, nullptr);
  CFRelease(aAliasUrl);
  CFRelease(aBookmarkData);
  return aWritten;
}

#endif

class LocalFileReadStreamV2 final : public FileReadStreamV2 {
 public:
  explicit LocalFileReadStreamV2(std::string pPath)
      : mPath(std::move(pPath)) {
    const std::filesystem::path aPath(mPath);
    std::error_code aError;
    const std::uintmax_t aRawLength = std::filesystem::file_size(aPath, aError);
    if (aError) {
      return;
    }
    mInput.open(aPath, std::ios::binary);
    if (!mInput.is_open()) {
      return;
    }
    mLength = static_cast<std::size_t>(aRawLength);
    mReady = true;
  }

  bool IsReady() const override {
    return mReady;
  }

  std::size_t GetLength() const override {
    return mLength;
  }

  bool Read(std::size_t pOffset,
            unsigned char* pDestination,
            std::size_t pLength) const override {
    if (!mReady) {
      return false;
    }
    if (pOffset > mLength || pLength > (mLength - pOffset)) {
      return false;
    }
    if (pLength == 0u) {
      return true;
    }
    if (pDestination == nullptr) {
      return false;
    }
    if (!mInput.is_open()) {
      return false;
    }

    if (!mCursorValid || mCursor != pOffset) {
      mInput.clear();
      mInput.seekg(static_cast<std::streamoff>(pOffset), std::ios::beg);
      if (!mInput.good()) {
        mCursorValid = false;
        return false;
      }
    }

    mInput.read(reinterpret_cast<char*>(pDestination),
                static_cast<std::streamsize>(pLength));
    if (mInput.gcount() != static_cast<std::streamsize>(pLength)) {
      mCursorValid = false;
      return false;
    }

    mCursor = pOffset + pLength;
    mCursorValid = true;
    return true;
  }

 private:
  std::string mPath;
  mutable std::ifstream mInput;
  mutable std::size_t mCursor = 0u;
  mutable bool mCursorValid = false;
  std::size_t mLength = 0u;
  bool mReady = false;
};

class LocalFileWriteStreamV2 final : public FileWriteStreamV2 {
 public:
  explicit LocalFileWriteStreamV2(std::string pPath)
      : mPath(std::move(pPath)) {
    errno = 0;
    mOutput = std::fopen(mPath.c_str(), "wb");
    if (mOutput == nullptr) {
      mLastErrorMessage = FormatErrnoMessage(errno);
    }
  }

  LocalFileWriteStreamV2(std::string pPath, std::string pInitialError)
      : mPath(std::move(pPath)),
        mLastErrorMessage(std::move(pInitialError)) {}

  bool IsReady() const override {
    return mOutput != nullptr;
  }

  bool Write(const unsigned char* pData, std::size_t pLength) override {
    if (mOutput == nullptr) {
      return false;
    }
    if (pLength == 0u) {
      return true;
    }
    if (pData == nullptr) {
      mLastErrorMessage = "null write buffer";
      return false;
    }

    errno = 0;
    const std::size_t aWritten = std::fwrite(pData, 1u, pLength, mOutput);
    if (aWritten != pLength) {
      mLastErrorMessage = FormatErrnoMessage(errno);
      if (mLastErrorMessage.empty()) {
        mLastErrorMessage = "short write";
      }
      return false;
    }

    mBytesWritten += pLength;
    mLastErrorMessage.clear();
    return true;
  }

  std::size_t GetBytesWritten() const override {
    return mBytesWritten;
  }

  bool Close() override {
    if (mOutput == nullptr) {
      return true;
    }

    errno = 0;
    if (std::fclose(mOutput) != 0) {
      mOutput = nullptr;
      mLastErrorMessage = FormatErrnoMessage(errno);
      if (mLastErrorMessage.empty()) {
        mLastErrorMessage = "close failed";
      }
      return false;
    }

    mOutput = nullptr;
    mLastErrorMessage.clear();
    return true;
  }

  std::string LastErrorMessage() const override {
    return mLastErrorMessage;
  }

 private:
  std::string mPath;
  std::FILE* mOutput = nullptr;
  std::size_t mBytesWritten = 0u;
  std::string mLastErrorMessage;
};

}  // namespace

std::string LocalFileSystemV2::CurrentWorkingDirectory() const {
  std::error_code aError;
  const std::filesystem::path aPath = std::filesystem::current_path(aError);
  if (aError) {
    return ".";
  }
  return aPath.lexically_normal().generic_string();
}

bool LocalFileSystemV2::Exists(const std::string& pPath) const {
  std::error_code aError;
  const bool aExists = std::filesystem::exists(std::filesystem::path(pPath), aError);
  return !aError && aExists;
}

bool LocalFileSystemV2::IsDirectory(const std::string& pPath) const {
  std::error_code aError;
  const bool aIsDirectory =
      std::filesystem::is_directory(std::filesystem::path(pPath), aError);
  return !aError && aIsDirectory;
}

bool LocalFileSystemV2::IsFile(const std::string& pPath) const {
  std::error_code aError;
  const bool aIsFile =
      std::filesystem::is_regular_file(std::filesystem::path(pPath), aError);
  return !aError && aIsFile;
}

bool LocalFileSystemV2::IsSymlink(const std::string& pPath) const {
  std::error_code aError;
  const std::filesystem::file_status aStatus =
      std::filesystem::symlink_status(std::filesystem::path(pPath), aError);
  return !aError && std::filesystem::is_symlink(aStatus);
}

bool LocalFileSystemV2::IsAlias(const std::string& pPath) const {
#if defined(__APPLE__)
  if (!IsFile(pPath) || IsSymlink(pPath)) {
    return false;
  }
  std::string aResolvedTargetPath;
  return TryResolveBookmarkFileTarget(pPath, aResolvedTargetPath);
#else
  (void)pPath;
  return false;
#endif
}

bool LocalFileSystemV2::RemovePath(const std::string& pPath) {
  std::error_code aError;
  std::filesystem::remove_all(std::filesystem::path(pPath), aError);
  if (aError) {
    return false;
  }
  std::error_code aExistsError;
  return !std::filesystem::exists(std::filesystem::path(pPath), aExistsError) &&
         !aExistsError;
}

bool LocalFileSystemV2::EnsureDirectory(const std::string& pPath) {
  std::error_code aError;
  std::filesystem::create_directories(std::filesystem::path(pPath), aError);
  if (aError) {
    return false;
  }
  std::error_code aIsDirectoryError;
  return std::filesystem::is_directory(std::filesystem::path(pPath),
                                       aIsDirectoryError) &&
         !aIsDirectoryError;
}

bool LocalFileSystemV2::ClearDirectory(const std::string& pPath) {
  std::error_code aError;
  std::filesystem::remove_all(std::filesystem::path(pPath), aError);
  if (aError) {
    return false;
  }
  return EnsureDirectory(pPath);
}

bool LocalFileSystemV2::DirectoryHasEntries(const std::string& pPath) const {
  std::error_code aDirectoryError;
  if (!std::filesystem::is_directory(std::filesystem::path(pPath),
                                     aDirectoryError) ||
      aDirectoryError) {
    return false;
  }
  std::error_code aIteratorError;
  std::filesystem::directory_iterator aIterator(std::filesystem::path(pPath),
                                                aIteratorError);
  return !aIteratorError && aIterator != std::filesystem::directory_iterator();
}

std::vector<DirectoryEntryV2> LocalFileSystemV2::ListFilesRecursive(
    const std::string& pRootPath,
    const std::function<bool(std::size_t)>& pProgressCallback) const {
  std::vector<DirectoryEntryV2> aEntries;
  const std::filesystem::path aRoot(pRootPath);
  std::error_code aRootError;
  if (!std::filesystem::is_directory(aRoot, aRootError) || aRootError) {
    return aEntries;
  }

  std::error_code aIteratorError;
  std::filesystem::recursive_directory_iterator aIterator(
      aRoot,
      std::filesystem::directory_options::skip_permission_denied,
      aIteratorError);
  std::filesystem::recursive_directory_iterator aEnd;
  while (!aIteratorError && aIterator != aEnd) {
    const std::filesystem::directory_entry aEntry = *aIterator;
    std::error_code aEntryError;
    if (!aEntry.is_regular_file(aEntryError) || aEntryError) {
      aIterator.increment(aIteratorError);
      continue;
    }

    std::error_code aRelativeError;
    const std::filesystem::path aRelativePath =
        std::filesystem::relative(aEntry.path(), aRoot, aRelativeError);
    if (aRelativeError) {
      aIterator.increment(aIteratorError);
      continue;
    }

    aEntries.push_back(
        {aEntry.path().lexically_normal().generic_string(),
         aRelativePath.generic_string(),
         false});
    if (pProgressCallback && !pProgressCallback(aEntries.size())) {
      break;
    }
    aIterator.increment(aIteratorError);
  }

  std::sort(aEntries.begin(), aEntries.end(), &RelativePathLess);
  return aEntries;
}

std::vector<DirectoryEntryV2> LocalFileSystemV2::ListDirectoriesRecursive(
    const std::string& pRootPath,
    const std::function<bool(std::size_t)>& pProgressCallback) const {
  std::vector<DirectoryEntryV2> aEntries;
  const std::filesystem::path aRoot(pRootPath);
  std::error_code aRootError;
  if (!std::filesystem::is_directory(aRoot, aRootError) || aRootError) {
    return aEntries;
  }

  std::error_code aIteratorError;
  std::filesystem::recursive_directory_iterator aIterator(
      aRoot,
      std::filesystem::directory_options::skip_permission_denied,
      aIteratorError);
  std::filesystem::recursive_directory_iterator aEnd;
  while (!aIteratorError && aIterator != aEnd) {
    const std::filesystem::directory_entry aEntry = *aIterator;
    std::error_code aEntryError;
    if (!aEntry.is_directory(aEntryError) || aEntryError) {
      aIterator.increment(aIteratorError);
      continue;
    }

    std::error_code aRelativeError;
    const std::filesystem::path aRelativePath =
        std::filesystem::relative(aEntry.path(), aRoot, aRelativeError);
    if (aRelativeError) {
      aIterator.increment(aIteratorError);
      continue;
    }

    aEntries.push_back(
        {aEntry.path().lexically_normal().generic_string(),
         aRelativePath.generic_string(),
         true});
    if (pProgressCallback && !pProgressCallback(aEntries.size())) {
      break;
    }
    aIterator.increment(aIteratorError);
  }

  std::sort(aEntries.begin(), aEntries.end(), &RelativePathLess);
  return aEntries;
}

std::vector<DirectoryEntryV2> LocalFileSystemV2::ListFiles(
    const std::string& pRootPath) const {
  std::vector<DirectoryEntryV2> aEntries;
  const std::filesystem::path aRoot(pRootPath);
  std::error_code aRootError;
  if (!std::filesystem::is_directory(aRoot, aRootError) || aRootError) {
    return aEntries;
  }

  std::error_code aIteratorError;
  std::filesystem::directory_iterator aIterator(
      aRoot,
      std::filesystem::directory_options::skip_permission_denied,
      aIteratorError);
  std::filesystem::directory_iterator aEnd;
  while (!aIteratorError && aIterator != aEnd) {
    const std::filesystem::directory_entry aEntry = *aIterator;
    std::error_code aTypeError;
    if (!aEntry.is_regular_file(aTypeError) || aTypeError) {
      aIterator.increment(aIteratorError);
      continue;
    }
    aEntries.push_back(
        {aEntry.path().lexically_normal().generic_string(),
         aEntry.path().filename().generic_string(),
         false});
    aIterator.increment(aIteratorError);
  }

  std::sort(aEntries.begin(), aEntries.end(), &RelativePathLess);
  return aEntries;
}

std::vector<DirectoryEntryV2> LocalFileSystemV2::ListDirectoryEntries(
    const std::string& pRootPath) const {
  std::vector<DirectoryEntryV2> aEntries;
  const std::filesystem::path aRoot(pRootPath);
  std::error_code aRootError;
  if (!std::filesystem::is_directory(aRoot, aRootError) || aRootError) {
    return aEntries;
  }

  std::error_code aIteratorError;
  std::filesystem::directory_iterator aIterator(
      aRoot,
      std::filesystem::directory_options::skip_permission_denied,
      aIteratorError);
  std::filesystem::directory_iterator aEnd;
  while (!aIteratorError && aIterator != aEnd) {
    const std::filesystem::directory_entry aEntry = *aIterator;
    std::error_code aTypeError;
    const bool aIsDirectory = aEntry.is_directory(aTypeError);
    const bool aIsRegularFile = !aTypeError && aEntry.is_regular_file(aTypeError);
    if (!aTypeError && (aIsDirectory || aIsRegularFile)) {
      aEntries.push_back(
          {aEntry.path().lexically_normal().generic_string(),
           aEntry.path().filename().generic_string(),
           aIsDirectory});
    }
    aIterator.increment(aIteratorError);
  }

  std::sort(aEntries.begin(), aEntries.end(), &RelativePathLess);
  return aEntries;
}

std::unique_ptr<FileReadStreamV2> LocalFileSystemV2::OpenReadStream(
    const std::string& pPath) const {
  return std::make_unique<LocalFileReadStreamV2>(pPath);
}

std::unique_ptr<FileWriteStreamV2> LocalFileSystemV2::OpenWriteStream(
    const std::string& pPath) {
  const std::string aParent = ParentLocalPath(pPath);
  if (!aParent.empty() && !EnsureDirectory(aParent)) {
    return std::make_unique<LocalFileWriteStreamV2>(
        pPath, "failed creating parent directory for write stream");
  }
  return std::make_unique<LocalFileWriteStreamV2>(pPath);
}

bool LocalFileSystemV2::AppendFile(const std::string& pPath,
                                   const unsigned char* pContents,
                                   std::size_t pLength) {
  const std::string aParent = ParentLocalPath(pPath);
  if (!aParent.empty() && !EnsureDirectory(aParent)) {
    return false;
  }

  errno = 0;
  std::FILE* aOutput = std::fopen(pPath.c_str(), "ab");
  if (aOutput == nullptr) {
    return false;
  }

  bool aSucceeded = true;
  if (pLength > 0u) {
    if (pContents == nullptr) {
      aSucceeded = false;
    } else {
      const std::size_t aWritten = std::fwrite(pContents, 1u, pLength, aOutput);
      aSucceeded = aWritten == pLength;
    }
  }

  const bool aCloseSucceeded = std::fclose(aOutput) == 0;
  return aSucceeded && aCloseSucceeded;
}

bool LocalFileSystemV2::OverwriteFileRegion(const std::string& pPath,
                                            std::size_t pOffset,
                                            const unsigned char* pContents,
                                            std::size_t pLength) {
  if (pLength > 0u && pContents == nullptr) {
    return false;
  }

  errno = 0;
  std::FILE* aFile = std::fopen(pPath.c_str(), "r+b");
  if (aFile == nullptr) {
    return false;
  }

  bool aSucceeded =
      std::fseek(aFile, static_cast<long>(pOffset), SEEK_SET) == 0;
  if (aSucceeded && pLength > 0u) {
    const std::size_t aWritten = std::fwrite(pContents, 1u, pLength, aFile);
    aSucceeded = aWritten == pLength;
  }

  const bool aCloseSucceeded = std::fclose(aFile) == 0;
  return aSucceeded && aCloseSucceeded;
}

bool LocalFileSystemV2::RenamePath(const std::string& pOldPath,
                                   const std::string& pNewPath) {
  const std::string aParent = ParentLocalPath(pNewPath);
  if (!aParent.empty() && !EnsureDirectory(aParent)) {
    return false;
  }

  std::error_code aError;
  std::filesystem::rename(std::filesystem::path(pOldPath),
                          std::filesystem::path(pNewPath),
                          aError);
  return !aError;
}

bool LocalFileSystemV2::TryReadSymlinkTarget(
    const std::string& pPath,
    std::string& pOutTargetPath) const {
  pOutTargetPath.clear();
  std::error_code aError;
  const std::filesystem::path aTarget =
      std::filesystem::read_symlink(std::filesystem::path(pPath), aError);
  if (aError) {
    return false;
  }
  pOutTargetPath = aTarget.lexically_normal().generic_string();
  return !pOutTargetPath.empty();
}

bool LocalFileSystemV2::TryReadAliasTarget(
    const std::string& pPath,
    std::string& pOutTargetPath) const {
#if defined(__APPLE__)
  return TryResolveBookmarkFileTarget(pPath, pOutTargetPath);
#else
  (void)pPath;
  pOutTargetPath.clear();
  return false;
#endif
}

bool LocalFileSystemV2::CreateSymlink(const std::string& pLinkPath,
                                      const std::string& pTargetPath,
                                      bool pTargetIsDirectory) {
  (void)pTargetIsDirectory;
  const std::string aParent = ParentLocalPath(pLinkPath);
  if (!aParent.empty() && !EnsureDirectory(aParent)) {
    return false;
  }

  std::error_code aError;
  std::filesystem::create_symlink(std::filesystem::path(pTargetPath),
                                  std::filesystem::path(pLinkPath),
                                  aError);
  return !aError;
}

bool LocalFileSystemV2::CreateAlias(const std::string& pAliasPath,
                                    const std::string& pTargetPath,
                                    bool pTargetIsDirectory) {
  (void)pTargetIsDirectory;
  const std::string aParent = ParentLocalPath(pAliasPath);
  if (!aParent.empty() && !EnsureDirectory(aParent)) {
    return false;
  }
#if defined(__APPLE__)
  return TryWriteBookmarkAliasFile(pAliasPath, pTargetPath);
#else
  (void)pAliasPath;
  (void)pTargetPath;
  return false;
#endif
}

std::string LocalFileSystemV2::JoinPath(const std::string& pLeft,
                                        const std::string& pRight) const {
  return JoinLocalPath(pLeft, pRight);
}

std::string LocalFileSystemV2::ParentPath(const std::string& pPath) const {
  return ParentLocalPath(pPath);
}

std::string LocalFileSystemV2::FileName(const std::string& pPath) const {
  return LocalFileName(pPath);
}

std::string LocalFileSystemV2::StemName(const std::string& pPath) const {
  return LocalStemName(pPath);
}

std::string LocalFileSystemV2::Extension(const std::string& pPath) const {
  return LocalExtension(pPath);
}

}  // namespace peanutbutter
