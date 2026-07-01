#include "sfall_file_system.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

#include "db.h"
#include "platform_compat.h"

namespace fallout {

static constexpr size_t kMaxVirtualFileSize = 0xA00000;

struct SfallVirtualFile {
    std::string name;
    std::vector<unsigned char> data;
    size_t position;
};

struct SfallVirtualFileStream {
    int fileId;
    size_t position;
};

static std::vector<SfallVirtualFile> gSfallVirtualFiles;

static int sfallFileSystemFind(const char* path)
{
    if (path == nullptr || path[0] == '\0') {
        return -1;
    }

    for (size_t index = 0; index < gSfallVirtualFiles.size(); index++) {
        if (compat_stricmp(gSfallVirtualFiles[index].name.c_str(), path) == 0) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

static SfallVirtualFile* sfallFileSystemGet(int id)
{
    if (id < 0 || static_cast<size_t>(id) >= gSfallVirtualFiles.size()) {
        return nullptr;
    }

    return &(gSfallVirtualFiles[id]);
}

int sfallFileSystemCopy(const char* path, const char* source)
{
    if (path == nullptr || path[0] == '\0' || source == nullptr || source[0] == '\0') {
        return -1;
    }

    int existingId = sfallFileSystemFind(path);
    if (existingId != -1) {
        return existingId;
    }

    File* stream = fileOpen(source, "rb");
    if (stream == nullptr) {
        return -1;
    }

    long fileSize = fileGetSize(stream);
    if (fileSize < 0 || static_cast<size_t>(fileSize) > kMaxVirtualFileSize) {
        fileClose(stream);
        return -1;
    }

    std::vector<unsigned char> data;
    data.resize(static_cast<size_t>(fileSize));

    if (fileSize > 0) {
        size_t bytesRead = fileRead(data.data(), 1, static_cast<size_t>(fileSize), stream);
        if (bytesRead != static_cast<size_t>(fileSize)) {
            fileClose(stream);
            return -1;
        }
    }

    fileClose(stream);

    SfallVirtualFile file;
    file.name = path;
    file.data = std::move(data);
    file.position = 0;

    gSfallVirtualFiles.push_back(std::move(file));
    return static_cast<int>(gSfallVirtualFiles.size() - 1);
}

void sfallFileSystemWriteShort(int id, int data)
{
    SfallVirtualFile* file = sfallFileSystemGet(id);
    if (file == nullptr || file->position + 2 > file->data.size()) {
        return;
    }

    file->data[file->position++] = static_cast<unsigned char>((data >> 8) & 0xFF);
    file->data[file->position++] = static_cast<unsigned char>(data & 0xFF);
}

int sfallFileSystemReadShort(int id)
{
    SfallVirtualFile* file = sfallFileSystemGet(id);
    if (file == nullptr || file->position + 2 > file->data.size()) {
        return 0;
    }

    int value = (static_cast<int>(file->data[file->position]) << 8)
        | static_cast<int>(file->data[file->position + 1]);
    file->position += 2;

    if ((value & 0x8000) != 0) {
        value -= 0x10000;
    }

    return value;
}

void sfallFileSystemSeek(int id, int pos)
{
    SfallVirtualFile* file = sfallFileSystemGet(id);
    if (file == nullptr || pos < 0 || static_cast<size_t>(pos) > file->data.size()) {
        return;
    }

    file->position = static_cast<size_t>(pos);
}

SfallVirtualFileStream* sfallFileStreamOpen(const char* path, const char* mode)
{
    if (mode == nullptr || strchr(mode, 'w') != nullptr || strchr(mode, 'a') != nullptr || strchr(mode, '+') != nullptr) {
        return nullptr;
    }

    int fileId = sfallFileSystemFind(path);
    if (fileId == -1) {
        return nullptr;
    }

    SfallVirtualFileStream* stream = new SfallVirtualFileStream();
    stream->fileId = fileId;
    stream->position = 0;
    return stream;
}

void sfallFileStreamClose(SfallVirtualFileStream* stream)
{
    delete stream;
}

int sfallFileStreamReadChar(SfallVirtualFileStream* stream)
{
    SfallVirtualFile* file = stream != nullptr ? sfallFileSystemGet(stream->fileId) : nullptr;
    if (file == nullptr || stream->position >= file->data.size()) {
        return EOF;
    }

    return file->data[stream->position++];
}

char* sfallFileStreamReadString(char* string, int size, SfallVirtualFileStream* stream)
{
    if (string == nullptr || size <= 0) {
        return nullptr;
    }

    SfallVirtualFile* file = stream != nullptr ? sfallFileSystemGet(stream->fileId) : nullptr;
    if (file == nullptr || stream->position >= file->data.size()) {
        return nullptr;
    }

    int index = 0;
    while (index < size - 1 && stream->position < file->data.size()) {
        char ch = static_cast<char>(file->data[stream->position++]);
        string[index++] = ch;
        if (ch == '\n') {
            break;
        }
    }

    if (index == 0) {
        return nullptr;
    }

    string[index] = '\0';
    return string;
}

size_t sfallFileStreamRead(void* ptr, size_t size, size_t count, SfallVirtualFileStream* stream)
{
    if (ptr == nullptr || size == 0 || count == 0) {
        return 0;
    }

    SfallVirtualFile* file = stream != nullptr ? sfallFileSystemGet(stream->fileId) : nullptr;
    if (file == nullptr || stream->position >= file->data.size()) {
        return 0;
    }

    size_t requested = size * count;
    if (requested / size != count) {
        requested = file->data.size() - stream->position;
    }

    size_t remaining = file->data.size() - stream->position;
    size_t bytesToRead = std::min(requested, remaining);

    memcpy(ptr, file->data.data() + stream->position, bytesToRead);
    stream->position += bytesToRead;

    return bytesToRead / size;
}

int sfallFileStreamSeek(SfallVirtualFileStream* stream, long offset, int origin)
{
    SfallVirtualFile* file = stream != nullptr ? sfallFileSystemGet(stream->fileId) : nullptr;
    if (file == nullptr) {
        return -1;
    }

    long base;
    switch (origin) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = static_cast<long>(stream->position);
        break;
    case SEEK_END:
        base = static_cast<long>(file->data.size());
        break;
    default:
        return -1;
    }

    long newPosition = base + offset;
    if (newPosition < 0 || static_cast<size_t>(newPosition) > file->data.size()) {
        return -1;
    }

    stream->position = static_cast<size_t>(newPosition);
    return 0;
}

long sfallFileStreamTell(SfallVirtualFileStream* stream)
{
    if (stream == nullptr) {
        return -1;
    }

    return static_cast<long>(stream->position);
}

void sfallFileStreamRewind(SfallVirtualFileStream* stream)
{
    if (stream != nullptr) {
        stream->position = 0;
    }
}

int sfallFileStreamEof(SfallVirtualFileStream* stream)
{
    SfallVirtualFile* file = stream != nullptr ? sfallFileSystemGet(stream->fileId) : nullptr;
    if (file == nullptr || stream->position >= file->data.size()) {
        return 1;
    }

    return 0;
}

long sfallFileStreamGetSize(SfallVirtualFileStream* stream)
{
    SfallVirtualFile* file = stream != nullptr ? sfallFileSystemGet(stream->fileId) : nullptr;
    if (file == nullptr) {
        return 0;
    }

    return static_cast<long>(file->data.size());
}

} // namespace fallout
