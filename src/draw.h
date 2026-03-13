#ifndef DRAW_H
#define DRAW_H

namespace fallout {

void bufferDrawLine(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void bufferDrawRect(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void bufferDrawRectShadowed(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int ltColor, int rbColor);
void blitBufferToBufferStretch(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void blitBufferToBufferStretchTrans(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void blitBufferToBuffer(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void blitBufferToBufferTrans(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
// [value] is an 8-bit fill byte; in most callers this is a palette color index.
void bufferFill(unsigned char* buf, int width, int height, int pitch, int value);
void _buf_texture(unsigned char* buf, int width, int height, int pitch, void* texture, int xOffset, int yOffset);
void _lighten_buf(unsigned char* buf, int width, int height, int pitch);
void _swap_color_buf(unsigned char* buf, int width, int height, int pitch, int color1, int color2);
void bufferOutline(unsigned char* buf, int width, int height, int pitch, int color);
void srcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);
void transSrcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);

} // namespace fallout

#endif /* DRAW_H */
