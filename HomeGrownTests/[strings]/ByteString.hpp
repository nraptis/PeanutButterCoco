#ifndef F_SILLY_STRING_H
#define F_SILLY_STRING_H

#include "namespaces.hpp"

class ByteString
{
public:
    
    ByteString();
    ByteString(const ByteString &pString);
    ByteString(const string &pString);
    ByteString(int pNumber);
    ByteString(float pNumber, int pDecimalCount);
    
    ~ByteString();
    
    unsigned char			            *mData;
    int								    mLength;
    int								    mSize;
    
    void					            Size(int pSize);
    
    void					            Free();
    void							    Clear();
    
    void                                Set(const unsigned char *pString, int pLength);
    void                                Set(const string &pString);
    void					            Set(const ByteString &pString);
    
    int                                 Compare(const unsigned char *pString, int pLength);
    int                                 Compare(const string &pString);
    int                                 Compare(const ByteString &pString);
    
    void                                Append(const unsigned char* pString, int pLength);
    void                                Append(const string &pString);
    void                                Append(const ByteString &pString);
        
    inline void                         ParseInt(int pNumber);
    inline void                         ParseFloat(float pFloat, int pDecimalCount);
    
    inline bool                         operator == (const ByteString &pString) { return (Compare(pString) == 0); }
    inline bool                         operator == (const string &pString) { return (Compare(pString) == 0); }
    
    inline void                         operator = (const ByteString &pString) { Set(pString); }
    inline void                         operator = (const string &pString) { Set(pString); }
    
    ByteString                          operator + (const ByteString &pRight) const;
    ByteString                          operator + (const string &pRight) const;

    //printf("%.*s\n", aByteString.mLength, (char*)aByteString.mData);
    
    
};


#endif
