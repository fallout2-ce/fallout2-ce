#include "dat_archive.h"
#include "platform_compat.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <zlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fallout {

namespace {

#ifdef _WIN32
using NativeStat = struct _stat;
#else
using NativeStat = struct stat;
#endif

constexpr int kMaxCreateRecursionDepth = 128;

struct Options {
    std::string archivePath;
    std::string command;
    std::vector<std::string> args;
    bool lowerExtractedPaths = false;
};

struct DatCreateEntry {
    std::string nativePath;
    std::string archivePath;
    bool compressed = false;
    int uncompressedSize = 0;
    int dataSize = 0;
    int dataOffset = 0;
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

std::string datPathToNativePath(std::string_view path)
{
    std::string value(path);
    for (char& ch : value) {
        if (ch == '\\') {
#ifdef _WIN32
            ch = '\\';
#else
            ch = '/';
#endif
        }
    }

    return value;
}

std::string toLowerDatPath(std::string path)
{
    for (char& ch : path) {
        if (ch == '/') {
            ch = '\\';
        } else {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }

    return path;
}

std::string toLowerAscii(std::string value)
{
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    return value;
}

bool isAbsoluteOutputPath(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    if (path[0] == '/' || path[0] == '\\') {
        return true;
    }

    return path.size() >= 2 && path[1] == ':';
}

bool isSafeRelativeOutputPath(const std::string& path)
{
    if (path.empty() || isAbsoluteOutputPath(path)) {
        return false;
    }

    size_t start = 0;
    while (start < path.size()) {
        size_t end = path.find_first_of("/\\", start);
        if (end == std::string::npos) {
            end = path.size();
        }

        std::string_view component(path.data() + start, end - start);
        if (component == "..") {
            return false;
        }

#ifdef _WIN32
        if (component.find(':') != std::string_view::npos) {
            return false;
        }
#endif

        start = end + 1;
    }

    return true;
}

std::string joinNativePath(const std::string& basePath, const std::string& relativePath)
{
    if (basePath.empty()) {
        return relativePath;
    }

#ifdef _WIN32
    constexpr char separator = '\\';
#else
    constexpr char separator = '/';
#endif

    if (basePath.back() == '/' || basePath.back() == '\\') {
        return basePath + relativePath;
    }

    return basePath + separator + relativePath;
}

bool ensureDirectoriesForFile(const std::string& filePath)
{
    size_t separator = filePath.find_last_of("/\\");
    if (separator == std::string::npos) {
        return true;
    }

    std::string directoryPath = filePath.substr(0, separator);
    if (directoryPath.empty()) {
        return true;
    }

    size_t start = 0;
    if (directoryPath.size() >= 2 && directoryPath[1] == ':') {
        start = 2;
        if (directoryPath.size() >= 3 && (directoryPath[2] == '/' || directoryPath[2] == '\\')) {
            start = 3;
        }
    } else if (directoryPath[0] == '/' || directoryPath[0] == '\\') {
        start = 1;
    }

    while (start <= directoryPath.size()) {
        size_t end = directoryPath.find_first_of("/\\", start);
        std::string partialPath = directoryPath.substr(0, end);
        if (!partialPath.empty() && compat_mkdir(partialPath.c_str()) != 0 && errno != EEXIST) {
            return false;
        }

        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
    }

    return true;
}

bool writeAll(FILE* stream, const void* data, size_t size)
{
    return size == 0 || fwrite(data, 1, size, stream) == size;
}

bool writeUInt8(FILE* stream, unsigned char value)
{
    return fwrite(&value, sizeof(value), 1, stream) == 1;
}

bool writeInt32Le(FILE* stream, int value)
{
    unsigned int unsignedValue = static_cast<unsigned int>(value);
    unsigned char bytes[4];
    bytes[0] = static_cast<unsigned char>(unsignedValue & 0xFF);
    bytes[1] = static_cast<unsigned char>((unsignedValue >> 8) & 0xFF);
    bytes[2] = static_cast<unsigned char>((unsignedValue >> 16) & 0xFF);
    bytes[3] = static_cast<unsigned char>((unsignedValue >> 24) & 0xFF);
    return writeAll(stream, bytes, sizeof(bytes));
}

bool checkedAddInt(int* value, int increment)
{
    if (increment < 0 || *value > std::numeric_limits<int>::max() - increment) {
        return false;
    }

    *value += increment;
    return true;
}

bool readFile(const std::string& path, std::vector<unsigned char>* data)
{
    FILE* rawInput = compat_fopen(path.c_str(), "rb");
    if (rawInput == nullptr) {
        return false;
    }

    std::unique_ptr<FILE, decltype(&fclose)> input(rawInput, &fclose);

    long fileSize = getFileSize(input.get());
    if (fileSize < 0 || fileSize > std::numeric_limits<int>::max()) {
        return false;
    }

    data->resize(static_cast<size_t>(fileSize));
    if (!data->empty()) {
        if (fread(data->data(), 1, data->size(), input.get()) != data->size()) {
            return false;
        }
    }

    return true;
}

bool compressData(const std::vector<unsigned char>& input, std::vector<unsigned char>* output)
{
    uLongf compressedSize = compressBound(static_cast<uLong>(input.size()));
    output->resize(static_cast<size_t>(compressedSize));

    int rc = compress2(output->data(), &compressedSize, input.data(), static_cast<uLong>(input.size()), Z_DEFAULT_COMPRESSION);
    if (rc != Z_OK) {
        return false;
    }

    output->resize(static_cast<size_t>(compressedSize));
    return true;
}

bool writeEntryData(FILE* output, DatCreateEntry* entry, int* dataOffset)
{
    std::vector<unsigned char> data;
    if (!readFile(entry->nativePath, &data)) {
        std::cerr << "Failed to read input file: " << entry->nativePath << "\n";
        return false;
    }

    if (data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        std::cerr << "Input file is too large for Fallout 2 DAT: " << entry->nativePath << "\n";
        return false;
    }

    std::vector<unsigned char> compressedData;
    bool useCompressedData = false;
    if (!data.empty()) {
        if (!compressData(data, &compressedData)) {
            std::cerr << "Failed to compress input file: " << entry->nativePath << "\n";
            return false;
        }

        useCompressedData = compressedData.size() < data.size();
    }

    const std::vector<unsigned char>& storedData = useCompressedData ? compressedData : data;
    if (storedData.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        std::cerr << "Stored file is too large for Fallout 2 DAT: " << entry->nativePath << "\n";
        return false;
    }

    entry->compressed = useCompressedData;
    entry->uncompressedSize = static_cast<int>(data.size());
    entry->dataSize = static_cast<int>(storedData.size());
    entry->dataOffset = *dataOffset;

    if (!writeAll(output, storedData.data(), storedData.size())) {
        std::cerr << "Failed to write archive data for: " << entry->archivePath << "\n";
        return false;
    }

    return checkedAddInt(dataOffset, entry->dataSize);
}

std::string joinNativePathForScan(const std::string& basePath, const std::string& name)
{
    if (basePath.empty()) {
        return name;
    }

#ifdef _WIN32
    constexpr char separator = '\\';
#else
    constexpr char separator = '/';
#endif

    if (basePath.back() == '/' || basePath.back() == '\\') {
        return basePath + name;
    }

    return basePath + separator + name;
}

std::string relativePathToDatPath(const std::string& path)
{
    return normalizeDatPath(path);
}

bool getFileStatus(const std::string& path, NativeStat* status)
{
#ifdef _WIN32
    return _stat(path.c_str(), status) == 0;
#else
    return stat(path.c_str(), status) == 0;
#endif
}

bool isDirectoryPath(const std::string& path)
{
    NativeStat status;
#ifdef _WIN32
    return getFileStatus(path, &status) && (status.st_mode & _S_IFDIR) != 0;
#else
    return getFileStatus(path, &status) && S_ISDIR(status.st_mode);
#endif
}

bool isRegularFilePath(const std::string& path)
{
    NativeStat status;
#ifdef _WIN32
    return getFileStatus(path, &status) && (status.st_mode & _S_IFREG) != 0;
#else
    return getFileStatus(path, &status) && S_ISREG(status.st_mode);
#endif
}

std::string absoluteExistingPath(const std::string& path)
{
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD length = GetFullPathNameA(path.c_str(), MAX_PATH, buffer, nullptr);
    if (length == 0 || length >= MAX_PATH) {
        return path;
    }
    return buffer;
#else
    char buffer[PATH_MAX];
    if (realpath(path.c_str(), buffer) != nullptr) {
        return buffer;
    }
    return path;
#endif
}

bool isSafeInputDatPath(const std::string& path)
{
    if (path.empty() || isAbsoluteOutputPath(path)) {
        return false;
    }

    size_t start = 0;
    while (start < path.size()) {
        size_t end = path.find('\\', start);
        if (end == std::string::npos) {
            end = path.size();
        }

        std::string_view component(path.data() + start, end - start);
        if (component.empty() || component == "." || component == "..") {
            return false;
        }

#ifdef _WIN32
        if (component.find(':') != std::string_view::npos) {
            return false;
        }
#endif

        start = end + 1;
    }

    return true;
}

bool pathsReferToSameFile(const std::string& lhs, const std::string& rhs)
{
#ifdef _WIN32
    return compat_stricmp(lhs.c_str(), rhs.c_str()) == 0;
#else
    return lhs == rhs;
#endif
}

bool shouldSkipScannedFile(const std::string& nativePath, const std::vector<std::string>& excludedAbsolutePaths)
{
    std::string absolutePath = absoluteExistingPath(nativePath);
    for (const std::string& excludedPath : excludedAbsolutePaths) {
        if (pathsReferToSameFile(absolutePath, excludedPath)) {
            return true;
        }
    }

    return false;
}

bool validateNativePathComponent(const std::string& name, const std::string& nativePath)
{
#ifndef _WIN32
    if (name.find('\\') != std::string::npos) {
        std::cerr << "Refusing to archive path with backslash in file name: " << nativePath << "\n";
        return false;
    }
#endif

    return true;
}

bool collectCreateEntriesRecursive(const std::string& nativeDir, const std::string& relativeDir, const std::vector<std::string>& excludedAbsolutePaths, int depth, std::map<std::string, std::string>* seenPaths, std::vector<DatCreateEntry>* entries)
{
    if (depth > kMaxCreateRecursionDepth) {
        std::cerr << "Input directory nesting is too deep near: " << nativeDir << "\n";
        return false;
    }

#ifdef _WIN32
    std::string searchPath = joinNativePathForScan(nativeDir, "*");
    WIN32_FIND_DATAA findData;
    HANDLE findHandle = FindFirstFileA(searchPath.c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to scan input directory: " << nativeDir << "\n";
        return false;
    }

    do {
        std::string name = findData.cFileName;
        if (name == "." || name == "..") {
            continue;
        }

        std::string nativePath = joinNativePathForScan(nativeDir, name);
        if (!validateNativePathComponent(name, nativePath)) {
            FindClose(findHandle);
            return false;
        }

        std::string relativePath = relativeDir.empty() ? name : joinNativePathForScan(relativeDir, name);

        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!collectCreateEntriesRecursive(nativePath, relativePath, excludedAbsolutePaths, depth + 1, seenPaths, entries)) {
                FindClose(findHandle);
                return false;
            }
            continue;
        }
#else
    DIR* rawDir = opendir(nativeDir.c_str());
    if (rawDir == nullptr) {
        std::cerr << "Failed to scan input directory: " << nativeDir << "\n";
        return false;
    }

    std::unique_ptr<DIR, decltype(&closedir)> dir(rawDir, &closedir);

    while (dirent* dirEntry = readdir(dir.get())) {
        std::string name = dirEntry->d_name;
        if (name == "." || name == "..") {
            continue;
        }

        std::string nativePath = joinNativePathForScan(nativeDir, name);
        if (!validateNativePathComponent(name, nativePath)) {
            return false;
        }

        std::string relativePath = relativeDir.empty() ? name : joinNativePathForScan(relativeDir, name);

        if (isDirectoryPath(nativePath)) {
            if (!collectCreateEntriesRecursive(nativePath, relativePath, excludedAbsolutePaths, depth + 1, seenPaths, entries)) {
                return false;
            }
            continue;
        }
#endif

        if (!isRegularFilePath(nativePath)) {
            continue;
        }

        if (shouldSkipScannedFile(nativePath, excludedAbsolutePaths)) {
            continue;
        }

        DatCreateEntry entry;
        entry.nativePath = nativePath;
        entry.archivePath = relativePathToDatPath(relativePath);
        if (!isSafeInputDatPath(entry.archivePath)) {
            std::cerr << "Refusing to archive invalid path: " << entry.archivePath << "\n";
            return false;
        }

        std::string lookupPath = toLowerDatPath(entry.archivePath);
        auto [seenIt, inserted] = seenPaths->emplace(lookupPath, entry.archivePath);
        if (!inserted) {
            std::cerr << "Refusing duplicate case-insensitive archive paths: "
                << seenIt->second << " and " << entry.archivePath << "\n";
            return false;
        }

        entries->push_back(std::move(entry));
#ifdef _WIN32
    } while (FindNextFileA(findHandle, &findData));

    FindClose(findHandle);
#else
    }
#endif

    return true;
}

bool collectCreateEntries(const std::string& inputDir, const std::string& archivePath, std::vector<DatCreateEntry>* entries)
{
    if (!isDirectoryPath(inputDir)) {
        std::cerr << "Input path is not a directory: " << inputDir << "\n";
        return false;
    }

    std::vector<std::string> excludedAbsolutePaths;
    if (isRegularFilePath(archivePath)) {
        excludedAbsolutePaths.push_back(absoluteExistingPath(archivePath));
    }

    std::map<std::string, std::string> seenPaths;
    if (!collectCreateEntriesRecursive(inputDir, "", excludedAbsolutePaths, 0, &seenPaths, entries)) {
        return false;
    }

    std::sort(entries->begin(), entries->end(), [](const DatCreateEntry& lhs, const DatCreateEntry& rhs) {
        return compat_stricmp(lhs.archivePath.c_str(), rhs.archivePath.c_str()) < 0;
    });

    return true;
}

std::string createTemporaryArchivePath()
{
#ifdef _WIN32
    unsigned long processId = GetCurrentProcessId();
#else
    long processId = static_cast<long>(getpid());
#endif

    for (int attempt = 0; attempt < 1000; attempt++) {
        std::string path = ".fallout2-dat-create-"
            + std::to_string(processId)
            + "-"
            + std::to_string(static_cast<long long>(std::time(nullptr)))
            + "-"
            + std::to_string(attempt)
            + ".tmp";
        if (!compat_file_exists(path.c_str())) {
            return path;
        }
    }

    return "";
}

bool replaceArchiveWithTemporaryArchive(const std::string& temporaryArchivePath, const std::string& archivePath)
{
#ifdef _WIN32
    if (MoveFileExA(temporaryArchivePath.c_str(), archivePath.c_str(), MOVEFILE_REPLACE_EXISTING) == 0) {
        return false;
    }

    return true;
#else
    return compat_rename(temporaryArchivePath.c_str(), archivePath.c_str()) == 0;
#endif
}

bool writeFo2DatArchiveToPath(const std::string& outputPath, std::vector<DatCreateEntry>* entries)
{
    FILE* rawOutput = compat_fopen(outputPath.c_str(), "wb");
    if (rawOutput == nullptr) {
        std::cerr << "Failed to create archive: " << outputPath << "\n";
        return false;
    }

    std::unique_ptr<FILE, decltype(&fclose)> output(rawOutput, &fclose);

    int dataOffset = 0;
    for (DatCreateEntry& entry : *entries) {
        if (!writeEntryData(output.get(), &entry, &dataOffset)) {
            return false;
        }
    }

    if (entries->size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        std::cerr << "Archive has too many entries\n";
        return false;
    }

    int entriesDataSize = 0;
    if (!checkedAddInt(&entriesDataSize, static_cast<int>(sizeof(int)))) {
        return false;
    }

    if (!writeInt32Le(output.get(), static_cast<int>(entries->size()))) {
        std::cerr << "Failed to write archive directory\n";
        return false;
    }

    for (const DatCreateEntry& entry : *entries) {
        if (entry.archivePath.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            std::cerr << "Archive path is too long: " << entry.archivePath << "\n";
            return false;
        }

        int pathLength = static_cast<int>(entry.archivePath.size());
        if (!checkedAddInt(&entriesDataSize, static_cast<int>(sizeof(int)))
            || !checkedAddInt(&entriesDataSize, pathLength)
            || !checkedAddInt(&entriesDataSize, static_cast<int>(sizeof(unsigned char) + sizeof(int) * 3))) {
            std::cerr << "Archive directory is too large\n";
            return false;
        }

        if (!writeInt32Le(output.get(), pathLength)
            || !writeAll(output.get(), entry.archivePath.data(), entry.archivePath.size())
            || !writeUInt8(output.get(), entry.compressed ? 1 : 0)
            || !writeInt32Le(output.get(), entry.uncompressedSize)
            || !writeInt32Le(output.get(), entry.dataSize)
            || !writeInt32Le(output.get(), entry.dataOffset)) {
            std::cerr << "Failed to write archive directory entry: " << entry.archivePath << "\n";
            return false;
        }
    }

    int dbaseDataSize = dataOffset;
    if (!checkedAddInt(&dbaseDataSize, entriesDataSize) || !checkedAddInt(&dbaseDataSize, static_cast<int>(sizeof(int) * 2))) {
        std::cerr << "Archive is too large for Fallout 2 DAT\n";
        return false;
    }

    if (!writeInt32Le(output.get(), entriesDataSize)
        || !writeInt32Le(output.get(), dbaseDataSize)) {
        std::cerr << "Failed to write archive footer\n";
        return false;
    }

    if (fclose(output.release()) != 0) {
        std::cerr << "Failed to close archive: " << outputPath << "\n";
        return false;
    }

    return true;
}

bool writeFo2DatArchive(const std::string& inputDir, const std::string& archivePath)
{
    std::vector<DatCreateEntry> entries;
    if (!collectCreateEntries(inputDir, archivePath, &entries)) {
        return false;
    }

    std::string temporaryArchivePath = createTemporaryArchivePath();
    if (temporaryArchivePath.empty()) {
        std::cerr << "Failed to choose a temporary archive path in the current directory\n";
        return false;
    }

    if (!writeFo2DatArchiveToPath(temporaryArchivePath, &entries)) {
        compat_remove(temporaryArchivePath.c_str());
        return false;
    }

    if (!replaceArchiveWithTemporaryArchive(temporaryArchivePath, archivePath)) {
        std::cerr << "Failed to move temporary archive to: " << archivePath << "\n";
        compat_remove(temporaryArchivePath.c_str());
        return false;
    }

    long long uncompressedBytes = 0;
    int compressedEntries = 0;
    for (const DatCreateEntry& entry : entries) {
        uncompressedBytes += entry.uncompressedSize;
        if (entry.compressed) {
            compressedEntries++;
        }
    }

    std::cout
        << "Created " << archivePath << "\n"
        << "entries: " << entries.size() << "\n"
        << "compressed_entries: " << compressedEntries << "\n"
        << "uncompressed_entries: " << entries.size() - compressedEntries << "\n"
        << "uncompressed_bytes: " << uncompressedBytes << "\n";

    return true;
}

void printUsage(std::ostream& stream)
{
    stream
        << "Usage:\n"
        << "  fallout2-dat create <input-dir> <archive.dat>\n"
        << "  fallout2-dat <archive.dat> list [pattern]\n"
        << "  fallout2-dat <archive.dat> info [pattern]\n"
        << "  fallout2-dat <archive.dat> extract [--lower] <output-dir> [pattern]\n"
        << "  fallout2-dat <archive.dat> cat <entry>\n"
        << "\n"
        << "Notes:\n"
        << "  - Create preserves input path casing and stores Windows-style archive paths.\n"
        << "  - Create compresses entries only when zlib output is smaller.\n"
        << "  - Patterns use the same Windows-style wildcard matching as the game.\n"
        << "  - Archive paths are case-insensitive and should use backslashes internally.\n";
}

bool parseOptions(int argc, char** argv, Options* options)
{
    if (argc < 3) {
        return false;
    }

    if (std::strcmp(argv[1], "create") == 0) {
        if (argc != 4) {
            return false;
        }

        options->command = argv[1];
        options->args.assign(argv + 2, argv + argc);
        return true;
    } else {
        options->archivePath = argv[1];
        options->command = argv[2];
        options->args.assign(argv + 3, argv + argc);
    }

    if (options->command == "extract") {
        auto lowerIt = std::find(options->args.begin(), options->args.end(), "--lower");
        if (lowerIt != options->args.end()) {
            options->lowerExtractedPaths = true;
            options->args.erase(lowerIt);
        }
    }

    return true;
}

bool extractEntry(const DatArchive& archive, const DatArchiveEntry& entry, const std::string& outputDir, bool lowerExtractedPaths)
{
    std::string relativePath = datPathToNativePath(entry.path);
    if (lowerExtractedPaths) {
        relativePath = toLowerAscii(std::move(relativePath));
    }

    if (!isSafeRelativeOutputPath(relativePath)) {
        std::cerr << "Refusing to extract invalid path: " << entry.path << "\n";
        return false;
    }

    std::string destination = joinNativePath(outputDir, relativePath);
    if (!ensureDirectoriesForFile(destination)) {
        std::cerr << "Failed to create output directory for: " << destination << "\n";
        return false;
    }

    std::unique_ptr<DatArchiveStream> stream = archive.openEntry(entry.path);
    if (stream == nullptr) {
        std::cerr << "Failed to open entry: " << entry.path << "\n";
        return false;
    }

    std::ofstream output(destination, std::ios::binary);
    if (!output.is_open()) {
        std::cerr << "Failed to create output file: " << destination << "\n";
        return false;
    }

    std::vector<char> buffer(64 * 1024);
    long remaining = stream->size();
    while (remaining > 0) {
        size_t chunkSize = static_cast<size_t>(std::min<long>(remaining, buffer.size()));
        size_t bytesRead = stream->read(buffer.data(), chunkSize);
        if (bytesRead == 0) {
            std::cerr << "Failed while reading entry: " << entry.path << "\n";
            return false;
        }

        output.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
        if (!output) {
            std::cerr << "Failed while writing output file: " << destination << "\n";
            return false;
        }

        remaining -= static_cast<long>(bytesRead);
    }

    std::cout << entry.path << " -> " << destination << "\n";
    return true;
}

int listCommand(const DatArchive& archive, const std::string& pattern)
{
    const std::vector<const DatArchiveEntry*> matches = archive.findEntries(pattern);
    if (matches.empty()) {
        std::cerr << "No entries matched pattern: " << pattern << "\n";
        return 1;
    }

    for (const DatArchiveEntry* entry : matches) {
        std::cout << entry->path << "\n";
    }

    return 0;
}

int infoCommand(const DatArchive& archive, const std::vector<std::string>& args)
{
    if (args.empty()) {
        long long uncompressedBytes = 0;
        int compressedEntries = 0;
        long long storedBytes = 0;

        for (const DatArchiveEntry& entry : archive.entries()) {
            uncompressedBytes += entry.uncompressedSize;
            if (entry.compressed) {
                compressedEntries++;
            }

            if (entry.storedSize.has_value()) {
                storedBytes += *entry.storedSize;
            }
        }

        std::cout
            << "archive: " << archive.path() << "\n"
            << "kind: " << archive.formatName() << "\n"
            << "entries: " << archive.entries().size() << "\n"
            << "uncompressed_bytes: " << uncompressedBytes << "\n";

        if (std::string(archive.formatName()) == "fallout2") {
            std::cout
                << "data_offset: " << *archive.dataOffset() << "\n"
                << "compressed_entries: " << compressedEntries << "\n"
                << "stored_bytes: " << storedBytes << "\n";
        }

        return 0;
    }

    std::string pattern = normalizeDatPath(args[0]);
    const std::vector<const DatArchiveEntry*> matches = archive.findEntries(pattern);
    if (matches.empty()) {
        std::cerr << "No entries matched pattern: " << pattern << "\n";
        return 1;
    }

    for (const DatArchiveEntry* entry : matches) {
        std::cout << entry->path << "\tkind=" << archive.formatName();
        std::cout << "\tcompressed=" << static_cast<int>(entry->compressed);
        if (entry->storedSize.has_value()) {
            std::cout << "\tstored=" << *entry->storedSize;
        }
        std::cout << "\tuncompressed=" << entry->uncompressedSize;
        if (entry->dataOffset.has_value()) {
            std::cout << "\toffset=" << *entry->dataOffset;
        }
        std::cout << "\n";
    }

    return 0;
}

int extractCommand(const DatArchive& archive, const std::vector<std::string>& args, bool lowerExtractedPaths)
{
    if (args.empty()) {
        std::cerr << "extract requires an output directory\n";
        return 1;
    }

    std::string outputDir = args[0];
    std::string pattern = "*";
    if (args.size() >= 2) {
        pattern = normalizeDatPath(args[1]);
    }

    if (compat_mkdir(outputDir.c_str()) != 0 && errno != EEXIST) {
        std::cerr << "Failed to create output directory: " << outputDir << "\n";
        return 1;
    }

    const std::vector<const DatArchiveEntry*> matches = archive.findEntries(pattern);
    if (matches.empty()) {
        std::cerr << "No entries matched pattern: " << pattern << "\n";
        return 1;
    }

    int extracted = 0;
    for (const DatArchiveEntry* entry : matches) {
        if (!extractEntry(archive, *entry, outputDir, lowerExtractedPaths)) {
            return 1;
        }
        extracted++;
    }

    std::cout << "Extracted " << extracted << " entr";
    std::cout << (extracted == 1 ? "y" : "ies") << "\n";
    return 0;
}

int catCommand(const DatArchive& archive, const std::vector<std::string>& args)
{
    if (args.size() != 1) {
        std::cerr << "cat requires exactly one archive entry path\n";
        return 1;
    }

    std::string entryPath = normalizeDatPath(args[0]);
    std::unique_ptr<DatArchiveStream> stream = archive.openEntry(entryPath);
    if (stream == nullptr) {
        std::cerr << "Entry not found: " << entryPath << "\n";
        return 1;
    }

#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        std::cerr << "Failed to switch stdout to binary mode\n";
        return 1;
    }
#endif

    std::vector<char> buffer(64 * 1024);
    long remaining = stream->size();
    while (remaining > 0) {
        size_t chunkSize = static_cast<size_t>(std::min<long>(remaining, buffer.size()));
        size_t bytesRead = stream->read(buffer.data(), chunkSize);
        if (bytesRead == 0) {
            std::cerr << "Failed while reading entry: " << entryPath << "\n";
            return 1;
        }

        if (bytesRead != chunkSize) {
            std::cerr << "Short read while reading entry: " << entryPath << "\n";
            return 1;
        }

        remaining -= static_cast<long>(bytesRead);

        std::cout.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
        if (!std::cout) {
            std::cerr << "Failed while writing to stdout\n";
            break;
        }
    }

    if (!std::cout) {
        return 1;
    }

    return 0;
}

int run(const Options& options)
{
    if (options.command == "create") {
        return writeFo2DatArchive(options.args[0], options.args[1]) ? 0 : 1;
    }

    std::unique_ptr<DatArchive> archive = DatArchive::open(options.archivePath);
    if (archive == nullptr) {
        std::cerr << "Failed to open archive: " << options.archivePath << "\n";
        return 1;
    }

    int rc = 0;
    if (options.command == "list") {
        std::string pattern = "*";
        if (!options.args.empty()) {
            pattern = normalizeDatPath(options.args[0]);
        }
        rc = listCommand(*archive, pattern);
    } else if (options.command == "info") {
        rc = infoCommand(*archive, options.args);
    } else if (options.command == "extract") {
        rc = extractCommand(*archive, options.args, options.lowerExtractedPaths);
    } else if (options.command == "cat") {
        rc = catCommand(*archive, options.args);
    } else {
        std::cerr << "Unknown command: " << options.command << "\n";
        printUsage(std::cerr);
        rc = 1;
    }
    return rc;
}

} // namespace

} // namespace fallout

int main(int argc, char** argv)
{
    fallout::Options options;
    if (!fallout::parseOptions(argc, argv, &options)) {
        fallout::printUsage(std::cerr);
        return 1;
    }

    try {
        return fallout::run(options);
    } catch (const std::exception& e) {
        std::cerr << "Unhandled error: " << e.what() << "\n";
        return 1;
    }
}
