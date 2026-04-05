//
//  Words.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/2/26.
//

#include "Words.hpp"
#include "Random.hpp"
#include "ByteMap.hpp"

vector<string> gTestLetters = {
    // --- Original "Fun" Categories ---
    "💡", "🏠", "🛸", "🌈", "⚙️", "🧬", "🚀", "⭐", "🤖", "⚡", // Emojis
    "龙", "大", "和", "明", "冰", "电", "力", "永", "人", "山", // Chinese
    "ぽ", "キ", "ぷ", "あ", "お", "ゐ", "め", "い", "ゑ", "の", // Japanese
    "ا", "ب", "ت", "ث", "ج",                               // Arabic
    "а", "б", "в", "г", "д",                               // Russian
    "Ω", "Ψ", "Ξ", "Φ", "Δ",                               // Greek

    // --- English Uppercase (A-Z) ---
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",

    // --- English Lowercase (a-z) ---
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
};

vector<char> gTestLettersOne = {
    
    // --- English Uppercase (A-Z) ---
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',

    // --- English Lowercase (a-z) ---
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

ByteString Words::GetRandomFileName(int pMinLength, int pMaxLength) {
    int aLength = Random::Get(pMinLength, pMaxLength);
    return GetRandomFileName(aLength);
}

ByteString Words::GetRandomFileName(int pLength) {
    if (pLength < 1) {
        ByteString aResult;
        string aLetter = GetRandomLetterOne();
        aResult.Set(aLetter);
        return aLetter;
    }
    vector<string> aLetters = GetRandomLetters(pLength - 1);
    if (aLetters.size() < 2) {
        string aLetter = GetRandomLetterOne();
        int aIndex = Random::Get((int)aLetters.size() + 1);
        aLetters.insert(aLetters.begin() + aIndex, aLetter);
    } else {
        string aLetter = string(1, '.');
        int aMaxIndex = (int)aLetters.size() - 1;
        int aIndex = Random::Get(1, aMaxIndex);
        aLetters.insert(aLetters.begin() + aIndex, aLetter);
    }
    ByteString aResult;
    for (int i=0;i<((int)aLetters.size());i++) {
        string aLetter = aLetters[i];
        aResult.Append(aLetter);
    }
    return aResult;
}

vector<ByteString> Words::GetRandomFileNames(int pCount, int pMinLength, int pMaxLength) {
    ByteMap aMap;
    vector<ByteString> aResult;
    int aTries = (pCount * 2 + 100);
    int aFudge = 0;
    while (((int)(aResult.size()) < pCount) && (aFudge < aTries)) {
        ByteString aString = GetRandomFileName(pMinLength, pMaxLength);
        if (aMap.Exists(aString) == false) {
            aMap.Add(aString);
            aResult.push_back(aString);
        }
        aFudge++;
    }
    return aResult;
}

vector<ByteString> Words::GetRandomFolderNames(int pCount, int pMinLength, int pMaxLength) {
    ByteMap aMap;
    vector<ByteString> aResult;
    int aTries = (pCount * 2 + 100);
    int aFudge = 0;
    while (((int)(aResult.size()) < pCount) && (aFudge < aTries)) {
        ByteString aString = GetRandomFileContent(pMinLength, pMaxLength);
        if (aMap.Exists(aString) == false) {
            aMap.Add(aString);
            aResult.push_back(aString);
        }
        aFudge++;
    }
    return aResult;
}

vector<FakeFile> Words::GetRandomFiles(int pCount, int pNameMinLength, int pNameMaxLength, int pContentMinLength, int pContentMaxLength) {
    vector<ByteString> aFileNames = GetRandomFileNames(pCount, pNameMinLength, pNameMaxLength);
    vector<FakeFile> aResult;
    
    for (int aIndex=0; aIndex<((int)aFileNames.size()); aIndex++) {
        FakeFile aFile;
        aFile.mName.Set(aFileNames[aIndex]);
        aFile.mContent.Set(GetRandomFileContent(pContentMinLength, pContentMaxLength));
        aResult.push_back(aFile);
    }
    return aResult;
}

vector<FakeFile> Words::GetRandomFolders(int pCount, int pNameMinLength, int pNameMaxLength) {
    vector<ByteString> aFolderNames = GetRandomFolderNames(pCount, pNameMinLength, pNameMaxLength);
    vector<FakeFile> aResult;
    
    for (int aIndex=0; aIndex<((int)aFolderNames.size()); aIndex++) {
        FakeFile aFile;
        aFile.mName.Set(aFolderNames[aIndex]);
        aFile.mIsFolder = true;
        aResult.push_back(aFile);
    }
    return aResult;
}

vector<FakeFile> Words::GetRandomFilesAndFolders(int pFileCount,
                                                 int pFileNameMinLength, int pFileNameMaxLength,
                                                 int pContentMinLength, int pContentMaxLength,
                                                 int pFolderCount,
                                                 int pFolderNameMinLength, int pFolderNameMaxLength) {
    
    ByteMap aMap;
    vector<FakeFile> aResult;
    int aTries = (pFileCount * 2 + 100);
    int aFudge = 0;
    while (((int)(aResult.size()) < pFileCount) && (aFudge < aTries)) {
        ByteString aName = GetRandomFileName(pFileNameMinLength, pFileNameMaxLength);
        
        if (aMap.Exists(aName) == false) {
            aMap.Add(aName);
        
            ByteString aContent = GetRandomFileContent(pContentMinLength, pContentMaxLength);
            
            FakeFile aFile;
            aFile.mName.Set(aName);
            aFile.mContent.Set(aContent);
            aFile.mIsFolder = false;
            aResult.push_back(aFile);
        }
        aFudge++;
    }
    
    aTries = (pFolderCount * 2 + 100);
    aFudge = 0;
    while (((int)(aResult.size()) < pFolderCount) && (aFudge < aTries)) {
        ByteString aName = GetRandomFileContent(pFolderNameMinLength, pFolderNameMaxLength);
        if (aMap.Exists(aName) == false) {
            aMap.Add(aName);
            FakeFile aFile;
            aFile.mName.Set(aName);
            aFile.mIsFolder = true;
            aResult.push_back(aFile);
        }
        aFudge++;
    }
    
    for (int aIndex=0; aIndex<((int)aResult.size()); aIndex++) {
        int aPick = Random::Get((int)aResult.size());
        if (aPick != aIndex) {
            swap(aResult[aPick], aResult[aIndex]);
        }
    }
    
    return aResult;
}

ByteString Words::GetRandomFileContent(int pMinLength, int pMaxLength) {
    int aLength = Random::Get(pMinLength, pMaxLength);
    return GetRandomFileContent(aLength);
}

ByteString Words::GetRandomFileContent(int pLength) {
    ByteString aResult;
    vector<string> aLetterList = GetRandomLetters(pLength);
    for (int i=0;i<((int)aLetterList.size());i++) {
        string aLetter = aLetterList[i];
        aResult.Append(aLetter);
    }
    return aResult;
}

string Words::GetRandomLetter() {
    int aIndex = Random::Get((int)gTestLetters.size());
    return gTestLetters[aIndex];
}

string Words::GetRandomLetterOne() {
    int aIndex = Random::Get((int)gTestLettersOne.size());
    char aCharacter = gTestLettersOne[aIndex];
    string aResult = string(1, aCharacter);
    return aResult;
}

vector<string> Words::GetRandomLetters(int pLength) {
    int aFailCount = 0;
    int aLength = 0;
    vector<string> aResult;
    while((aFailCount < 3) && (aLength < pLength)) {
        string aLetter = GetRandomLetter();
        int aLetterLength = (int)(aLetter.length());
        if ((aLength + aLetterLength) < pLength) {
            aLength += aLetterLength;
            aResult.push_back(aLetter);
        } else {
            aFailCount++;
        }
    }
    while(aLength < pLength) {
        string aLetter = GetRandomLetterOne();
        aResult.push_back(aLetter);
        aLength++;
    }
    return aResult;
}

vector<string> Words::GetRandomLettersOne(int pLength) {
    int aLength = 0;
    vector<string> aResult;
    while(aLength < pLength) {
        string aLetter = GetRandomLetterOne();
        aResult.push_back(aLetter);
        aLength++;
    }
    return aResult;
}
