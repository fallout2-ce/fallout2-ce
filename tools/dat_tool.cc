#include "dat_archive.h"
#include "platform_compat.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
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
    bool lowerExtractedPaths = false;
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

void printUsage(std::ostream& stream)
{
    stream
        << "Usage:\n"
        << "  fallout2-dat <archive.dat> list [pattern]\n"
        << "  fallout2-dat <archive.dat> info [pattern]\n"
        << "  fallout2-dat <archive.dat> extract [--lower] <output-dir> [pattern]\n"
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
