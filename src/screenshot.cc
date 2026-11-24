#include "screenshot.h"

#include <lodepng.h>

#include <vector>

#include "color.h"
#include "draw.h"
#include "platform_compat.h"
#include "memory.h"
#include "mouse.h"
#include "svga.h"
#include "window_manager.h"

namespace fallout {

static void screenshotBlitter(unsigned char* src, int src_pitch, int src_bpp, int x, int y, int width, int height, int dest_x, int dest_y);

// 0x6AC760 screendump_key
static int gScreenshotKeyCode;

// 0x6AC76C screendump_func
static ScreenshotHandler* gScreenshotHandler;

// 0x6AC774 screendump_buf
static unsigned char* gScreenshotBuffer;

bool handleScreenshotShortcut(int code)
{
    if (code != gScreenshotKeyCode) {
        return false;
    }
    takeScreenshot();
    return true;
}

// 0x4C8F4C
void takeScreenshot()
{
    int width = _scr_size.right - _scr_size.left + 1;
    int height = _scr_size.bottom - _scr_size.top + 1;
    gScreenshotBuffer = (unsigned char*)internal_malloc(static_cast<size_t>(width) * height);
    if (gScreenshotBuffer == nullptr) {
        return;
    }

    const auto old_src = _scr_blit;
    _scr_blit = screenshotBlitter;

    windowRefreshAll(&_scr_size);
    _scr_blit = old_src;
    mouseDrawScreenshotCursor(gScreenshotBuffer, width, height);

    // Save screenshots with the palette currently presented on screen so the
    // exported colors match brightness-adjusted rendering.
    unsigned char* palette = gSdlSurface != nullptr ? directDrawGetPalette() : _getSystemPalette();
    gScreenshotHandler(width, height, gScreenshotBuffer, palette);
    internal_free(gScreenshotBuffer);
}

// 0x4C8FF0
static void screenshotBlitter(unsigned char* src, int srcPitch, int _, int srcX, int srcY, int width, int height, int destX, int destY)
{
    int destWidth = _scr_size.right - _scr_size.left + 1;
    blitBufferToBuffer(src + srcPitch * srcY + srcX, width, height, srcPitch, gScreenshotBuffer + destWidth * destY + destX, destWidth);
}

// 0x4C9048
int screenshotHandlerDefaultImpl(int width, int height, unsigned char* data, unsigned char* palette)
{
    char fileName[16];
    FILE* stream;
    int index;
    unsigned int intValue;
    unsigned short shortValue;

    for (index = 0; index < 100000; index++) {
        snprintf(fileName, sizeof(fileName), "scr%.5d.bmp", index);

        stream = compat_fopen(fileName, "rb");
        if (stream == nullptr) {
            break;
        }

        fclose(stream);
    }

    if (index == 100000) {
        return -1;
    }

    stream = compat_fopen(fileName, "wb");
    if (stream == nullptr) {
        return -1;
    }

    // bfType
    shortValue = 0x4D42;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // bfSize
    // 14 - sizeof(BITMAPFILEHEADER)
    // 40 - sizeof(BITMAPINFOHEADER)
    // 1024 - sizeof(RGBQUAD) * 256
    intValue = width * height + 14 + 40 + 1024;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // bfReserved1
    shortValue = 0;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // bfReserved2
    shortValue = 0;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // bfOffBits
    intValue = 14 + 40 + 1024;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biSize
    intValue = 40;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biWidth
    intValue = width;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biHeight
    intValue = height;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biPlanes
    shortValue = 1;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // biBitCount
    shortValue = 8;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // biCompression
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biSizeImage
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biXPelsPerMeter
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biYPelsPerMeter
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biClrUsed
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biClrImportant
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    for (int index = 0; index < 256; index++) {
        unsigned char rgbReserved = 0;
        unsigned char rgbRed = palette[index * 3] << 2;
        unsigned char rgbGreen = palette[index * 3 + 1] << 2;
        unsigned char rgbBlue = palette[index * 3 + 2] << 2;

        fwrite(&rgbBlue, sizeof(rgbBlue), 1, stream);
        fwrite(&rgbGreen, sizeof(rgbGreen), 1, stream);
        fwrite(&rgbRed, sizeof(rgbRed), 1, stream);
        fwrite(&rgbReserved, sizeof(rgbReserved), 1, stream);
    }

    for (int y = height - 1; y >= 0; y--) {
        unsigned char* dataPtr = data + y * width;
        fwrite(dataPtr, 1, width, stream);
    }

    fflush(stream);
    fclose(stream);

    return 0;
}

int screenshotHandlerPngImpl(int width, int height, unsigned char* data, unsigned char* palette)
{
    char fileName[16];
    FILE* stream;
    int fileIndex;

    for (fileIndex = 0; fileIndex < 100000; fileIndex++) {
        snprintf(fileName, sizeof(fileName), "scr%.5d.png", fileIndex);

        stream = compat_fopen(fileName, "rb");
        if (stream == nullptr) {
            break;
        }

        fclose(stream);
    }

    if (fileIndex == 100000) {
        return -1;
    }

    stream = compat_fopen(fileName, "wb");
    if (stream == nullptr) {
        return -1;
    }
    fclose(stream);

    int imgLen = width * height;
    std::vector<unsigned char> image(imgLen * 3);

    for (size_t i = 0; i < imgLen; i++) {
        image[i * 3] = palette[data[i] * 3] << 2;
        image[i * 3 + 1] = palette[data[i] * 3 + 1] << 2;
        image[i * 3 + 2] = palette[data[i] * 3 + 2] << 2;
    }

    unsigned error = lodepng::encode(fileName, image, width, height, LCT_RGB, 8);
    if (error) {
        return -1;
    }

    return 0;
}

// 0x4C9358
void screenshotHandlerConfigure(int keyCode, ScreenshotHandler* handler)
{
    gScreenshotKeyCode = keyCode;

    if (handler == nullptr) {
        handler = screenshotHandlerDefaultImpl;
    }

    gScreenshotHandler = handler;
}

} // namespace fallout
