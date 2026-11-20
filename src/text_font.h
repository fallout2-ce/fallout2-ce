#ifndef TEXT_FONT_H
#define TEXT_FONT_H

#include "color.h"
#include "draw.h"

#include <algorithm>

namespace fallout {

typedef void FontManagerSetCurrentFontProc(int font);
typedef void FontManagerDrawTextProc(unsigned char* buffer, const char* string, int length, int pitch, ColorWithFlags color);
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

extern FontManager gTextFontManager;
extern int gCurrentFont;
extern int gFontManagersCount;
extern FontManagerDrawTextProc* fontDrawTextPtr;
extern FontManagerGetLineHeightProc* fontGetLineHeight;
extern FontManagerGetStringWidthProc* fontGetStringWidth;
extern FontManagerGetCharacterWidthProc* fontGetCharacterWidth;
extern FontManagerGetMonospacedStringWidthProc* fontGetMonospacedStringWidth;
extern FontManagerGetLetterSpacingProc* fontGetLetterSpacing;
extern FontManagerGetBufferSizeProc* fontGetBufferSize;
extern FontManagerGetMonospacedCharacterWidth* fontGetMonospacedCharacterWidth;

inline void fontDrawText(unsigned char* buffer, const char* string, int length, int pitch, ColorWithFlags color)
{
    if (fontDrawTextPtr != nullptr) {
        fontDrawTextPtr(buffer, string, length, pitch, color);
    }
}

inline void fontDrawText(unsigned char* buffer, const char* string, int length, int pitch, Color color)
{
    fontDrawText(buffer, string, length, pitch, color | DRAW_TEXT_FLAG_NONE);
}

void fontDrawText2D(const Buffer2D& dest, int xPos, int yPos, const char* string, int length, ColorWithFlags color);

inline void fontDrawText2D(const Buffer2D& dest, int xPos, int yPos, const char* string, int length, Color color)
{
    fontDrawText2D(dest, xPos, yPos, string, length, color | DRAW_TEXT_FLAG_NONE);
}

int textFontsInit();
void textFontsExit();
int textFontLoad(int font);
int fontManagerAdd(FontManager* fontManager);
int fontGetCurrent();
void fontSetCurrent(int font);

class ScopedFont {
public:
    ScopedFont(int font)
        : _previousFont(fontGetCurrent())
    {
        fontSetCurrent(font);
    }

    ~ScopedFont()
    {
        fontSetCurrent(_previousFont);
    }

    ScopedFont(const ScopedFont&) = delete;
    ScopedFont& operator=(const ScopedFont&) = delete;

private:
    int _previousFont;
};

} // namespace fallout

#endif /* TEXT_FONT_H */
