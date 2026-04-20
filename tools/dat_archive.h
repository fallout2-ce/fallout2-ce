#ifndef FALLOUT_TOOLS_DAT_ARCHIVE_H_
#define FALLOUT_TOOLS_DAT_ARCHIVE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fallout {

class DatArchiveStream {
public:
    virtual ~DatArchiveStream() = default;

    virtual size_t read(void* buffer, size_t size) = 0;
    virtual bool seek(long offset, int origin) = 0;
    virtual long tell() const = 0;
    virtual long size() const = 0;
};

struct DatArchiveEntry {
    std::string path;
    int flags = 0;
    bool compressed = false;
    std::optional<long> storedSize;
    long uncompressedSize = 0;
    std::optional<long> dataOffset;
};

class DatArchive {
public:
    virtual ~DatArchive() = default;

    virtual const char* formatName() const = 0;
    virtual const std::string& path() const = 0;
    virtual const std::vector<DatArchiveEntry>& entries() const = 0;
    virtual std::optional<long> dataOffset() const = 0;
    virtual std::unique_ptr<DatArchiveStream> openEntry(const std::string& path) const = 0;

    const DatArchiveEntry* findEntry(const std::string& path) const;
    std::vector<const DatArchiveEntry*> findEntries(const std::string& pattern) const;

    static std::unique_ptr<DatArchive> open(const std::string& path);
};

} // namespace fallout

#endif /* FALLOUT_TOOLS_DAT_ARCHIVE_H_ */
