//
//  FakeFile.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/3/26.
//

#ifndef FakeFile_hpp
#define FakeFile_hpp

#include "ByteString.hpp"

class FakeFile {
public:
    FakeFile();
    ~FakeFile();
    
    ByteString              mName;
    ByteString              mContent;
    bool                    mIsFolder;
    
    bool                    HasName(ByteString pName);
    bool                    HasContent(ByteString pContent);
    
    bool                    ToPayload(ByteString *pPayload, ByteString *pError);
    bool                    FromPayload(ByteString &pPayload, ByteString *pError);
    
    bool                    ToPreviewPayload(ByteString *pPayload, ByteString *pError);
    
    void                    RecoverTruncate(int pSize);
    void                    RecoverDelete();
    
    bool                    mIsRecoverDeleted;
    bool                    mIsRecoverPartial;
    
    bool                    mIsPartial;
    
};

#endif /* FakeFile_hpp */

