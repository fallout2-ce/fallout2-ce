#include "dat_archive.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "dfile.h"
#include "fpattern_windows.h"
#include "platform_compat.h"
#include "fo1_dat_lzss.h"

namespace fallout {

namespace {

struct Fo1AssocHeader {
    int size = 0;
    int max = 0;
    int dataSize = 0;
    int unused = 0;
};

std::string normalizeDatPath(std::string path)
{
    for (char& ch : path) {
        if (ch == '/') {
            ch = '\\';
        }
    }

    return path;
}

bool normalizeAndComparePath(const std::string& a, const std::string& b)
{
    return compat_stricmp(normalizeDatPath(a).c_str(), normalizeDatPath(b).c_str()) == 0;
}

bool readBe32(FILE* stream, int* value)
{
    unsigned char bytes[4];
    if (fread(bytes, sizeof(bytes), 1, stream) != 1) {
        return false;
    }

    *value = (static_cast<int>(bytes[0]) << 24)
        | (static_cast<int>(bytes[1]) << 16)
        | (static_cast<int>(bytes[2]) << 8)
        | static_cast<int>(bytes[3]);
    return true;
}

bool readBe16(FILE* stream, unsigned short* value)
{
    unsigned char bytes[2];
    if (fread(bytes, sizeof(bytes), 1, stream) != 1) {
        return false;
    }

    *value = static_cast<unsigned short>((bytes[0] << 8) | bytes[1]);
    return true;
}

bool readFo1AssocHeader(FILE* stream, Fo1AssocHeader* header)
{
    return readBe32(stream, &header->size)
        && readBe32(stream, &header->max)
        && readBe32(stream, &header->dataSize)
        && readBe32(stream, &header->unused);
}

bool readFo1AssocKey(FILE* stream, std::string* key)
{
    int keyLength = fgetc(stream);
    if (keyLength < 0) {
        return false;
    }

    key->resize(static_cast<size_t>(keyLength));
    if (keyLength != 0 && fread(key->data(), static_cast<size_t>(keyLength), 1, stream) != 1) {
        return false;
    }

    return true;
}

bool skipFo1AssocValue(FILE* stream, size_t size)
{
    std::vector<unsigned char> buffer(4096);
    while (size > 0) {
        size_t chunkSize = std::min(size, buffer.size());
        if (fread(buffer.data(), 1, chunkSize, stream) != chunkSize) {
            return false;
        }
        size -= chunkSize;
    }

    return true;
}

bool readFo1DirEntry(FILE* stream, DatArchiveEntry* entry)
{
    int flags;
    int offset;
    int length;
    int storedLength;

    if (!readBe32(stream, &flags)
        || !readBe32(stream, &offset)
        || !readBe32(stream, &length)
        || !readBe32(stream, &storedLength)) {
        return false;
    }

    if (flags < 0 || offset < 0 || length < 0 || storedLength < 0) {
        return false;
    }

    if (flags == 0) {
        flags = 16;
    }

    entry->flags = flags;
    entry->compressed = (flags & 0xF0) != 0x20;
    entry->uncompressedSize = length;
    entry->dataOffset = offset;

    switch (flags & 0xF0) {
    case 0x10:
        entry->storedSize = storedLength;
        break;
    case 0x20:
        entry->storedSize = length;
        break;
    default:
        entry->storedSize = std::nullopt;
        break;
    }

    return true;
}

bool readFo1EntryData(FILE* stream, const DatArchiveEntry& entry, unsigned char* dest)
{
    if (fseek(stream, static_cast<long>(*entry.dataOffset), SEEK_SET) != 0) {
        return false;
    }

    switch (entry.flags & 0xF0) {
    case 0x10:
        if (!entry.storedSize.has_value()) {
            return false;
        }

        return fo1LzssDecodeToBuf(stream, dest, static_cast<unsigned int>(*entry.storedSize)) == entry.uncompressedSize;
    case 0x20:
        return fread(dest, 1, static_cast<size_t>(entry.uncompressedSize), stream) == static_cast<size_t>(entry.uncompressedSize);
    case 0x40: {
        size_t produced = 0;
        while (produced < static_cast<size_t>(entry.uncompressedSize)) {
            unsigned short chunkHeader;
            if (!readBe16(stream, &chunkHeader)) {
                return false;
            }

            if ((chunkHeader & 0x8000) != 0) {
                size_t chunkSize = static_cast<size_t>(chunkHeader & 0x7FFF);
                if (produced + chunkSize > static_cast<size_t>(entry.uncompressedSize)) {
                    return false;
                }

                if (fread(dest + produced, 1, chunkSize, stream) != chunkSize) {
                    return false;
                }

                produced += chunkSize;
            } else {
                int bytesWritten = fo1LzssDecodeToBuf(stream, dest + produced, chunkHeader);
                if (bytesWritten <= 0 || produced + static_cast<size_t>(bytesWritten) > static_cast<size_t>(entry.uncompressedSize)) {
                    return false;
                }

                produced += static_cast<size_t>(bytesWritten);
            }
        }

        return true;
    }
    default:
        return false;
    }
}

class Fo2Stream final : public DatArchiveStream {
public:
    explicit Fo2Stream(DFile* stream)
        : stream_(stream)
    {
    }

    ~Fo2Stream() override
    {
        if (stream_ != nullptr) {
            dfileClose(stream_);
        }
    }

    size_t read(void* buffer, size_t size) override
    {
        return dfileRead(buffer, 1, size, stream_);
    }

    bool seek(long offset, int origin) override
    {
        return dfileSeek(stream_, offset, origin) == 0;
    }

    long tell() const override
    {
        return dfileTell(stream_);
    }

    long size() const override
    {
        return dfileGetSize(stream_);
    }

private:
    DFile* stream_ = nullptr;
};

class Fo2Archive final : public DatArchive {
public:
    explicit Fo2Archive(DBase* dbase)
        : dbase_(dbase)
    {
        path_ = dbase_->path != nullptr ? dbase_->path : "";
        entries_.reserve(static_cast<size_t>(dbase_->entriesLength));
        for (int index = 0; index < dbase_->entriesLength; index++) {
            const DBaseEntry& entry = dbase_->entries[index];
            DatArchiveEntry archiveEntry;
            archiveEntry.path = entry.path != nullptr ? entry.path : "";
            archiveEntry.flags = entry.compressed ? 0x10 : 0x20;
            archiveEntry.compressed = entry.compressed != 0;
            archiveEntry.storedSize = entry.dataSize;
            archiveEntry.uncompressedSize = entry.uncompressedSize;
            archiveEntry.dataOffset = entry.dataOffset;
            entries_.push_back(std::move(archiveEntry));
        }
    }

    ~Fo2Archive() override
    {
        if (dbase_ != nullptr) {
            dbaseClose(dbase_);
        }
    }

    const char* formatName() const override
    {
        return "fallout2";
    }

    const std::string& path() const override
    {
        return path_;
    }

    const std::vector<DatArchiveEntry>& entries() const override
    {
        return entries_;
    }

    std::optional<long> dataOffset() const override
    {
        return dbase_->dataOffset;
    }

    std::unique_ptr<DatArchiveStream> openEntry(const std::string& path) const override
    {
        DFile* stream = dfileOpen(dbase_, path.c_str(), "rb");
        if (stream == nullptr) {
            return nullptr;
        }

        return std::make_unique<Fo2Stream>(stream);
    }

private:
    DBase* dbase_ = nullptr;
    std::string path_;
    std::vector<DatArchiveEntry> entries_;
};

class Fo1BufferStream final : public DatArchiveStream {
public:
    explicit Fo1BufferStream(std::vector<unsigned char> data)
        : data_(std::move(data))
    {
    }

    size_t read(void* buffer, size_t size) override
    {
        size_t remaining = data_.size() - position_;
        size_t bytesToRead = std::min(size, remaining);
        if (bytesToRead == 0) {
            return 0;
        }

        memcpy(buffer, data_.data() + position_, bytesToRead);
        position_ += bytesToRead;
        return bytesToRead;
    }

    bool seek(long offset, int origin) override
    {
        long target;
        switch (origin) {
        case SEEK_SET:
            target = offset;
            break;
        case SEEK_CUR:
            target = static_cast<long>(position_) + offset;
            break;
        case SEEK_END:
            target = static_cast<long>(data_.size()) + offset;
            break;
        default:
            return false;
        }

        if (target < 0 || target > static_cast<long>(data_.size())) {
            return false;
        }

        position_ = static_cast<size_t>(target);
        return true;
    }

    long tell() const override
    {
        return static_cast<long>(position_);
    }

    long size() const override
    {
        return static_cast<long>(data_.size());
    }

private:
    std::vector<unsigned char> data_;
    size_t position_ = 0;
};

class Fo1Archive final : public DatArchive {
public:
    explicit Fo1Archive(std::string path)
        : path_(std::move(path))
    {
    }

    static std::unique_ptr<Fo1Archive> open(const std::string& path)
    {
        FILE* stream = compat_fopen(path.c_str(), "rb");
        if (stream == nullptr) {
            return nullptr;
        }

        std::unique_ptr<FILE, decltype(&fclose)> file(stream, &fclose);

        Fo1AssocHeader rootHeader;
        if (!readFo1AssocHeader(file.get(), &rootHeader) || rootHeader.size < 0 || rootHeader.size > 100000 || rootHeader.dataSize < 0) {
            return nullptr;
        }

        std::vector<std::string> directories;
        directories.reserve(static_cast<size_t>(rootHeader.size));
        for (int index = 0; index < rootHeader.size; index++) {
            std::string directory;
            if (!readFo1AssocKey(file.get(), &directory) || !skipFo1AssocValue(file.get(), static_cast<size_t>(rootHeader.dataSize))) {
                return nullptr;
            }

            directories.push_back(std::move(directory));
        }

        auto archive = std::unique_ptr<Fo1Archive>(new Fo1Archive(path));
        archive->entries_.reserve(256);

        for (const std::string& directory : directories) {
            Fo1AssocHeader directoryHeader;
            if (!readFo1AssocHeader(file.get(), &directoryHeader) || directoryHeader.size < 0 || directoryHeader.size > 100000) {
                return nullptr;
            }

            if (directoryHeader.dataSize != static_cast<int>(sizeof(int) * 4)) {
                return nullptr;
            }

            for (int index = 0; index < directoryHeader.size; index++) {
                std::string name;
                DatArchiveEntry entry;
                if (!readFo1AssocKey(file.get(), &name) || !readFo1DirEntry(file.get(), &entry)) {
                    return nullptr;
                }

                if (!directory.empty()) {
                    entry.path = directory + "\\" + name;
                } else {
                    entry.path = name;
                }

                archive->entries_.push_back(std::move(entry));
            }
        }

        return archive;
    }

    const char* formatName() const override
    {
        return "fallout1";
    }

    const std::string& path() const override
    {
        return path_;
    }

    const std::vector<DatArchiveEntry>& entries() const override
    {
        return entries_;
    }

    std::optional<long> dataOffset() const override
    {
        return std::nullopt;
    }

    std::unique_ptr<DatArchiveStream> openEntry(const std::string& path) const override
    {
        const DatArchiveEntry* entry = findEntry(path);
        if (entry == nullptr) {
            return nullptr;
        }

        FILE* stream = compat_fopen(path_.c_str(), "rb");
        if (stream == nullptr) {
            return nullptr;
        }

        std::unique_ptr<FILE, decltype(&fclose)> file(stream, &fclose);

        std::vector<unsigned char> buffer(static_cast<size_t>(entry->uncompressedSize));
        if (!readFo1EntryData(file.get(), *entry, buffer.data())) {
            return nullptr;
        }

        return std::make_unique<Fo1BufferStream>(std::move(buffer));
    }

private:
    std::string path_;
    std::vector<DatArchiveEntry> entries_;
};

} // namespace

const DatArchiveEntry* DatArchive::findEntry(const std::string& path) const
{
    std::string normalizedPath = normalizeDatPath(path);
    for (const DatArchiveEntry& entry : entries()) {
        if (normalizeAndComparePath(entry.path, normalizedPath)) {
            return &entry;
        }
    }

    return nullptr;
}

std::vector<const DatArchiveEntry*> DatArchive::findEntries(const std::string& pattern) const
{
    std::vector<const DatArchiveEntry*> matches;
    std::string normalizedPattern = normalizeDatPath(pattern);
    for (const DatArchiveEntry& entry : entries()) {
        if (fpattern_windows_match(normalizedPattern.c_str(), entry.path.c_str())) {
            matches.push_back(&entry);
        }
    }

    return matches;
}

std::unique_ptr<DatArchive> DatArchive::open(const std::string& path)
{
    if (DBase* dbase = dbaseOpen(path.c_str()); dbase != nullptr) {
        return std::unique_ptr<DatArchive>(new Fo2Archive(dbase));
    }

    return Fo1Archive::open(path);
}

} // namespace fallout
