#include "datafile.h"
#include "color.h"
#include "db.h"
#include "memory_manager.h"
#include "pcx.h"
#include "platform_compat.h"
#include <string.h>

namespace fallout {

constexpr size_t DATA_FILE_PALETTE_MAX = 768;
constexpr size_t INDEXED_PALETTE_MAX = 256;

// 0x5184AC loadFunc
DatafileLoader* gDatafileLoader = nullptr;

// 0x5184B0 mangleName
DatafileNameMangler* gDatafileNameMangler = datafileDefaultNameManglerImpl;

// 0x56D7E0 pal
uint8_t gDatafilePalette[DATA_FILE_PALETTE_MAX];

// 0x42EE70 defaultMangleName
char* datafileDefaultNameManglerImpl(char* path)
{
    return path;
}

// NOTE: Unused.
//
// 0x42EE74 datafileSetFilenameFunc
void datafileSetNameMangler(DatafileNameMangler* mangler)
{
    gDatafileNameMangler = mangler;
}

// NOTE: Unused.
//
// 0x42EE7C setBitmapLoadFunc
void datafileSetLoader(DatafileLoader* loader)
{
    gDatafileLoader = loader;
}

// 0x42EE84 datafileConvertData
void datafileRemapPixelsRgb8(uint8_t* data, uint8_t* palette, int width, int height)
{
    uint8_t indexedPalette[INDEXED_PALETTE_MAX];

    indexedPalette[0] = 0;
    for (int index = 1; index < INDEXED_PALETTE_MAX; index++) {
        int r = palette[index * 3] >> 3;
        int g = palette[index * 3 + 1] >> 3;
        int b = palette[index * 3 + 2] >> 3;
        int colorTableIndex = (r << 10) | (g << 5) | b;
        indexedPalette[index] = _colorTable[colorTableIndex];
    }

    int size = width * height;
    for (int index = 0; index < size; index++) {
        data[index] = indexedPalette[data[index]];
    }
}

// NOTE: Unused.
//
// 0x42EEF8 datafileConvertDataVGA
void datafileRemapPixelsRgb6(uint8_t* data, uint8_t* palette, int width, int height)
{
    uint8_t indexedPalette[INDEXED_PALETTE_MAX];

    indexedPalette[0] = 0;
    for (int index = 1; index < INDEXED_PALETTE_MAX; index++) {
        int r = palette[index * 3] >> 1;
        int g = palette[index * 3 + 1] >> 1;
        int b = palette[index * 3 + 2] >> 1;
        int colorTableIndex = (r << 10) | (g << 5) | b;
        indexedPalette[index] = _colorTable[colorTableIndex];
    }

    int size = width * height;
    for (int index = 0; index < size; index++) {
        data[index] = indexedPalette[data[index]];
    }
}

// 0x42EF60 loadRawDataFile
uint8_t* datafileReadRaw(char* path, int* widthPtr, int* heightPtr)
{
    char* mangledPath = gDatafileNameMangler(path);
    char* dot = strrchr(mangledPath, '.');
    if (dot != nullptr) {
        if (compat_stricmp(dot + 1, "pcx") == 0) {
            return pcxRead(mangledPath, widthPtr, heightPtr, gDatafilePalette);
        }
    }

    if (gDatafileLoader != nullptr) {
        return gDatafileLoader(mangledPath, gDatafilePalette, widthPtr, heightPtr);
    }

    return nullptr;
}

// 0x42EFCC loadDataFile
uint8_t* datafileRead(char* path, int* widthPtr, int* heightPtr)
{
    uint8_t* imageData = datafileReadRaw(path, widthPtr, heightPtr);
    if (imageData != nullptr) {
        datafileRemapPixelsRgb8(imageData, gDatafilePalette, *widthPtr, *heightPtr);
    }
    return imageData;
}

// NOTE: Unused
//
// 0x42EFF4 load256Palette
uint8_t* datafileLoadPalette(char* path)
{
    int width;
    int height;
    uint8_t* imageData = datafileReadRaw(path, &width, &height);
    if (imageData != nullptr) {
        internal_free_safe(imageData, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 148
        return gDatafilePalette;
    }

    return nullptr;
}

// NOTE: Unused.
//
// 0x42F024 trimBuffer
void datafilePackUntilZero(uint8_t* data, int* widthPtr, int* heightPtr)
{
    int width = *widthPtr;
    int height = *heightPtr;
    uint8_t* compactDataWritePtr = (uint8_t*)internal_malloc_safe(width * height, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 157

    // NOTE: Original code does not initialize `x`.
    int y = 0;
    int x = 0;
    uint8_t* rowStart = data;

    for (y = 0; y < height; y++) {
        if (*rowStart == 0) {
            break;
        }

        uint8_t* currentPixel = rowStart;
        for (x = 0; x < width; x++) {
            if (*currentPixel == 0) {
                break;
            }

            *compactDataWritePtr++ = *currentPixel++;
        }

        rowStart += width;
    }

    memcpy(data, compactDataWritePtr, x * y);
    internal_free_safe(compactDataWritePtr, __FILE__, __LINE__); // // "..\\int\\DATAFILE.C", 171
}

// 0x42F0E4 datafileGetPalette
uint8_t* datafileGetPalette()
{
    return gDatafilePalette;
}

// NOTE: Unused.
//
// 0x42F0EC datafileLoadBlock
uint8_t* datafileLoad(char* path, int* sizePtr)
{
    const char* mangledPath = gDatafileNameMangler(path);
    File* stream = fileOpen(mangledPath, "rb");
    if (stream == nullptr) {
        return nullptr;
    }

    int size = fileGetSize(stream);
    uint8_t* data = (uint8_t*)internal_malloc_safe(size, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 185
    if (data == nullptr) {
        // NOTE: This code is unreachable, internal_malloc_safe never fails.
        // Otherwise it leaks stream.
        *sizePtr = 0;
        return nullptr;
    }

    fileRead(data, 1, size, stream);
    fileClose(stream);
    *sizePtr = size;
    return data;
}

} // namespace fallout
