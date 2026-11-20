#ifndef DATAFILE_H
#define DATAFILE_H

#include <cstdint>

namespace fallout {

typedef uint8_t*(DatafileLoader)(char* path, uint8_t* palette, int* widthPtr, int* heightPtr);
typedef char*(DatafileNameMangler)(char* path);

char* datafileDefaultNameManglerImpl(char* path);
void datafileRemapPixelsRgb8(uint8_t* data, uint8_t* palette, int width, int height);
uint8_t* datafileReadRaw(char* path, int* widthPtr, int* heightPtr);
uint8_t* datafileRead(char* path, int* widthPtr, int* heightPtr);
uint8_t* datafileGetPalette();
uint8_t* datafileLoad(char* path, int* sizePtr);

} // namespace fallout

#endif /* DATAFILE_H */
