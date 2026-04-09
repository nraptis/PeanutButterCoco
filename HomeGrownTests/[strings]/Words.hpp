//
//  Words.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/2/26.
//

#ifndef Words_hpp
#define Words_hpp

#include "namespaces.hpp"
#include "ByteString.hpp"
#include "FakeFile.hpp"

class Words {
    
public:
    
    static ByteString           GetRandomFileName(int pMinLength, int pMaxLength);
    static ByteString           GetRandomFileName(int pLength);
    
    static ByteString           GetRandomFileContent(int pMinLength, int pMaxLength);
    static ByteString           GetRandomFileContent(int pLength);
    
    static vector<ByteString>   GetRandomFolderNames(int pCount, int pMinLength, int pMaxLength);
    
    static vector<ByteString>   GetRandomFileNames(int pCount, int pMinLength, int pMaxLength);
    static vector<FakeFile>     GetRandomFiles(int pCount, int pNameMinLength, int pNameMaxLength, int pContentMinLength, int pContentMaxLength);
    static vector<FakeFile>     GetRandomFolders(int pCount, int pNameMinLength, int pNameMaxLength);
    
    static vector<FakeFile>     GetRandomFilesAndFolders(int pFileCount,
                                                         int pFileNameMinLength, int pFileNameMaxLength,
                                                         int pContentMinLength, int pContentMaxLength,
                                                         int pFolderCount,
                                                         int pFolderNameMinLength, int pFolderNameMaxLength);
    
    
private:
    
    static string               GetRandomLetter();
    static string               GetRandomLetterOne();
    static vector<string>       GetRandomLetters(int pLength);
    static vector<string>       GetRandomLettersOne(int pLength);
    
    
};

extern vector<string> gTestLetters;
extern vector<char> gTestLettersOne;


#endif /* Words_hpp */
