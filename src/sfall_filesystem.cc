#include "sfall_filesystem.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "art.h"
#include "db.h"
#include "platform_compat.h"

namespace fallout {

typedef struct SfallFileSystemAlias {
    bool active;
    std::string path;
    std::string source;
    std::string normalizedPath;
} SfallFileSystemAlias;

static std::vector<SfallFileSystemAlias> aliases;

static std::string normalizePath(const char* path)
{
    std::string normalized = path != nullptr ? path : "";
    for (char& ch : normalized) {
        if (ch == '/') {
            ch = '\\';
        } else if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return normalized;
}

static bool isSoundEffectsPath(const std::string& normalizedPath)
{
    return normalizedPath.rfind("sound\\sfx\\", 0) == 0;
}

static int findAliasIndex(const char* path)
{
    if (path == nullptr || path[0] == '\0') {
        return -1;
    }

    std::string normalized = normalizePath(path);
    for (size_t index = 0; index < aliases.size(); index++) {
        if (aliases[index].active && aliases[index].normalizedPath == normalized) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

int sfallFileSystemCopy(const char* path, const char* source)
{
    if (path == nullptr || path[0] == '\0' || source == nullptr || source[0] == '\0') {
        return -1;
    }

    int existingIndex = findAliasIndex(path);
    if (existingIndex != -1) {
        return existingIndex;
    }

    int fileSize = 0;
    if (dbGetFileSize(source, &fileSize) != 0) {
        return -1;
    }

    SfallFileSystemAlias alias;
    alias.active = true;
    alias.path = path;
    alias.source = normalizePath(source);
    alias.normalizedPath = normalizePath(path);

    for (size_t index = 0; index < aliases.size(); index++) {
        if (!aliases[index].active) {
            aliases[index] = alias;
            artCacheFlush();
            return static_cast<int>(index);
        }
    }

    aliases.push_back(alias);
    artCacheFlush();
    return static_cast<int>(aliases.size() - 1);
}

int sfallFileSystemFind(const char* path)
{
    return findAliasIndex(path);
}

void sfallFileSystemDelete(int id)
{
    if (id < 0 || id >= static_cast<int>(aliases.size()) || !aliases[id].active) {
        return;
    }

    aliases[id].active = false;
    aliases[id].path.clear();
    aliases[id].source.clear();
    aliases[id].normalizedPath.clear();
    artCacheFlush();
}

void sfallFileSystemReset()
{
    if (aliases.empty()) {
        return;
    }

    aliases.clear();
    artCacheFlush();
}

bool sfallFileSystemResolveAlias(const char* path, char* resolvedPath, size_t resolvedPathSize)
{
    if (path == nullptr || path[0] == '\0' || resolvedPath == nullptr || resolvedPathSize == 0) {
        return false;
    }

    std::string normalized = normalizePath(path);
    if (isSoundEffectsPath(normalized)) {
        return false;
    }

    int aliasIndex = findAliasIndex(path);
    if (aliasIndex == -1 || aliases[aliasIndex].source == path) {
        return false;
    }

    int written = snprintf(resolvedPath, resolvedPathSize, "%s", aliases[aliasIndex].source.c_str());
    return written >= 0 && static_cast<size_t>(written) < resolvedPathSize;
}

} // namespace fallout
