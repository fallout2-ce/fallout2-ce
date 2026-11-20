#ifndef SFALL_FILESYSTEM_H
#define SFALL_FILESYSTEM_H

#include <stddef.h>

namespace fallout {

int sfallFileSystemCopy(const char* path, const char* source);
int sfallFileSystemFind(const char* path);
void sfallFileSystemDelete(int id);
void sfallFileSystemReset();
bool sfallFileSystemResolveAlias(const char* path, char* resolvedPath, size_t resolvedPathSize);

} // namespace fallout

#endif // SFALL_FILESYSTEM_H
