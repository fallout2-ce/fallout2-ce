#include "string_parsers.h"

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "platform_compat.h"

namespace fallout {

// strParseInt
// 0x4AFD10
int strParseInt(char** stringPtr, int* valuePtr)
{
    char *str, *remaining_str;
    size_t leadingSpacesLength, tokenLength, consumedLength;
    char savedDelimiter;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    compat_strlwr(str);

    leadingSpacesLength = strspn(str, " ");
    str += leadingSpacesLength;

    tokenLength = strcspn(str, ",");
    consumedLength = leadingSpacesLength + tokenLength;

    remaining_str = *stringPtr + consumedLength;
    *stringPtr = remaining_str;

    if (*remaining_str != '\0') {
        *stringPtr = remaining_str + 1;
    }

    if (tokenLength != 0) {
        savedDelimiter = *(str + tokenLength);
        *(str + tokenLength) = '\0';
    }

    *valuePtr = atoi(str);

    if (tokenLength != 0) {
        *(str + tokenLength) = savedDelimiter;
    }

    return 0;
}

// strParseStrFromList
// 0x4AFE08
int strParseStrFromList(char** stringPtr, int* valuePtr, const char** stringList, int stringListLength)
{
    int i;
    char *str, *remaining_str;
    size_t leadingSpacesLength, tokenLength, consumedLength;
    char savedDelimiter;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    compat_strlwr(str);

    leadingSpacesLength = strspn(str, " ");
    str += leadingSpacesLength;

    tokenLength = strcspn(str, ",");
    consumedLength = leadingSpacesLength + tokenLength;

    remaining_str = *stringPtr + consumedLength;
    *stringPtr = remaining_str;

    if (*remaining_str != '\0') {
        *stringPtr = remaining_str + 1;
    }

    if (tokenLength != 0) {
        savedDelimiter = *(str + tokenLength);
        *(str + tokenLength) = '\0';
    }

    for (i = 0; i < stringListLength; i++) {
        if (compat_stricmp(str, stringList[i]) == 0) {
            break;
        }
    }

    if (tokenLength != 0) {
        *(str + tokenLength) = savedDelimiter;
    }

    if (i == stringListLength) {
        debugPrint("\nstrParseStrFromList Error: Couldn't find match for string: %s!", str);
        *valuePtr = -1;
        return -1;
    }

    *valuePtr = i;

    return 0;
}

// strParseStrFromFunc
// 0x4AFEDC
int strParseStrFromFunc(char** stringPtr, int* valuePtr, StringParserCallback* callback)
{
    char *str, *remaining_str;
    size_t leadingSpacesLength, tokenLength, consumedLength;
    char savedDelimiter;
    int result;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    compat_strlwr(str);

    leadingSpacesLength = strspn(str, " ");
    str += leadingSpacesLength;

    tokenLength = strcspn(str, ",");
    consumedLength = leadingSpacesLength + tokenLength;

    remaining_str = *stringPtr + consumedLength;
    *stringPtr = remaining_str;

    if (*remaining_str != '\0') {
        *stringPtr = remaining_str + 1;
    }

    if (tokenLength != 0) {
        savedDelimiter = *(str + tokenLength);
        *(str + tokenLength) = '\0';
    }

    result = callback(str, valuePtr);

    if (tokenLength != 0) {
        *(str + tokenLength) = savedDelimiter;
    }

    if (result != 0) {
        debugPrint("\nstrParseStrFromFunc Error: Couldn't find match for string: %s!", str);
        *valuePtr = -1;
        return -1;
    }

    return 0;
}

// 0x4AFF7C
int strParseIntWithKey(char** stringPtr, const char* key, int* valuePtr, const char* delimeter)
{
    char* str;
    size_t leadingSpacesLength, segmentLength, consumedLength, keyLength;
    char savedSegmentDelimiter, savedKeyDelimiter;
    int result;

    result = -1;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    if (*str == '\0') {
        return -1;
    }

    compat_strlwr(str);

    if (*str == ',') {
        str++;
        *stringPtr = *stringPtr + 1;
    }

    leadingSpacesLength = strspn(str, " ");
    str += leadingSpacesLength;

    segmentLength = strcspn(str, ",");
    consumedLength = leadingSpacesLength + segmentLength;

    savedSegmentDelimiter = *(str + segmentLength);
    *(str + segmentLength) = '\0';

    keyLength = strcspn(str, delimeter);

    savedKeyDelimiter = *(str + keyLength);
    *(str + keyLength) = '\0';

    if (strcmp(str, key) == 0) {
        *stringPtr = *stringPtr + consumedLength;
        *valuePtr = atoi(str + keyLength + 1);
        result = 0;
    }

    *(str + keyLength) = savedKeyDelimiter;
    *(str + segmentLength) = savedSegmentDelimiter;

    if (**stringPtr == ',') {
        *stringPtr = *stringPtr + 1;
    }

    return result;
}

// 0x4B005C
int strParseKeyValue(char** stringPtr, char* key, int* valuePtr, const char* delimiter)
{
    char* str;
    size_t leadingSpacesLength, segmentLength, consumedLength, keyLength;
    char savedSegmentDelimiter, savedKeyDelimiter;

    if (*stringPtr == nullptr) {
        return 0;
    }

    str = *stringPtr;

    if (*str == '\0') {
        return -1;
    }

    compat_strlwr(str);

    if (*str == ',') {
        str++;
        *stringPtr = *stringPtr + 1;
    }

    leadingSpacesLength = strspn(str, " ");
    str += leadingSpacesLength;

    segmentLength = strcspn(str, ",");
    consumedLength = leadingSpacesLength + segmentLength;

    savedSegmentDelimiter = *(str + segmentLength);
    *(str + segmentLength) = '\0';

    keyLength = strcspn(str, delimiter);

    savedKeyDelimiter = *(str + keyLength);
    *(str + keyLength) = '\0';

    strcpy(key, str);

    *stringPtr = *stringPtr + consumedLength;
    *valuePtr = atoi(str + keyLength + 1);

    *(str + keyLength) = savedKeyDelimiter;
    *(str + segmentLength) = savedSegmentDelimiter;

    return 0;
}

} // namespace fallout
