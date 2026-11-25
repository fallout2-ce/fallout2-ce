#include "mouse.h"

#include <algorithm>

#if __APPLE__
#include <TargetConditionals.h>
#endif

#include "color.h"
#include "dinput.h"
#include "input.h"
#include "interface.h"
#include "kb.h"
#include "platform/ios/quick_toolbar.h"
#include "svga.h"
#include "touch.h"
#include "win32.h"
#include "window_manager.h"

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
// 0x51E250 or_mask
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

// 0x51E290 mouse_idling
static int _mouse_idling = 0;

// 0x51E298 mouse_shape
static unsigned char* _mouse_shape = nullptr;

// 0x51E29C mouse_fptr
static unsigned char* _mouse_fptr = nullptr;

// 0x51E2A0 mouse_sensitivity
static double gMouseSensitivity = 1.0;

// 0x51E2AC last_buttons
static MouseButtonEvent last_buttons = MouseButtonEvent::None;

// 0x6AC790 mouse_is_hidden
static bool gCursorIsHidden;

// 0x6AC794 raw_x
static int _raw_x;

// 0x6AC798 mouse_length
static int gMouseCursorHeight;

// 0x6AC79C raw_y
static int _raw_y;

// 0x6AC7A0 raw_buttons
static MouseButtonEvent _raw_buttons;

// 0x6AC7A4 mouse_y
static int gMouseCursorY;

// 0x6AC7A8 mouse_x
static int gMouseCursorX;

// 0x6AC7AC mouse_disabled
static int _mouse_disabled;

// 0x6AC7B0 mouse_buttons
static MouseButtonEvent gMouseEvent;

// 0x6AC7B4 mouse_speed
static unsigned int _mouse_speed;

// 0x6AC7B8 mouse_curr_frame
static int _mouse_curr_frame;

// 0x6AC7BC have_mouse
static bool gMouseInitialized;

// 0x6AC7C0 mouse_pitch
static int gMouseCursorPitch;

// 0x6AC7C4 mouse_width
static int gMouseCursorWidth;

// 0x6AC7C8 mouse_num_frames
static int _mouse_num_frames;

// 0x6AC7CC mouse_hoty
static int _mouse_hoty;

// 0x6AC7D0 mouse_hotx
static int _mouse_hotx;

// 0x6AC7D4 mouse_idle_start_time
static unsigned int _mouse_idle_start_time;

// 0x6AC7E0 mouse_trans
static char _mouse_trans;

static int gMouseWheelX = 0;
static int gMouseWheelY = 0;
static bool pointerUsesTouch = false;

namespace {

    struct RelativeMotion {
        double x;
        double y;

        SDL_Point pixels() const
        {
            return { static_cast<int>(x), static_cast<int>(y) };
        }
    };

    RelativeMotion relativeRemainder {};
    unsigned int relativeGeneration = 0;

    RelativeMotion scaleRelativeMotion(SDL_Point delta, unsigned int generation)
    {
        // The device discards pending deltas when changing mouse mode. Any
        // fractional movement left over from the old mode expires with them.
        const auto remainder = generation == relativeGeneration ? relativeRemainder : RelativeMotion {};
        return { delta.x * gMouseSensitivity + remainder.x, delta.y * gMouseSensitivity + remainder.y };
    }

    SDL_Point clipCursorPosition(SDL_Point position)
    {
        return {
            std::clamp(position.x, _scr_size.left - _mouse_hotx, _scr_size.right - _mouse_hotx),
            std::clamp(position.y, _scr_size.top - _mouse_hoty, _scr_size.bottom - _mouse_hoty),
        };
    }

} // namespace

// 0x4C9F40
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

// 0x4C9FD8
void mouseFree()
{
    mouseDeviceUnacquire();

    gMouseInitialized = false;

    if (_mouse_fptr != nullptr) {
        tickersRemove(_mouse_anim);
        _mouse_fptr = nullptr;
    }
}

// 0x4CA01C
static void mousePrepareDefaultCursor()
{
    for (int index = 0; index < 64; index++) {
        switch (gMouseDefaultCursor[index]) {
        case 0:
            gMouseDefaultCursor[index] = COLOR_BLACK;
            break;
        case 1:
            gMouseDefaultCursor[index] = COLOR_DARK_GREY;
            break;
        case 15:
            gMouseDefaultCursor[index] = COLOR_WHITE;
            break;
        }
    }
}

// 0x4CA0AC
int mouseSetFrame(unsigned char* frame, int width, int height, int pitch, int hotX, int hotY, char transparentColor)
{
    if (frame == nullptr) {
        return mouseSetFrame(gMouseDefaultCursor, MOUSE_DEFAULT_CURSOR_WIDTH, MOUSE_DEFAULT_CURSOR_HEIGHT, MOUSE_DEFAULT_CURSOR_WIDTH, 1, 1, COLOR_BLACK);
    }

    gMouseCursorWidth = width;
    gMouseCursorHeight = height;
    gMouseCursorPitch = pitch;
    _mouse_shape = frame;
    _mouse_trans = transparentColor;

    if (_mouse_fptr) {
        tickersRemove(_mouse_anim);
        _mouse_fptr = nullptr;
    }

    gMouseCursorX += _mouse_hotx - hotX;
    gMouseCursorY += _mouse_hoty - hotY;
    _mouse_hotx = hotX;
    _mouse_hoty = hotY;
    _mouse_clip();

    _raw_x = gMouseCursorX;
    _raw_y = gMouseCursorY;

    return 0;
}

// NOTE: Looks like this code is not reachable.
//
// 0x4CA2D0
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
    }
}

// 0x4CA34C
void mouseShowCursor()
{
    if (gMouseInitialized) {
        gCursorIsHidden = false;
    }
}

// 0x4CA534
void mouseHideCursor()
{
    gCursorIsHidden = true;
}

SDL_Point mouseGetCursorDrawPosition()
{
    return { gMouseCursorX, gMouseCursorY };
}

SDL_Point mouseSampleCursorDrawPosition()
{
    // Refresh SDL's device state. The input update will drain the event queue.
    SDL_PumpEvents();

    if (!gMouseInitialized || gCursorIsHidden || _mouse_disabled || pointerUsesTouch || !gProgramIsActive) {
        return mouseGetCursorDrawPosition();
    }

    auto point = mouseDeviceSamplePosition();
    if (mouseDeviceUsesRelativeMode()) {
        const auto delta = scaleRelativeMotion(point, mouseDeviceMotionGeneration()).pixels();
        point.x = gMouseCursorX + delta.x;
        point.y = gMouseCursorY + delta.y;
    } else {
        point.x -= _mouse_hotx;
        point.y -= _mouse_hoty;
    }
    return clipCursorPosition(point);
}

SDL_Rect mouseGetCursorDrawBounds(SDL_Point position)
{
    if (!gMouseInitialized || gCursorIsHidden || _mouse_shape == nullptr) {
        return {};
    }

    return { position.x, position.y, gMouseCursorWidth, gMouseCursorHeight };
}

namespace {

    struct CursorPalette {
        const SDL_PixelFormat* format;
        const SDL_Palette& palette;

        Uint32 operator()(unsigned char index) const
        {
            const auto& color = palette.colors[index];
            return SDL_MapRGB(format, color.r, color.g, color.b);
        }
    };

    unsigned char cursorPaletteIndex(unsigned char index)
    {
        return index;
    }

    template <typename Pixel, typename MapColor>
    void drawCursor(Pixel* pixels, int width, int height, int stride, SDL_Point position, MapColor mapColor)
    {
        const auto bounds = mouseGetCursorDrawBounds(position);
        if (SDL_RectEmpty(&bounds)) {
            return;
        }

        const int left = std::max(0, position.x);
        const int top = std::max(0, position.y);
        const int right = std::min(width, bounds.x + bounds.w);
        const int bottom = std::min(height, bounds.y + bounds.h);
        const auto transparent = static_cast<unsigned char>(_mouse_trans);

        for (int y = top; y < bottom; ++y) {
            auto* row = pixels + y * stride;
            const auto* source = _mouse_shape + (y - position.y) * gMouseCursorPitch;
            for (int x = left; x < right; ++x) {
                const auto color = source[x - position.x];
                if (color != transparent) {
                    row[x] = mapColor(color);
                }
            }
        }
    }

} // namespace

void mouseDrawCursor(SDL_Surface& target, const SDL_Palette& palette, SDL_Point position)
{
    drawCursor(static_cast<Uint32*>(target.pixels), target.w, target.h, target.pitch / sizeof(Uint32), position,
        CursorPalette { target.format, palette });
}

void mouseDrawScreenshotCursor(unsigned char* pixels, int width, int height)
{
    drawCursor(pixels, width, height, width, mouseGetCursorDrawPosition(), cursorPaletteIndex);
}

#if __APPLE__ && TARGET_OS_IOS
// Checks whether a tap lands on an interface-bar button and, if so,
// injects the corresponding keyCode so the cursor never moves.
// Returns true if the tap was consumed (button injected or bare chrome hit).
static bool handleHudTapThrough(const Gesture& gesture)
{
    if (!mouseDeviceUsesRelativeMode() || touch_get_touchscreen_mode() || gInterfaceBarWindow == -1) {
        return false;
    }

    Window* hudWindow = windowGetWindow(gInterfaceBarWindow);
    if (hudWindow == nullptr || (hudWindow->flags & WINDOW_HIDDEN) != 0) {
        return false;
    }

    Rect hudRect;
    if (windowGetRect(gInterfaceBarWindow, &hudRect) != 0
        || gesture.x < hudRect.left || gesture.x > hudRect.right
        || gesture.y < hudRect.top || gesture.y > hudRect.bottom) {
        return false;
    }

    if (gesture.numberOfTouches == 1 || gesture.numberOfTouches == 2) {
        for (Button* button = hudWindow->buttonListHead; button != nullptr; button = button->next) {
            if ((button->flags & BUTTON_FLAG_DISABLED) != 0) {
                continue;
            }
            int left = hudWindow->rect.left + button->rect.left;
            int top = hudWindow->rect.top + button->rect.top;
            int right = hudWindow->rect.left + button->rect.right;
            int bottom = hudWindow->rect.top + button->rect.bottom;
            if (gesture.x < left || gesture.x > right || gesture.y < top || gesture.y > bottom) {
                continue;
            }
            int keyCode = gesture.numberOfTouches == 1
                ? button->leftMouseUpEventCode
                : button->rightMouseUpEventCode;
            if (keyCode == -1) {
                break;
            }
            enqueueInputEvent(keyCode);
            return true;
        }
    }

    // Tap landed on belt chrome (no button under it). Consume silently
    // rather than teleporting the cursor to an inert region.
    return true;
}
#endif

// 0x4CA59C
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
    pointerUsesTouch = touch_get_gesture(&gesture);
    if (pointerUsesTouch) {
        static int prevx;
        static int prevy;

        // Multi-finger gestures for keyboard-less touch play:
        //   3-finger swipe down → ESC (options menu)
        //   3-finger long press → hold Left Shift (highlights interactables)
        //   4-finger long press → F6  (quicksave)
        if (gesture.type == kPan && gesture.numberOfTouches == 3) {
            static int swipeStartY;
            if (gesture.state == kBegan) {
                swipeStartY = gesture.y;
            } else if (gesture.state == kEnded) {
                int dy = gesture.y - swipeStartY;
                if (dy > screenGetHeight() / 4) {
                    enqueueInputEvent(KEY_ESCAPE);
                }
            }
            return;
        }

        // Four-finger long press → F6 (quicksave). Long-press is more
        // reliable than a tap since all 4 fingers rarely land and lift
        // within the 75ms tap window; and more reliable than a swipe
        // since iPadOS intercepts multi-finger vertical swipes.
        if (gesture.type == kLongPress && gesture.numberOfTouches == 4) {
            if (gesture.state == kBegan) {
                enqueueInputEvent(KEY_F6);
            }
            return;
        }

        // FO2tweaks' highlighting uses sfall's key_pressed(), which reads
        // SDL's own SDL_GetKeyboardState. Engine-internal _kb_simulate_key
        // bypasses that, so push real SDL_KEYDOWN/UP events instead.
        if (gesture.type == kLongPress && gesture.numberOfTouches == 3) {
            static bool shiftHeld = false;
            SDL_Event ev;
            SDL_zero(ev);
            ev.key.keysym.scancode = SDL_SCANCODE_LSHIFT;
            ev.key.keysym.sym = SDLK_LSHIFT;
            if (gesture.state == kBegan && !shiftHeld) {
                ev.type = SDL_KEYDOWN;
                ev.key.state = SDL_PRESSED;
                SDL_PushEvent(&ev);
                shiftHeld = true;
            } else if (gesture.state == kEnded && shiftHeld) {
                ev.type = SDL_KEYUP;
                ev.key.state = SDL_RELEASED;
                SDL_PushEvent(&ev);
                shiftHeld = false;
            }
            return;
        }

        switch (gesture.type) {
        case kTap: {
            // Toolbar taps bypass the mouse pipeline entirely: the handler
            // invokes the action in place, so the cursor never moves.
            // Skip when touchscreen mode is active (dialog, inventory, etc.)
            // so toolbar doesn't intercept taps meant for overlapping UI.
            if (!touch_get_touchscreen_mode()
                && gesture.numberOfTouches == 1
                && quickToolbarContainsPoint(gesture.x, gesture.y)) {
                if (quickToolbarHandleTap(gesture.x, gesture.y)) {
                    break;
                }
            }

#if __APPLE__ && TARGET_OS_IOS
            if (handleHudTapThrough(gesture)) {
                goto tap_done;
            }
#endif

            if (mouseDeviceUsesRelativeMode()) {
                if (gesture.numberOfTouches == 1) {
                    _mouse_simulate_input(0, 0, MouseButtonEvent::LeftButtonDown);
                } else if (gesture.numberOfTouches == 2) {
                    _mouse_simulate_input(0, 0, MouseButtonEvent::RightButtonDown);
                }
            } else {
                _mouse_set_position(gesture.x, gesture.y);
                if (gesture.numberOfTouches == 1) {
                    _mouse_simulate_input(gesture.x, gesture.y, MouseButtonEvent::LeftButtonDown);
                } else if (gesture.numberOfTouches == 2) {
                    _mouse_simulate_input(gesture.x, gesture.y, MouseButtonEvent::RightButtonDown);
                }
            }
        tap_done:
            break;
        }
        case kLongPress:
        case kPan:
            if (gesture.state == kBegan) {
                prevx = gesture.x;
                prevy = gesture.y;
            }
            if (!mouseDeviceUsesRelativeMode()) {
                prevx = 0;
                prevy = 0;
            }

            if (gesture.type == kLongPress) {
                if (gesture.numberOfTouches == 1) {
                    _mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MouseButtonEvent::LeftButtonDown);
                } else if (gesture.numberOfTouches == 2) {
                    _mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MouseButtonEvent::RightButtonDown);
                }
            } else if (gesture.type == kPan) {
                if (!touch_get_pan_mode() && gesture.numberOfTouches == 1) {
                    _mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MouseButtonEvent::None);
                } else if (touch_get_pan_mode() || gesture.numberOfTouches == 2) {
                    int coefficient = touch_get_pan_mode() ? 8 : 2;
                    gMouseWheelX = (prevx - gesture.x) / coefficient;
                    gMouseWheelY = (gesture.y - prevy) / coefficient;

                    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
                        gMouseEvent |= MouseButtonEvent::Wheel;
                        _raw_buttons |= MouseButtonEvent::Wheel;
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
    MouseButtonEvent buttons = MouseButtonEvent::None;

    MouseData mouseData {};
    if (mouseDeviceGetData(&mouseData)) {
        x = mouseData.x;
        y = mouseData.y;

        if (mouseData.buttons[0] == 1) {
            buttons |= MouseButtonEvent::LeftButtonDown;
        }

        if (mouseData.buttons[1] == 1) {
            buttons |= MouseButtonEvent::RightButtonDown;
        }
    } else {
        x = 0;
        y = 0;
    }

    // Mouse sensitivity only applies to relative movement. In windowed mode
    // SDL provides absolute coordinates that should not be scaled.
    if (mouseDeviceUsesRelativeMode()) {
        const auto generation = mouseDeviceMotionGeneration();
        const auto motion = scaleRelativeMotion({ x, y }, generation);
        const auto delta = motion.pixels();
        relativeRemainder = { motion.x - delta.x, motion.y - delta.y };
        relativeGeneration = generation;
        x = delta.x;
        y = delta.y;
    }

    _mouse_simulate_input(x, y, buttons);

    // TODO: Move to `_mouse_simulate_input`.
    gMouseWheelX = mouseData.wheelX;
    gMouseWheelY = mouseData.wheelY;

    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
        gMouseEvent |= MouseButtonEvent::Wheel;
        _raw_buttons |= MouseButtonEvent::Wheel;
    }
}

// 0x4CA698
void _mouse_simulate_input(int delta_x, int delta_y, MouseButtonEvent buttons)
{
    // 0x6AC7E4
    static unsigned int previousRightButtonTimestamp;

    // 0x6AC7E8
    static unsigned int previousLeftButtonTimestamp;

    // 0x6AC7EC
    static MouseButtonEvent previousEvent;

    if (!gMouseInitialized || gCursorIsHidden) {
        return;
    }

    const bool relative = mouseDeviceUsesRelativeMode();
    // Absolute coordinates include the screen origin and may be unchanged
    // even when nonzero. Touch gestures can also supply relative movement.
    const bool moved = relative || pointerUsesTouch
        ? (delta_x != 0 || delta_y != 0)
        : (delta_x != gMouseCursorX + _mouse_hotx || delta_y != gMouseCursorY + _mouse_hoty);
    if (!moved && buttons == last_buttons) {
        if (last_buttons == MouseButtonEvent::None) {
            if (!_mouse_idling) {
                _mouse_idle_start_time = getTicks();
                _mouse_idling = 1;
            }

            last_buttons = MouseButtonEvent::None;
            _raw_buttons = MouseButtonEvent::None;
            gMouseEvent = MouseButtonEvent::None;

            return;
        }
    }

    _mouse_idling = 0;
    last_buttons = buttons;
    previousEvent = gMouseEvent;
    gMouseEvent = MouseButtonEvent::None;

    if ((previousEvent & MouseButtonEvent::LeftButtonDownRepeat) != MouseButtonEvent::None) {
        if ((buttons & MouseButtonEvent::LeftButtonDown) != MouseButtonEvent::None) {
            gMouseEvent |= MouseButtonEvent::LeftButtonRepeat;

            if (getTicksSince(previousLeftButtonTimestamp) > BUTTON_REPEAT_TIME) {
                gMouseEvent |= MouseButtonEvent::LeftButtonDown;
                previousLeftButtonTimestamp = getTicks();
            }
        } else {
            gMouseEvent |= MouseButtonEvent::LeftButtonUp;
        }
    } else {
        if ((buttons & MouseButtonEvent::LeftButtonDown) != MouseButtonEvent::None) {
            gMouseEvent |= MouseButtonEvent::LeftButtonDown;
            previousLeftButtonTimestamp = getTicks();
        }
    }

    if ((previousEvent & MouseButtonEvent::RightButtonDownRepeat) != MouseButtonEvent::None) {
        if ((buttons & MouseButtonEvent::RightButtonDown) != MouseButtonEvent::None) {
            gMouseEvent |= MouseButtonEvent::RightButtonRepeat;
            if (getTicksSince(previousRightButtonTimestamp) > BUTTON_REPEAT_TIME) {
                gMouseEvent |= MouseButtonEvent::RightButtonDown;
                previousRightButtonTimestamp = getTicks();
            }
        } else {
            gMouseEvent |= MouseButtonEvent::RightButtonUp;
        }
    } else {
        if ((buttons & MouseButtonEvent::RightButtonDown) != MouseButtonEvent::None) {
            gMouseEvent |= MouseButtonEvent::RightButtonDown;
            previousRightButtonTimestamp = getTicks();
        }
    }

    _raw_buttons = gMouseEvent;

    if (moved) {
        if (relative) {
            gMouseCursorX += delta_x;
            gMouseCursorY += delta_y;
        } else {
            _mouse_set_position(delta_x, delta_y);
        }
        _mouse_clip();

        if (relative) {
            _raw_x = gMouseCursorX;
            _raw_y = gMouseCursorY;
        } else {
            _raw_x = delta_x;
            _raw_y = delta_y;
        }
    }
}

// 0x4CA934
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

// 0x4CA9DC
void mouseGetPosition(int* xPtr, int* yPtr)
{
    *xPtr = _mouse_hotx + gMouseCursorX;
    *yPtr = _mouse_hoty + gMouseCursorY;
}

// 0x4CAA04
void _mouse_set_position(int x, int y)
{
    gMouseCursorX = x - _mouse_hotx;
    gMouseCursorY = y - _mouse_hoty;
    _raw_y = y - _mouse_hoty;
    _raw_x = x - _mouse_hotx;
    _mouse_clip();
}

// 0x4CAA38
static void _mouse_clip()
{
    const auto position = clipCursorPosition(mouseGetCursorDrawPosition());
    gMouseCursorX = position.x;
    gMouseCursorY = position.y;
}

// 0x4CAAA0
MouseButtonEvent mouseGetEvent()
{
    return gMouseEvent;
}

// 0x4CAAA8
bool cursorIsHidden()
{
    return gCursorIsHidden;
}

// 0x4CAB5C
void _mouse_get_raw_state(int* out_x, int* out_y, MouseButtonEvent* out_buttons)
{
    MouseData mouseData;
    if (!mouseDeviceGetData(&mouseData)) {
        mouseData.x = 0;
        mouseData.y = 0;
        mouseData.buttons[0] = (gMouseEvent & MouseButtonEvent::LeftButtonDown) != MouseButtonEvent::None;
        mouseData.buttons[1] = (gMouseEvent & MouseButtonEvent::RightButtonDown) != MouseButtonEvent::None;
    }

    _raw_buttons = MouseButtonEvent::None;
    if (mouseDeviceUsesRelativeMode()) {
        _raw_x += mouseData.x;
        _raw_y += mouseData.y;
    } else {
        _raw_x = mouseData.x;
        _raw_y = mouseData.y;
    }

    if (mouseData.buttons[0] != 0) {
        _raw_buttons |= MouseButtonEvent::LeftButtonDown;
    }

    if (mouseData.buttons[1] != 0) {
        _raw_buttons |= MouseButtonEvent::RightButtonDown;
    }

    *out_x = _raw_x;
    *out_y = _raw_y;
    *out_buttons = _raw_buttons;
}

// 0x4CAC3C
void mouseSetSensitivity(double value)
{
    if (value >= MOUSE_SENSITIVITY_MIN && value <= MOUSE_SENSITIVITY_MAX) {
        gMouseSensitivity = value;
        relativeRemainder = {};
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
        if ((mouseGetEvent() & MouseButtonEvent::Wheel) != MouseButtonEvent::None) {
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

MouseButtonEvent mouse_get_last_buttons()
{
    return last_buttons;
}

} // namespace fallout
