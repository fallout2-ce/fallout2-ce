#ifndef TEXT_FONT_H
#define TEXT_FONT_H

#include "draw.h"

#include <algorithm>

namespace fallout {

typedef void FontManagerSetCurrentFontProc(int font);
typedef void FontManagerDrawTextProc(unsigned char* buffer, const char* string, int length, int pitch, int color);
typedef int FontManagerGetLineHeightProc();
typedef int FontManagerGetStringWidthProc(const char* string);
typedef int FontManagerGetCharacterWidthProc(int ch);
typedef int FontManagerGetMonospacedStringWidthProc(const char* string);
typedef int FontManagerGetLetterSpacingProc();
typedef int FontManagerGetBufferSizeProc(const char* string);
typedef int FontManagerGetMonospacedCharacterWidth();

typedef struct FontManager {
    int minFont;
    int maxFont;
    FontManagerSetCurrentFontProc* setCurrentProc;
    FontManagerDrawTextProc* drawTextProc;
    FontManagerGetLineHeightProc* getLineHeightProc;
    FontManagerGetStringWidthProc* getStringWidthProc;
    FontManagerGetCharacterWidthProc* getCharacterWidthProc;
    FontManagerGetMonospacedStringWidthProc* getMonospacedStringWidthProc;
    FontManagerGetLetterSpacingProc* getLetterSpacingProc;
    FontManagerGetBufferSizeProc* getBufferSizeProc;
    FontManagerGetMonospacedCharacterWidth* getMonospacedCharacterWidthProc;
} FontManager;

#define FONT_SHADOW (0x10000)
#define FONT_UNDERLINE (0x20000)
#define FONT_MONO (0x40000)

extern FontManager gTextFontManager;
extern int gCurrentFont;
extern int gFontManagersCount;
extern FontManagerDrawTextProc* fontDrawText;
extern FontManagerGetLineHeightProc* fontGetLineHeight;
extern FontManagerGetStringWidthProc* fontGetStringWidth;
extern FontManagerGetCharacterWidthProc* fontGetCharacterWidth;
extern FontManagerGetMonospacedStringWidthProc* fontGetMonospacedStringWidth;
extern FontManagerGetLetterSpacingProc* fontGetLetterSpacing;
extern FontManagerGetBufferSizeProc* fontGetBufferSize;
extern FontManagerGetMonospacedCharacterWidth* fontGetMonospacedCharacterWidth;

inline void fontDrawText2D(const Buffer2D& dest, int xPos, int yPos, const char* string, int length, int color)
{
    xPos = std::clamp(xPos, 0, dest.width - 1);
    yPos = std::clamp(yPos, 0, dest.height - 1);
    length = std::clamp(length, 0, dest.width - 1);
    fontDrawText(dest.data + dest.width * yPos + xPos, string, length, dest.width, color);
}

int textFontsInit();
void textFontsExit();
int textFontLoad(int font);
int fontManagerAdd(FontManager* fontManager);
int fontGetCurrent();
void fontSetCurrent(int font);

} // namespace fallout

#endif /* TEXT_FONT_H */
