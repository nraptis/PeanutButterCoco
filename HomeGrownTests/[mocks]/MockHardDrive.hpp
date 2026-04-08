//
//  MockHardDrive.hpp
//  PeanutButterArchiver
//
//  Created by Magneto on 3/23/26.
//

#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "ByteString.hpp"

using namespace std;

class JobBundle;

class MockHardDrive {
public:
    MockHardDrive();
    
    void Clear();
    
    vector<string> SplitPathTokens(const std::string& pPath) const;
    bool IsPathInSubtree(const std::string& pRootPath,
                         const std::string& pPath,
                         bool pIncludeRoot) const;
    
    std::string Normalize(const std::string& pPath) const;
    
    std::string JoinPath(const std::string& pLeft, const std::string& pRight) const;
    std::string JoinPath(const std::string& pLeft, const std::string& pMiddle, const std::string& pRight) const;
    
    std::string ParentPath(const std::string& pPath) const;
    std::string FileName(const std::string& pPath) const;
    std::string StemName(const std::string& pPath) const;
    std::string Extension(const std::string& pPath) const;
    
    bool RenamePath(const std::string& pOldPath, const std::string& pNewPath);
    
    bool OverwriteFileRegion(const std::string& pPath,
                                                std::size_t pOffset,
                                                const unsigned char* pContents,
                                             std::size_t pLength);
    
    bool HasPath(const std::string& pPath) const;
    bool HasDirectory(const std::string& pPath) const;
    bool HasFile(const std::string& pPath) const;
    bool RemovePath(const std::string& pPath);
    bool DeleteFile(const std::string& pPath);
    
    bool EnsureDirectory(const std::string& pPath);
    bool ClearDirectory(const std::string& pPath);
    bool DirectoryHasEntries(const std::string& pPath) const;
    std::vector<std::string> ListFilesRecursive(const std::string& pRootPath) const;
    std::vector<std::string> ListDirectoriesRecursive(const std::string& pRootPath) const;
    std::vector<std::string> ListFiles(const std::string& pRootPath) const;
    std::vector<std::string> ListDirectoryEntries(const std::string& pRootPath) const;
    
    std::size_t GetFileLength(const std::string& pPath) const;
    bool ReadFileBytes(const std::string& pPath,
                       std::size_t pOffset,
                       unsigned char* pDestination,
                       std::size_t pLength) const;
    bool ClearFileBytes(const std::string& pPath);
    bool AppendFileBytes(const std::string& pPath, const unsigned char* pData, std::size_t pLength);
    
    void EnsureParents(const std::string& pPath);
    
    std::map<std::string, std::vector<unsigned char>> mFiles;
    std::set<std::string> mDirectories;
    
    
    bool MangleBlock(const std::string& pPath,
                     int pBlockIndex,
                     const JobBundle &pJob,
                     ByteString *pError); // Make all bytes in this block FF.
    bool DeleteBlock(const std::string& pPath,
                     int pBlockIndex,
                     const JobBundle &pJob,
                     ByteString *pError); // Remove all bytes from this block; file becomes smaller.
    
    bool DeleteBlockGhost(const std::string& pPath,
                     int pBlockIndex,
                     const JobBundle &pJob,
                     ByteString *pError); // Remove this block, then append a duplicate of the original end block.
    
    bool MangleFile(const std::string& pPath,
                    ByteString *pError); // Make all file bytes FF.
    
    bool SwapBlocks(const std::string& pPath,
                    int pBlockIndexA,
                    int pBlockIndexB,
                    const JobBundle &pJob,
                    ByteString *pError); // Swap two full blocks in-place.
    
    bool SwapBlocksByPath(const std::string& pPathA,
                          int pBlockIndexA,
                          const std::string& pPathB,
                          int pBlockIndexB,
                          const JobBundle &pJob,
                          ByteString *pError); // Swap full blocks across one or two files.
    
    
    
    
    
    
};
