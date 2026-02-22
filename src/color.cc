#include "color.h"

#include <math.h>
#include <string.h>

#include <algorithm>

#include "db.h"
#include "memory.h"
#include "svga.h"

namespace fallout {

static void _setIntensityTableColor(int color);
static void _setIntensityTables();
static void _setMixTableColor(int color);
static void _buildBlendTable(unsigned char* ptr, unsigned char ch);
static void _rebuildColorBlendTables();

// 0x50F930
static char _aColor_cNoError[] = "color.c: No errors\n";

// 0x50F95C
static char _aColor_cColorTa[] = "color.c: color table not found\n";

// 0x50F984
static char _aColor_cColorpa[] = "color.c: colorpalettestack overflow";

// 0x50F9AC
static char aColor_cColor_0[] = "color.c: colorpalettestack underflow";

// 0x51DF10
static char* _errorStr = _aColor_cNoError;

// 0x51DF14
static bool _colorsInited = false;

// 0x51DF18
static double gBrightness = 1.0;

// 0x51DF20
static ColorTransitionCallback* gColorPaletteTransitionCallback = nullptr;

// 0x51DF30
static ColorFileNameManger* gColorFileNameMangler = nullptr;

// 0x51DF34
unsigned char _cmap[768] = {
    0x3F, 0x3F, 0x3F
};

// 0x673090
unsigned char _systemCmap[256 * 3];

// 0x673390
unsigned char _currentGammaTable[64];

// 0x6733D0
unsigned char* _blendTable[256];

// 0x6737D0
unsigned char _mappedColor[256];

// 0x6738D0
Color colorMixAddTable[256][256];

// 0x6838D0
Color intensityColorTable[256][256];

// 0x6938D0
Color colorMixMulTable[256][256];

// 0x6A38D0
unsigned char _colorTable[32768];

// 0x4C72B4
int _calculateColor(int intensity, Color color)
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

// Performs animated palette transition.
//
// 0x4C7320
void colorPaletteFadeBetween(unsigned char* oldPalette, unsigned char* newPalette, int steps)
{
    for (int step = 0; step < steps; step++) {
        sharedFpsLimiter.mark();

        unsigned char palette[768];

        for (int index = 0; index < 768; index++) {
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

// 0x4C73D4
void colorPaletteSetTransitionCallback(ColorTransitionCallback* callback)
{
    gColorPaletteTransitionCallback = callback;
}

// 0x4C73E4
void _setSystemPalette(unsigned char* palette)
{
    unsigned char newPalette[768];

    for (int index = 0; index < 768; index++) {
        newPalette[index] = _currentGammaTable[palette[index]];
        _systemCmap[index] = palette[index];
    }

    directDrawSetPalette(newPalette);
}

// 0x4C7420
unsigned char* _getSystemPalette()
{
    return _systemCmap;
}

// 0x4C7428
void _setSystemPaletteEntries(unsigned char* palette, int start, int end)
{
    unsigned char newPalette[768];

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

// 0x4C7550
static void _setIntensityTableColor(int cc)
{
    int shift = 0;

    for (int index = 0; index < 128; index++) {
        int r = (Color2RGB(cc) & 0x7C00) >> 10;
        int g = (Color2RGB(cc) & 0x3E0) >> 5;
        int b = (Color2RGB(cc) & 0x1F);

        int darkerR = ((r * shift) >> 16);
        int darkerG = ((g * shift) >> 16);
        int darkerB = ((b * shift) >> 16);
        int darkerColor = (darkerR << 10) | (darkerG << 5) | darkerB;
        intensityColorTable[cc][index] = _colorTable[darkerColor];

        int lighterR = r + (((0x1F - r) * shift) >> 16);
        int lighterG = g + (((0x1F - g) * shift) >> 16);
        int lighterB = b + (((0x1F - b) * shift) >> 16);
        int lighterColor = (lighterR << 10) | (lighterG << 5) | lighterB;
        intensityColorTable[cc][128 + index] = _colorTable[lighterColor];

        shift += 512;
    }
}

// 0x4C7658
static void _setIntensityTables()
{
    for (int index = 0; index < 256; index++) {
        if (_mappedColor[index] != 0) {
            _setIntensityTableColor(index);
        } else {
            memset(intensityColorTable[index], 0, 256);
        }
    }
}

// 0x4C769C
static void _setMixTableColor(int color)
{
    for (int otherColor = 0; otherColor < 256; otherColor++) {
        if (_mappedColor[color] && _mappedColor[otherColor]) {
            int colorRgb = Color2RGB(color);
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

            int additiveColor;
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
                int saturatedColor = _colorTable[saturatedPaletteIndex];

                int intensity = (int)((((double)maxAddedChannel + (-31.0)) * 0.0078125 + 1.0) * 65536.0);
                additiveColor = _calculateColor(intensity, saturatedColor);
            }

            colorMixAddTable[color][otherColor] = additiveColor;

            int multipliedR = (colorR * otherColorR) >> 5;
            int multipliedG = (colorG * otherColorG) >> 5;
            int multipliedB = (colorB * otherColorB) >> 5;

            int multiplyPaletteIndex = (multipliedR << 10) | (multipliedG << 5) | multipliedB;
            colorMixMulTable[color][otherColor] = _colorTable[multiplyPaletteIndex];
        } else {
            if (_mappedColor[otherColor]) {
                colorMixAddTable[color][otherColor] = otherColor;
                colorMixMulTable[color][otherColor] = otherColor;
            } else {
                colorMixAddTable[color][otherColor] = color;
                colorMixMulTable[color][otherColor] = color;
            }
        }
    }
}

// 0x4C78E4
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

    for (int index = 0; index < 256; index++) {
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
    fileRead(_colorTable, 0x8000, 1, stream);

    unsigned int type;
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

        for (int index = 0; index < 256; index++) {
            _setMixTableColor(index);
        }
    }

    _rebuildColorBlendTables();

    // NOTE: Uninline.
    fileClose(stream);

    return true;
}

// 0x4C7AB4
char* _colorError()
{
    return _errorStr;
}

// 0x4C7B44
static void _buildBlendTable(unsigned char* ptr, unsigned char ch)
{
    int r, g, b;
    int i, j;
    int mixedR, mixedG, mixedB;
    unsigned char* beg;

    beg = ptr;

    r = (Color2RGB(ch) & 0x7C00) >> 10;
    g = (Color2RGB(ch) & 0x3E0) >> 5;
    b = (Color2RGB(ch) & 0x1F);

    for (i = 0; i < 256; i++) {
        ptr[i] = i;
    }

    ptr += 256;

    int b_1 = b;
    int blendWeight = 6;
    int g_1 = g;
    int r_1 = r;

    int b_2 = b_1;
    int g_2 = g_1;
    int r_2 = r_1;

    for (j = 0; j < 7; j++) {
        for (i = 0; i < 256; i++) {
            mixedR = (Color2RGB(i) & 0x7C00) >> 10;
            mixedG = (Color2RGB(i) & 0x3E0) >> 5;
            mixedB = (Color2RGB(i) & 0x1F);
            int index = 0;
            index |= (r_2 + mixedR * blendWeight) / 7 << 10;
            index |= (g_2 + mixedG * blendWeight) / 7 << 5;
            index |= (b_2 + mixedB * blendWeight) / 7;
            ptr[i] = _colorTable[index];
        }
        blendWeight--;
        ptr += 256;
        r_2 += r_1;
        g_2 += g_1;
        b_2 += b_1;
    }

    int shadeStep = 0;
    for (j = 0; j < 6; j++) {
        int shadeIntensity = shadeStep / 7 + 0xFFFF;

        for (i = 0; i < 256; i++) {
            ptr[i] = _calculateColor(shadeIntensity, ch);
        }

        shadeStep += 0x10000;
        ptr += 256;
    }
}

// 0x4C7D90
static void _rebuildColorBlendTables()
{
    int i;

    for (i = 0; i < 256; i++) {
        if (_blendTable[i]) {
            _buildBlendTable(_blendTable[i], i);
        }
    }
}

// 0x4C7DC0
unsigned char* _getColorBlendTable(int ch)
{
    unsigned char* ptr;

    if (_blendTable[ch] == nullptr) {
        ptr = (unsigned char*)internal_malloc(4100);
        *(int*)ptr = 1;
        _blendTable[ch] = ptr + 4;
        _buildBlendTable(_blendTable[ch], ch);
    }

    ptr = _blendTable[ch];
    *(int*)((unsigned char*)ptr - 4) = *(int*)((unsigned char*)ptr - 4) + 1;

    return ptr;
}

// 0x4C7E20
void _freeColorBlendTable(int color)
{
    unsigned char* blendTable = _blendTable[color];
    if (blendTable != nullptr) {
        int* count = (int*)(blendTable - sizeof(int));
        *count -= 1;
        if (*count == 0) {
            internal_free(count);
            _blendTable[color] = nullptr;
        }
    }
}

// 0x4C7E6C
void colorSetBrightness(double value)
{
    gBrightness = value;

    for (int i = 0; i < 64; i++) {
        double value = pow(i, gBrightness);
        _currentGammaTable[i] = (unsigned char)std::clamp(value, 0.0, 63.0);
    }

    _setSystemPalette(_systemCmap);
}

// 0x4C89CC
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

// 0x4C8A18
void _colorsClose()
{
    for (int index = 0; index < 256; index++) {
        _freeColorBlendTable(index);
    }
}

} // namespace fallout
