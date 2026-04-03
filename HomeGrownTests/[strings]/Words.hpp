//
//  Words.hpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
//

#ifndef Words_hpp
#define Words_hpp

#include "namespaces.hpp"
#include "ByteString.hpp"

class Words {
    
public:
    
    static ByteString           GetRandomFileName(int pMinLength, int pMaxLength);
    static ByteString           GetRandomFileName(int pLength);
    
    static ByteString           GetRandomFileContent(int pMinLength, int pMaxLength);
    static ByteString           GetRandomFileContent(int pLength);
    
private:
    
    static string               GetRandomLetter();
    static string               GetRandomLetterOne();
    static vector<string>       GetRandomLetters(int pLength);
    static vector<string>       GetRandomLettersOne(int pLength);
    
    
};

extern vector<string> gTestLetters;
extern vector<char> gTestLettersOne;


#endif /* Words_hpp */
