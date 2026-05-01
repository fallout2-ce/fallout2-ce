#include <lodepng.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "platform_compat.h"

using namespace fallout;

namespace {

constexpr int kRotationCount = 6;
constexpr int kPaletteSize = 256 * 3;
constexpr int kFrmHeaderSize = 62;
constexpr int kFrmFrameHeaderSize = 12;
constexpr size_t kMaxFrmPixels = 16 * 1024 * 1024;

struct FrmHeader {
    int32_t version;
    int16_t framesPerSecond;
    int16_t actionFrame;
    int16_t frameCount;
    std::array<int16_t, kRotationCount> xOffsets;
    std::array<int16_t, kRotationCount> yOffsets;
    std::array<int32_t, kRotationCount> dataOffsets;
    int32_t dataSize;
};

struct FrmFrame {
    int16_t width;
    int16_t height;
    int32_t size;
    int16_t x;
    int16_t y;
    std::vector<uint8_t> pixels;
};

struct Options {
    std::string inputPath;
    std::string outputPath;
    std::optional<std::string> palettePath;
    int frame = 0;
    int direction = 0;
    bool transparent = true;
};

int16_t readInt16Big(const unsigned char* bytes)
{
    return static_cast<int16_t>((bytes[0] << 8) | bytes[1]);
}

int32_t readInt32Big(const unsigned char* bytes)
{
    return static_cast<int32_t>((static_cast<uint32_t>(bytes[0]) << 24)
        | (static_cast<uint32_t>(bytes[1]) << 16)
        | (static_cast<uint32_t>(bytes[2]) << 8)
        | static_cast<uint32_t>(bytes[3]));
}

bool readFrmHeader(std::istream& stream, FrmHeader& header)
{
    std::array<unsigned char, kFrmHeaderSize> bytes{};
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        return false;
    }

    header.version = readInt32Big(bytes.data());
    header.framesPerSecond = readInt16Big(bytes.data() + 4);
    header.actionFrame = readInt16Big(bytes.data() + 6);
    header.frameCount = readInt16Big(bytes.data() + 8);

    for (int rotation = 0; rotation < kRotationCount; rotation++) {
        header.xOffsets[rotation] = readInt16Big(bytes.data() + 10 + rotation * 2);
        header.yOffsets[rotation] = readInt16Big(bytes.data() + 22 + rotation * 2);
        header.dataOffsets[rotation] = readInt32Big(bytes.data() + 34 + rotation * 4);
    }

    header.dataSize = readInt32Big(bytes.data() + 58);
    return true;
}

bool readFrmFrame(std::istream& stream, FrmFrame& frame)
{
    std::array<unsigned char, kFrmFrameHeaderSize> bytes{};
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        return false;
    }

    frame.width = readInt16Big(bytes.data());
    frame.height = readInt16Big(bytes.data() + 2);
    frame.size = readInt32Big(bytes.data() + 4);
    frame.x = readInt16Big(bytes.data() + 8);
    frame.y = readInt16Big(bytes.data() + 10);

    if (frame.width < 0 || frame.height < 0 || frame.size < 0) {
        return false;
    }

    size_t expectedSize = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
    // This matches fallback behavior in artReadHeader.
    if (expectedSize != static_cast<size_t>(frame.size) && frame.size != 0) {
        return false;
    }

    if (expectedSize > kMaxFrmPixels) {
        return false;
    }

    frame.pixels.resize(expectedSize);
    return stream.read(reinterpret_cast<char*>(frame.pixels.data()), expectedSize).good();
}

bool pathExists(const std::string& path)
{
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
}

std::string pathDirname(const std::string& path)
{
    const size_t separator = path.find_last_of("/\\");
    if (separator == std::string::npos) {
        return {};
    }

    return path.substr(0, separator);
}

std::string pathJoin(std::string base, std::string_view child)
{
    if (base.empty()) {
        return std::string(child);
    }

    if (base.back() != '/' && base.back() != '\\') {
        base += '/';
    }

    base.append(child.begin(), child.end());
    return base;
}

std::string replaceExtensionWithPng(std::string path)
{
    const size_t separator = path.find_last_of("/\\");
    const size_t extension = path.find_last_of('.');
    if (extension != std::string::npos && (separator == std::string::npos || extension > separator)) {
        path.resize(extension);
    }

    path += ".png";
    return path;
}

bool ensureDirectoriesForFile(const std::string& path)
{
    const std::string directory = pathDirname(path);
    if (directory.empty()) {
        return true;
    }

    return compat_mkdir_recursive(directory.c_str()) == 0 || errno == EEXIST;
}

std::optional<FrmFrame> loadFrmFrame(const std::string& inputPath, int frameIndex, int direction, FrmHeader& header)
{
    std::unique_ptr<std::istream> ownedStream;
    std::istream* input = nullptr;

    if (inputPath == "-") {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        input = &std::cin;
    } else {
        auto fileStream = std::make_unique<std::ifstream>(inputPath.c_str(), std::ios::binary);
        if (!fileStream->is_open()) {
            std::cerr << "Failed to open FRM: " << inputPath << "\n";
            return std::nullopt;
        }
        input = fileStream.get();
        ownedStream = std::move(fileStream);
    }

    if (!readFrmHeader(*input, header)) {
        std::cerr << "Failed to read FRM header: " << inputPath << "\n";
        return std::nullopt;
    }

    if (header.frameCount <= 0) {
        std::cerr << "FRM has no frames: " << inputPath << "\n";
        return std::nullopt;
    }

    if (direction < 0 || direction >= kRotationCount) {
        std::cerr << "Direction out of range: " << direction << " (expected 0-5)\n";
        return std::nullopt;
    }

    if (frameIndex < 0 || frameIndex >= header.frameCount) {
        std::cerr << "Frame out of range: " << frameIndex << " (expected 0-" << (header.frameCount - 1) << ")\n";
        return std::nullopt;
    }

    const int32_t dataOffset = header.dataOffsets[direction];
    if (dataOffset < 0) {
        std::cerr << "Invalid direction offset in FRM header\n";
        return std::nullopt;
    }

    if (inputPath == "-") {
        if (direction != 0) {
            std::cerr << "Streaming input only supports direction 0 unless the FRM stores shared offsets\n";
            return std::nullopt;
        }
    } else {
        input->seekg(kFrmHeaderSize + dataOffset, std::ios::beg);
        if (!input->good()) {
            std::cerr << "Failed to seek to direction " << direction << "\n";
            return std::nullopt;
        }
    }

    FrmFrame frame{};
    for (int index = 0; index <= frameIndex; index++) {
        if (!readFrmFrame(*input, frame)) {
            std::cerr << "Failed to read frame " << index << " from " << inputPath << "\n";
            return std::nullopt;
        }
    }

    return frame;
}

std::optional<std::array<uint8_t, kPaletteSize>> loadPalette(const std::string& palettePath)
{
    std::ifstream stream(palettePath.c_str(), std::ios::binary);
    if (!stream.is_open()) {
        return std::nullopt;
    }

    std::array<uint8_t, kPaletteSize> palette{};
    if (!stream.read(reinterpret_cast<char*>(palette.data()), palette.size())) {
        return std::nullopt;
    }

    return palette;
}

std::optional<std::string> findPalettePath(const Options& options)
{
    if (options.palettePath.has_value()) {
        return *options.palettePath;
    }

    std::vector<std::string> candidates;
    const std::string inputDir = pathDirname(options.inputPath);
    if (!inputDir.empty()) {
        candidates.push_back(pathJoin(inputDir, "color.pal"));
        candidates.push_back(pathJoin(inputDir, "COLOR.PAL"));

        const std::string parentDir = pathDirname(inputDir);
        if (!parentDir.empty()) {
            candidates.push_back(pathJoin(parentDir, "color.pal"));
            candidates.push_back(pathJoin(parentDir, "COLOR.PAL"));
        }
    }

    candidates.push_back("color.pal");
    candidates.push_back("COLOR.PAL");

    for (const std::string& candidate : candidates) {
        if (!candidate.empty() && pathExists(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

std::vector<unsigned char> convertToRgba(const FrmFrame& frame, const std::array<uint8_t, kPaletteSize>& palette, bool transparent)
{
    std::vector<unsigned char> rgba(frame.pixels.size() * 4);

    for (size_t index = 0; index < frame.pixels.size(); index++) {
        const uint8_t paletteIndex = frame.pixels[index];
        rgba[index * 4] = palette[paletteIndex * 3] << 2;
        rgba[index * 4 + 1] = palette[paletteIndex * 3 + 1] << 2;
        rgba[index * 4 + 2] = palette[paletteIndex * 3 + 2] << 2;
        rgba[index * 4 + 3] = transparent && paletteIndex == 0 ? 0 : 255;
    }

    return rgba;
}

std::string defaultOutputPath(const Options& options)
{
    if (options.inputPath == "-") {
        return "-";
    }

    return replaceExtensionWithPng(options.inputPath);
}

void printUsage()
{
    std::cout
        << "Usage: ce-frm2png <input.frm> [output.png] [options]\n"
        << "       ce-frm2png - [output.png|-] [options]\n"
        << "\n"
        << "Options:\n"
        << "  --palette <path>     Palette to use, usually color.pal\n"
        << "  --frame <index>      Frame index to export, default 0\n"
        << "  --direction <index>  Direction index to export, default 0\n"
        << "  --opaque             Keep palette index 0 opaque instead of transparent\n"
        << "  Use '-' as input to read FRM from stdin, and '-' as output to write PNG to stdout.\n"
        << "  --help               Show this help\n";
}

bool parseInt(std::string_view text, int& value)
{
    if (text.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    long parsed = std::strtol(text.data(), &end, 10);
    if (errno != 0 || end != text.data() + text.size()) {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

std::optional<Options> parseOptions(int argc, char** argv)
{
    if (argc < 2) {
        printUsage();
        return std::nullopt;
    }

    Options options;
    bool inputAssigned = false;
    bool outputAssigned = false;

    for (int index = 1; index < argc; index++) {
        std::string_view arg(argv[index]);

        if (arg == "--help") {
            printUsage();
            return std::nullopt;
        }

        if (arg == "--palette") {
            if (index + 1 >= argc) {
                std::cerr << "--palette requires a value\n";
                return std::nullopt;
            }
            options.palettePath = argv[++index];
            continue;
        }

        if (arg == "--frame") {
            if (index + 1 >= argc || !parseInt(argv[index + 1], options.frame)) {
                std::cerr << "--frame requires an integer value\n";
                return std::nullopt;
            }
            index++;
            continue;
        }

        if (arg == "--direction") {
            if (index + 1 >= argc || !parseInt(argv[index + 1], options.direction)) {
                std::cerr << "--direction requires an integer value\n";
                return std::nullopt;
            }
            index++;
            continue;
        }

        if (arg == "--opaque") {
            options.transparent = false;
            continue;
        }

        if (!inputAssigned) {
            options.inputPath = std::string(arg);
            inputAssigned = true;
            continue;
        }

        if (!outputAssigned) {
            options.outputPath = std::string(arg);
            outputAssigned = true;
            continue;
        }

        std::cerr << "Unexpected argument: " << arg << "\n";
        return std::nullopt;
    }

    if (!inputAssigned) {
        printUsage();
        return std::nullopt;
    }

    if (!outputAssigned) {
        options.outputPath = defaultOutputPath(options);
    }

    return options;
}

} // namespace

int main(int argc, char** argv)
{
    std::optional<Options> options = parseOptions(argc, argv);
    if (!options.has_value()) {
        return argc >= 2 && std::string_view(argv[1]) == "--help" ? 0 : 1;
    }

    std::optional<std::string> palettePath = findPalettePath(*options);
    if (!palettePath.has_value()) {
        std::cerr << "Could not find color.pal. Pass it explicitly with --palette <path>.\n";
        return 1;
    }

    std::optional<std::array<uint8_t, kPaletteSize>> palette = loadPalette(*palettePath);
    if (!palette.has_value()) {
        std::cerr << "Failed to read palette: " << *palettePath << "\n";
        return 1;
    }

    FrmHeader header{};
    std::optional<FrmFrame> frame = loadFrmFrame(options->inputPath, options->frame, options->direction, header);
    if (!frame.has_value()) {
        return 1;
    }

    std::vector<unsigned char> rgba = convertToRgba(*frame, *palette, options->transparent);
    if (options->outputPath == "-") {
        std::vector<unsigned char> png;
        unsigned error = lodepng::encode(png, rgba, frame->width, frame->height);
        if (error != 0) {
            std::cerr << "PNG encode failed: " << lodepng_error_text(error) << "\n";
            return 1;
        }

#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        std::cout.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
        if (!std::cout) {
            std::cerr << "Failed while writing PNG to stdout\n";
            return 1;
        }
    } else {
        if (!ensureDirectoriesForFile(options->outputPath)) {
            std::cerr << "Failed to create output directory for " << options->outputPath << "\n";
            return 1;
        }

        unsigned error = lodepng::encode(options->outputPath, rgba, frame->width, frame->height);
        if (error != 0) {
            std::cerr << "PNG encode failed: " << lodepng_error_text(error) << "\n";
            return 1;
        }

        std::cout << "Wrote " << options->outputPath
                  << " from frame " << options->frame
                  << ", direction " << options->direction
                  << " using palette " << *palettePath << "\n";
    }

    return 0;
}
