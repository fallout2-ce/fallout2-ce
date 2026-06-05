#include "platform_compat.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <SDL.h>

namespace fallout {

int compat_stricmp(const char* string1, const char* string2)
{
    return SDL_strcasecmp(string1, string2);
}

int compat_strnicmp(const char* string1, const char* string2, size_t size)
{
    return SDL_strncasecmp(string1, string2, size);
}

char* compat_strupr(char* string)
{
    return SDL_strupr(string);
}

char* compat_strlwr(char* string)
{
    return SDL_strlwr(string);
}

char* compat_itoa(int value, char* buffer, int radix)
{
    return SDL_itoa(value, buffer, radix);
}

void compat_splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
    std::filesystem::path fsPath(path);

    if (drive != nullptr) {
        strncpy(drive, fsPath.root_name().string().c_str(), COMPAT_MAX_DRIVE - 1);
    }

    if (dir != nullptr) {
        strncpy(dir, fsPath.parent_path().string().c_str(), COMPAT_MAX_DIR - 1);
    }

    if (fname != nullptr) {
        strncpy(fname, fsPath.stem().string().c_str(), COMPAT_MAX_FNAME - 1);
    }

    if (ext != nullptr) {
        strncpy(ext, fsPath.extension().string().c_str(), COMPAT_MAX_EXT - 1);
    }
}

void compat_makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
    std::filesystem::path fsPath;

    if (drive != nullptr && *drive != '\0') {
        fsPath /= drive;
    }

    if (dir != nullptr && *dir != '\0') {
        fsPath /= dir;
    }

    if (fname != nullptr && *fname != '\0') {
        fsPath /= fname;
    }

    if (ext != nullptr && *ext != '\0') {
        fsPath += ext;
    }

    strncpy(path, fsPath.string().c_str(), COMPAT_MAX_PATH - 1);
}

long compat_tell(int fd)
{
    return lseek(fd, 0, SEEK_CUR);
}

long compat_filelength(const char* path)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    return std::filesystem::file_size(nativePath);
}

int compat_mkdir(const char* path)
{
    std::error_code ec;
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    std::filesystem::create_directory(nativePath, ec);
    return ec.value();
}

int compat_mkdir_recursive(const char* path)
{
    std::error_code ec;
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    std::filesystem::create_directories(nativePath, ec);
    return ec.value();
}

bool compat_file_exists(const char* filePath)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, filePath);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    return std::filesystem::exists(nativePath);
}

unsigned int compat_timeGetTime()
{
    return SDL_GetTicks64();
}

FILE* compat_fopen(const char* path, const char* mode)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    return fopen(nativePath, mode);
}

gzFile compat_gzopen(const char* path, const char* mode)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    return gzopen(nativePath, mode);
}

char* compat_fgets(char* buffer, int maxCount, FILE* stream)
{
    buffer = fgets(buffer, maxCount, stream);

    if (buffer != nullptr) {
        size_t len = strlen(buffer);
        if (len >= 2 && buffer[len - 1] == '\n' && buffer[len - 2] == '\r') {
            buffer[len - 2] = '\n';
            buffer[len - 1] = '\0';
        }
    }

    return buffer;
}

char* compat_gzgets(gzFile stream, char* buffer, int maxCount)
{
    buffer = gzgets(stream, buffer, maxCount);

    if (buffer != nullptr) {
        size_t len = strlen(buffer);
        if (len >= 2 && buffer[len - 1] == '\n' && buffer[len - 2] == '\r') {
            buffer[len - 2] = '\n';
            buffer[len - 1] = '\0';
        }
    }

    return buffer;
}

int compat_remove(const char* path)
{
    std::error_code err;
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    std::filesystem::remove(nativePath, err);
    return err.value();
}

int compat_rename(const char* oldFileName, const char* newFileName)
{
    std::error_code err;

    char nativeOldFileName[COMPAT_MAX_PATH];
    strcpy(nativeOldFileName, oldFileName);
    compat_windows_path_to_native(nativeOldFileName);
    compat_resolve_path(nativeOldFileName);

    char nativeNewFileName[COMPAT_MAX_PATH];
    strcpy(nativeNewFileName, newFileName);
    compat_windows_path_to_native(nativeNewFileName);
    compat_resolve_path(nativeNewFileName);

    std::filesystem::rename(nativeOldFileName, nativeNewFileName, err);
    return err.value();
}

void compat_windows_path_to_native(char* path)
{
#ifndef _WIN32
    char* pch = path;
    while (*pch != '\0') {
        if (*pch == '\\') {
            *pch = '/';
        }
        pch++;
    }
#endif
}

void compat_resolve_path(char* path)
{
#ifndef _WIN32
    char* pch = path;

    DIR* dir;
    if (pch[0] == '/') {
        dir = opendir("/");
        pch++;
    } else {
        dir = opendir(".");
    }

    while (dir != nullptr) {
        char* sep = strchr(pch, '/');
        size_t length;
        if (sep != nullptr) {
            length = sep - pch;
        } else {
            length = strlen(pch);
        }

        bool found = false;

        struct dirent* entry = readdir(dir);
        while (entry != nullptr) {
            if (strlen(entry->d_name) == length && compat_strnicmp(pch, entry->d_name, length) == 0) {
                strncpy(pch, entry->d_name, length);
                found = true;
                break;
            }
            entry = readdir(dir);
        }

        closedir(dir);
        dir = nullptr;

        if (!found) {
            break;
        }

        if (sep == nullptr) {
            break;
        }

        *sep = '\0';
        dir = opendir(path);
        *sep = '/';

        pch = sep + 1;
    }
#endif
}

int compat_access(const char* path, int mode)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    std::filesystem::perms targetPrms = static_cast<std::filesystem::perms>(mode);
    std::filesystem::perms fsPrms = std::filesystem::status(nativePath).permissions();
    return targetPrms == fsPrms;
}

char* compat_strdup(const char* string)
{
    return SDL_strdup(string);
}

// It's a replacement for compat_filelength(fileno(stream)) on platforms without
// fileno defined.
long getFileSize(FILE* stream)
{
    long originalOffset = ftell(stream);
    fseek(stream, 0, SEEK_END);
    long filesize = ftell(stream);
    fseek(stream, originalOffset, SEEK_SET);
    return filesize;
}

} // namespace fallout
