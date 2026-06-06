#include <lodepng.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>
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
constexpr int32_t kFrmVersion = 4;
constexpr size_t kMaxFrmPixels = 16 * 1024 * 1024;
constexpr size_t kMaxPngInputBytes = kMaxFrmPixels * 8;

enum class ConversionMode {
    FrmToPng,
    PngToFrm,
    PngToIndexedPng,
};

enum class InputFormat {
    Frm,
    Png,
};

enum class OutputFormat {
    Frm,
    Png,
};

struct FrmHeader {
    int32_t version = kFrmVersion;
    int16_t framesPerSecond = 10;
    int16_t actionFrame = 0;
    int16_t frameCount = 1;
    std::array<int16_t, kRotationCount> xOffsets{};
    std::array<int16_t, kRotationCount> yOffsets{};
    std::array<int32_t, kRotationCount> dataOffsets{};
    int32_t dataSize = 0;
};

struct FrmFrame {
    int16_t width = 0;
    int16_t height = 0;
    int32_t size = 0;
    int16_t x = 0;
    int16_t y = 0;
    std::vector<uint8_t> pixels;
};

struct Options {
    std::string inputPath;
    std::string outputPath;
    std::optional<std::string> palettePath;
    std::optional<ConversionMode> mode;
    std::optional<InputFormat> inputFormat;
    std::optional<OutputFormat> outputFormat;
    int frame = 0;
    int direction = 0;
    int framesPerSecond = 10;
    int actionFrame = 0;
    int xOffset = 0;
    int yOffset = 0;
    bool transparent = true;
    bool frameSpecified = false;
    bool directionSpecified = false;
    bool framesPerSecondSpecified = false;
    bool actionFrameSpecified = false;
    bool xOffsetSpecified = false;
    bool yOffsetSpecified = false;
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

void writeInt16Big(std::ostream& stream, int16_t value)
{
    unsigned char bytes[2] = {
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>(value & 0xFF),
    };
    stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

void writeInt32Big(std::ostream& stream, int32_t value)
{
    unsigned char bytes[4] = {
        static_cast<unsigned char>((value >> 24) & 0xFF),
        static_cast<unsigned char>((value >> 16) & 0xFF),
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>(value & 0xFF),
    };
    stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
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
    if (expectedSize != static_cast<size_t>(frame.size)) {
        return false;
    }

    if (expectedSize > kMaxFrmPixels) {
        return false;
    }

    frame.pixels.resize(expectedSize);
    return stream.read(reinterpret_cast<char*>(frame.pixels.data()), frame.pixels.size()).good();
}

bool writeFrmHeader(std::ostream& stream, const FrmHeader& header)
{
    writeInt32Big(stream, header.version);
    writeInt16Big(stream, header.framesPerSecond);
    writeInt16Big(stream, header.actionFrame);
    writeInt16Big(stream, header.frameCount);

    for (int rotation = 0; rotation < kRotationCount; rotation++) {
        writeInt16Big(stream, header.xOffsets[rotation]);
    }

    for (int rotation = 0; rotation < kRotationCount; rotation++) {
        writeInt16Big(stream, header.yOffsets[rotation]);
    }

    for (int rotation = 0; rotation < kRotationCount; rotation++) {
        writeInt32Big(stream, header.dataOffsets[rotation]);
    }

    writeInt32Big(stream, header.dataSize);
    return stream.good();
}

bool writeFrmFrame(std::ostream& stream, const FrmFrame& frame)
{
    writeInt16Big(stream, frame.width);
    writeInt16Big(stream, frame.height);
    writeInt32Big(stream, frame.size);
    writeInt16Big(stream, frame.x);
    writeInt16Big(stream, frame.y);
    stream.write(reinterpret_cast<const char*>(frame.pixels.data()), frame.pixels.size());
    return stream.good();
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

char asciiToLower(char ch)
{
    return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
}

bool hasExtension(std::string_view path, std::string_view extension)
{
    if (path.size() < extension.size()) {
        return false;
    }

    for (size_t index = 0; index < extension.size(); index++) {
        if (asciiToLower(path[path.size() - extension.size() + index]) != asciiToLower(extension[index])) {
            return false;
        }
    }

    return true;
}

std::string replaceExtension(std::string path, std::string_view extension)
{
    const size_t separator = path.find_last_of("/\\");
    const size_t oldExtension = path.find_last_of('.');
    if (oldExtension != std::string::npos && (separator == std::string::npos || oldExtension > separator)) {
        path.resize(oldExtension);
    }

    path.append(extension.begin(), extension.end());
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

bool fitsInt16(int value)
{
    return value >= SHRT_MIN && value <= SHRT_MAX;
}

bool fitsInt32(size_t value)
{
    return value <= static_cast<size_t>(INT32_MAX);
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

unsigned addPaletteToPngState(lodepng::State& state, const std::array<uint8_t, kPaletteSize>& palette, bool transparent)
{
    for (int index = 0; index < 256; index++) {
        unsigned char alpha = transparent && index == 0 ? 0 : 255;
        unsigned error = lodepng_palette_add(&state.info_raw,
            palette[index * 3] << 2,
            palette[index * 3 + 1] << 2,
            palette[index * 3 + 2] << 2,
            alpha);
        if (error != 0) {
            return error;
        }

        error = lodepng_palette_add(&state.info_png.color,
            palette[index * 3] << 2,
            palette[index * 3 + 1] << 2,
            palette[index * 3 + 2] << 2,
            alpha);
        if (error != 0) {
            return error;
        }
    }

    return 0;
}

unsigned encodeIndexedPng(std::vector<unsigned char>& png, const FrmFrame& frame, const std::array<uint8_t, kPaletteSize>& palette, bool transparent)
{
    lodepng::State state;
    state.encoder.auto_convert = 0;
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 8;
    state.info_png.color.colortype = LCT_PALETTE;
    state.info_png.color.bitdepth = 8;

    unsigned error = addPaletteToPngState(state, palette, transparent);
    if (error != 0) {
        return error;
    }

    return lodepng::encode(png, frame.pixels, frame.width, frame.height, state);
}

std::optional<std::vector<unsigned char>> readBinaryInput(const std::string& inputPath, const char* description)
{
    std::vector<unsigned char> data;

    if (inputPath == "-") {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        constexpr size_t kChunkSize = 16 * 1024;
        std::array<char, kChunkSize> buffer{};
        while (std::cin.good()) {
            std::cin.read(buffer.data(), buffer.size());
            std::streamsize count = std::cin.gcount();
            if (count > 0) {
                size_t chunkSize = static_cast<size_t>(count);
                if (chunkSize > kMaxPngInputBytes || data.size() > kMaxPngInputBytes - chunkSize) {
                    std::cerr << description << " is too large (limit " << kMaxPngInputBytes << " bytes)\n";
                    return std::nullopt;
                }
                data.insert(data.end(),
                    reinterpret_cast<unsigned char*>(buffer.data()),
                    reinterpret_cast<unsigned char*>(buffer.data()) + count);
            }
        }

        if (!std::cin.eof()) {
            std::cerr << "Failed while reading " << description << " from stdin\n";
            return std::nullopt;
        }
    } else {
        std::ifstream stream(inputPath.c_str(), std::ios::binary);
        if (!stream.is_open()) {
            std::cerr << "Failed to open " << description << ": " << inputPath << "\n";
            return std::nullopt;
        }

        stream.seekg(0, std::ios::end);
        std::streamoff size = stream.tellg();
        if (size < 0) {
            std::cerr << "Failed to read " << description << ": " << inputPath << "\n";
            return std::nullopt;
        }
        if (size > static_cast<std::streamoff>(kMaxPngInputBytes)) {
            std::cerr << description << " is too large (limit " << kMaxPngInputBytes << " bytes): " << inputPath << "\n";
            return std::nullopt;
        }
        stream.seekg(0, std::ios::beg);

        data.resize(static_cast<size_t>(size));
        if (!data.empty() && !stream.read(reinterpret_cast<char*>(data.data()), data.size())) {
            std::cerr << "Failed to read " << description << ": " << inputPath << "\n";
            return std::nullopt;
        }
    }

    return data;
}

uint8_t findNearestPaletteIndex(const std::array<uint8_t, kPaletteSize>& palette, uint8_t red, uint8_t green, uint8_t blue, bool includeTransparentIndex)
{
    int startIndex = includeTransparentIndex ? 0 : 1;
    int bestIndex = startIndex;
    int bestDistance = INT_MAX;

    for (int index = startIndex; index < 256; index++) {
        int paletteRed = palette[index * 3] << 2;
        int paletteGreen = palette[index * 3 + 1] << 2;
        int paletteBlue = palette[index * 3 + 2] << 2;

        int deltaRed = static_cast<int>(red) - paletteRed;
        int deltaGreen = static_cast<int>(green) - paletteGreen;
        int deltaBlue = static_cast<int>(blue) - paletteBlue;
        int distance = deltaRed * deltaRed + deltaGreen * deltaGreen + deltaBlue * deltaBlue;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
            if (distance == 0) {
                break;
            }
        }
    }

    return static_cast<uint8_t>(bestIndex);
}

bool unpackIndexedPngPixels(const std::vector<unsigned char>& indexedData, unsigned width, unsigned height, unsigned bitdepth, std::vector<uint8_t>& pixels)
{
    size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    pixels.resize(pixelCount);

    if (bitdepth == 8) {
        if (indexedData.size() < pixelCount) {
            return false;
        }

        std::copy(indexedData.begin(), indexedData.begin() + pixelCount, pixels.begin());
        return true;
    }

    unsigned char mask = static_cast<unsigned char>((1 << bitdepth) - 1);
    size_t neededBytes = (pixelCount * bitdepth + 7) / 8;
    if (indexedData.size() < neededBytes) {
        return false;
    }

    for (size_t index = 0; index < pixelCount; index++) {
        size_t bitOffset = index * bitdepth;
        unsigned char byte = indexedData[bitOffset / 8];
        unsigned shift = 8 - bitdepth - static_cast<unsigned>(bitOffset % 8);
        pixels[index] = (byte >> shift) & mask;
    }

    return true;
}

bool pngPaletteMatchesFalloutPalette(const LodePNGColorMode& pngColor, const std::array<uint8_t, kPaletteSize>& palette, const std::vector<uint8_t>& pixels, bool transparent)
{
    if (pngColor.colortype != LCT_PALETTE || pngColor.palette == nullptr) {
        return false;
    }

    std::array<bool, 256> usedIndexes{};
    for (uint8_t paletteIndex : pixels) {
        usedIndexes[paletteIndex] = true;
    }

    for (size_t index = 0; index < usedIndexes.size(); index++) {
        if (!usedIndexes[index]) {
            continue;
        }
        if (pngColor.palettesize <= index) {
            return false;
        }
        const unsigned char* pngPalette = pngColor.palette + index * 4;
        if (pngPalette[0] != static_cast<unsigned char>(palette[index * 3] << 2)
            || pngPalette[1] != static_cast<unsigned char>(palette[index * 3 + 1] << 2)
            || pngPalette[2] != static_cast<unsigned char>(palette[index * 3 + 2] << 2)) {
            return false;
        }
        if (transparent) {
            if (index == 0) {
                if (pngPalette[3] != 0) {
                    return false;
                }
            } else if (pngPalette[3] != 255) {
                return false;
            }
        }
    }

    return true;
}

std::optional<FrmFrame> makePngFrame(const Options& options, unsigned width, unsigned height)
{
    if (width == 0 || height == 0) {
        std::cerr << "PNG must have non-zero dimensions\n";
        return std::nullopt;
    }

    size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (pixelCount > kMaxFrmPixels) {
        std::cerr << "PNG is too large: " << width << "x" << height << "\n";
        return std::nullopt;
    }

    if (width > static_cast<unsigned>(SHRT_MAX) || height > static_cast<unsigned>(SHRT_MAX)) {
        std::cerr << "PNG dimensions exceed FRM limits: " << width << "x" << height << "\n";
        return std::nullopt;
    }

    FrmFrame frame{};
    frame.width = static_cast<int16_t>(width);
    frame.height = static_cast<int16_t>(height);
    frame.size = static_cast<int32_t>(pixelCount);
    frame.x = static_cast<int16_t>(options.xOffset);
    frame.y = static_cast<int16_t>(options.yOffset);
    frame.pixels.resize(pixelCount);
    return frame;
}

void quantizeRgbaToFrame(const std::vector<unsigned char>& rgba, const std::array<uint8_t, kPaletteSize>& palette, bool transparent, FrmFrame& frame)
{
    std::unordered_map<uint32_t, uint8_t> paletteCache;
    paletteCache.reserve(std::min<size_t>(frame.pixels.size(), 4096));

    bool includeTransparentIndex = !transparent;
    for (size_t index = 0; index < frame.pixels.size(); index++) {
        uint8_t red = rgba[index * 4];
        uint8_t green = rgba[index * 4 + 1];
        uint8_t blue = rgba[index * 4 + 2];
        uint8_t alpha = rgba[index * 4 + 3];

        if (transparent && alpha < 128) {
            frame.pixels[index] = 0;
            continue;
        }

        uint32_t colorKey = (static_cast<uint32_t>(red) << 16)
            | (static_cast<uint32_t>(green) << 8)
            | static_cast<uint32_t>(blue);

        auto it = paletteCache.find(colorKey);
        if (it != paletteCache.end()) {
            frame.pixels[index] = it->second;
            continue;
        }

        uint8_t paletteIndex = findNearestPaletteIndex(palette, red, green, blue, includeTransparentIndex);
        paletteCache.emplace(colorKey, paletteIndex);
        frame.pixels[index] = paletteIndex;
    }
}

std::optional<FrmFrame> loadPngFrame(const Options& options, const std::array<uint8_t, kPaletteSize>& palette)
{
    std::optional<std::vector<unsigned char>> encoded = readBinaryInput(options.inputPath, "PNG");
    if (!encoded.has_value()) {
        return std::nullopt;
    }

    lodepng::State indexedState;
    indexedState.decoder.color_convert = 0;
    std::vector<unsigned char> indexedData;
    unsigned width = 0;
    unsigned height = 0;
    unsigned error = lodepng::decode(indexedData, width, height, indexedState, *encoded);

    if (error != 0) {
        std::cerr << "PNG decode failed: " << lodepng_error_text(error) << "\n";
        return std::nullopt;
    }

    std::optional<FrmFrame> frame = makePngFrame(options, width, height);
    if (!frame.has_value()) {
        return std::nullopt;
    }

    if (indexedState.info_png.color.colortype == LCT_PALETTE) {
        std::vector<uint8_t> indexedPixels;
        if (!unpackIndexedPngPixels(indexedData, width, height, indexedState.info_png.color.bitdepth, indexedPixels)) {
            std::cerr << "PNG indexed pixel data is invalid\n";
            return std::nullopt;
        }

        if (pngPaletteMatchesFalloutPalette(indexedState.info_png.color, palette, indexedPixels, options.transparent)) {
            frame->pixels = std::move(indexedPixels);
            return frame;
        }
    }

    std::vector<unsigned char> rgba;
    error = lodepng::decode(rgba, width, height, *encoded);
    if (error != 0) {
        std::cerr << "PNG decode failed: " << lodepng_error_text(error) << "\n";
        return std::nullopt;
    }

    quantizeRgbaToFrame(rgba, palette, options.transparent, *frame);
    return frame;
}

FrmHeader makeSingleFrameHeader(const Options& options, const FrmFrame& frame)
{
    FrmHeader header{};
    header.framesPerSecond = static_cast<int16_t>(options.framesPerSecond);
    header.actionFrame = static_cast<int16_t>(options.actionFrame);
    header.frameCount = 1;

    int32_t directionSize = kFrmFrameHeaderSize + frame.size;
    for (int rotation = 0; rotation < kRotationCount; rotation++) {
        header.xOffsets[rotation] = 0;
        header.yOffsets[rotation] = 0;
        header.dataOffsets[rotation] = 0;
    }

    header.dataSize = directionSize;
    return header;
}

bool writeFrm(const std::string& outputPath, const FrmHeader& header, const FrmFrame& frame)
{
    std::unique_ptr<std::ostream> ownedStream;
    std::ostream* output = nullptr;

    if (outputPath == "-") {
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        output = &std::cout;
    } else {
        if (!ensureDirectoriesForFile(outputPath)) {
            std::cerr << "Failed to create output directory for " << outputPath << "\n";
            return false;
        }

        auto fileStream = std::make_unique<std::ofstream>(outputPath.c_str(), std::ios::binary);
        if (!fileStream->is_open()) {
            std::cerr << "Failed to open output FRM: " << outputPath << "\n";
            return false;
        }

        output = fileStream.get();
        ownedStream = std::move(fileStream);
    }

    if (!writeFrmHeader(*output, header)) {
        std::cerr << "Failed while writing FRM header\n";
        return false;
    }

    for (int rotation = 0; rotation < kRotationCount; rotation++) {
        if (rotation != 0 && header.dataOffsets[rotation - 1] == header.dataOffsets[rotation]) {
            continue;
        }

        if (!writeFrmFrame(*output, frame)) {
            std::cerr << "Failed while writing FRM frame data\n";
            return false;
        }
    }

    return output->good();
}

bool writePngOutput(const std::string& outputPath, const std::vector<unsigned char>& png)
{
    if (outputPath == "-") {
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        std::cout.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
        if (!std::cout) {
            std::cerr << "Failed while writing PNG to stdout\n";
            return false;
        }
    } else {
        if (!ensureDirectoriesForFile(outputPath)) {
            std::cerr << "Failed to create output directory for " << outputPath << "\n";
            return false;
        }

        unsigned error = lodepng::save_file(png, outputPath);
        if (error != 0) {
            std::cerr << "PNG write failed: " << lodepng_error_text(error) << "\n";
            return false;
        }
    }

    return true;
}

bool encodeAndWriteIndexedPng(const std::string& outputPath, const FrmFrame& frame, const std::array<uint8_t, kPaletteSize>& palette, bool transparent)
{
    std::vector<unsigned char> png;
    unsigned error = encodeIndexedPng(png, frame, palette, transparent);
    if (error != 0) {
        std::cerr << "PNG encode failed: " << lodepng_error_text(error) << "\n";
        return false;
    }

    return writePngOutput(outputPath, png);
}

std::optional<InputFormat> detectInputFormat(const Options& options)
{
    if (hasExtension(options.inputPath, ".frm")) {
        return InputFormat::Frm;
    }

    if (hasExtension(options.inputPath, ".png")) {
        return InputFormat::Png;
    }

    return std::nullopt;
}

const char* expectedOutputExtension(OutputFormat outputFormat)
{
    return outputFormat == OutputFormat::Frm ? ".frm" : ".png";
}

bool outputExtensionMatches(const std::string& outputPath, OutputFormat outputFormat)
{
    if (outputPath == "-") {
        return true;
    }

    return hasExtension(outputPath, expectedOutputExtension(outputFormat));
}

std::optional<ConversionMode> conversionModeForFormats(InputFormat inputFormat, OutputFormat outputFormat)
{
    if (inputFormat == InputFormat::Frm && outputFormat == OutputFormat::Png) {
        return ConversionMode::FrmToPng;
    }

    if (inputFormat == InputFormat::Png && outputFormat == OutputFormat::Frm) {
        return ConversionMode::PngToFrm;
    }

    if (inputFormat == InputFormat::Png && outputFormat == OutputFormat::Png) {
        return ConversionMode::PngToIndexedPng;
    }

    return std::nullopt;
}

OutputFormat defaultOutputFormat(InputFormat inputFormat)
{
    return inputFormat == InputFormat::Frm ? OutputFormat::Png : OutputFormat::Frm;
}

std::optional<OutputFormat> detectOutputFormat(const std::string& outputPath)
{
    if (hasExtension(outputPath, ".frm")) {
        return OutputFormat::Frm;
    }

    if (hasExtension(outputPath, ".png")) {
        return OutputFormat::Png;
    }

    return std::nullopt;
}

std::string defaultOutputPath(const Options& options, OutputFormat outputFormat)
{
    if (options.inputPath == "-") {
        return "-";
    }

    return replaceExtension(options.inputPath, expectedOutputExtension(outputFormat));
}

void printUsage()
{
    std::cout
        << "Usage: ce-frm2png <input.frm> [output.png] [options]\n"
        << "       ce-frm2png <input.png> [output.frm] [options]\n"
        << "       ce-frm2png <input.png> <output.png> [options]\n"
        << "       ce-frm2png - [output|-] (--from-frm|--from-png) [--to-png|--to-frm] [options]\n"
        << "\n"
        << "Mode selection:\n"
        << "  Input type is auto-detected from .frm/.png paths. Use --from-frm or --from-png for stdin or ambiguous paths.\n"
        << "  Output type is auto-detected from explicit .frm/.png output paths.\n"
        << "  Output defaults to .png for FRM input and .frm for PNG input when output is omitted or '-'.\n"
        << "  PNG -> indexed PNG requires an explicit output path unless you're streaming to '-'.\n"
        << "  All PNG output is Fallout palette-indexed.\n"
        << "  --from-* selectors are mutually exclusive; --to-* selectors are mutually exclusive.\n"
        << "\n"
        << "Common options:\n"
        << "  --palette <path>      Palette to use, usually color.pal\n"
        << "  --opaque              Treat palette index 0 as opaque. For PNG input, disables alpha->index0 mapping.\n"
        << "  --to-png              Write indexed PNG output\n"
        << "  --to-frm              Write FRM output\n"
        << "  --help                Show this help\n"
        << "\n"
        << "FRM -> PNG options:\n"
        << "  --frame <index>       Frame index to export, default 0\n"
        << "  --direction <index>   Direction index to export, default 0\n"
        << "\n"
        << "PNG -> FRM options:\n"
        << "  --fps <value>         Frames per second for the FRM header, default 10\n"
        << "  --action-frame <n>    Action frame for the FRM header, default 0\n"
        << "  --x-offset <value>    Per-frame x offset to store, default 0\n"
        << "  --y-offset <value>    Per-frame y offset to store, default 0\n"
        << "  PNG input currently writes one frame payload and points all six directions at it.\n"
        << "\n"
        << "Streaming:\n"
        << "  Use '-' as input to read from stdin, and '-' as output to write to stdout.\n";
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
    std::optional<std::string_view> inputSelector;
    std::optional<std::string_view> outputSelector;

    for (int index = 1; index < argc; index++) {
        std::string_view arg(argv[index]);

        if (arg == "--help") {
            printUsage();
            return std::nullopt;
        }

        if (arg == "--from-frm") {
            if (inputSelector.has_value()) {
                std::cerr << "Only one input selector is allowed (" << *inputSelector << " and " << arg << " were provided)\n";
                return std::nullopt;
            }
            inputSelector = arg;
            options.inputFormat = InputFormat::Frm;
            continue;
        }

        if (arg == "--from-png") {
            if (inputSelector.has_value()) {
                std::cerr << "Only one input selector is allowed (" << *inputSelector << " and " << arg << " were provided)\n";
                return std::nullopt;
            }
            inputSelector = arg;
            options.inputFormat = InputFormat::Png;
            continue;
        }

        if (arg == "--to-png") {
            if (outputSelector.has_value()) {
                std::cerr << "Only one output selector is allowed (" << *outputSelector << " and " << arg << " were provided)\n";
                return std::nullopt;
            }
            outputSelector = arg;
            options.outputFormat = OutputFormat::Png;
            continue;
        }

        if (arg == "--to-frm") {
            if (outputSelector.has_value()) {
                std::cerr << "Only one output selector is allowed (" << *outputSelector << " and " << arg << " were provided)\n";
                return std::nullopt;
            }
            outputSelector = arg;
            options.outputFormat = OutputFormat::Frm;
            continue;
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
            options.frameSpecified = true;
            index++;
            continue;
        }

        if (arg == "--direction") {
            if (index + 1 >= argc || !parseInt(argv[index + 1], options.direction)) {
                std::cerr << "--direction requires an integer value\n";
                return std::nullopt;
            }
            options.directionSpecified = true;
            index++;
            continue;
        }

        if (arg == "--fps") {
            if (index + 1 >= argc || !parseInt(argv[index + 1], options.framesPerSecond)) {
                std::cerr << "--fps requires an integer value\n";
                return std::nullopt;
            }
            options.framesPerSecondSpecified = true;
            index++;
            continue;
        }

        if (arg == "--action-frame") {
            if (index + 1 >= argc || !parseInt(argv[index + 1], options.actionFrame)) {
                std::cerr << "--action-frame requires an integer value\n";
                return std::nullopt;
            }
            options.actionFrameSpecified = true;
            index++;
            continue;
        }

        if (arg == "--x-offset") {
            if (index + 1 >= argc || !parseInt(argv[index + 1], options.xOffset)) {
                std::cerr << "--x-offset requires an integer value\n";
                return std::nullopt;
            }
            options.xOffsetSpecified = true;
            index++;
            continue;
        }

        if (arg == "--y-offset") {
            if (index + 1 >= argc || !parseInt(argv[index + 1], options.yOffset)) {
                std::cerr << "--y-offset requires an integer value\n";
                return std::nullopt;
            }
            options.yOffsetSpecified = true;
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

    std::optional<InputFormat> detectedInputFormat = detectInputFormat(options);
    if (options.inputPath == "-" && !options.inputFormat.has_value()) {
        std::cerr << "Reading from stdin requires --from-frm or --from-png.\n";
        return std::nullopt;
    }

    if (options.inputFormat.has_value()) {
        if (detectedInputFormat.has_value() && detectedInputFormat != options.inputFormat) {
            std::cerr << *inputSelector << " contradicts input path extension: " << options.inputPath << "\n";
            return std::nullopt;
        }
    } else if (detectedInputFormat.has_value()) {
        options.inputFormat = detectedInputFormat;
    } else {
        std::cerr << "Could not determine input type from path. Use --from-frm or --from-png.\n";
        return std::nullopt;
    }

    if (options.outputFormat.has_value()) {
        std::optional<OutputFormat> detectedOutputFormat = detectOutputFormat(options.outputPath);
        if (outputAssigned && detectedOutputFormat.has_value() && detectedOutputFormat != options.outputFormat) {
            std::cerr << *outputSelector << " contradicts output path extension: " << options.outputPath << "\n";
            return std::nullopt;
        }
    } else if (outputAssigned && options.outputPath != "-") {
        std::optional<OutputFormat> detectedOutputFormat = detectOutputFormat(options.outputPath);
        if (!detectedOutputFormat.has_value()) {
            std::cerr << "Could not determine output type from path. Use --to-png or --to-frm.\n";
            return std::nullopt;
        }
        options.outputFormat = detectedOutputFormat;
    } else {
        options.outputFormat = defaultOutputFormat(*options.inputFormat);
    }

    if (!outputAssigned) {
        options.outputPath = defaultOutputPath(options, *options.outputFormat);
    }

    if (!outputExtensionMatches(options.outputPath, *options.outputFormat)) {
        std::cerr << "Output path " << options.outputPath << " does not match selected output type (expected "
                  << expectedOutputExtension(*options.outputFormat) << ")\n";
        return std::nullopt;
    }

    std::optional<ConversionMode> mode = conversionModeForFormats(*options.inputFormat, *options.outputFormat);
    if (!mode.has_value()) {
        std::cerr << "Unsupported conversion. Supported conversions are FRM -> PNG, PNG -> FRM, and PNG -> indexed PNG.\n";
        return std::nullopt;
    }
    options.mode = mode;

    if (*options.mode == ConversionMode::PngToIndexedPng && !outputAssigned && options.inputPath != "-") {
        std::cerr << "PNG -> indexed PNG requires an explicit output path to avoid overwriting the input\n";
        return std::nullopt;
    }

    const bool frmOnlyOptionSpecified = options.frameSpecified || options.directionSpecified;
    const bool pngToFrmOnlyOptionSpecified = options.framesPerSecondSpecified
        || options.actionFrameSpecified
        || options.xOffsetSpecified
        || options.yOffsetSpecified;

    if (*options.inputFormat != InputFormat::Frm && frmOnlyOptionSpecified) {
        std::cerr << "--frame and --direction are only supported for FRM input\n";
        return std::nullopt;
    }

    if (*options.mode != ConversionMode::PngToFrm && pngToFrmOnlyOptionSpecified) {
        std::cerr << "--fps, --action-frame, --x-offset, and --y-offset are only supported for PNG -> FRM output\n";
        return std::nullopt;
    }

    if (!fitsInt16(options.framesPerSecond)) {
        std::cerr << "--fps is out of FRM range\n";
        return std::nullopt;
    }

    if (!fitsInt16(options.actionFrame)) {
        std::cerr << "--action-frame is out of FRM range\n";
        return std::nullopt;
    }

    if (!fitsInt16(options.xOffset)) {
        std::cerr << "--x-offset is out of FRM range\n";
        return std::nullopt;
    }

    if (!fitsInt16(options.yOffset)) {
        std::cerr << "--y-offset is out of FRM range\n";
        return std::nullopt;
    }

    return options;
}

int runFrmToPng(const Options& options, const std::array<uint8_t, kPaletteSize>& palette)
{
    FrmHeader header{};
    std::optional<FrmFrame> frame = loadFrmFrame(options.inputPath, options.frame, options.direction, header);
    if (!frame.has_value()) {
        return 1;
    }

    if (!encodeAndWriteIndexedPng(options.outputPath, *frame, palette, options.transparent)) {
        return 1;
    }

    if (options.outputPath != "-") {
        std::cout << "Wrote " << options.outputPath
                  << " from frame " << options.frame
                  << ", direction " << options.direction
                  << " using palette " << (options.palettePath.has_value() ? *options.palettePath : "auto-detected")
                  << "\n";
    }

    return 0;
}

int runPngToFrm(const Options& options, const std::array<uint8_t, kPaletteSize>& palette)
{
    std::optional<FrmFrame> frame = loadPngFrame(options, palette);
    if (!frame.has_value()) {
        return 1;
    }

    if (!fitsInt32(kFrmFrameHeaderSize + static_cast<size_t>(frame->size))) {
        std::cerr << "FRM output would exceed supported size\n";
        return 1;
    }

    FrmHeader header = makeSingleFrameHeader(options, *frame);
    if (!writeFrm(options.outputPath, header, *frame)) {
        return 1;
    }

    if (options.outputPath != "-") {
        std::cout << "Wrote " << options.outputPath
                  << " as a single-frame FRM with one shared direction payload"
                  << " using palette " << (options.palettePath.has_value() ? *options.palettePath : "auto-detected")
                  << "\n";
    }

    return 0;
}

int runPngToIndexedPng(const Options& options, const std::array<uint8_t, kPaletteSize>& palette)
{
    std::optional<FrmFrame> frame = loadPngFrame(options, palette);
    if (!frame.has_value()) {
        return 1;
    }

    if (!encodeAndWriteIndexedPng(options.outputPath, *frame, palette, options.transparent)) {
        return 1;
    }

    if (options.outputPath != "-") {
        std::cout << "Wrote indexed PNG " << options.outputPath
                  << " using palette " << (options.palettePath.has_value() ? *options.palettePath : "auto-detected")
                  << "\n";
    }

    return 0;
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

    if (!options->palettePath.has_value()) {
        options->palettePath = *palettePath;
    }

    switch (*options->mode) {
    case ConversionMode::FrmToPng:
        return runFrmToPng(*options, *palette);
    case ConversionMode::PngToFrm:
        return runPngToFrm(*options, *palette);
    case ConversionMode::PngToIndexedPng:
        return runPngToIndexedPng(*options, *palette);
    }

    return 1;
}
