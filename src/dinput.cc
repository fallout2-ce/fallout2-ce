#include "dinput.h"

#include "svga.h"

namespace fallout {

static int gMouseWheelDeltaX = 0;
static int gMouseWheelDeltaY = 0;

// 0x4E0400 dxinput_init_
bool directInputInit()
{
    if (!mouseDeviceInit()) {
        goto err;
    }

    if (!keyboardDeviceInit()) {
        goto err;
    }

    return true;

err:

    directInputFree();

    return false;
}

// 0x4E0478 dxinput_exit_
void directInputFree()
{
}

// 0x4E04E8 dxinput_acquire_mouse_
bool mouseDeviceAcquire()
{
    return true;
}

// 0x4E0514 dxinput_unacquire_mouse_
bool mouseDeviceUnacquire()
{
    return true;
}

// 0x4E053C dxinput_get_mouse_state_
bool mouseDeviceGetData(MouseData* mouseState)
{
    // CE: This function is sometimes called outside loops calling `get_input`
    // and subsequently `GNW95_process_message`, so mouse events might not be
    // handled by SDL yet.
    //
    // TODO: Move mouse events processing into `GNW95_process_message` and
    // update mouse position manually.
    SDL_PumpEvents();
    Uint32 buttons = screenIsFullscreen()
        ? SDL_GetRelativeMouseState(&(mouseState->x), &(mouseState->y))
        : SDL_GetMouseState(&(mouseState->x), &(mouseState->y));
    mouseState->buttons[0] = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    mouseState->buttons[1] = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    mouseState->wheelX = gMouseWheelDeltaX;
    mouseState->wheelY = gMouseWheelDeltaY;

    gMouseWheelDeltaX = 0;
    gMouseWheelDeltaY = 0;

    return true;
}

// 0x4E05A8 dxinput_acquire_keyboard_
bool keyboardDeviceAcquire()
{
    return true;
}

// 0x4E05D4 dxinput_unacquire_keyboard_
bool keyboardDeviceUnacquire()
{
    return true;
}

// 0x4E05FC dxinput_flush_keyboard_buffer_
bool keyboardDeviceReset()
{
    SDL_FlushEvents(SDL_KEYDOWN, SDL_TEXTINPUT);
    return true;
}

// 0x4E0650 dxinput_read_keyboard_buffer_
bool keyboardDeviceGetData(KeyboardData* keyboardData)
{
    return true;
}

// 0x4E070C dxinput_mouse_init_
bool mouseDeviceInit()
{
    return screenIsFullscreen()
        ? SDL_SetRelativeMouseMode(SDL_TRUE) == 0
        : true;
}

// 0x4E078C dxinput_mouse_exit_
void mouseDeviceFree()
{
}

// 0x4E07B8 dxinput_keyboard_init_
bool keyboardDeviceInit()
{
    return true;
}

// 0x4E0874 dxinput_keyboard_exit_
void keyboardDeviceFree()
{
}

void handleMouseEvent(SDL_Event* event)
{
    // Mouse movement and buttons are accumulated in SDL itself and will be
    // processed later in `mouseDeviceGetData` via `SDL_GetRelativeMouseState`.

    if (event->type == SDL_MOUSEWHEEL) {
        gMouseWheelDeltaX += event->wheel.x;
        gMouseWheelDeltaY += event->wheel.y;
    }
}

} // namespace fallout
