#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include "text_font.h"

namespace fallout {

extern FontManager gModernFontManager;

int interfaceFontsInit();
void interfaceFontsExit();
void interfaceFontDrawTextScaled2D(const Buffer2D& dest, int x, int y, const char* string, int color, float scale);
int interfaceFontGetStringWidthScaled(const char* string, int color, float scale);

} // namespace fallout

#endif /* FONT_MANAGER_H */
