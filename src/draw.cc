#include "draw.h"

#include <algorithm>
#include <string.h>

#include "color.h"
#include "svga.h"

namespace fallout {

static void blitBuffer2DScaledImpl(ConstBuffer2D src, int srcX, int srcY, int srcWidth, int srcHeight, Buffer2D dst, int dstX, int dstY, int dstWidth, int dstHeight, bool transparent);
static int scaledCeilDiv(int value, int numerator, int denominator);

// 0x4D2FC0
void bufferDrawLine(unsigned char* buf, int pitch, int x1, int y1, int x2, int y2, int color)
{
    int temp;
    int dx;
    int dy;
    unsigned char* p1;
    unsigned char* p2;
    unsigned char* p3;
    unsigned char* p4;

    if (x1 == x2) {
        if (y1 > y2) {
            temp = y1;
            y1 = y2;
            y2 = temp;
        }

        p1 = buf + pitch * y1 + x1;
        p2 = buf + pitch * y2 + x2;
        while (p1 <= p2) {
            *p1 = color;
            *p2 = color;
            p1 += pitch;
            p2 -= pitch;
        }
    } else {
        if (x1 > x2) {
            temp = x1;
            x1 = x2;
            x2 = temp;

            temp = y1;
            y1 = y2;
            y2 = temp;
        }

        p1 = buf + pitch * y1 + x1;
        p2 = buf + pitch * y2 + x2;
        if (y1 == y2) {
            memset(p1, color, p2 - p1);
        } else {
            dx = x2 - x1;

            int rowStep;
            int middlePointOffset;
            int midX = x1 + (x2 - x1) / 2;
            if (y1 <= y2) {
                dy = y2 - y1;
                rowStep = pitch;
                middlePointOffset = midX + ((y2 - y1) / 2 + y1) * pitch;
            } else {
                dy = y1 - y2;
                rowStep = -pitch;
                middlePointOffset = midX + (y1 - (y1 - y2) / 2) * pitch;
            }

            p3 = buf + middlePointOffset;
            p4 = p3;

            if (dx <= dy) {
                int midpointError = dx - (dy / 2);
                int remainingSteps = dy / 4;
                while (true) {
                    *p1 = color;
                    *p2 = color;
                    *p3 = color;
                    *p4 = color;

                    if (remainingSteps == 0) {
                        break;
                    }

                    if (midpointError >= 0) {
                        p3++;
                        p2--;
                        p4--;
                        p1++;
                        midpointError -= dy;
                    }

                    p3 += rowStep;
                    p2 -= rowStep;
                    p4 -= rowStep;
                    p1 += rowStep;
                    midpointError += dx;

                    remainingSteps--;
                }
            } else {
                int midpointError = dy - (dx / 2);
                int remainingSteps = dx / 4;
                while (true) {
                    *p1 = color;
                    *p2 = color;
                    *p3 = color;
                    *p4 = color;

                    if (remainingSteps == 0) {
                        break;
                    }

                    if (midpointError >= 0) {
                        p3 += rowStep;
                        p2 -= rowStep;
                        p4 -= rowStep;
                        p1 += rowStep;
                        midpointError -= dx;
                    }

                    p3++;
                    p2--;
                    p4--;
                    p1++;
                    midpointError += dy;

                    remainingSteps--;
                }
            }
        }
    }
}

// 0x4D31A4
void bufferDrawRect(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color)
{
    bufferDrawLine(buf, pitch, left, top, right, top, color);
    bufferDrawLine(buf, pitch, left, bottom, right, bottom, color);
    bufferDrawLine(buf, pitch, left, top, left, bottom, color);
    bufferDrawLine(buf, pitch, right, top, right, bottom, color);
}

// 0x4D322C
void bufferDrawRectShadowed(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int ltColor, int rbColor)
{
    bufferDrawLine(buf, pitch, left, top, right, top, ltColor);
    bufferDrawLine(buf, pitch, left, bottom, right, bottom, rbColor);
    bufferDrawLine(buf, pitch, left, top, left, bottom, ltColor);
    bufferDrawLine(buf, pitch, right, top, right, bottom, rbColor);
}

// 0x4D33F0
void blitBufferToBufferStretch(const unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch)
{
    int stepX = (destWidth << 16) / srcWidth;
    int stepY = (destHeight << 16) / srcHeight;

    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int startDestY = (srcY * stepY) >> 16;
        int endDestY = ((srcY + 1) * stepY) >> 16;

        const unsigned char* currSrc = src + srcPitch * srcY;
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int startDestX = (srcX * stepX) >> 16;
            int endDestX = ((srcX + 1) * stepX) >> 16;

            for (int destY = startDestY; destY < endDestY; destY += 1) {
                unsigned char* currDest = dest + destPitch * destY + startDestX;
                for (int destX = startDestX; destX < endDestX; destX += 1) {
                    *currDest++ = *currSrc;
                }
            }

            currSrc++;
        }
    }
}

// 0x4D3560
void blitBufferToBufferStretchTrans(const unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch)
{
    int stepX = (destWidth << 16) / srcWidth;
    int stepY = (destHeight << 16) / srcHeight;

    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int startDestY = (srcY * stepY) >> 16;
        int endDestY = ((srcY + 1) * stepY) >> 16;

        const unsigned char* currSrc = src + srcPitch * srcY;
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int startDestX = (srcX * stepX) >> 16;
            int endDestX = ((srcX + 1) * stepX) >> 16;

            if (*currSrc != 0) {
                for (int destY = startDestY; destY < endDestY; destY += 1) {
                    unsigned char* currDest = dest + destPitch * destY + startDestX;
                    for (int destX = startDestX; destX < endDestX; destX += 1) {
                        *currDest++ = *currSrc;
                    }
                }
            }

            currSrc++;
        }
    }
}

// 0x4D36D4
void blitBufferToBuffer(const unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch)
{
    srcCopy(dest, destPitch, src, srcPitch, width, height);
}

// 0x4D3704
void blitBufferToBufferTrans(const unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch)
{
    transSrcCopy(dest, destPitch, src, srcPitch, width, height);
}

void blitBuffer2D(ConstBuffer2D src, Buffer2D dst, int dstX, int dstY)
{
    blitBuffer2D(src, 0, 0, src.width, src.height, dst, dstX, dstY);
}

void blitBuffer2D(ConstBuffer2D src, int srcX, int srcY, int width, int height,
    Buffer2D dst, int dstX, int dstY)
{
    // Clip source region to src bounds.
    if (srcX < 0) {
        width += srcX;
        dstX -= srcX;
        srcX = 0;
    }
    if (srcY < 0) {
        height += srcY;
        dstY -= srcY;
        srcY = 0;
    }
    if (srcX + width > src.width) {
        width = src.width - srcX;
    }
    if (srcY + height > src.height) {
        height = src.height - srcY;
    }

    // Clip destination region to dst bounds.
    if (dstX < 0) {
        srcX -= dstX;
        width += dstX;
        dstX = 0;
    }
    if (dstY < 0) {
        srcY -= dstY;
        height += dstY;
        dstY = 0;
    }
    if (dstX + width > dst.width) {
        width = dst.width - dstX;
    }
    if (dstY + height > dst.height) {
        height = dst.height - dstY;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    srcCopy(
        dst.data + dstY * dst.width + dstX, dst.width,
        src.data + srcY * src.width + srcX, src.width,
        width, height);
}

void blitBuffer2DScaled(ConstBuffer2D src, Buffer2D dst, int dstX, int dstY, int dstWidth, int dstHeight)
{
    blitBuffer2DScaled(src, 0, 0, src.width, src.height, dst, dstX, dstY, dstWidth, dstHeight);
}

void blitBuffer2DScaled(ConstBuffer2D src, int srcX, int srcY, int srcWidth, int srcHeight,
    Buffer2D dst, int dstX, int dstY, int dstWidth, int dstHeight)
{
    blitBuffer2DScaledImpl(src, srcX, srcY, srcWidth, srcHeight, dst, dstX, dstY, dstWidth, dstHeight, false);
}

void blitBuffer2DScaledTrans(ConstBuffer2D src, Buffer2D dst, int dstX, int dstY, int dstWidth, int dstHeight)
{
    blitBuffer2DScaledTrans(src, 0, 0, src.width, src.height, dst, dstX, dstY, dstWidth, dstHeight);
}

void blitBuffer2DScaledTrans(ConstBuffer2D src, int srcX, int srcY, int srcWidth, int srcHeight,
    Buffer2D dst, int dstX, int dstY, int dstWidth, int dstHeight)
{
    blitBuffer2DScaledImpl(src, srcX, srcY, srcWidth, srcHeight, dst, dstX, dstY, dstWidth, dstHeight, true);
}

void bufferFill2D(Buffer2D dst, int value)
{
    if (!dst || dst.width <= 0 || dst.height <= 0) {
        return;
    }

    bufferFill(dst.data, dst.width, dst.height, dst.width, value);
}

static void blitBuffer2DScaledImpl(ConstBuffer2D src, int srcX, int srcY, int srcWidth, int srcHeight, Buffer2D dst, int dstX, int dstY, int dstWidth, int dstHeight, bool transparent)
{
    if (!src || !dst || src.width <= 0 || src.height <= 0 || srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0) {
        return;
    }

    int clippedSrcLeft = std::max(srcX, 0);
    int clippedSrcTop = std::max(srcY, 0);
    int clippedSrcRight = std::min(srcX + srcWidth, src.width);
    int clippedSrcBottom = std::min(srcY + srcHeight, src.height);
    if (clippedSrcLeft >= clippedSrcRight || clippedSrcTop >= clippedSrcBottom) {
        return;
    }

    int left = dstX + scaledCeilDiv(clippedSrcLeft - srcX, dstWidth, srcWidth);
    int top = dstY + scaledCeilDiv(clippedSrcTop - srcY, dstHeight, srcHeight);
    int right = dstX + scaledCeilDiv(clippedSrcRight - srcX, dstWidth, srcWidth);
    int bottom = dstY + scaledCeilDiv(clippedSrcBottom - srcY, dstHeight, srcHeight);

    left = std::max(left, 0);
    top = std::max(top, 0);
    right = std::min(right, dst.width);
    bottom = std::min(bottom, dst.height);
    if (left >= right || top >= bottom) {
        return;
    }

    for (int destY = top; destY < bottom; destY++) {
        int sourceY = srcY + ((destY - dstY) * srcHeight) / dstHeight;
        sourceY = std::clamp(sourceY, srcY, srcY + srcHeight - 1);

        unsigned char* destRow = dst.data + destY * dst.width;
        const unsigned char* srcRow = src.data + sourceY * src.width;
        for (int destX = left; destX < right; destX++) {
            int sourceX = srcX + ((destX - dstX) * srcWidth) / dstWidth;
            sourceX = std::clamp(sourceX, srcX, srcX + srcWidth - 1);

            unsigned char value = srcRow[sourceX];
            if (!transparent || value != 0) {
                destRow[destX] = value;
            }
        }
    }
}

static int scaledCeilDiv(int value, int numerator, int denominator)
{
    return static_cast<int>((static_cast<int64_t>(value) * numerator + denominator - 1) / denominator);
}

// 0x4D387C
void bufferFill(unsigned char* buf, int width, int height, int pitch, int value)
{
    int y;

    for (y = 0; y < height; y++) {
        memset(buf, value, width);
        buf += pitch;
    }
}

// 0x4D38E0
void _buf_texture(unsigned char* buf, int width, int height, int pitch, void* texture, int xOffset, int yOffset)
{
    // Intended to tile GNW window texture data when window color is 256.
    // In current CE code paths this is effectively dormant because
    // `_GNW_texture` is never populated and callers fall back to flat fills.
    (void)buf;
    (void)width;
    (void)height;
    (void)pitch;
    (void)texture;
    (void)xOffset;
    (void)yOffset;
    // TODO: Incomplete.
}

// 0x4D3A48
void _lighten_buf(unsigned char* buf, int width, int height, int pitch)
{
    int skip = pitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char color = *buf;
            *buf++ = intensityColorTable[color][147];
        }
        buf += skip;
    }
}

// Swaps two colors in the buffer.
//
// 0x4D3A8C
void _swap_color_buf(unsigned char* buf, int width, int height, int pitch, int color1, int color2)
{
    int step = pitch - width;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int currentColor = *buf & 0xFF;
            if (currentColor == color1) {
                *buf = color2 & 0xFF;
            } else if (currentColor == color2) {
                *buf = color1 & 0xFF;
            }
            buf++;
        }
        buf += step;
    }
}

// 0x4D3AE0
void bufferOutline(unsigned char* buf, int width, int height, int pitch, int color)
{
    unsigned char* ptr = buf + pitch;

    bool cycle;
    for (int y = 0; y < height - 2; y++) {
        cycle = true;

        for (int x = 0; x < width; x++) {
            if (*ptr != 0 && cycle) {
                *(ptr - 1) = color & 0xFF;
                cycle = false;
            } else if (*ptr == 0 && !cycle) {
                *ptr = color & 0xFF;
                cycle = true;
            }

            ptr++;
        }

        ptr += pitch - width;
    }

    for (int x = 0; x < width; x++) {
        ptr = buf + x;
        cycle = true;

        for (int y = 0; y < height; y++) {
            if (*ptr != 0 && cycle) {
                // TODO: Check in debugger, might be a bug.
                *(ptr - pitch) = color & 0xFF;
                cycle = false;
            } else if (*ptr == 0 && !cycle) {
                *ptr = color & 0xFF;
                cycle = true;
            }

            ptr += pitch;
        }
    }
}

// 0x4E0DB0
void srcCopy(unsigned char* dest, int destPitch, const unsigned char* src, int srcPitch, int width, int height)
{
    for (int y = 0; y < height; y++) {
        memcpy(dest, src, width);
        dest += destPitch;
        src += srcPitch;
    }
}

// 0x4E0ED5
void transSrcCopy(unsigned char* dest, int destPitch, const unsigned char* src, int srcPitch, int width, int height)
{
    int destSkip = destPitch - width;
    int srcSkip = srcPitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char c = *src++;
            if (c != 0) {
                *dest = c;
            }
            dest++;
        }
        src += srcSkip;
        dest += destSkip;
    }
}

} // namespace fallout
