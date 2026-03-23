#include "LocalFileSystem.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

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

std::string RelativeLocalPath(const std::string& pBasePath, const std::string& pPath) {
  if (pPath.empty()) {
    return {};
  }

  if (pBasePath.empty()) {
    return std::filesystem::path(pPath).lexically_normal().generic_string();
  }

  const std::filesystem::path aBase = std::filesystem::path(pBasePath).lexically_normal();
  const std::filesystem::path aPath = std::filesystem::path(pPath).lexically_normal();

  std::error_code aError;
  std::filesystem::path aRelative = std::filesystem::relative(aPath, aBase, aError);
  if (aError || aRelative.empty()) {
    aRelative = aPath.lexically_relative(aBase);
  }
  if (aRelative.empty()) {
    return aPath.generic_string();
  }
  return aRelative.generic_string();
}

std::string ParentLocalPath(const std::string& pPath) {
  return std::filesystem::path(pPath).parent_path().lexically_normal().generic_string();
}

std::string LocalFileName(const std::string& pPath) {
  return std::filesystem::path(pPath).filename().generic_string();
}

std::string LocalStemName(const std::string& pPath) {
  std::filesystem::path aPath(pPath);
  std::filesystem::path aStem = aPath.filename();
  if (aStem.empty()) {
    aStem = aPath.stem();
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
  return std::filesystem::current_path().lexically_normal().generic_string();
}

bool LocalFileSystemV2::Exists(const std::string& pPath) const {
  return std::filesystem::exists(std::filesystem::path(pPath));
}

bool LocalFileSystemV2::IsDirectory(const std::string& pPath) const {
  return std::filesystem::is_directory(std::filesystem::path(pPath));
}

bool LocalFileSystemV2::IsFile(const std::string& pPath) const {
  return std::filesystem::is_regular_file(std::filesystem::path(pPath));
}

bool LocalFileSystemV2::EnsureDirectory(const std::string& pPath) {
  std::error_code aError;
  std::filesystem::create_directories(std::filesystem::path(pPath), aError);
  return !aError && std::filesystem::is_directory(std::filesystem::path(pPath));
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
  if (!std::filesystem::is_directory(std::filesystem::path(pPath))) {
    return false;
  }
  return std::filesystem::directory_iterator(std::filesystem::path(pPath)) !=
         std::filesystem::directory_iterator();
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
  if (!std::filesystem::is_directory(aRoot)) {
    return aEntries;
  }

  for (const auto& aEntry : std::filesystem::directory_iterator(aRoot)) {
    if (!aEntry.is_regular_file()) {
      continue;
    }
    aEntries.push_back(
        {aEntry.path().lexically_normal().generic_string(),
         aEntry.path().filename().generic_string(),
         false});
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

bool LocalFileSystemV2::RemovePath(const std::string& pPath) {
  std::error_code aError;
  std::filesystem::remove_all(std::filesystem::path(pPath), aError);
  return !aError;
}

std::string LocalFileSystemV2::JoinPath(const std::string& pLeft,
                                        const std::string& pRight) const {
  return JoinLocalPath(pLeft, pRight);
}

std::string LocalFileSystemV2::RelativePathFrom(const std::string& pBasePath,
                                                const std::string& pPath) const {
  return RelativeLocalPath(pBasePath, pPath);
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
