//
//  UnbundleVerify.cpp
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#include "UnbundleVerify.hpp"
#include "Layout.hpp"
#include "ByteMap.hpp"

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
