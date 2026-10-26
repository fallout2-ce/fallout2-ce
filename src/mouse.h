#ifndef FALLOUT_MOUSE_H_
#define FALLOUT_MOUSE_H_

#include "geometry.h"
#include "window.h"

namespace fallout {

#define MOUSE_DEFAULT_CURSOR_WIDTH 8
#define MOUSE_DEFAULT_CURSOR_HEIGHT 8
#define MOUSE_DEFAULT_CURSOR_SIZE (MOUSE_DEFAULT_CURSOR_WIDTH * MOUSE_DEFAULT_CURSOR_HEIGHT)

enum MouseEvent : unsigned int {
    MOUSE_EVENT_NONE = 0x00,
    MOUSE_EVENT_LEFT_BUTTON_DOWN = 0x01,
    MOUSE_EVENT_RIGHT_BUTTON_DOWN = 0x02,
    MOUSE_EVENT_LEFT_BUTTON_REPEAT = 0x04,
    MOUSE_EVENT_RIGHT_BUTTON_REPEAT = 0x08,
    MOUSE_EVENT_LEFT_BUTTON_UP = 0x10,
    MOUSE_EVENT_RIGHT_BUTTON_UP = 0x20,
    MOUSE_EVENT_ANY_BUTTON_DOWN = (MOUSE_EVENT_LEFT_BUTTON_DOWN | MOUSE_EVENT_RIGHT_BUTTON_DOWN),
    MOUSE_EVENT_ANY_BUTTON_REPEAT = (MOUSE_EVENT_LEFT_BUTTON_REPEAT | MOUSE_EVENT_RIGHT_BUTTON_REPEAT),
    MOUSE_EVENT_ANY_BUTTON_UP = (MOUSE_EVENT_LEFT_BUTTON_UP | MOUSE_EVENT_RIGHT_BUTTON_UP),
    MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT = (MOUSE_EVENT_LEFT_BUTTON_DOWN | MOUSE_EVENT_LEFT_BUTTON_REPEAT),
    MOUSE_EVENT_RIGHT_BUTTON_DOWN_REPEAT = (MOUSE_EVENT_RIGHT_BUTTON_DOWN | MOUSE_EVENT_RIGHT_BUTTON_REPEAT),
    MOUSE_EVENT_WHEEL = 0x40    
};

constexpr inline MouseEvent operator&(MouseEvent lhs, MouseEvent rhs)
{
    return static_cast<MouseEvent>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline MouseEvent operator|(MouseEvent lhs, MouseEvent rhs)
{
    return static_cast<MouseEvent>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

inline MouseEvent& operator&=(MouseEvent& lhs, MouseEvent rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline MouseEvent& operator|=(MouseEvent& lhs, MouseEvent rhs)
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
void _mouse_simulate_input(int delta_x, int delta_y, MouseEvent buttons);
bool _mouse_in(int left, int top, int right, int bottom);
bool _mouse_click_in(int left, int top, int right, int bottom);
void mouseGetRect(Rect* rect);
void mouseGetPosition(int* out_x, int* out_y);
void _mouse_set_position(int x, int y);
MouseEvent mouseGetEvent();
bool cursorIsHidden();
void _mouse_get_raw_state(int* out_x, int* out_y, MouseEvent* out_buttons);
void mouseSetSensitivity(double value);

void mouseGetPositionInWindow(int win, int* x, int* y);
bool mouseHitTestInWindow(int win, int left, int top, int right, int bottom);
void mouseGetWheel(int* x, int* y);
void convertMouseWheelToArrowKey(int* keyCodePtr);
int mouse_get_last_buttons();

} // namespace fallout

#endif /* FALLOUT_MOUSE_H_ */
