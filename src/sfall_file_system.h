#ifndef SFALL_FILE_SYSTEM_H
#define SFALL_FILE_SYSTEM_H

#include <stddef.h>

namespace fallout {

struct SfallVirtualFileStream;

int sfallFileSystemCopy(const char* path, const char* source);
void sfallFileSystemWriteShort(int id, int data);
int sfallFileSystemReadShort(int id);
void sfallFileSystemSeek(int id, int pos);

SfallVirtualFileStream* sfallFileStreamOpen(const char* path, const char* mode);
void sfallFileStreamClose(SfallVirtualFileStream* stream);
int sfallFileStreamReadChar(SfallVirtualFileStream* stream);
char* sfallFileStreamReadString(char* string, int size, SfallVirtualFileStream* stream);
size_t sfallFileStreamRead(void* ptr, size_t size, size_t count, SfallVirtualFileStream* stream);
int sfallFileStreamSeek(SfallVirtualFileStream* stream, long offset, int origin);
long sfallFileStreamTell(SfallVirtualFileStream* stream);
void sfallFileStreamRewind(SfallVirtualFileStream* stream);
int sfallFileStreamEof(SfallVirtualFileStream* stream);
long sfallFileStreamGetSize(SfallVirtualFileStream* stream);

} // namespace fallout

#endif /* SFALL_FILE_SYSTEM_H */
