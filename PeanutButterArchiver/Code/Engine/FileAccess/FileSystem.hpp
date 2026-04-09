#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "../../Knobs.hpp"
#include "FileReadStream.hpp"
#include "FileWriteStream.hpp"

namespace peanutbutter {

class ByteBufferV2 {
public:
    ByteBufferV2() = default;
    explicit ByteBufferV2(std::size_t pLength) {
        Resize(pLength);
    }
    ByteBufferV2(const ByteBufferV2& pOther) {
        *this = pOther;
    }
    ByteBufferV2& operator=(const ByteBufferV2& pOther) {
        if (this == &pOther) {
            return *this;
        }
        if (pOther.mLength == 0u) {
            Clear();
            return *this;
        }
        if (!Resize(pOther.mLength)) {
            Clear();
            return *this;
        }
        std::memcpy(Data(), pOther.Data(), pOther.mLength);
        return *this;
    }
    ByteBufferV2(ByteBufferV2&&) noexcept = default;
    ByteBufferV2& operator=(ByteBufferV2&&) noexcept = default;
    
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
    
    std::unique_ptr<unsigned char[]> mStorage;
    std::size_t mLength = 0u;
};

class FixedBlockBufferV2 {
public:
    static constexpr std::size_t kCapacity = knobs::kDefaultArchiveBlockBytesV2;
    // This type stores a full archive block inline (~1MB by default).
    // Prefer heap-backed ownership (e.g. as a member of a heap object)
    // and avoid stack allocation in thread-limited paths.

    FixedBlockBufferV2() = default;

    explicit FixedBlockBufferV2(std::size_t pLength) {
        (void)Resize(pLength);
    }

    FixedBlockBufferV2(const FixedBlockBufferV2& pOther) {
        *this = pOther;
    }

    FixedBlockBufferV2& operator=(const FixedBlockBufferV2& pOther) {
        if (this == &pOther) {
            return *this;
        }
        Clear();
        if (pOther.mBufferSize <= 0) {
            return *this;
        }
        const std::size_t aLength = static_cast<std::size_t>(pOther.mBufferSize);
        std::memcpy(mBuffer, pOther.mBuffer, aLength);
        mBufferSize = pOther.mBufferSize;
        return *this;
    }

    FixedBlockBufferV2(FixedBlockBufferV2&& pOther) noexcept {
        *this = std::move(pOther);
    }

    FixedBlockBufferV2& operator=(FixedBlockBufferV2&& pOther) noexcept {
        if (this == &pOther) {
            return *this;
        }
        Clear();
        if (pOther.mBufferSize > 0) {
            const std::size_t aLength = static_cast<std::size_t>(pOther.mBufferSize);
            std::memcpy(mBuffer, pOther.mBuffer, aLength);
            mBufferSize = pOther.mBufferSize;
        }
        pOther.Clear();
        return *this;
    }

    bool Resize(std::size_t pLength) {
        if (pLength > kCapacity) {
            return false;
        }

        const std::size_t aCurrent = Size();
        if (pLength > aCurrent) {
            std::memset(mBuffer + aCurrent, 0, pLength - aCurrent);
        }

        mBufferSize = static_cast<int>(pLength);
        return true;
    }

    void Clear() {
        mBufferSize = 0;
    }

    unsigned char* Data() {
        return mBuffer;
    }

    const unsigned char* Data() const {
        return mBuffer;
    }

    std::size_t Size() const {
        return mBufferSize <= 0 ? 0u : static_cast<std::size_t>(mBufferSize);
    }

    std::size_t Capacity() const {
        return kCapacity;
    }

    bool Empty() const {
        return mBufferSize <= 0;
    }

private:
    unsigned char mBuffer[kCapacity] = {};
    int mBufferSize = 0;
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
    virtual bool IsSymlink(const std::string& pPath) const {
        (void)pPath;
        return false;
    }
    virtual bool IsAlias(const std::string& pPath) const {
        (void)pPath;
        return false;
    }
    virtual bool RemovePath(const std::string& pPath) = 0;
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
    virtual std::vector<DirectoryEntryV2> ListDirectoryEntries(
                                                               const std::string& pRootPath) const = 0;
    virtual std::unique_ptr<FileReadStreamV2> OpenReadStream(
                                                             const std::string& pPath) const = 0;
    virtual std::unique_ptr<FileWriteStreamV2> OpenWriteStream(
                                                               const std::string& pPath) = 0;
    virtual std::unique_ptr<FileWriteStreamV2> OpenAppendStream(
                                                                 const std::string& pPath) = 0;
    virtual bool ResizeFile(const std::string& pPath,
                            std::uint64_t pLength) = 0;
    virtual bool AppendFile(const std::string& pPath,
                            const unsigned char* pContents,
                            std::size_t pLength) = 0;
    virtual bool OverwriteFileRegion(const std::string& pPath,
                                     std::size_t pOffset,
                                     const unsigned char* pContents,
                                     std::size_t pLength) = 0;
    virtual bool RenamePath(const std::string& pOldPath,
                            const std::string& pNewPath) = 0;
    virtual bool TryReadSymlinkTarget(const std::string& pPath,
                                      std::string& pOutTargetPath) const {
        (void)pPath;
        pOutTargetPath.clear();
        return false;
    }
    virtual bool TryReadAliasTarget(const std::string& pPath,
                                    std::string& pOutTargetPath) const {
        (void)pPath;
        pOutTargetPath.clear();
        return false;
    }
    virtual bool CreateSymlink(const std::string& pLinkPath,
                               const std::string& pTargetPath,
                               bool pTargetIsDirectory = false) {
        (void)pLinkPath;
        (void)pTargetPath;
        (void)pTargetIsDirectory;
        return false;
    }
    virtual bool CreateAlias(const std::string& pAliasPath,
                             const std::string& pTargetPath,
                             bool pTargetIsDirectory = false) {
        (void)pAliasPath;
        (void)pTargetPath;
        (void)pTargetIsDirectory;
        return false;
    }
    
    virtual std::string JoinPath(const std::string& pLeft,
                                 const std::string& pRight) const = 0;
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
