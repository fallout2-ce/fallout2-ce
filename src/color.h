#ifndef COLOR_H
#define COLOR_H

namespace fallout {

enum Color : unsigned char {
    COLOR_FIRST = 0,
    COLOR_LAST = 255
};

enum DrawTextFlags : unsigned int {
    DRAW_TEXT_FLAG_NONE = 0x0000000,
    DRAW_TEXT_FLAG_SHADOWED = 0x0010000,
    DRAW_TEXT_FLAG_UNDERLINED = 0x0020000,
    DRAW_TEXT_FLAG_MONOSPACED = 0x0040000,
    DRAW_TEXT_FLAG_REFRESH = 0x01000000,
    DRAW_TEXT_FLAG_NO_BG = 0x02000000,
    DRAW_TEXT_FLAG_OVERFLOW = 0x04000000,
};

constexpr inline DrawTextFlags operator|(DrawTextFlags lhs, DrawTextFlags rhs) {
    return static_cast<DrawTextFlags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

constexpr inline DrawTextFlags operator~(DrawTextFlags rhs)
{
    return static_cast<DrawTextFlags>(~static_cast<unsigned int>(rhs));
}

enum ColorWithFlags : int {
    COLOR_INVALID = -1
};

constexpr inline DrawTextFlags operator&(ColorWithFlags lhs, DrawTextFlags rhs) {
    return static_cast<DrawTextFlags>(static_cast<int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline ColorWithFlags operator|(ColorWithFlags lhs, DrawTextFlags rhs) {
    return static_cast<ColorWithFlags>(static_cast<int>(lhs) | static_cast<unsigned int>(rhs));
}

constexpr inline ColorWithFlags operator&(Color lhs, DrawTextFlags rhs) {
    return static_cast<ColorWithFlags>(static_cast<unsigned char>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline ColorWithFlags operator|(Color lhs, DrawTextFlags rhs) {
    return static_cast<ColorWithFlags>(static_cast<unsigned char>(lhs) | static_cast<unsigned int>(rhs));
}

inline ColorWithFlags& operator&=(ColorWithFlags& lhs, DrawTextFlags rhs)
{
    lhs = static_cast<ColorWithFlags>(static_cast<int>(lhs) & static_cast<unsigned int>(rhs));
    return lhs;
}

inline ColorWithFlags& operator|=(ColorWithFlags& lhs, DrawTextFlags rhs)
{
    lhs = static_cast<ColorWithFlags>(static_cast<int>(lhs) | static_cast<unsigned int>(rhs));
    return lhs;
}

typedef const char*(ColorFileNameManger)(const char*);
typedef void(ColorTransitionCallback)();

#define COLOR_COUNT (256)
#define COLOR_COMPONENTS_RGB (3)
#define COLOR_MAP_SIZE (COLOR_COUNT * COLOR_COMPONENTS_RGB)
#define COLOR_PALETTE_SIZE_15BIT (32768)

extern unsigned char _cmap[COLOR_MAP_SIZE];

extern unsigned char _systemCmap[COLOR_MAP_SIZE];
extern unsigned char _currentGammaTable[64];
extern Color* _blendTable[COLOR_COUNT];
extern unsigned char _mappedColor[COLOR_COUNT];
extern Color colorMixAddTable[COLOR_COUNT][COLOR_COUNT];
extern Color intensityColorTable[COLOR_COUNT][COLOR_COUNT];
extern Color colorMixMulTable[COLOR_COUNT][COLOR_COUNT];
extern Color _colorTable[COLOR_PALETTE_SIZE_15BIT];

Color _calculateColor(int intensity, Color color);
int Color2RGB(Color c);
void colorPaletteFadeBetween(unsigned char* oldPalette, unsigned char* newPalette, int steps);
void colorPaletteSetTransitionCallback(ColorTransitionCallback* callback);
void _setSystemPalette(unsigned char* palette);
unsigned char* _getSystemPalette();
void _setSystemPaletteEntries(unsigned char* palette, int start, int end);
bool colorPaletteLoad(const char* path);
char* _colorError();
Color* _getColorBlendTable(Color ch);
void _freeColorBlendTable(Color color);
void colorSetBrightness(double value);
bool _initColors();
void _colorsClose();

// Converts a 24-bit 0xRRGGBB color to a 15-bit RGB555 index used by _colorTable.
//
// _colorTable maps RGB555 indices to the nearest 8-bit palette entry so that
// callers can request a logical color without knowing the current palette layout.
//
// RGB555 packs three 5-bit channels into a 15-bit integer:
//   bits 14-10 = R (red,   0-31)
//   bits  9- 5 = G (green, 0-31)
//   bits  4- 0 = B (blue,  0-31)
//
// Each 8-bit input channel is reduced to 5 bits by discarding the 3 least
// significant bits (>> 3), which is equivalent to dividing by 8.
// The three resulting 5-bit values are then packed into the RGB555 layout:
//   R5 = (rgb >> 16 & 0xFF) >> 3  =  rgb >> 19  (masked to 5 bits with & 0x1F)
//   G5 = (rgb >>  8 & 0xFF) >> 3  =  rgb >> 11  (masked to 5 bits with & 0x1F)
//   B5 = (rgb >>  0 & 0xFF) >> 3  =  rgb >>  3  (masked to 5 bits with & 0x1F)
constexpr int rgb555(int rgb)
{
    return ((rgb >> 19) & 0x1F) << 10 | ((rgb >> 11) & 0x1F) << 5 | ((rgb >> 3) & 0x1F);
}

#define COLOR_LIGHT_RED _colorTable[rgb555(0xFF5252)]
#define COLOR_RED _colorTable[rgb555(0xFF0000)]
#define COLOR_RED_2 _colorTable[rgb555(0xE60000)]
#define COLOR_DARK_RED _colorTable[rgb555(0xBD1042)]

#define COLOR_WHITE _colorTable[rgb555(0xFFFFFF)]
#define COLOR_PALE_BLUE _colorTable[rgb555(0xC5D6FF)]

#define COLOR_LIGHT_GREY _colorTable[rgb555(0xA5A5A5)]
#define COLOR_LIGHT_GREY_2 _colorTable[rgb555(0x9C94A5)]
#define COLOR_GREY _colorTable[rgb555(0x8C8C8C)]
#define COLOR_GREY_2 _colorTable[rgb555(0x7B7B7B)]
#define COLOR_DARK_GREY _colorTable[rgb555(0x424242)]
#define COLOR_DARK_GREY_2 _colorTable[rgb555(0x525252)]
#define COLOR_DARK_GREY_3 _colorTable[rgb555(0x212121)]

#define COLOR_BLACK _colorTable[rgb555(0x000000)]
#define COLOR_BLACK_2 _colorTable[rgb555(0x000010)]

#define COLOR_CYAN _colorTable[rgb555(0x4AFFFF)]
#define COLOR_LIGHT_BLUE _colorTable[rgb555(0x315A8C)]
#define COLOR_BLUE _colorTable[rgb555(0x0000FF)]
#define COLOR_BLUE_2 _colorTable[rgb555(0x0000AD)]
#define COLOR_SAGE_GREEN _colorTable[rgb555(0x8CBD8C)]

#define COLOR_LIGHT_PINK _colorTable[rgb555(0xFF7B7B)]
#define COLOR_MAGENTA _colorTable[rgb555(0xFF00FF)]
#define COLOR_LIGHT_PINK_2 _colorTable[rgb555(0xF77B7B)]

#define COLOR_LIGHT_GREEN _colorTable[rgb555(0x007300)]
#define COLOR_LIGHT_GREEN_2 _colorTable[rgb555(0x429C21)]
#define COLOR_LIGHT_GREEN_3 _colorTable[rgb555(0x00C500)]
#define COLOR_GREEN _colorTable[rgb555(0x00FF00)]
#define COLOR_DARK_GREEN _colorTable[rgb555(0x007B00)]
#define COLOR_DARK_GREEN_2 _colorTable[rgb555(0x084A08)]
#define COLOR_GRASS_GREEN _colorTable[rgb555(0x7B7B29)]

#define COLOR_LIGHT_YELLOW _colorTable[rgb555(0xFFFF5A)]
#define COLOR_LIGHT_GOLD _colorTable[rgb555(0xF7EF10)]
#define COLOR_LIGHT_GOLD_2 _colorTable[rgb555(0xEFEF42)]
#define COLOR_AMBER _colorTable[rgb555(0xFF9442)]
#define COLOR_YELLOW_2 _colorTable[rgb555(0xA59C19)]
#define COLOR_SAND _colorTable[rgb555(0xAD844A)]
#define COLOR_YELLOW_GREEN _colorTable[rgb555(0xEFFF08)]
#define COLOR_DARK_YELLOW _colorTable[rgb555(0x948C19)]
#define COLOR_DARK_YELLOW_2 _colorTable[rgb555(0x736319)]
#define COLOR_DARK_YELLOW_3 _colorTable[rgb555(0x947B29)]
#define COLOR_DARK_YELLOW_4 _colorTable[rgb555(0x948C08)]
#define COLOR_LIGHT_ORANGE _colorTable[rgb555(0xFFBD7B)]

#define COLOR_DARK_BROWN _colorTable[rgb555(0x634210)]
#define COLOR_OLIVE _colorTable[rgb555(0xADAD5A)]
#define COLOR_DULL_BROWN _colorTable[rgb555(0x6B5A4A)]

} // namespace fallout

#endif /* COLOR_H */
