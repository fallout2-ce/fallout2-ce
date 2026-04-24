#ifndef DRAW_H
#define DRAW_H

#include <type_traits>

namespace fallout {

template <typename T>
struct Buffer2DBase {
    T* data = nullptr;
    int width = 0;
    int height = 0;

    Buffer2DBase() = default;
    Buffer2DBase(T* data, int width, int height)
        : data(data)
        , width(width)
        , height(height)
    {
    }

    // Implicit conversion Buffer2D -> ConstBuffer2D.
    template <typename U, typename = std::enable_if_t<std::is_same_v<const U, T>>>
    Buffer2DBase(const Buffer2DBase<U>& other)
        : data(other.data)
        , width(other.width)
        , height(other.height)
    {
    }

    explicit operator bool() const { return data != nullptr; }
};

using Buffer2D = Buffer2DBase<unsigned char>;
using ConstBuffer2D = Buffer2DBase<const unsigned char>;

// Copies src into dst at (dstX, dstY). Clips to dst bounds; no-op if fully outside.
void blitBuffer2D(ConstBuffer2D src, Buffer2D dst, int dstX = 0, int dstY = 0);

// Copies a region of src (starting at srcX, srcY with given width/height) into
// dst at (dstX, dstY). Clips to both src and dst bounds.
void blitBuffer2D(ConstBuffer2D src, int srcX, int srcY, int width, int height,
    Buffer2D dst, int dstX = 0, int dstY = 0);

void bufferDrawLine(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void bufferDrawRect(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void bufferDrawRectShadowed(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int ltColor, int rbColor);
void blitBufferToBufferStretch(const unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void blitBufferToBufferStretchTrans(const unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void blitBufferToBuffer(const unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void blitBufferToBufferTrans(const unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
// [value] is an 8-bit fill byte; in most callers this is a palette color index.
void bufferFill(unsigned char* buf, int width, int height, int pitch, int value);
void _buf_texture(unsigned char* buf, int width, int height, int pitch, void* texture, int xOffset, int yOffset);
void _lighten_buf(unsigned char* buf, int width, int height, int pitch);
void _swap_color_buf(unsigned char* buf, int width, int height, int pitch, int color1, int color2);
void bufferOutline(unsigned char* buf, int width, int height, int pitch, int color);
void srcCopy(unsigned char* dest, int destPitch, const unsigned char* src, int srcPitch, int width, int height);
void transSrcCopy(unsigned char* dest, int destPitch, const unsigned char* src, int srcPitch, int width, int height);

} // namespace fallout

#endif /* DRAW_H */
