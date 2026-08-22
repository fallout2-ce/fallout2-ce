#ifndef GRAPH_LIB_H
#define GRAPH_LIB_H

#include "color.h"

namespace fallout {

unsigned char HighRGB(Color color);
int load_lbm_to_buf(const char* path, unsigned char* dstBuffer, int xMin, int yMin, int xMax, int yMax);
int graphCompress(unsigned char* a1, unsigned char* a2, int a3);
int graphDecompress(unsigned char* a1, unsigned char* a2, int a3);
void grayscalePaletteUpdate(Color lowColor, Color highColor);
void grayscalePaletteApply(unsigned char* surface, int width, int height, int pitch);

} // namespace fallout

#endif /* GRAPH_LIB_H */
