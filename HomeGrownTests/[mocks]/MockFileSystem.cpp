#include "MockFileSystem.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

MockFileSystem::MockFileSystem(MockHardDrive *pDrive) {
    mDrive = pDrive;
    mCWD = "/root/";
    mDrive->EnsureDirectory(mCWD);
}

std::string MockFileSystem::CurrentWorkingDirectory() const {
    return mCWD;
}

void MockFileSystem::Clear() {
    mDrive->Clear();
    mDrive->EnsureDirectory(mCWD);
}

bool MockFileSystem::Exists(const std::string& pPath) const {
    return mDrive->HasPath(pPath);
}

bool MockFileSystem::IsDirectory(const std::string& pPath) const {
    return mDrive->HasDirectory(pPath);
}

bool MockFileSystem::IsFile(const std::string& pPath) const {
    return mDrive->HasFile(pPath);
}

bool MockFileSystem::EnsureDirectory(const std::string& pPath) {
    return mDrive->EnsureDirectory(pPath);
}

bool MockFileSystem::ClearDirectory(const std::string& pPath) {
    return mDrive->ClearDirectory(pPath);
}

bool MockFileSystem::DirectoryHasEntries(const std::string& pPath) const {
    return mDrive->DirectoryHasEntries(pPath);
}

std::vector<DirectoryEntryV2> MockFileSystem::ListFilesRecursive(const std::string& pRootPath,
                                                                 const std::function<bool(std::size_t)>& pProgressCallback) const {
    std::vector<DirectoryEntryV2> aEntries;
    for (const std::string& aPath : mDrive->ListFilesRecursive(pRootPath)) {
        aEntries.push_back({aPath, RelativeToRoot(pRootPath, aPath), false});
    }
    return aEntries;
}

std::vector<DirectoryEntryV2> MockFileSystem::ListDirectoriesRecursive(const std::string& pRootPath,
                                                                       const std::function<bool(std::size_t)>& pProgressCallback) const {
    std::vector<DirectoryEntryV2> aEntries;
    for (const std::string& aPath : mDrive->ListDirectoriesRecursive(pRootPath)) {
        aEntries.push_back({aPath, RelativeToRoot(pRootPath, aPath), true});
    }
    return aEntries;
}

std::vector<DirectoryEntryV2> MockFileSystem::ListFiles(const std::string& pRootPath) const {
    std::vector<DirectoryEntryV2> aEntries;
    
    for (const std::string& aPath : mDrive->ListFiles(pRootPath)) {
        aEntries.push_back({aPath, RelativeToRoot(pRootPath, aPath), false});
    }
    return aEntries;
}

std::vector<DirectoryEntryV2> MockFileSystem::ListDirectoryEntries(const std::string& pRootPath) const {
    std::vector<DirectoryEntryV2> aEntries;

    for (const std::string& aPath : mDrive->ListDirectoryEntries(pRootPath)) {
        aEntries.push_back({aPath,
                            RelativeToRoot(pRootPath, aPath),
                            mDrive->HasDirectory(aPath)});
    }
    return aEntries;
}

std::unique_ptr<FileReadStreamV2> MockFileSystem::OpenReadStream(const std::string& pPath) const {
    return std::make_unique<MockFileReadStream>(mDrive, pPath);
}

std::unique_ptr<FileWriteStreamV2> MockFileSystem::OpenWriteStream(const std::string& pPath) {
    return std::make_unique<MockFileWriteStream>(mDrive, pPath);
}

bool MockFileSystem::AppendFile(const std::string& pPath,
                                const unsigned char* pContents,
                                std::size_t pLength) {
    return mDrive->AppendFileBytes(pPath, pContents, pLength);
    
}

bool MockFileSystem::OverwriteFileRegion(const std::string& pPath,
                                         std::size_t pOffset,
                                         const unsigned char* pContents,
                                         std::size_t pLength) {
    return mDrive->OverwriteFileRegion(pPath, pOffset, pContents, pLength);
}

bool MockFileSystem::RenamePath(const std::string& pOldPath,
                                const std::string& pNewPath) {
    
    return mDrive->RenamePath(pOldPath, pNewPath);
}

std::string MockFileSystem::JoinPath(const std::string& pLeft,
                                     const std::string& pRight) const {
    return mDrive->JoinPath(pLeft, pRight);
}

std::string MockFileSystem::ParentPath(const std::string& pPath) const {
    return mDrive->ParentPath(pPath);
}

std::string MockFileSystem::FileName(const std::string& pPath) const {
    return mDrive->FileName(pPath);
}

std::string MockFileSystem::StemName(const std::string& pPath) const {
    return mDrive->StemName(pPath);
}

std::string MockFileSystem::Extension(const std::string& pPath) const {
    return mDrive->Extension(pPath);
}

std::string MockFileSystem::RelativeToRoot(const std::string& pRootPath, const std::string& pPath) const {
    const std::string aRoot = mDrive->Normalize(pRootPath);
    const std::string aPath = mDrive->Normalize(pPath);
    if (aRoot == "/") {
        return aPath == "/" ? std::string() : aPath.substr(1);
    }
    if (aPath == aRoot) {
        return {};
    }
    if (aPath.size() > aRoot.size() + 1 &&
        aPath.compare(0, aRoot.size(), aRoot) == 0 &&
        aPath[aRoot.size()] == '/') {
        return aPath.substr(aRoot.size() + 1);
    }
    return FileName(aPath);
}
