#include "dinput.h"

#include "sfall_kb_helpers.h"
#include "svga.h"

namespace fallout {

static int gMouseWheelDeltaX = 0;
static int gMouseWheelDeltaY = 0;
static int mouseWindowMappingWindowWidth = 0;
static int mouseWindowMappingWindowHeight = 0;
static int mouseWindowMappingLogicalWidth = 0;
static int mouseWindowMappingLogicalHeight = 0;
static bool mouseRelativeMode = false;

static void mouseDeviceMapWindowToLogicalPosition(int* x, int* y);

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

bool mouseDeviceUsesRelativeMode()
{
    return mouseRelativeMode;
}

bool mouseDeviceInitMode()
{
    // "Relative mode" means cursor position is owned by the application, and we move based on mouse deltas
    // "Absolute mode" means cursor position is controlled by the OS, and we read it directly.
    // Mouse sensitivity settings can only apply in relative mode.

    // TODO: add setting for mouse capture (relative mode) in windowed, differentiated from the web version.
    bool wantsRelativeMode = screenIsFullscreen();

    if (wantsRelativeMode) {
        mouseRelativeMode = true;
        return SDL_SetRelativeMouseMode(SDL_TRUE) == 0;
    }

    if (SDL_SetRelativeMouseMode(SDL_FALSE) != 0) {
        return false;
    }

    mouseRelativeMode = false;
    mouseDeviceRefreshWindowMapping();
    return true;
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
    Uint32 buttons = mouseDeviceUsesRelativeMode()
        ? SDL_GetRelativeMouseState(&(mouseState->x), &(mouseState->y))
        : SDL_GetMouseState(&(mouseState->x), &(mouseState->y));
    if (!mouseDeviceUsesRelativeMode()) {
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
    sfall_kb_clear_synthetic_key_events();
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
    return mouseDeviceInitMode();
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
    if (mouseWindowMappingWindowWidth <= 0 || mouseWindowMappingWindowHeight <= 0) {
        return;
    }

    *x = *x * mouseWindowMappingLogicalWidth / mouseWindowMappingWindowWidth;
    *y = *y * mouseWindowMappingLogicalHeight / mouseWindowMappingWindowHeight;
}

void mouseDeviceRefreshWindowMapping()
{
    // When mouse is in "absolute mode" and scaling is > 1, we need to transform screen coordinates to game coordinates.
    // Cache the window sizes so we have them available in mouseDeviceMapWindowToLogicalPosition.
    // Note: if we add letterboxing or other more complex scaling, we'll have to account for it here.
    mouseWindowMappingLogicalWidth = screenGetWidth();
    mouseWindowMappingLogicalHeight = screenGetHeight();

    if (gSdlWindow != nullptr) {
        SDL_GetWindowSize(gSdlWindow, &mouseWindowMappingWindowWidth, &mouseWindowMappingWindowHeight);
    } else {
        mouseWindowMappingWindowWidth = 0;
        mouseWindowMappingWindowHeight = 0;
    }
}

} // namespace fallout
