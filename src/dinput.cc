#include "dinput.h"

#include "svga.h"

namespace fallout {

static int gMouseWheelDeltaX = 0;
static int gMouseWheelDeltaY = 0;
static int gMouseWindowMappingWindowWidth = 0;
static int gMouseWindowMappingWindowHeight = 0;
static int gMouseWindowMappingLogicalWidth = 0;
static int gMouseWindowMappingLogicalHeight = 0;

static void mouseDeviceMapWindowToLogicalPosition(int* x, int* y);
static void mouseDeviceRefreshWindowMapping();

// 0x4E0400
bool directInputInit()
{
    mouseDeviceRefreshWindowMapping();

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

// 0x4E0478
void directInputFree()
{
}

// 0x4E04E8
bool mouseDeviceAcquire()
{
    return true;
}

// 0x4E0514
bool mouseDeviceUnacquire()
{
    return true;
}

// 0x4E053C
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
    if (!screenIsFullscreen()) {
        mouseDeviceMapWindowToLogicalPosition(&(mouseState->x), &(mouseState->y));
    }
    mouseState->buttons[0] = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    mouseState->buttons[1] = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    mouseState->wheelX = gMouseWheelDeltaX;
    mouseState->wheelY = gMouseWheelDeltaY;

    gMouseWheelDeltaX = 0;
    gMouseWheelDeltaY = 0;

    return true;
}

// 0x4E05A8
bool keyboardDeviceAcquire()
{
    return true;
}

// 0x4E05D4
bool keyboardDeviceUnacquire()
{
    return true;
}

// 0x4E05FC
bool keyboardDeviceReset()
{
    SDL_FlushEvents(SDL_KEYDOWN, SDL_TEXTINPUT);
    return true;
}

// 0x4E0650
bool keyboardDeviceGetData(KeyboardData* keyboardData)
{
    return true;
}

// 0x4E070C
bool mouseDeviceInit()
{
    mouseDeviceRefreshWindowMapping();

    return screenIsFullscreen()
        ? SDL_SetRelativeMouseMode(SDL_TRUE) == 0
        : true;
}

// 0x4E078C
void mouseDeviceFree()
{
}

// 0x4E07B8
bool keyboardDeviceInit()
{
    return true;
}

// 0x4E0874
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

static void mouseDeviceMapWindowToLogicalPosition(int* x, int* y)
{
    if (gMouseWindowMappingWindowWidth <= 0 || gMouseWindowMappingWindowHeight <= 0) {
        return;
    }

    *x = *x * gMouseWindowMappingLogicalWidth / gMouseWindowMappingWindowWidth;
    *y = *y * gMouseWindowMappingLogicalHeight / gMouseWindowMappingWindowHeight;
}

static void mouseDeviceRefreshWindowMapping()
{
    gMouseWindowMappingLogicalWidth = screenGetWidth();
    gMouseWindowMappingLogicalHeight = screenGetHeight();

    if (gSdlWindow != nullptr) {
        SDL_GetWindowSize(gSdlWindow, &gMouseWindowMappingWindowWidth, &gMouseWindowMappingWindowHeight);
    } else {
        gMouseWindowMappingWindowWidth = 0;
        gMouseWindowMappingWindowHeight = 0;
    }
}

} // namespace fallout
