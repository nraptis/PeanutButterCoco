#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "FileReadStream.hpp"
#include "FileWriteStream.hpp"

namespace peanutbutter {

class ByteBufferV2 {
 public:
  ByteBufferV2() = default;
  explicit ByteBufferV2(std::size_t pLength) {
    Resize(pLength);
  }

  bool Resize(std::size_t pLength) {
    if (pLength == 0u) {
      Clear();
      return true;
    }

    std::unique_ptr<unsigned char[]> aStorage(
        new (std::nothrow) unsigned char[pLength] {});
    if (!aStorage) {
      return false;
    }

    mStorage = std::move(aStorage);
    mLength = pLength;
    return true;
  }

  void Clear() {
    mStorage.reset();
    mLength = 0u;
  }

  unsigned char* Data() {
    return mStorage.get();
  }

  const unsigned char* Data() const {
    return mStorage.get();
  }

  std::size_t Size() const {
    return mLength;
  }

  bool Empty() const {
    return mLength == 0u;
  }

 private:
  std::unique_ptr<unsigned char[]> mStorage;
  std::size_t mLength = 0u;
};

struct DirectoryEntryV2 {
  std::string mPath;
  std::string mRelativePath;
  bool mIsDirectory = false;
};

class FileSystemV2 {
 public:
  virtual ~FileSystemV2() = default;

  virtual std::string CurrentWorkingDirectory() const = 0;
  virtual bool Exists(const std::string& pPath) const = 0;
  virtual bool IsDirectory(const std::string& pPath) const = 0;
  virtual bool IsFile(const std::string& pPath) const = 0;
  virtual bool EnsureDirectory(const std::string& pPath) = 0;
  virtual bool ClearDirectory(const std::string& pPath) = 0;
  virtual bool DirectoryHasEntries(const std::string& pPath) const = 0;
  virtual std::vector<DirectoryEntryV2> ListFilesRecursive(
      const std::string& pRootPath,
      const std::function<bool(std::size_t)>& pProgressCallback = {}) const = 0;
  virtual std::vector<DirectoryEntryV2> ListDirectoriesRecursive(
      const std::string& pRootPath,
      const std::function<bool(std::size_t)>& pProgressCallback = {}) const = 0;
  virtual std::vector<DirectoryEntryV2> ListFiles(
      const std::string& pRootPath) const = 0;
  virtual std::unique_ptr<FileReadStreamV2> OpenReadStream(
      const std::string& pPath) const = 0;
  virtual std::unique_ptr<FileWriteStreamV2> OpenWriteStream(
      const std::string& pPath) = 0;
  virtual bool AppendFile(const std::string& pPath,
                          const unsigned char* pContents,
                          std::size_t pLength) = 0;
  virtual bool OverwriteFileRegion(const std::string& pPath,
                                   std::size_t pOffset,
                                   const unsigned char* pContents,
                                   std::size_t pLength) = 0;
  virtual bool RenamePath(const std::string& pOldPath,
                          const std::string& pNewPath) = 0;
  virtual bool RemovePath(const std::string& pPath) = 0;
  virtual std::string JoinPath(const std::string& pLeft,
                               const std::string& pRight) const = 0;
  virtual std::string RelativePathFrom(const std::string& pBasePath,
                                       const std::string& pPath) const = 0;
  virtual std::string ParentPath(const std::string& pPath) const = 0;
  virtual std::string FileName(const std::string& pPath) const = 0;
  virtual std::string StemName(const std::string& pPath) const = 0;
  virtual std::string Extension(const std::string& pPath) const = 0;

  bool ReadFile(const std::string& pPath, ByteBufferV2& pContents) const;
  bool ReadTextFile(const std::string& pPath, std::string& pContents) const;
  bool WriteFile(const std::string& pPath, const ByteBufferV2& pContents);
  bool WriteFile(const std::string& pPath,
                 const unsigned char* pContents,
                 std::size_t pLength);
  bool WriteTextFile(const std::string& pPath, const std::string& pContents);
  bool AppendTextFile(const std::string& pPath, const std::string& pContents);
};

}  // namespace peanutbutter
