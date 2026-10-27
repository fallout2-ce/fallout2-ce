#ifndef FALLOUT_MOUSE_H_
#define FALLOUT_MOUSE_H_

#include "geometry.h"
#include "window_defs.h"

namespace fallout {

#define MOUSE_DEFAULT_CURSOR_WIDTH 8
#define MOUSE_DEFAULT_CURSOR_HEIGHT 8
#define MOUSE_DEFAULT_CURSOR_SIZE (MOUSE_DEFAULT_CURSOR_WIDTH * MOUSE_DEFAULT_CURSOR_HEIGHT)

enum class MouseButtonEvent : unsigned int {
    None = 0x00,
    LeftButtonDown = 0x01,
    RightButtonDown = 0x02,
    LeftButtonRepeat = 0x04,
    RightButtonRepeat = 0x08,
    LeftButtonUp = 0x10,
    RightButtonUp = 0x20,
    Wheel = 0x40,
    AnyButtonDown = LeftButtonDown | RightButtonDown,
    AnyButtonRepeat = LeftButtonRepeat | RightButtonRepeat,
    AnyButtonUp = LeftButtonUp | RightButtonUp,
    LeftButtonDownRepeat = LeftButtonDown | LeftButtonRepeat,
    RightButtonDownRepeat = RightButtonDown | RightButtonRepeat
};

constexpr inline MouseButtonEvent operator&(MouseButtonEvent lhs, MouseButtonEvent rhs)
{
    return static_cast<MouseButtonEvent>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline MouseButtonEvent operator|(MouseButtonEvent lhs, MouseButtonEvent rhs)
{
    return static_cast<MouseButtonEvent>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

inline MouseButtonEvent& operator&=(MouseButtonEvent& lhs, MouseButtonEvent rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline MouseButtonEvent& operator|=(MouseButtonEvent& lhs, MouseButtonEvent rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

#define BUTTON_REPEAT_TIME 250

#define MOUSE_SENSITIVITY_MIN 0.25
#define MOUSE_SENSITIVITY_MAX 2.5

extern WindowDrawingProc2* _mouse_blit_trans;
extern WINDOWDRAWINGPROC _mouse_blit;

int mouseInit();
void mouseFree();
int mouseSetFrame(unsigned char* frame, int width, int height, int pitch, int hotX, int hotY, char transparentColor);
void mouseShowCursor();
void mouseHideCursor();
void _mouse_info();
void _mouse_simulate_input(int delta_x, int delta_y, MouseButtonEvent buttons);
bool _mouse_in(int left, int top, int right, int bottom);
bool _mouse_click_in(int left, int top, int right, int bottom);
void mouseGetRect(Rect* rect);
void mouseGetPosition(int* out_x, int* out_y);
void _mouse_set_position(int x, int y);
MouseButtonEvent mouseGetEvent();
bool cursorIsHidden();
void _mouse_get_raw_state(int* out_x, int* out_y, MouseButtonEvent* out_buttons);
void mouseSetSensitivity(double value);

void mouseGetPositionInWindow(int win, int* x, int* y);
bool mouseHitTestInWindow(int win, int left, int top, int right, int bottom);
void mouseGetWheel(int* x, int* y);
void convertMouseWheelToArrowKey(int* keyCodePtr);
MouseButtonEvent mouse_get_last_buttons();

} // namespace fallout

#endif /* FALLOUT_MOUSE_H_ */
