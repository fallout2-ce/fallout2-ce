#include "color.h"

#include <math.h>
#include <string.h>

#include <algorithm>

#include "db.h"
#include "memory.h"
#include "svga.h"

namespace fallout {

static void _setIntensityTableColor(Color color);
static void _setIntensityTables();
static void _setMixTableColor(Color color);
static void _buildBlendTable(Color* ptr, Color ch);
static void _rebuildColorBlendTables();

// 0x50F930 aColor_cNoError
static char _aColor_cNoError[] = "color.c: No errors\n";

// 0x50F95C aColor_cColorTa
static char _aColor_cColorTa[] = "color.c: color table not found\n";

// 0x50F984 aColor_cColorpa
static char _aColor_cColorpa[] = "color.c: colorpalettestack overflow";

// 0x50F9AC aColor_cColor_0
static char aColor_cColor_0[] = "color.c: colorpalettestack underflow";

// 0x51DF10 errorStr
static char* _errorStr = _aColor_cNoError;

// 0x51DF14 colorsInited
static bool _colorsInited = false;

// 0x51DF18 currentGamma
static double gBrightness = 1.0;

// 0x51DF20 colorFadeBkFuncP
static ColorTransitionCallback* gColorPaletteTransitionCallback = nullptr;

// 0x51DF30 colorNameMangler
static ColorFileNameManger* gColorFileNameMangler = nullptr;

// 0x51DF34 cmap
unsigned char _cmap[COLOR_MAP_SIZE] = {
    0x3F, 0x3F, 0x3F
};

// 0x673090 systemCmap1
unsigned char _systemCmap[COLOR_MAP_SIZE];

// 0x673390 currentGammaTable
unsigned char _currentGammaTable[64];

// 0x6733D0 blendTable
Color* _blendTable[COLOR_COUNT];

// 0x6737D0 mappedColor
unsigned char _mappedColor[COLOR_COUNT];

// 0x6738D0 colorMixAddTable
Color colorMixAddTable[COLOR_COUNT][COLOR_COUNT];

// 0x6838D0 intensityColorTable
Color intensityColorTable[COLOR_COUNT][COLOR_COUNT];

// 0x6938D0 colorMixMulTable
Color colorMixMulTable[COLOR_COUNT][COLOR_COUNT];

// 0x6A38D0 colorTable
Color _colorTable[COLOR_PALETTE_SIZE_15BIT];

// 0x4C72B4 calculateColor
Color _calculateColor(int intensity, Color color)
{
    return intensityColorTable[color][intensity / 512];
}

// 0x4C72E0
int Color2RGB(Color c)
{
    int r = _cmap[3 * c] >> 1;
    int g = _cmap[3 * c + 1] >> 1;
    int b = _cmap[3 * c + 2] >> 1;

    return (r << 10) | (g << 5) | b;
}

Color colorPaletteFindDarkest(const unsigned char* palette)
{
    if (palette == nullptr) {
        return COLOR_FIRST;
    }

    int darkestColor = 0;
    int darkestValue = palette[0] + palette[1] + palette[2];

    for (int index = 1; index < COLOR_COUNT; index++) {
        int value = palette[index * COLOR_COMPONENTS_RGB] + palette[index * COLOR_COMPONENTS_RGB + 1] + palette[index * COLOR_COMPONENTS_RGB + 2];
        if (value < darkestValue) {
            darkestValue = value;
            darkestColor = index;
        }
    }

    return static_cast<Color>(darkestColor);
}

// Performs animated palette transition.
//
// 0x4C7320 fadeSystemPalette
void colorPaletteFadeBetween(unsigned char* oldPalette, unsigned char* newPalette, int steps)
{
    for (int step = 0; step < steps; step++) {
        sharedFpsLimiter.mark();

        unsigned char palette[COLOR_MAP_SIZE];

        for (int index = 0; index < COLOR_MAP_SIZE; index++) {
            palette[index] = oldPalette[index] - (oldPalette[index] - newPalette[index]) * step / steps;
        }

        if (gColorPaletteTransitionCallback != nullptr) {
            if (step % 128 == 0) {
                gColorPaletteTransitionCallback();
            }
        }

        _setSystemPalette(palette);
        renderPresent();
        sharedFpsLimiter.throttle();
    }

    sharedFpsLimiter.mark();
    _setSystemPalette(newPalette);
    renderPresent();
    sharedFpsLimiter.throttle();
}

// 0x4C73D4 colorSetFadeBkFunc
void colorPaletteSetTransitionCallback(ColorTransitionCallback* callback)
{
    gColorPaletteTransitionCallback = callback;
}

// 0x4C73E4 setSystemPalette
void _setSystemPalette(unsigned char* palette)
{
    unsigned char newPalette[COLOR_MAP_SIZE];

    for (int index = 0; index < COLOR_MAP_SIZE; index++) {
        newPalette[index] = _currentGammaTable[palette[index]];
        _systemCmap[index] = palette[index];
    }

    directDrawSetPalette(newPalette);
}

// 0x4C7420 getSystemPalette
unsigned char* _getSystemPalette()
{
    return _systemCmap;
}

// 0x4C7428 setSystemPaletteEntries
void _setSystemPaletteEntries(unsigned char* palette, int start, int end)
{
    unsigned char newPalette[COLOR_MAP_SIZE];

    int length = end - start + 1;
    for (int index = 0; index < length; index++) {
        newPalette[index * 3] = _currentGammaTable[palette[index * 3]];
        newPalette[index * 3 + 1] = _currentGammaTable[palette[index * 3 + 1]];
        newPalette[index * 3 + 2] = _currentGammaTable[palette[index * 3 + 2]];

        _systemCmap[start * 3 + index * 3] = palette[index * 3];
        _systemCmap[start * 3 + index * 3 + 1] = palette[index * 3 + 1];
        _systemCmap[start * 3 + index * 3 + 2] = palette[index * 3 + 2];
    }

    directDrawSetPaletteInRange(newPalette, start, end - start + 1);
}

// 0x4C7550 setIntensityTableColor
static void _setIntensityTableColor(Color color)
{
    int shift = 0;
    int rgb = Color2RGB(color);

    for (int index = 0; index < 128; index++) {
        int r = (rgb & 0x7C00) >> 10;
        int g = (rgb & 0x3E0) >> 5;
        int b = (rgb & 0x1F);

        int darkerR = ((r * shift) >> 16);
        int darkerG = ((g * shift) >> 16);
        int darkerB = ((b * shift) >> 16);
        int darkerColor = (darkerR << 10) | (darkerG << 5) | darkerB;
        intensityColorTable[color][index] = _colorTable[darkerColor];

        int lighterR = r + (((0x1F - r) * shift) >> 16);
        int lighterG = g + (((0x1F - g) * shift) >> 16);
        int lighterB = b + (((0x1F - b) * shift) >> 16);
        int lighterColor = (lighterR << 10) | (lighterG << 5) | lighterB;
        intensityColorTable[color][128 + index] = _colorTable[lighterColor];

        shift += 512;
    }
}

// 0x4C7658 setIntensityTables
static void _setIntensityTables()
{
    for (int index = COLOR_FIRST; index < COLOR_COUNT; index++) {
        if (_mappedColor[index] != 0) {
            _setIntensityTableColor(static_cast<Color>(index & COLOR_LAST));
        } else {
            memset(intensityColorTable[index], COLOR_FIRST, COLOR_COUNT);
        }
    }
}

// 0x4C769C setMixTableColor
static void _setMixTableColor(Color color)
{
    int colorRgb = Color2RGB(color);
    for (int otherColorIndex = COLOR_FIRST; otherColorIndex < COLOR_COUNT; otherColorIndex++) {
        Color otherColor = static_cast<Color>(otherColorIndex & COLOR_LAST);
        if (_mappedColor[color] && _mappedColor[otherColorIndex]) {
            int otherColorRgb = Color2RGB(otherColor);

            int colorR = (colorRgb & 0x7C00) >> 10;
            int colorG = (colorRgb & 0x3E0) >> 5;
            int colorB = colorRgb & 0x1F;

            int otherColorR = (otherColorRgb & 0x7C00) >> 10;
            int otherColorG = (otherColorRgb & 0x3E0) >> 5;
            int otherColorB = otherColorRgb & 0x1F;

            int addedR = colorR + otherColorR;
            int addedG = colorG + otherColorG;
            int addedB = colorB + otherColorB;

            int maxAddedChannel = addedR;
            if (addedG > maxAddedChannel) {
                maxAddedChannel = addedG;
            }
            if (addedB > maxAddedChannel) {
                maxAddedChannel = addedB;
            }

            Color additiveColor;
            if (maxAddedChannel <= 0x1F) {
                int paletteIndex = (addedR << 10) | (addedG << 5) | addedB;
                additiveColor = _colorTable[paletteIndex];
            } else {
                int overflow = maxAddedChannel - 0x1F;

                int normalizedR = addedR - overflow;
                int normalizedG = addedG - overflow;
                int normalizedB = addedB - overflow;

                if (normalizedR < 0) {
                    normalizedR = 0;
                }
                if (normalizedG < 0) {
                    normalizedG = 0;
                }
                if (normalizedB < 0) {
                    normalizedB = 0;
                }

                int saturatedPaletteIndex = (normalizedR << 10) | (normalizedG << 5) | normalizedB;
                Color saturatedColor = _colorTable[saturatedPaletteIndex];

                int intensity = (int)((((double)maxAddedChannel + (-31.0)) * 0.0078125 + 1.0) * 65536.0);
                additiveColor = _calculateColor(intensity, saturatedColor);
            }

            colorMixAddTable[color][otherColorIndex] = additiveColor;

            int multipliedR = (colorR * otherColorR) >> 5;
            int multipliedG = (colorG * otherColorG) >> 5;
            int multipliedB = (colorB * otherColorB) >> 5;

            int multiplyPaletteIndex = (multipliedR << 10) | (multipliedG << 5) | multipliedB;
            colorMixMulTable[color][otherColorIndex] = _colorTable[multiplyPaletteIndex];
        } else {
            if (_mappedColor[otherColorIndex]) {
                colorMixAddTable[color][otherColorIndex] = otherColor;
                colorMixMulTable[color][otherColorIndex] = otherColor;
            } else {
                colorMixAddTable[color][otherColorIndex] = color;
                colorMixMulTable[color][otherColorIndex] = color;
            }
        }
    }
}

// 0x4C78E4 loadColorTable
bool colorPaletteLoad(const char* path)
{
    if (gColorFileNameMangler != nullptr) {
        path = gColorFileNameMangler(path);
    }

    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        _errorStr = _aColor_cColorTa;
        return false;
    }

    for (int index = COLOR_FIRST; index < COLOR_COUNT; index++) {
        unsigned char r;
        unsigned char g;
        unsigned char b;

        // NOTE: Uninline.
        fileRead(&r, sizeof(r), 1, stream);

        // NOTE: Uninline.
        fileRead(&g, sizeof(g), 1, stream);

        // NOTE: Uninline.
        fileRead(&b, sizeof(b), 1, stream);

        if (r <= 0x3F && g <= 0x3F && b <= 0x3F) {
            _mappedColor[index] = 1;
        } else {
            r = 0;
            g = 0;
            b = 0;
            _mappedColor[index] = 0;
        }

        _cmap[index * 3] = r;
        _cmap[index * 3 + 1] = g;
        _cmap[index * 3 + 2] = b;
    }

    // NOTE: Uninline.
    fileRead(_colorTable, COLOR_PALETTE_SIZE_15BIT, 1, stream);

    unsigned int type = 0;
    // NOTE: Uninline.
    fileRead(&type, sizeof(type), 1, stream);

    // NOTE: The value is "NEWC". Original code uses cmp opcode, not stricmp,
    // or comparing characters one-by-one.
    if (type == 'NEWC') {
        // NOTE: Uninline.
        fileRead(intensityColorTable, sizeof(intensityColorTable), 1, stream);

        // NOTE: Uninline.
        fileRead(colorMixAddTable, sizeof(colorMixAddTable), 1, stream);

        // NOTE: Uninline.
        fileRead(colorMixMulTable, sizeof(colorMixMulTable), 1, stream);
    } else {
        _setIntensityTables();

        for (int index = COLOR_FIRST; index < COLOR_COUNT; index++) {
            _setMixTableColor(static_cast<Color>(index & COLOR_LAST));
        }
    }

    _rebuildColorBlendTables();

    // NOTE: Uninline.
    fileClose(stream);

    return true;
}

// 0x4C7AB4 colorError
char* _colorError()
{
    return _errorStr;
}

// 0x4C7B44 buildBlendTable
static void _buildBlendTable(Color* ptr, Color ch)
{
    int r, g, b;
    int mixedR, mixedG, mixedB;
    Color* beg;

    beg = ptr;

    r = (Color2RGB(ch) & 0x7C00) >> 10;
    g = (Color2RGB(ch) & 0x3E0) >> 5;
    b = (Color2RGB(ch) & 0x1F);

    for (int i = COLOR_FIRST; i < COLOR_COUNT; i++) {
        ptr[i] = static_cast<Color>(i & COLOR_LAST);
    }

    ptr += COLOR_COUNT;

    int b_1 = b;
    int blendWeight = 6;
    int g_1 = g;
    int r_1 = r;

    int b_2 = b_1;
    int g_2 = g_1;
    int r_2 = r_1;

    for (int j = 0; j < 7; j++) {
        for (int i = COLOR_FIRST; i < COLOR_COUNT; i++) {
            int iColorRgb = Color2RGB(static_cast<Color>(i & COLOR_LAST));
            mixedR = (iColorRgb & 0x7C00) >> 10;
            mixedG = (iColorRgb & 0x3E0) >> 5;
            mixedB = (iColorRgb & 0x1F);
            int index = 0;
            index |= (r_2 + mixedR * blendWeight) / 7 << 10;
            index |= (g_2 + mixedG * blendWeight) / 7 << 5;
            index |= (b_2 + mixedB * blendWeight) / 7;
            ptr[i] = _colorTable[index];
        }
        blendWeight--;
        ptr += COLOR_COUNT;
        r_2 += r_1;
        g_2 += g_1;
        b_2 += b_1;
    }

    int shadeStep = 0;
    for (int j = 0; j < 6; j++) {
        int shadeIntensity = shadeStep / 7 + 0xFFFF;

        for (int i = COLOR_FIRST; i < COLOR_COUNT; i++) {
            ptr[i] = _calculateColor(shadeIntensity, ch);
        }

        shadeStep += 0x10000;
        ptr += COLOR_COUNT;
    }
}

// 0x4C7D90 rebuildColorBlendTables
static void _rebuildColorBlendTables()
{
    for (int i = COLOR_FIRST; i < COLOR_COUNT; i++) {
        if (_blendTable[i]) {
            _buildBlendTable(_blendTable[i], static_cast<Color>(i & COLOR_LAST));
        }
    }
}

// 0x4C7DC0 getColorBlendTable
Color* _getColorBlendTable(Color ch)
{
    Color* ptr;

    if (_blendTable[ch] == nullptr) {
        ptr = (Color*)internal_malloc(4100);
        *(int*)ptr = 1;
        _blendTable[ch] = ptr + 4;
        _buildBlendTable(_blendTable[ch], ch);
    }

    ptr = _blendTable[ch];
    *(int*)((unsigned char*)ptr - 4) = *(int*)((unsigned char*)ptr - 4) + 1;

    return ptr;
}

// 0x4C7E20 freeColorBlendTable
void _freeColorBlendTable(Color color)
{
    Color* blendTable = _blendTable[color];
    if (blendTable != nullptr) {
        int* count = (int*)(blendTable - sizeof(int));
        *count -= 1;
        if (*count == 0) {
            internal_free(count);
            _blendTable[color] = nullptr;
        }
    }
}

// 0x4C7E6C colorGamma
void colorSetBrightness(double value)
{
    gBrightness = value;

    for (int i = 0; i < 64; i++) {
        double value = pow(i, gBrightness);
        _currentGammaTable[i] = (unsigned char)std::clamp(value, 0.0, 63.0);
    }

    _setSystemPalette(_systemCmap);
}

// 0x4C89CC initColors
bool _initColors()
{
    if (_colorsInited) {
        return true;
    }

    _colorsInited = true;

    colorSetBrightness(1.0);

    if (!colorPaletteLoad("color.pal")) {
        return false;
    }

    _setSystemPalette(_cmap);

    return true;
}

// 0x4C8A18 colorsClose
void _colorsClose()
{
    for (int index = COLOR_FIRST; index < COLOR_COUNT; index++) {
        _freeColorBlendTable(static_cast<Color>(index & COLOR_LAST));
    }
}

} // namespace fallout
