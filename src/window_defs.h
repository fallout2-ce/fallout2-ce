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

} // namespace fallout

#endif /* WINDOW_DEFS */
