#ifndef STRING_PARSERS_H
#define STRING_PARSERS_H

namespace fallout {

typedef int(StringParserCallback)(char* string, int* valuePtr);

template <typename T>
int strParseInt(char** stringPtr, T* valuePtr)
{
    int temp;
    int result = strParseInt(stringPtr, &temp);
    *valuePtr = static_cast<T>(temp);
    return result;
}

int strParseInt(char** stringPtr, int* valuePtr);

template <typename T>
int strParseStrFromList(char** stringPtr, T* valuePtr, const char** list, int count)
{
    int temp;
    int result = strParseStrFromList(stringPtr, &temp, list, count);
    *valuePtr = static_cast<T>(temp);
    return result;
}

int strParseStrFromList(char** stringPtr, int* valuePtr, const char** list, int count);

int strParseStrFromFunc(char** stringPtr, int* valuePtr, StringParserCallback* callback);
int strParseIntWithKey(char** stringPtr, const char* key, int* valuePtr, const char* delimeter);
int strParseKeyValue(char** stringPtr, char* key, int* valuePtr, const char* delimeter);

} // namespace fallout

#endif /* STRING_PARSERS_H */
