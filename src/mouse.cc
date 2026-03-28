#include "mouse.h"

#include "color.h"
#include "dinput.h"
#include "input.h"
#include "kb.h"
#include "memory.h"
#include "svga.h"
#include "touch.h"

namespace fallout {

static void mousePrepareDefaultCursor();
static void _mouse_anim();
static void _mouse_clip();

// The default mouse cursor buffer.
//
// Initially it contains color codes, which will be replaced at startup
// according to loaded palette.
//
// Available color codes:
// - 0: transparent
// - 1: white
// - 15: black
//
// 0x51E250
static unsigned char gMouseDefaultCursor[MOUSE_DEFAULT_CURSOR_SIZE] = {
    // clang-format off
    1,  1,  1,  1,  1,  1,  1, 0,
    1, 15, 15, 15, 15, 15,  1, 0,
    1, 15, 15, 15, 15,  1,  1, 0,
    1, 15, 15, 15, 15,  1,  1, 0,
    1, 15, 15, 15, 15, 15,  1, 1,
    1, 15,  1,  1, 15, 15, 15, 1,
    1,  1,  1,  1,  1, 15, 15, 1,
    0,  0,  0,  0,  1,  1,  1, 1,
    // clang-format on
};

// 0x51E290
static int _mouse_idling = 0;

// 0x51E294
static unsigned char* gMouseCursorData = nullptr;

// 0x51E298
static unsigned char* _mouse_shape = nullptr;

// 0x51E29C
static unsigned char* _mouse_fptr = nullptr;

// 0x51E2A0
static double gMouseSensitivity = 1.0;

// 0x51E2AC
static int last_buttons = 0;

// 0x6AC790
static bool gCursorIsHidden;

// 0x6AC794
static int _raw_x;

// 0x6AC798
static int gMouseCursorHeight;

// 0x6AC79C
static int _raw_y;

// 0x6AC7A0
static int _raw_buttons;

// 0x6AC7A4
static int gMouseCursorY;

// 0x6AC7A8
static int gMouseCursorX;

// 0x6AC7AC
static int _mouse_disabled;

// 0x6AC7B0
static int gMouseEvent;

// 0x6AC7B4
static unsigned int _mouse_speed;

// 0x6AC7B8
static int _mouse_curr_frame;

// 0x6AC7BC
static bool gMouseInitialized;

// 0x6AC7C0
static int gMouseCursorPitch;

// 0x6AC7C4
static int gMouseCursorWidth;

// 0x6AC7C8
static int _mouse_num_frames;

// 0x6AC7CC
static int _mouse_hoty;

// 0x6AC7D0
static int _mouse_hotx;

// 0x6AC7D4
static unsigned int _mouse_idle_start_time;

// 0x6AC7D8
WindowDrawingProc2* _mouse_blit_trans;

// 0x6AC7DC
WINDOWDRAWINGPROC _mouse_blit;

// 0x6AC7E0
static char _mouse_trans;

static int gMouseWheelX = 0;
static int gMouseWheelY = 0;

// 0x4C9F40 GNW_mouse_init_
int mouseInit()
{
    gMouseInitialized = false;
    _mouse_disabled = 0;

    gCursorIsHidden = true;

    mousePrepareDefaultCursor();

    if (mouseSetFrame(nullptr, 0, 0, 0, 0, 0, 0) == -1) {
        return -1;
    }

    if (!mouseDeviceAcquire()) {
        return -1;
    }

    gMouseInitialized = true;
    gMouseCursorX = _scr_size.right / 2;
    gMouseCursorY = _scr_size.bottom / 2;
    _raw_x = _scr_size.right / 2;
    _raw_y = _scr_size.bottom / 2;
    _mouse_idle_start_time = getTicks();

    return 0;
}

// 0x4C9FD8 GNW_mouse_exit_
void mouseFree()
{
    mouseDeviceUnacquire();

    if (gMouseCursorData != nullptr) {
        internal_free(gMouseCursorData);
        gMouseCursorData = nullptr;
    }

    if (_mouse_fptr != nullptr) {
        tickersRemove(_mouse_anim);
        _mouse_fptr = nullptr;
    }
}

// 0x4CA01C mouse_colorize_
static void mousePrepareDefaultCursor()
{
    for (int index = 0; index < 64; index++) {
        switch (gMouseDefaultCursor[index]) {
        case 0:
            gMouseDefaultCursor[index] = _colorTable[0];
            break;
        case 1:
            gMouseDefaultCursor[index] = _colorTable[8456];
            break;
        case 15:
            gMouseDefaultCursor[index] = _colorTable[32767];
            break;
        }
    }
}

// 0x4CA0AC mouse_set_shape_
int mouseSetFrame(unsigned char* frame, int width, int height, int pitch, int hotX, int hotY, char transparentColor)
{
    Rect rect;
    unsigned char* cursorFrame;
    int hotXDelta;
    int hotYDelta;

    cursorFrame = frame;

    if (frame == nullptr) {
        // NOTE: Original code looks tail recursion optimization.
        return mouseSetFrame(gMouseDefaultCursor, MOUSE_DEFAULT_CURSOR_WIDTH, MOUSE_DEFAULT_CURSOR_HEIGHT, MOUSE_DEFAULT_CURSOR_WIDTH, 1, 1, _colorTable[0]);
    }

    bool cursorWasHidden = gCursorIsHidden;
    if (!gCursorIsHidden && gMouseInitialized) {
        gCursorIsHidden = true;
        mouseGetRect(&rect);
        windowRefreshAll(&rect);
    }

    if (width != gMouseCursorWidth || height != gMouseCursorHeight) {
        unsigned char* buf = (unsigned char*)internal_malloc(width * height);
        if (buf == nullptr) {
            if (!cursorWasHidden) {
                mouseShowCursor();
            }
            return -1;
        }

        if (gMouseCursorData != nullptr) {
            internal_free(gMouseCursorData);
        }

        gMouseCursorData = buf;
    }

    gMouseCursorWidth = width;
    gMouseCursorHeight = height;
    gMouseCursorPitch = pitch;
    _mouse_shape = cursorFrame;
    _mouse_trans = transparentColor;

    if (_mouse_fptr) {
        tickersRemove(_mouse_anim);
        _mouse_fptr = nullptr;
    }

    hotXDelta = _mouse_hotx - hotX;
    _mouse_hotx = hotX;

    gMouseCursorX += hotXDelta;

    hotYDelta = _mouse_hoty - hotY;
    _mouse_hoty = hotY;

    gMouseCursorY += hotYDelta;

    _mouse_clip();

    if (!cursorWasHidden) {
        mouseShowCursor();
    }

    _raw_x = gMouseCursorX;
    _raw_y = gMouseCursorY;

    return 0;
}

// NOTE: Looks like this code is not reachable.
//
// 0x4CA2D0 mouse_anim_
static void _mouse_anim()
{
    // 0x51E2A8
    static unsigned int ticker = 0;

    if (getTicksSince(ticker) >= _mouse_speed) {
        ticker = getTicks();

        if (++_mouse_curr_frame == _mouse_num_frames) {
            _mouse_curr_frame = 0;
        }

        _mouse_shape = gMouseCursorWidth * _mouse_curr_frame * gMouseCursorHeight + _mouse_fptr;

        if (!gCursorIsHidden) {
            mouseShowCursor();
        }
    }
}

// 0x4CA34C mouse_show_
void mouseShowCursor()
{
    unsigned char* cursorData;
    int clipX;
    int clipWidth;
    int clipY;
    int clipHeight;
    int cursorDataIndex;

    cursorData = gMouseCursorData;
    if (gMouseInitialized) {
        if (!_mouse_blit_trans || !gCursorIsHidden) {
            _win_get_mouse_buf(gMouseCursorData);
            cursorData = gMouseCursorData;
            cursorDataIndex = 0;

            for (int y = 0; y < gMouseCursorHeight; y++) {
                for (int x = 0; x < gMouseCursorWidth; x++) {
                    unsigned char pixel = _mouse_shape[y * gMouseCursorPitch + x];
                    if (pixel != _mouse_trans) {
                        cursorData[cursorDataIndex] = pixel;
                    }
                    cursorDataIndex++;
                }
            }
        }

        if (gMouseCursorX >= _scr_size.left) {
            if (gMouseCursorWidth + gMouseCursorX - 1 <= _scr_size.right) {
                clipWidth = gMouseCursorWidth;
                clipX = 0;
            } else {
                clipX = 0;
                clipWidth = _scr_size.right - gMouseCursorX + 1;
            }
        } else {
            clipX = _scr_size.left - gMouseCursorX;
            clipWidth = gMouseCursorWidth - (_scr_size.left - gMouseCursorX);
        }

        if (gMouseCursorY >= _scr_size.top) {
            if (gMouseCursorHeight + gMouseCursorY - 1 <= _scr_size.bottom) {
                clipY = 0;
                clipHeight = gMouseCursorHeight;
            } else {
                clipY = 0;
                clipHeight = _scr_size.bottom - gMouseCursorY + 1;
            }
        } else {
            clipY = _scr_size.top - gMouseCursorY;
            clipHeight = gMouseCursorHeight - (_scr_size.top - gMouseCursorY);
        }

        gMouseCursorData = cursorData;
        if (_mouse_blit_trans && gCursorIsHidden) {
            _mouse_blit_trans(_mouse_shape, gMouseCursorPitch, gMouseCursorHeight, clipX, clipY, clipWidth, clipHeight, clipX + gMouseCursorX, clipY + gMouseCursorY, _mouse_trans);
        } else {
            _mouse_blit(gMouseCursorData, gMouseCursorWidth, gMouseCursorHeight, clipX, clipY, clipWidth, clipHeight, clipX + gMouseCursorX, clipY + gMouseCursorY);
        }

        cursorData = gMouseCursorData;
        gCursorIsHidden = false;
    }
    gMouseCursorData = cursorData;
}

// 0x4CA534 mouse_hide_
void mouseHideCursor()
{
    Rect rect;

    if (gMouseInitialized) {
        if (!gCursorIsHidden) {
            rect.left = gMouseCursorX;
            rect.top = gMouseCursorY;
            rect.right = gMouseCursorX + gMouseCursorWidth - 1;
            rect.bottom = gMouseCursorY + gMouseCursorHeight - 1;

            gCursorIsHidden = true;
            windowRefreshAll(&rect);
        }
    }
}

// 0x4CA59C mouse_info_
void _mouse_info()
{
    if (!gMouseInitialized) {
        return;
    }

    if (gCursorIsHidden) {
        return;
    }

    if (_mouse_disabled) {
        return;
    }

    Gesture gesture;
    if (touch_get_gesture(&gesture)) {
        static int prevx;
        static int prevy;

        switch (gesture.type) {
        case kTap:
            if (screenIsFullscreen()) {
                if (gesture.numberOfTouches == 1) {
                    _mouse_simulate_input(0, 0, MOUSE_STATE_LEFT_BUTTON_DOWN);
                } else if (gesture.numberOfTouches == 2) {
                    _mouse_simulate_input(0, 0, MOUSE_STATE_RIGHT_BUTTON_DOWN);
                }
            } else {
                if (gesture.numberOfTouches == 1) {
                    _mouse_simulate_input(gesture.x, gesture.y, MOUSE_STATE_LEFT_BUTTON_DOWN);
                } else if (gesture.numberOfTouches == 2) {
                    _mouse_simulate_input(gesture.x, gesture.y, MOUSE_STATE_RIGHT_BUTTON_DOWN);
                }
            }
            break;
        case kLongPress:
        case kPan:
            if (gesture.state == kBegan) {
                prevx = gesture.x;
                prevy = gesture.y;
            }
            if (!screenIsFullscreen()) {
                prevx = 0;
                prevy = 0;
            }

            if (gesture.type == kLongPress) {
                if (gesture.numberOfTouches == 1) {
                    _mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MOUSE_STATE_LEFT_BUTTON_DOWN);
                } else if (gesture.numberOfTouches == 2) {
                    _mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MOUSE_STATE_RIGHT_BUTTON_DOWN);
                }
            } else if (gesture.type == kPan) {
                if (!touch_get_pan_mode() && gesture.numberOfTouches == 1) {
                    _mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, 0);
                } else if (touch_get_pan_mode() || gesture.numberOfTouches == 2) {
                    int coefficient = touch_get_pan_mode() ? 8 : 2;
                    gMouseWheelX = (prevx - gesture.x) / coefficient;
                    gMouseWheelY = (gesture.y - prevy) / coefficient;

                    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
                        gMouseEvent |= MOUSE_EVENT_WHEEL;
                        _raw_buttons |= MOUSE_EVENT_WHEEL;
                    }
                }
            }

            prevx = gesture.x;
            prevy = gesture.y;
            break;
        case kUnrecognized:
            break;
        }

        return;
    }

    int x;
    int y;
    int buttons = 0;

    MouseData mouseData;
    if (mouseDeviceGetData(&mouseData)) {
        x = mouseData.x;
        y = mouseData.y;

        if (mouseData.buttons[0] == 1) {
            buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
        }

        if (mouseData.buttons[1] == 1) {
            buttons |= MOUSE_STATE_RIGHT_BUTTON_DOWN;
        }
    } else {
        x = 0;
        y = 0;
    }

    // Adjust for mouse senstivity.
    x = (int)(x * gMouseSensitivity);
    y = (int)(y * gMouseSensitivity);

    _mouse_simulate_input(x, y, buttons);

    // TODO: Move to `_mouse_simulate_input`.
    gMouseWheelX = mouseData.wheelX;
    gMouseWheelY = mouseData.wheelY;

    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
        gMouseEvent |= MOUSE_EVENT_WHEEL;
        _raw_buttons |= MOUSE_EVENT_WHEEL;
    }
}

// 0x4CA698 mouse_simulate_input_
void _mouse_simulate_input(int delta_x, int delta_y, int buttons)
{
    // 0x6AC7E4
    static unsigned int previousRightButtonTimestamp;

    // 0x6AC7E8
    static unsigned int previousLeftButtonTimestamp;

    // 0x6AC7EC
    static int previousEvent;

    if (!gMouseInitialized || gCursorIsHidden) {
        return;
    }

    if (delta_x == 0 && delta_y == 0 && buttons == last_buttons) {
        if (last_buttons == 0) {
            if (!_mouse_idling) {
                _mouse_idle_start_time = getTicks();
                _mouse_idling = 1;
            }

            last_buttons = 0;
            _raw_buttons = 0;
            gMouseEvent = 0;

            return;
        }
    }

    _mouse_idling = 0;
    last_buttons = buttons;
    previousEvent = gMouseEvent;
    gMouseEvent = 0;

    if ((previousEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x01) != 0) {
            gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_REPEAT;

            if (getTicksSince(previousLeftButtonTimestamp) > BUTTON_REPEAT_TIME) {
                gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
                previousLeftButtonTimestamp = getTicks();
            }
        } else {
            gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_UP;
        }
    } else {
        if ((buttons & 0x01) != 0) {
            gMouseEvent |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
            previousLeftButtonTimestamp = getTicks();
        }
    }

    if ((previousEvent & MOUSE_EVENT_RIGHT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x02) != 0) {
            gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_REPEAT;
            if (getTicksSince(previousRightButtonTimestamp) > BUTTON_REPEAT_TIME) {
                gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
                previousRightButtonTimestamp = getTicks();
            }
        } else {
            gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_UP;
        }
    } else {
        if (buttons & 0x02) {
            gMouseEvent |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
            previousRightButtonTimestamp = getTicks();
        }
    }

    _raw_buttons = gMouseEvent;

    if (delta_x != 0 || delta_y != 0) {
        Rect mouseRect;
        mouseRect.left = gMouseCursorX;
        mouseRect.top = gMouseCursorY;
        mouseRect.right = gMouseCursorWidth + gMouseCursorX - 1;
        mouseRect.bottom = gMouseCursorHeight + gMouseCursorY - 1;
        if (screenIsFullscreen()) {
            gMouseCursorX += delta_x;
            gMouseCursorY += delta_y;
        } else {
            gMouseCursorX = delta_x;
            gMouseCursorY = delta_y;
        }
        _mouse_clip();

        windowRefreshAll(&mouseRect);

        mouseShowCursor();

        _raw_x = gMouseCursorX;
        _raw_y = gMouseCursorY;
    }
}

// 0x4CA8C8 mouse_in_
bool _mouse_in(int left, int top, int right, int bottom)
{
    if (!gMouseInitialized) {
        return false;
    }

    return gMouseCursorHeight + gMouseCursorY > top
        && right >= gMouseCursorX
        && gMouseCursorWidth + gMouseCursorX > left
        && bottom >= gMouseCursorY;
}

// 0x4CA934 mouse_click_in_
bool _mouse_click_in(int left, int top, int right, int bottom)
{
    if (!gMouseInitialized) {
        return false;
    }

    return _mouse_hoty + gMouseCursorY >= top
        && _mouse_hotx + gMouseCursorX <= right
        && _mouse_hotx + gMouseCursorX >= left
        && _mouse_hoty + gMouseCursorY <= bottom;
}

// 0x4CA9A0 mouse_get_rect_
void mouseGetRect(Rect* rect)
{
    rect->left = gMouseCursorX;
    rect->top = gMouseCursorY;
    rect->right = gMouseCursorWidth + gMouseCursorX - 1;
    rect->bottom = gMouseCursorHeight + gMouseCursorY - 1;
}

// 0x4CA9DC mouse_get_position_
void mouseGetPosition(int* xPtr, int* yPtr)
{
    *xPtr = _mouse_hotx + gMouseCursorX;
    *yPtr = _mouse_hoty + gMouseCursorY;
}

// 0x4CAA04 mouse_set_position_
void _mouse_set_position(int x, int y)
{
    gMouseCursorX = x - _mouse_hotx;
    gMouseCursorY = y - _mouse_hoty;
    _raw_y = y - _mouse_hoty;
    _raw_x = x - _mouse_hotx;
    _mouse_clip();
}

// 0x4CAA38 mouse_clip_
static void _mouse_clip()
{
    if (_mouse_hotx + gMouseCursorX < _scr_size.left) {
        gMouseCursorX = _scr_size.left - _mouse_hotx;
    } else if (_mouse_hotx + gMouseCursorX > _scr_size.right) {
        gMouseCursorX = _scr_size.right - _mouse_hotx;
    }

    if (_mouse_hoty + gMouseCursorY < _scr_size.top) {
        gMouseCursorY = _scr_size.top - _mouse_hoty;
    } else if (_mouse_hoty + gMouseCursorY > _scr_size.bottom) {
        gMouseCursorY = _scr_size.bottom - _mouse_hoty;
    }
}

// 0x4CAAA0 mouse_get_buttons_
int mouseGetEvent()
{
    return gMouseEvent;
}

// 0x4CAAA8 mouse_hidden_
bool cursorIsHidden()
{
    return gCursorIsHidden;
}

// 0x4CAB5C mouse_get_raw_state_
void _mouse_get_raw_state(int* out_x, int* out_y, int* out_buttons)
{
    MouseData mouseData;
    if (!mouseDeviceGetData(&mouseData)) {
        mouseData.x = 0;
        mouseData.y = 0;
        mouseData.buttons[0] = (gMouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0;
        mouseData.buttons[1] = (gMouseEvent & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0;
    }

    _raw_buttons = 0;
    if (screenIsFullscreen()) {
        _raw_x += mouseData.x;
        _raw_y += mouseData.y;
    } else {
        _raw_x = mouseData.x;
        _raw_y = mouseData.y;
    }

    if (mouseData.buttons[0] != 0) {
        _raw_buttons |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
    }

    if (mouseData.buttons[1] != 0) {
        _raw_buttons |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
    }

    *out_x = _raw_x;
    *out_y = _raw_y;
    *out_buttons = _raw_buttons;
}

// 0x4CAC3C mouse_set_sensitivity_
void mouseSetSensitivity(double value)
{
    if (value >= MOUSE_SENSITIVITY_MIN && value <= MOUSE_SENSITIVITY_MAX) {
        gMouseSensitivity = value;
    }
}

void mouseGetPositionInWindow(int win, int* x, int* y)
{
    mouseGetPosition(x, y);

    Window* window = windowGetWindow(win);
    if (window != nullptr) {
        *x -= window->rect.left;
        *y -= window->rect.top;
    }
}

bool mouseHitTestInWindow(int win, int left, int top, int right, int bottom)
{
    Window* window = windowGetWindow(win);
    if (window != nullptr) {
        left += window->rect.left;
        top += window->rect.top;
        right += window->rect.left;
        bottom += window->rect.top;
    }

    return _mouse_click_in(left, top, right, bottom);
}

void mouseGetWheel(int* x, int* y)
{
    *x = gMouseWheelX;
    *y = gMouseWheelY;
}

void convertMouseWheelToArrowKey(int* keyCodePtr)
{
    if (*keyCodePtr == -1) {
        if ((mouseGetEvent() & MOUSE_EVENT_WHEEL) != 0) {
            int wheelX;
            int wheelY;
            mouseGetWheel(&wheelX, &wheelY);

            if (wheelY > 0) {
                *keyCodePtr = KEY_ARROW_UP;
            } else if (wheelY < 0) {
                *keyCodePtr = KEY_ARROW_DOWN;
            }
        }
    }
}

int mouse_get_last_buttons()
{
    return last_buttons;
}

} // namespace fallout
