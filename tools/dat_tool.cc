#include "dfile.h"
#include "platform_compat.h"

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace fallout {

namespace {

struct Options {
    std::string archivePath;
    std::string command;
    std::vector<std::string> args;
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
            ch = '/';
        }
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
        size_t end = path.find('/', start);
        if (end == std::string::npos) {
            end = path.size();
        }

        std::string_view component(path.data() + start, end - start);
        if (component == "..") {
            return false;
        }

        start = end + 1;
    }

    return true;
}

std::string joinNativePath(const std::string& basePath, const std::string& relativePath)
{
    if (basePath.empty()) {
        return relativePath;
    }

    if (basePath.back() == '/' || basePath.back() == '\\') {
        return basePath + relativePath;
    }

    return basePath + "/" + relativePath;
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

void printUsage(std::ostream& stream)
{
    stream
        << "Usage:\n"
        << "  fallout2-dat <archive.dat> list [pattern]\n"
        << "  fallout2-dat <archive.dat> info [pattern]\n"
        << "  fallout2-dat <archive.dat> extract <output-dir> [pattern]\n"
        << "  fallout2-dat <archive.dat> cat <entry>\n"
        << "\n"
        << "Notes:\n"
        << "  - This tool is currently read-only.\n"
        << "  - Patterns use the same Windows-style wildcard matching as the game.\n"
        << "  - Archive paths are case-insensitive and should use backslashes internally.\n";
}

bool parseOptions(int argc, char** argv, Options* options)
{
    if (argc < 3) {
        return false;
    }

    options->archivePath = argv[1];
    options->command = argv[2];
    options->args.assign(argv + 3, argv + argc);

    return true;
}

bool extractEntry(DBase* dbase, const DBaseEntry& entry, const std::string& outputDir)
{
    std::string relativePath = datPathToNativePath(entry.path);
    if (!isSafeRelativeOutputPath(relativePath)) {
        std::cerr << "Refusing to extract invalid path: " << entry.path << "\n";
        return false;
    }

    std::string destination = joinNativePath(outputDir, relativePath);
    if (!ensureDirectoriesForFile(destination)) {
        std::cerr << "Failed to create output directory for: " << destination << "\n";
        return false;
    }

    DFile* stream = dfileOpen(dbase, entry.path, "rb");
    if (stream == nullptr) {
        std::cerr << "Failed to open entry: " << entry.path << "\n";
        return false;
    }

    std::ofstream output(destination, std::ios::binary);
    if (!output.is_open()) {
        std::cerr << "Failed to create output file: " << destination << "\n";
        dfileClose(stream);
        return false;
    }

    std::vector<char> buffer(64 * 1024);
    long remaining = dfileGetSize(stream);
    while (remaining > 0) {
        size_t chunkSize = static_cast<size_t>(std::min<long>(remaining, buffer.size()));
        size_t bytesRead = dfileRead(buffer.data(), 1, chunkSize, stream);
        if (bytesRead == 0) {
            std::cerr << "Failed while reading entry: " << entry.path << "\n";
            dfileClose(stream);
            return false;
        }

        output.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
        if (!output) {
            std::cerr << "Failed while writing output file: " << destination << "\n";
            dfileClose(stream);
            return false;
        }

        remaining -= static_cast<long>(bytesRead);
    }

    if (dfileClose(stream) != 0) {
        std::cerr << "Failed to close entry stream cleanly: " << entry.path << "\n";
        return false;
    }

    std::cout << entry.path << " -> " << destination << "\n";
    return true;
}

int listCommand(DBase* dbase, const std::string& pattern)
{
    DFileFindData findData;
    if (!dbaseFindFirstEntry(dbase, &findData, pattern.c_str())) {
        return 1;
    }

    do {
        std::cout << findData.fileName << "\n";
    } while (dbaseFindNextEntry(dbase, &findData));

    return 0;
}

int infoCommand(DBase* dbase, const std::vector<std::string>& args)
{
    if (args.empty()) {
        long long compressedBytes = 0;
        long long uncompressedBytes = 0;
        int compressedEntries = 0;

        for (int index = 0; index < dbase->entriesLength; index++) {
            const DBaseEntry& entry = dbase->entries[index];
            compressedBytes += entry.dataSize;
            uncompressedBytes += entry.uncompressedSize;
            if (entry.compressed == 1) {
                compressedEntries++;
            }
        }

        std::cout
            << "archive: " << dbase->path << "\n"
            << "entries: " << dbase->entriesLength << "\n"
            << "data_offset: " << dbase->dataOffset << "\n"
            << "compressed_entries: " << compressedEntries << "\n"
            << "stored_bytes: " << compressedBytes << "\n"
            << "uncompressed_bytes: " << uncompressedBytes << "\n";
        return 0;
    }

    std::string pattern = normalizeDatPath(args[0]);
    DFileFindData findData;
    if (!dbaseFindFirstEntry(dbase, &findData, pattern.c_str())) {
        std::cerr << "No entries matched pattern: " << pattern << "\n";
        return 1;
    }

    do {
        const DBaseEntry* entry = &dbase->entries[findData.index];
        std::cout
            << entry->path
            << "\tcompressed=" << static_cast<int>(entry->compressed)
            << "\tstored=" << entry->dataSize
            << "\tuncompressed=" << entry->uncompressedSize
            << "\toffset=" << entry->dataOffset
            << "\n";
    } while (dbaseFindNextEntry(dbase, &findData));

    return 0;
}

int extractCommand(DBase* dbase, const std::vector<std::string>& args)
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

    DFileFindData findData;
    if (!dbaseFindFirstEntry(dbase, &findData, pattern.c_str())) {
        std::cerr << "No entries matched pattern: " << pattern << "\n";
        return 1;
    }

    int extracted = 0;
    do {
        const DBaseEntry& entry = dbase->entries[findData.index];
        if (!extractEntry(dbase, entry, outputDir)) {
            return 1;
        }
        extracted++;
    } while (dbaseFindNextEntry(dbase, &findData));

    std::cout << "Extracted " << extracted << " entr";
    std::cout << (extracted == 1 ? "y" : "ies") << "\n";
    return 0;
}

int catCommand(DBase* dbase, const std::vector<std::string>& args)
{
    if (args.size() != 1) {
        std::cerr << "cat requires exactly one archive entry path\n";
        return 1;
    }

    std::string entryPath = normalizeDatPath(args[0]);
    DFile* stream = dfileOpen(dbase, entryPath.c_str(), "rb");
    if (stream == nullptr) {
        std::cerr << "Entry not found: " << entryPath << "\n";
        return 1;
    }

#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        dfileClose(stream);
        std::cerr << "Failed to switch stdout to binary mode\n";
        return 1;
    }
#endif

    std::vector<char> buffer(64 * 1024);
    long remaining = dfileGetSize(stream);
    while (remaining > 0) {
        size_t chunkSize = static_cast<size_t>(std::min<long>(remaining, buffer.size()));
        size_t bytesRead = dfileRead(buffer.data(), 1, chunkSize, stream);
        if (bytesRead == 0) {
            dfileClose(stream);
            std::cerr << "Failed while reading entry: " << entryPath << "\n";
            return 1;
        }

        if (bytesRead != chunkSize) {
            dfileClose(stream);
            std::cerr << "Short read while reading entry: " << entryPath << "\n";
            return 1;
        }

        remaining -= static_cast<long>(bytesRead);

        std::cout.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
        if (!std::cout) {
            dfileClose(stream);
            std::cerr << "Failed while writing to stdout\n";
            break;
        }
    }

    if (!std::cout) {
        return 1;
    }

    if (dfileClose(stream) != 0) {
        std::cerr << "Failed to close entry stream cleanly: " << entryPath << "\n";
        return 1;
    }

    return 0;
}

int run(const Options& options)
{
    DBase* dbase = dbaseOpen(options.archivePath.c_str());
    if (dbase == nullptr) {
        std::cerr << "Failed to open archive: " << options.archivePath << "\n";
        return 1;
    }

    int rc = 0;
    if (options.command == "list") {
        std::string pattern = "*";
        if (!options.args.empty()) {
            pattern = normalizeDatPath(options.args[0]);
        }
        rc = listCommand(dbase, pattern);
    } else if (options.command == "info") {
        rc = infoCommand(dbase, options.args);
    } else if (options.command == "extract") {
        rc = extractCommand(dbase, options.args);
    } else if (options.command == "cat") {
        rc = catCommand(dbase, options.args);
    } else {
        std::cerr << "Unknown command: " << options.command << "\n";
        printUsage(std::cerr);
        rc = 1;
    }

    dbaseClose(dbase);
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
