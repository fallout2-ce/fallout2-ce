#include "select_file_list.h"

#include <stdlib.h>
#include <string.h>

#include "db.h"

namespace fallout {

// 0x4AA250
int _compare(const void* a, const void* b)
{
    const char* c1 = *(const char**)a;
    const char* c2 = *(const char**)b;
    return strcmp(c1, c2);
}

// 0x4AA2A4
char** _getFileList(const char* pattern, int* fileNameListLengthPtr)
{
    char** fileNameList;
    int fileNameListLength = fileNameListInit(pattern, &fileNameList);
    *fileNameListLengthPtr = fileNameListLength;
    if (fileNameListLength == 0) {
        return nullptr;
    }

    qsort(fileNameList, fileNameListLength, sizeof(*fileNameList), _compare);

    return fileNameList;
}

// 0x4AA2DC
void _freeFileList(char** fileList)
{
    fileNameListFree(&fileList, 0);
}

} // namespace fallout
