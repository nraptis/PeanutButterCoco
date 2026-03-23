#pragma once

#include "FileSystem.hpp"

namespace peanutbutter {

class LocalFileSystemV2 final : public FileSystemV2 {
 public:
  std::string CurrentWorkingDirectory() const override;
  bool Exists(const std::string& pPath) const override;
  bool IsDirectory(const std::string& pPath) const override;
  bool IsFile(const std::string& pPath) const override;
  bool EnsureDirectory(const std::string& pPath) override;
  bool ClearDirectory(const std::string& pPath) override;
  bool DirectoryHasEntries(const std::string& pPath) const override;
  std::vector<DirectoryEntryV2> ListFilesRecursive(
      const std::string& pRootPath,
      const std::function<bool(std::size_t)>& pProgressCallback = {}) const override;
  std::vector<DirectoryEntryV2> ListDirectoriesRecursive(
      const std::string& pRootPath,
      const std::function<bool(std::size_t)>& pProgressCallback = {}) const override;
  std::vector<DirectoryEntryV2> ListFiles(
      const std::string& pRootPath) const override;
  std::unique_ptr<FileReadStreamV2> OpenReadStream(
      const std::string& pPath) const override;
  std::unique_ptr<FileWriteStreamV2> OpenWriteStream(
      const std::string& pPath) override;
  bool AppendFile(const std::string& pPath,
                  const unsigned char* pContents,
                  std::size_t pLength) override;
  bool OverwriteFileRegion(const std::string& pPath,
                           std::size_t pOffset,
                           const unsigned char* pContents,
                           std::size_t pLength) override;
  bool RenamePath(const std::string& pOldPath,
                  const std::string& pNewPath) override;
  bool RemovePath(const std::string& pPath) override;
  std::string JoinPath(const std::string& pLeft,
                       const std::string& pRight) const override;
  std::string RelativePathFrom(const std::string& pBasePath,
                               const std::string& pPath) const override;
  std::string ParentPath(const std::string& pPath) const override;
  std::string FileName(const std::string& pPath) const override;
  std::string StemName(const std::string& pPath) const override;
  std::string Extension(const std::string& pPath) const override;
};

}  // namespace peanutbutter
