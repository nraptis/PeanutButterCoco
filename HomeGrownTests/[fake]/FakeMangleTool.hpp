//
//  FakeMangleTool.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#ifndef FakeMangleTool_hpp
#define FakeMangleTool_hpp

#include "ByteString.hpp"
#include "FakeArchive.hpp"
#include "FakeFileBlockSpan.hpp"
#include "FakeFile.hpp"

#include <vector>

using namespace std;

class FakeMangleTool {
public:
    
    
    
    
    
    static bool                         MangleBlock(int pArchiveIndex,
                                                    int pBlockIndex,
                                                    vector<FakeFile> *pFileList,
                                                    vector<FakeArchive> *pArchiveList,
                                                    vector<FakeFileBlockSpan> *pSpanList,
                                                    ByteString *pError);
    static bool                         DeleteBlock(int pArchiveIndex,
                                                    int pBlockIndex,
                                                    vector<FakeFile> *pFileList,
                                                    vector<FakeArchive> *pArchiveList,
                                                    vector<FakeFileBlockSpan> *pSpanList,
                                                    ByteString *pError);
    
    
private:
    
    static bool                         FileByteRangeInBlock(FakeFile pFile,
                                                             int pArchiveUUID,
                                                             int pBlockUUID,
                                                             vector<FakeFileBlockSpan> *pSpanList,
                                                             int *pStartIndex,
                                                             int *pEndIndex);
    
    static int                          FileFirstByteInBlock(FakeFile pFile,
                                                             int pArchiveUUID,
                                                             int pBlockUUID,
                                                    vector<FakeFileBlockSpan> *pSpanList);
    static int                          FileLastByteInBlock(FakeFile pFile,
                                                            int pArchiveUUID,
                                                            int pBlockUUID,
                                                            vector<FakeFileBlockSpan> *pSpanList);
    
    
    

};

#endif /* FakeMangleTool_hpp */
