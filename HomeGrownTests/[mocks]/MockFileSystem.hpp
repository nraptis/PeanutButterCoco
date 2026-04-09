//
//  MockFileSystem.h
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 3/23/26.
//

#pragma once

#import "FileSystem.hpp"
#include "MockHardDrive.hpp"
#include "FileReadStream.hpp"
#include "FileWriteStream.hpp"
#include "ByteString.hpp"

using namespace peanutbutter;

class MockFileReadStream : public FileReadStreamV2 {
public:
    explicit MockFileReadStream(const MockHardDrive *pDrive, std::string pPath) {
        mDrive = pDrive;
        mPath = mDrive->Normalize(pPath);
        mReady = mDrive->HasFile(pPath);
        if (mReady) {
            mLength = mDrive->GetFileLength(mPath);
        } else {
            mLength = 0;
        }
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
        if (pLength == 0) {
            return true;
        }
        return mDrive->ReadFileBytes(mPath, pOffset, pDestination, pLength);
    }
    
private:
    const MockHardDrive *mDrive;
    std::string mPath;
    bool mReady = false;
    std::size_t mLength = 0;
};

class MockFileWriteStream : public FileWriteStreamV2 {
public:
    MockFileWriteStream(MockHardDrive *pDrive, std::string pPath, bool pTruncateExisting) {
        mDrive = pDrive;
        mPath = mDrive->Normalize(std::move(pPath));
        mReady = pTruncateExisting
            ? mDrive->ClearFileBytes(mPath)
            : mDrive->AppendFileBytes(mPath, nullptr, 0);
    }
    
    ~MockFileWriteStream() {
        
    }
    
    bool IsReady() const override {
        return mReady;
    }
    
    bool Write(const unsigned char* pData, std::size_t pLength) override {
        if (!mReady || mClosed) {
            return false;
        }
        if (pLength == 0) {
            return true;
        }
        if (pData == nullptr) {
            return false;
        }
        if (!mDrive->AppendFileBytes(mPath, pData, pLength)) {
            return false;
        }
        mBytesWritten += pLength;
        return true;
    }
    
    std::size_t GetBytesWritten() const override {
        return mBytesWritten;
    }
    
    bool Close() override {
        mClosed = true;
        return true;
    }
    
    std::string LastErrorMessage() const override {
        return "";
    }
    
private:
    MockHardDrive *mDrive;
    std::string mPath;
    bool mReady = false;
    bool mClosed = false;
    std::size_t mBytesWritten = 0;
};

class MockFileSystem final : public FileSystemV2 {
public:
    
    MockFileSystem(MockHardDrive *pDrive);
    
    void Clear();
    
    std::string CurrentWorkingDirectory() const override;
    bool Exists(const std::string& pPath) const override;
    bool IsDirectory(const std::string& pPath) const override;
    bool IsFile(const std::string& pPath) const override;
    bool RemovePath(const std::string& pPath) override;
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
    std::vector<DirectoryEntryV2> ListDirectoryEntries(
                                                       const std::string& pRootPath) const override;
    std::unique_ptr<FileReadStreamV2> OpenReadStream(
                                                     const std::string& pPath) const override;
    std::unique_ptr<FileWriteStreamV2> OpenWriteStream(
                                                       const std::string& pPath) override;
    std::unique_ptr<FileWriteStreamV2> OpenAppendStream(
                                                        const std::string& pPath) override;
    bool ResizeFile(const std::string& pPath,
                    std::uint64_t pLength) override;
    
    ByteString                        Load(const std::string& pPath);
    
    
    bool AppendFile(const std::string& pPath,
                    const unsigned char* pContents,
                    std::size_t pLength) override;
    bool OverwriteFileRegion(const std::string& pPath,
                             std::size_t pOffset,
                             const unsigned char* pContents,
                             std::size_t pLength) override;
    bool RenamePath(const std::string& pOldPath,
                    const std::string& pNewPath) override;
    
    std::string JoinPath(const std::string& pLeft,
                         const std::string& pRight) const override;
    
    std::string ParentPath(const std::string& pPath) const override;
    std::string FileName(const std::string& pPath) const override;
    std::string StemName(const std::string& pPath) const override;
    std::string Extension(const std::string& pPath) const override;
    
    std::string RelativeToRoot(const std::string& pRootPath, const std::string& pPath) const;
    
    MockHardDrive *mDrive;
    std::string mCWD;
};
