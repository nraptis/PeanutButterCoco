//
//  WrappedArchiveAssembler.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/4/26.
//

#ifndef WrappedArchiveAssembler_hpp
#define WrappedArchiveAssembler_hpp

#include "WrappedArchive.hpp"
#include "MockFileSystem.hpp"
#include "ByteString.hpp"
#include "JobBundle.hpp"
#include <vector>
#include <string>

using namespace std;

class WrappedArchiveAssembler {
public:
    
    static vector<WrappedArchive>   Get(string pDirectory,
                                        MockFileSystem &pFileSystem,
                                        int pBlocksPerArchive,
                                        int pBytesPerBlock);
    
};

#endif /* WrappedArchiveAssembler_hpp */
