//
//  UnbundleVerify.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#include "UnbundleVerify.hpp"
#include "Layout.hpp"
#include "ByteMap.hpp"

namespace {
constexpr const char* kRecoverPartialPrefix = "$PARTIAL_";

bool DecodeRecoverRealName(const ByteString& pRealName,
                           ByteString* pOutDecodedName,
                           bool* pOutIsPartialByName) {
    if ((pOutDecodedName == NULL) || (pOutIsPartialByName == NULL)) {
        return false;
    }
    
    pOutDecodedName->Set(pRealName);
    *pOutIsPartialByName = false;
    
    const string aRealName = pRealName.ToString();
    const std::size_t aLastSlash = aRealName.find_last_of('/');
    const std::size_t aLeafStart =
        (aLastSlash == std::string::npos) ? 0u : (aLastSlash + 1u);
    const string aPrefix = kRecoverPartialPrefix;
    if (aRealName.compare(aLeafStart, aPrefix.size(), aPrefix) != 0) {
        return true;
    }
    
    string aDecoded = aRealName;
    aDecoded.erase(aLeafStart, aPrefix.size());
    pOutDecodedName->Set(aDecoded);
    *pOutIsPartialByName = true;
    return true;
}

bool NamesMatchDamagedExpectation(const ByteString& pRealName,
                                  const ByteString& pMockName,
                                  bool* pOutIsPartialByName) {
    if (pOutIsPartialByName == NULL) {
        return false;
    }
    
    ByteString aDecodedReal;
    bool aIsPartialByName = false;
    if (!DecodeRecoverRealName(pRealName, &aDecodedReal, &aIsPartialByName)) {
        return false;
    }
    
    if (aDecodedReal != pMockName) {
        *pOutIsPartialByName = false;
        return false;
    }
    
    *pOutIsPartialByName = aIsPartialByName;
    return true;
}

bool ByteStringIsPrefixOf(const ByteString& pFull, const ByteString& pPrefix) {
    if (pPrefix.mLength > pFull.mLength) {
        return false;
    }
    for (int aIndex=0; aIndex<pPrefix.mLength; aIndex++) {
        if (pFull.mData[aIndex] != pPrefix.mData[aIndex]) {
            return false;
        }
    }
    return true;
}

bool ByteStringsShareExactPrefixRun(const ByteString& pLeft, const ByteString& pRight) {
    return ByteStringIsPrefixOf(pLeft, pRight) || ByteStringIsPrefixOf(pRight, pLeft);
}

bool FileMatchesDamagedExpectation(const FakeFile &pReal,
                                   const FakeFile &pMock,
                                   bool pRealNameIndicatesPartial,
                                   ByteString *pMismatchReason) {
    bool aRealNameIndicatesPartial = false;
    if (!NamesMatchDamagedExpectation(pReal.mName,
                                      pMock.mName,
                                      &aRealNameIndicatesPartial)) {
        if (pMismatchReason != NULL) {
            ByteString aError = ByteString("Unbundle (damaged), real file {") + pReal.mName +
                                ByteString("} did not match mock file {") + pMock.mName +
                                ByteString("}.");
            pMismatchReason->Set(aError);
        }
        return false;
    }
    
    if (pReal.mIsFolder != pMock.mIsFolder) {
        if (pMismatchReason != NULL) {
            ByteString aError = ByteString("Unbundle (damaged), real file IsFolder(") +
                                ByteString(pReal.mIsFolder) +
                                ByteString(") did not match mock file IsFolder(") +
                                ByteString(pMock.mIsFolder) + ByteString(").");
            pMismatchReason->Set(aError);
        }
        return false;
    }
    
    if (pReal.mIsFolder) {
        return true;
    }
    
    const bool aTreatAsPartial = pMock.mIsRecoverPartial || aRealNameIndicatesPartial || pRealNameIndicatesPartial;
    if (aTreatAsPartial) {
        if (!ByteStringsShareExactPrefixRun(pReal.mContent, pMock.mContent)) {
            if (pMismatchReason != NULL) {
                ByteString aErrorA = ByteString("Unbundle (damaged), real partial content for file {") +
                                     pReal.mName + ByteString("} was {") + pReal.mContent +
                                     ByteString("} and mock content {") + pMock.mContent +
                                     ByteString("} do not share an exact prefix relationship.");
                pMismatchReason->Set(aErrorA);
            }
            return false;
        }
        return true;
    }
    
    if (pReal.mContent != pMock.mContent) {
        if (pMismatchReason != NULL) {
            ByteString aErrorA = ByteString("Unbundle (damaged), real file {") + pReal.mName +
                                 ByteString("} did not match mock file {") + pMock.mName +
                                 ByteString("}.");
            ByteString aErrorB = ByteString("\nUnbundle (damaged), real content {") + pReal.mContent +
                                 ByteString("} did not match mock content {") + pMock.mContent +
                                 ByteString("}.");
            pMismatchReason->Set(aErrorA + aErrorB);
        }
        return false;
    }
    
    return true;
}
} // namespace

bool UnbundleVerify::Execute(vector<FakeFile> &pFilesReal, vector<FakeFile> &pFilesMock, ByteString *pError) {
    
    FileMap aMapReal;
    for (auto aFile: pFilesReal) {
        aMapReal.Add(aFile.mName, aFile);
    }
    
    FileMap aMapMock;
    for (auto aFile: pFilesMock) {
        aMapMock.Add(aFile.mName, aFile);
    }
    
    int aMatchCount = 0;
    
    for (auto aFileA: pFilesReal) {
        
        FakeFile aFileB;
        if (!aMapMock.Get(aFileA.mName, &aFileB, pError)) {
            return false;
        }
        
        if (aFileA.mName != aFileB.mName) {
            if (pError != NULL) {
                ByteString aError = ByteString("Unbundle, real file {") + aFileA.mName + ByteString("} did not match mock file {") + aFileB.mName + ByteString("}.");
                pError->Set(aError);
            }
            return false;
        }
        
        if (aFileA.mIsFolder != aFileB.mIsFolder) {
            if (pError != NULL) {
                ByteString aError = ByteString("Unbundle, real file IsFolder(") + ByteString(aFileA.mIsFolder) + ByteString(") did not match mock file IsFolder(") + aFileB.mName + ByteString(").");
                pError->Set(aError);
            }
            return false;
        } else {
            
            if (aFileA.mIsFolder == false) {
                if (aFileA.mContent != aFileB.mContent) {
                    if (pError != NULL) {
                        ByteString aErrorA = ByteString("Unbundle, real file {") + aFileA.mName + ByteString("} did not match mock file {") + aFileB.mName + ByteString("}.");
                        ByteString aErrorB = ByteString("\nUnbundle, real content {") + aFileA.mContent + ByteString("} did not match mock content {") + aFileB.mContent + ByteString("}.");
                        pError->Set(aErrorA + aErrorB);
                    }
                    return false;
                }
            }
        }
        aMatchCount++;
    }
    
    for (auto aFile: pFilesReal) {
        if (aMapMock.Exists(aFile.mName) == false) {
            if (pError != NULL) {
                ByteString aError = ByteString("Unbundle, real file {") + aFile.mName + ByteString("} is missing from mock files.");
                pError->Set(aError);
            }
            return false;
        }
    }
    
    for (auto aFile: pFilesMock) {
        if (aMapReal.Exists(aFile.mName) == false) {
            if (pError != NULL) {
                ByteString aError = ByteString("Unbundle, mock file {") + aFile.mName + ByteString("} is missing from real files.");
                pError->Set(aError);
            }
            return false;
        }
    }
    
    //printf("There were %d matches...\n", aMatchCount);
    
    return true;
}

bool UnbundleVerify::Execute_Damaged(vector<FakeFile> &pFilesReal, vector<FakeFile> &pFilesMock, ByteString *pError) {
    vector<int> aMatchedMock;
    aMatchedMock.resize(pFilesMock.size(), 0);
    
    for (int aRealIndex=0; aRealIndex<((int)pFilesReal.size()); aRealIndex++) {
        const FakeFile &aReal = pFilesReal[aRealIndex];
        
        if (aReal.mIsRecoverDeleted) {
            continue;
        }
        
        int aMatchedMockIndex = -1;
        bool aHasDeletedCollision = false;
        bool aHadUnmatchedActiveCandidate = false;
        ByteString aFirstMismatch;
        bool aHasFirstMismatch = false;
        
        for (int aMockIndex=0; aMockIndex<((int)pFilesMock.size()); aMockIndex++) {
            const FakeFile &aMock = pFilesMock[aMockIndex];
            bool aRealNameIndicatesPartial = false;
            if (!NamesMatchDamagedExpectation(aReal.mName,
                                              aMock.mName,
                                              &aRealNameIndicatesPartial)) {
                continue;
            }
            
            if (aMock.mIsRecoverDeleted) {
                aHasDeletedCollision = true;
                continue;
            }
            
            if (aMatchedMock[aMockIndex] == 1) {
                continue;
            }
            
            aHadUnmatchedActiveCandidate = true;
            
            ByteString aMismatch;
            if (FileMatchesDamagedExpectation(aReal,
                                              aMock,
                                              aRealNameIndicatesPartial,
                                              &aMismatch)) {
                aMatchedMockIndex = aMockIndex;
                break;
            }
            
            if (!aHasFirstMismatch) {
                aFirstMismatch.Set(aMismatch);
                aHasFirstMismatch = true;
            }
        }
        
        if (aMatchedMockIndex >= 0) {
            aMatchedMock[aMatchedMockIndex] = 1;
            continue;
        }
        
        if (aHadUnmatchedActiveCandidate) {
            if (pError != NULL) {
                if (aHasFirstMismatch) {
                    pError->Set(aFirstMismatch);
                } else {
                    ByteString aError = ByteString("Unbundle (damaged), real file {") +
                                        aReal.mName +
                                        ByteString("} did not match any mock candidate.");
                    pError->Set(aError);
                }
            }
            return false;
        }
        
        if (aHasDeletedCollision) {
            // Deleted files are ignored in both directions, even under name collisions.
            continue;
        }
        
        if (pError != NULL) {
            ByteString aError = ByteString("Unbundle (damaged), real file {") + aReal.mName +
                                ByteString("} is missing from mock files.");
            pError->Set(aError);
        }
        return false;
    }
    
    for (int aMockIndex=0; aMockIndex<((int)pFilesMock.size()); aMockIndex++) {
        const FakeFile &aMock = pFilesMock[aMockIndex];
        
        if (aMock.mIsRecoverDeleted) {
            continue;
        }
        
        if (aMatchedMock[aMockIndex] == 1) {
            continue;
        }
        
        if (pError != NULL) {
            ByteString aError = ByteString("Unbundle (damaged), mock file {") + aMock.mName +
                                ByteString("} is missing from real files.");
            if (aMock.mIsRecoverPartial) {
                aError = aError + ByteString(" (expected exact partial prefix content.)");
            }
            pError->Set(aError);
        }
        return false;
    }
    
    return true;
}
