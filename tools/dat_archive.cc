#include "dat_archive.h"

#include <cassert>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

#include "dfile.h"
#include "fpattern_windows.h"
#include "platform_compat.h"
#include "fo1_dat_lzss.h"

namespace fallout {

namespace {

constexpr int fo1NoCompressionFlags = 0;
constexpr int fo1DefaultFlags = 0x10;
constexpr int fo1CompressedFlagMask = 0xF0;
constexpr int fo1StoredFlags = 0x10;
constexpr int fo1UncompressedFlags = 0x20;
constexpr int fo1ChunkedFlags = 0x40;
constexpr unsigned int fo1CompressedChunkMask = 0x7FFF;
constexpr unsigned int fo1ChunkLiteralMask = 0x8000;
constexpr size_t fo1AssocValueBufferSize = 4096;
constexpr size_t fo1MaxStoredLength = 100000;

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
    std::vector<unsigned char> buffer(fo1AssocValueBufferSize);
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

    if (flags == fo1NoCompressionFlags) {
        flags = fo1DefaultFlags;
    }

    entry->flags = flags;
    entry->compressed = (flags & fo1CompressedFlagMask) != fo1UncompressedFlags;
    entry->uncompressedSize = length;
    entry->dataOffset = offset;

    switch (flags & fo1CompressedFlagMask) {
    case fo1StoredFlags:
        entry->storedSize = storedLength;
        break;
    case fo1UncompressedFlags:
        entry->storedSize = length;
        break;
    default:
        entry->storedSize = std::nullopt;
        break;
    }

    return true;
}

size_t safeFo1DecodeBufferSize(unsigned int compressedLength)
{
    return static_cast<size_t>(compressedLength) * 18 + 18;
}

bool trySafeFo1DecodeBufferSize(unsigned int compressedLength, size_t* size)
{
    assert(size != nullptr);

    if constexpr (sizeof(size_t) < sizeof(unsigned int)) {
        if (compressedLength > (std::numeric_limits<size_t>::max() - 18) / 18) {
            return false;
        }
    }

    *size = safeFo1DecodeBufferSize(compressedLength);
    return true;
}

bool readFo1EntryData(FILE* stream, const DatArchiveEntry& entry, std::vector<unsigned char>* output)
{
    if (fseek(stream, static_cast<long>(*entry.dataOffset), SEEK_SET) != 0) {
        return false;
    }

    switch (entry.flags & fo1CompressedFlagMask) {
    case fo1StoredFlags:
        if (!entry.storedSize.has_value()) {
            return false;
        }

        output->resize(static_cast<size_t>(entry.uncompressedSize));
        {
            int bytesWritten = lzss_decode_to_buf(stream, output->data(), static_cast<unsigned int>(*entry.storedSize));
            if (bytesWritten != entry.uncompressedSize) {
                return false;
            }

            return true;
        }
    case fo1UncompressedFlags:
        output->resize(static_cast<size_t>(entry.uncompressedSize));
        return fread(output->data(), 1, static_cast<size_t>(entry.uncompressedSize), stream) == static_cast<size_t>(entry.uncompressedSize);
    case fo1ChunkedFlags: {
        output->clear();
        output->reserve(static_cast<size_t>(entry.uncompressedSize));
        std::vector<unsigned char> decodeBuffer;
        while (output->size() < static_cast<size_t>(entry.uncompressedSize)) {
            unsigned short chunkHeader;
            if (!readBe16(stream, &chunkHeader)) {
                return false;
            }

            if ((chunkHeader & fo1ChunkLiteralMask) != 0) {
                size_t chunkSize = static_cast<size_t>(chunkHeader & fo1CompressedChunkMask);
                if (output->size() + chunkSize > static_cast<size_t>(entry.uncompressedSize)) {
                    return false;
                }

                size_t oldSize = output->size();
                output->resize(oldSize + chunkSize);
                if (fread(output->data() + oldSize, 1, chunkSize, stream) != chunkSize) {
                    return false;
                }
            } else {
                size_t decodeBufferSize = 0;
                if (!trySafeFo1DecodeBufferSize(chunkHeader, &decodeBufferSize)) {
                    return false;
                }

                if (decodeBuffer.size() < decodeBufferSize) {
                    decodeBuffer.resize(decodeBufferSize);
                }

                int bytesWritten = lzss_decode_to_buf(stream, decodeBuffer.data(), chunkHeader);
                if (bytesWritten <= 0 || output->size() + static_cast<size_t>(bytesWritten) > static_cast<size_t>(entry.uncompressedSize)) {
                    return false;
                }

                output->insert(output->end(), decodeBuffer.begin(), decodeBuffer.begin() + bytesWritten);
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

    ~Fo1Archive() override
    {
        if (stream_ != nullptr) {
            fclose(stream_);
        }
    }

    static std::unique_ptr<Fo1Archive> open(const std::string& path)
    {
        FILE* stream = compat_fopen(path.c_str(), "rb");
        if (stream == nullptr) {
            return nullptr;
        }

        std::unique_ptr<FILE, decltype(&fclose)> file(stream, &fclose);

        Fo1AssocHeader rootHeader;
        if (!readFo1AssocHeader(file.get(), &rootHeader) || rootHeader.size < 0 || rootHeader.size > fo1MaxStoredLength || rootHeader.dataSize < 0) {
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
            if (!readFo1AssocHeader(file.get(), &directoryHeader) || directoryHeader.size < 0 || directoryHeader.size > fo1MaxStoredLength) {
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

                if (!directory.empty() && directory != ".") {
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

        if (stream_ == nullptr) {
            stream_ = compat_fopen(path_.c_str(), "rb");
            if (stream_ == nullptr) {
                return nullptr;
            }
        }

        std::vector<unsigned char> buffer;
        if (!readFo1EntryData(stream_, *entry, &buffer)) {
            return nullptr;
        }

        return std::make_unique<Fo1BufferStream>(std::move(buffer));
    }

private:
    std::string path_;
    std::vector<DatArchiveEntry> entries_;
    mutable FILE* stream_ = nullptr;
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
