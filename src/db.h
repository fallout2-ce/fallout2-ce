#ifndef DB_H
#define DB_H

#include <stddef.h>

#include "xfile.h"

namespace fallout {

typedef XFile File;
typedef void FileReadProgressHandler();
typedef char* StrdupProc(const char* string);

int dbOpen(const char* filePath1, const char* filePath2 = nullptr);
int db_total();
void dbCloseAll();
int dbGetFileSize(const char* filePath, int* sizePtr);
int dbGetFileContents(const char* filePath, void* ptr);
int fileClose(File* stream);
File* fileOpen(const char* filename, const char* mode);
int filePrintFormatted(File* stream, const char* format, ...);
int fileReadChar(File* stream);
char* fileReadString(char* str, size_t size, File* stream);
int fileWriteString(const char* s, File* stream);
size_t fileRead(void* buf, size_t size, size_t count, File* stream);
size_t fileWrite(const void* buf, size_t size, size_t count, File* stream);
int fileSeek(File* stream, long offset, int origin);
long fileTell(File* stream);
void fileRewind(File* stream);
int fileEof(File* stream);
int fileReadUInt8(File* stream, unsigned char* valuePtr);

// Reads a 16-bit big-endian integer from stream and stores it in host byte order.
int fileReadInt16(File* stream, short* valuePtr);

// Reads a 16-bit big-endian unsigned integer from stream and stores it in host byte order.
int fileReadUInt16(File* stream, unsigned short* valuePtr);

// Reads a 32-bit big-endian integer from stream and stores it in host byte order.
int fileReadInt32(File* stream, int* valuePtr);

// Reads a 32-bit big-endian unsigned integer from stream and stores it in host byte order.
int fileReadUInt32(File* stream, unsigned int* valuePtr);

// Reads a 32-bit big-endian integer from stream and stores it in host byte order (alias).
int _db_freadInt(File* stream, int* valuePtr);

// Reads a 32-bit big-endian floating-point value from stream and stores it in host byte order.
int fileReadFloat(File* stream, float* valuePtr);

// Reads a 32-bit big-endian integer from stream and stores the boolean equivalent in host byte order.
int fileReadBool(File* stream, bool* valuePtr);
int fileWriteUInt8(File* stream, unsigned char value);

// Writes a 16-bit integer to stream in big-endian byte order.
int fileWriteInt16(File* stream, short value);

// Writes a 16-bit unsigned integer to stream in big-endian byte order.
int fileWriteUInt16(File* stream, unsigned short value);

// Writes a 32-bit integer to stream in big-endian byte order.
int fileWriteInt32(File* stream, int value);

// Writes a 32-bit integer to stream in big-endian byte order (alias).
int _db_fwriteLong(File* stream, int value);

// Writes a 32-bit unsigned integer to stream in big-endian byte order.
int fileWriteUInt32(File* stream, unsigned int value);

// Writes a 32-bit floating-point value to stream in big-endian byte order.
int fileWriteFloat(File* stream, float value);

// Writes a boolean value to stream as a 32-bit big-endian integer (1 or 0).
int fileWriteBool(File* stream, bool value);
int fileReadUInt8List(File* stream, unsigned char* arr, int count);
int fileReadFixedLengthString(File* stream, char* string, int length);

// Reads a list of 16-bit big-endian integers from stream into arr (in host byte order).
int fileReadInt16List(File* stream, short* arr, int count);

// Reads a list of 16-bit big-endian unsigned integers from stream into arr (in host byte order).
int fileReadUInt16List(File* stream, unsigned short* arr, int count);

// Reads a list of 32-bit big-endian integers from stream into arr (in host byte order).
int fileReadInt32List(File* stream, int* arr, int count);

// Reads a list of 32-bit big-endian integers from stream into arr (in host byte order, alias).
int _db_freadIntCount(File* stream, int* arr, int count);

// Reads a list of 32-bit big-endian unsigned integers from stream into arr (in host byte order).
int fileReadUInt32List(File* stream, unsigned int* arr, int count);
int fileWriteUInt8List(File* stream, unsigned char* arr, int count);
int fileWriteFixedLengthString(File* stream, char* string, int length);

// Writes a list of 16-bit integers to stream in big-endian byte order.
int fileWriteInt16List(File* stream, short* arr, int count);

// Writes a list of 16-bit unsigned integers to stream in big-endian byte order.
int fileWriteUInt16List(File* stream, unsigned short* arr, int count);

// Writes a list of 32-bit integers to stream in big-endian byte order.
int fileWriteInt32List(File* stream, int* arr, int count);

// Writes a list of 32-bit integers to stream in big-endian byte order (alias).
int _db_fwriteLongCount(File* stream, int* arr, int count);

// Writes a list of 32-bit unsigned integers to stream in big-endian byte order.
int fileWriteUInt32List(File* stream, unsigned int* arr, int count);
int fileNameListInit(const char* pattern, char*** fileNames);
void fileNameListFree(char*** fileNames, int unused);
int fileGetSize(File* stream);
void fileSetReadProgressHandler(FileReadProgressHandler* handler, int size);

} // namespace fallout

#endif /* DB_H */
