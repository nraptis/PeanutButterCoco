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

using namespace std;

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
};
