#include "ByteString.hpp"

ByteString::ByteString() {
    mData = NULL; mLength = 0; mSize = 0;
}

ByteString::ByteString(const ByteString &pString) {
    mData = NULL; mLength = 0; mSize = 0;
    Set(pString);
}

ByteString::ByteString(const string &pString) {
    mData = NULL; mLength = 0; mSize = 0;
    Set(pString);
}

ByteString::ByteString(int pNumber) {
    mData = NULL; mLength = 0; mSize = 0;
    ParseInt(pNumber);
}

ByteString::ByteString(float pNumber, int pDecimalCount) {
    mData = NULL; mLength = 0; mSize = 0;
    ParseFloat(pNumber, pDecimalCount);
}

ByteString::~ByteString() {
    Free();
}

void ByteString::Free() {
    if (mData != NULL) {
        delete [] mData;
        mData = nullptr;
    }
    mLength = 0;
    mSize = 0;
}

void ByteString::Clear() {
    mLength = 0;
}

void ByteString::Size(int pSize) {
    if (pSize <= 0) {
        Free();
    } else if (pSize != mSize) {
        mSize = pSize;
        unsigned char *aNew = new unsigned char[mSize];
        
        // If our current content is longer than the new buffer, truncate it
        if (mLength > mSize) {
            mLength = mSize;
        }

        // Copy existing data to the new buffer
        for (int i = 0; i < mLength; i++) {
            aNew[i] = mData[i];
        }

        delete[] mData;
        mData = aNew;
    }
}

void ByteString::Set(const unsigned char *pString, int pLength) {
    if (pString == NULL) {
        mLength = 0;
        return;
    }
    if (pLength <= 0) {
        mLength = 0;
        return;
    }

    // Only reallocate if the new string literally won't fit in the current mSize
    if (pLength > mSize) {
        Size(pLength);
    }

    for (int i = 0; i < pLength; i++) {
        mData[i] = pString[i];
    }
    mLength = pLength;
}

void ByteString::Set(const string &pString) {
    /*
       c_str() and UTF-8:
       - "asdf": c_str() points to ['a','s','d','f','\0'].
         pString.length() is 4. We copy those 4 bytes.
       
       - "大": This character is 3 bytes in UTF-8 (0xE5, 0xA4, 0xA7).
         c_str() points to [0xE5, 0xA4, 0xA7, '\0'].
         pString.length() is 3. We copy all 3 bytes.
    */
    Set((const unsigned char*)pString.c_str(), (int)pString.length());
}

void ByteString::Set(const ByteString &pString) {
    if (this == &pString) {
        return;
    }
    Set(pString.mData, pString.mLength);
}

int ByteString::Compare(const unsigned char *pString, int pLength) {
    int aShortest = (mLength < pLength) ? mLength : pLength;
    for (int i = 0; i < aShortest; i++) {
        if (mData[i] < pString[i]) return -1;
        if (mData[i] > pString[i]) return 1;
    }
    if (mLength < pLength) return -1;
    if (mLength > pLength) return 1;
    return 0;
}

int ByteString::Compare(const string &pString) {
    return Compare((const unsigned char*)pString.c_str(), (int)pString.length());
}

int ByteString::Compare(const ByteString &pString) {
    return Compare(pString.mData, pString.mLength);
}

void ByteString::Append(const unsigned char* pString, int pLength) {
    if (!pString || pLength <= 0) return;
    
    int newLength = mLength + pLength;
    if (newLength > mSize) {
        Size(newLength); // Grow buffer if needed
    }
    
    for (int i = 0; i < pLength; i++) {
        mData[mLength + i] = pString[i];
    }
    mLength = newLength;
}

void ByteString::Append(const string &pString) {
    Append((const unsigned char*)pString.c_str(), (int)pString.length());
}

void ByteString::Append(const ByteString &pString) {
    Append(pString.mData, pString.mLength);
}

void ByteString::ParseInt(int pNumber) {
    // 12 bytes is enough for "-2147483648" and a null terminator
    char aBuffer[16];
    int aWritePos = 0;
    
    // Handle 0 explicitly
    if (pNumber == 0) {
        aBuffer[0] = '0';
        Set((const unsigned char*)aBuffer, 1);
        return;
    }

    // Use a long long to safely handle the "negative limit" case (-2147483648)
    long long n = pNumber;
    if (n < 0) {
        aBuffer[aWritePos++] = '-';
        n = -n;
    }

    // Find where the number ends so we can fill the buffer backwards
    long long temp = n;
    while (temp > 0) {
        temp /= 10;
        aWritePos++;
    }

    int aTotalLength = aWritePos;
    
    // Fill the buffer from right to left
    int i = aTotalLength - 1;
    while (n > 0) {
        aBuffer[i--] = (char)('0' + (n % 10));
        n /= 10;
    }

    // Set the internal mData using our local buffer
    Set((const unsigned char*)aBuffer, aTotalLength);
}

void ByteString::ParseFloat(float pFloat, int pDecimalCount) {
    // 64 bytes is plenty for any float in xxx.yyy notation
    unsigned char aBuffer[64];
    int aWritePos = 0;

    if (pFloat < 0.0f) {
        aBuffer[aWritePos++] = '-';
        pFloat = -pFloat;
    }

    // Cap at 100 million as per your original logic
    if (pFloat > 100000000.0f) {
        pFloat = 100000000.0f;
    }

    // Use double for math to avoid float precision drift (0.57 -> 0.5699)
    double aVal = (double)pFloat;
    
    // Rounding logic: add 0.5 of the last visible decimal place
    double aRounding = 0.5;
    for (int i = 0; i < pDecimalCount; ++i) aRounding /= 10.0;
    aVal += aRounding;

    long long aWholePart = (long long)aVal;
    double aFractionPart = aVal - (double)aWholePart;

    // 1. Write Whole Number (Backwards then Flip)
    if (aWholePart == 0) {
        aBuffer[aWritePos++] = '0';
    } else {
        int aDigitsStart = aWritePos;
        while (aWholePart > 0) {
            aBuffer[aWritePos++] = (unsigned char)('0' + (aWholePart % 10));
            aWholePart /= 10;
        }
        // Flip the whole number digits
        for (int i = 0; (aDigitsStart + i) < (aWritePos - 1 - i); ++i) {
            unsigned char temp = aBuffer[aDigitsStart + i];
            aBuffer[aDigitsStart + i] = aBuffer[aWritePos - 1 - i];
            aBuffer[aWritePos - 1 - i] = temp;
        }
    }

    // 2. Write Decimal Point and Fraction
    if (pDecimalCount > 0) {
        aBuffer[aWritePos++] = '.';
        for (int i = 0; i < pDecimalCount; ++i) {
            aFractionPart *= 10.0;
            int aDigit = (int)aFractionPart;
            aBuffer[aWritePos++] = (unsigned char)('0' + aDigit);
            aFractionPart -= aDigit;
        }
    }

    // 3. Commit to UString
    Set(aBuffer, aWritePos);
}

string ByteString::ToString() const {
    return string((const char*)mData, mLength);
}

// Inside the ByteString class:
ByteString ByteString::operator+(const ByteString &pRight) const {
    ByteString result = *this;
    result.Append(pRight);
    return result;
}

ByteString ByteString::operator+(const string &pRight) const {
    ByteString result = *this;
    result.Append(pRight);
    return result;
}
