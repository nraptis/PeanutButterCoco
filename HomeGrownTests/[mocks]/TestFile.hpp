//
//  TestFile.hpp
//  HomeGrownTests
//
//  Created by Magneto on 3/24/26.
//

#ifndef TestFile_hpp
#define TestFile_hpp

#include <stdio.h>
#include <string>

class TestFile {
    
public:
    
    TestFile();
    TestFile(std::string pPath);
    TestFile(std::string pPath, std::string pContent);
    
    bool ContentsEqual(TestFile *pFile);
    
    std::string mPath;
    std::string mContent;
    
};

#endif /* TestFile_hpp */
