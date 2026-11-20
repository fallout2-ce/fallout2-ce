#ifndef WINDOW_DEFS
#define WINDOW_DEFS

namespace fallout {

enum TextAlignment : int {
    TEXT_ALIGNMENT_LEFT,
    TEXT_ALIGNMENT_RIGHT,
    TEXT_ALIGNMENT_CENTER,
};

constexpr inline bool textAlignmentIsValid(int textAlignment)
{
    return textAlignment >= TEXT_ALIGNMENT_LEFT && textAlignment <= TEXT_ALIGNMENT_CENTER;
}

typedef void (*WINDOWDRAWINGPROC)(unsigned char* src, int src_pitch, int _, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y);
typedef void WindowDrawingProc2(unsigned char* buf, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, unsigned char a10);

} // namespace fallout

#endif /* WINDOW_DEFS */
