#include "dinput.h"

#include "debug.h"
#include "settings.h"
#include "svga.h"
#include "win32.h"

#include <utility>

namespace fallout {

static int gMouseWheelDeltaX = 0;
static int gMouseWheelDeltaY = 0;
static int mouseWindowMappingWindowWidth = 0;
static int mouseWindowMappingWindowHeight = 0;
static int mouseWindowMappingLogicalWidth = 0;
static int mouseWindowMappingLogicalHeight = 0;
static bool mouseRelativeMode = false;
static SDL_Point sampledMotion {};
static Uint32 canceledButtons = 0;
static unsigned int motionGeneration = 0;

static void mouseDeviceMapWindowToLogicalPosition(int* x, int* y);

// 0x4E0400
bool directInputInit()
{
    mouseDeviceRefreshWindowMapping();

    if (!mouseDeviceInit()) {
        debugPrint("directInputInit: mouseDeviceInit failed: %s\n", SDL_GetError());
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

unsigned int mouseDeviceMotionGeneration()
{
    return motionGeneration;
}

bool mouseDeviceInitMode()
{
    sampledMotion = {};
    ++motionGeneration;
    // "Relative mode" means cursor position is owned by the application, and we move based on mouse deltas
    // "Absolute mode" means cursor position is controlled by the OS, and we read it directly.
    // Mouse sensitivity settings can only apply in relative mode.

    bool wantsRelativeMode = gProgramIsActive && (screenIsExclusiveFullscreen() || settings.screen.mouse_lock);

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

void cancelMouseDeviceInput()
{
    // SDL may report buttons held across a reset. Wait for their releases
    // before exposing another press to the game.
    canceledButtons = SDL_GetMouseState(nullptr, nullptr);
    gMouseWheelDeltaX = 0;
    gMouseWheelDeltaY = 0;
}

// 0x4E053C
bool mouseDeviceGetData(MouseData* mouseState)
{
    // Refresh device state while leaving event dispatch to the caller's loop.
    SDL_PumpEvents();
    Uint32 buttons = mouseDeviceUsesRelativeMode()
        ? SDL_GetRelativeMouseState(&(mouseState->x), &(mouseState->y))
        : SDL_GetMouseState(&(mouseState->x), &(mouseState->y));
    if (!mouseDeviceUsesRelativeMode()) {
        mouseDeviceMapWindowToLogicalPosition(&(mouseState->x), &(mouseState->y));
    } else {
        const auto motion = std::exchange(sampledMotion, SDL_Point {});
        mouseState->x += motion.x;
        mouseState->y += motion.y;
    }
    canceledButtons &= buttons;
    buttons &= ~canceledButtons;
    mouseState->buttons[0] = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    mouseState->buttons[1] = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    mouseState->wheelX = std::exchange(gMouseWheelDeltaX, 0);
    mouseState->wheelY = std::exchange(gMouseWheelDeltaY, 0);

    return true;
}

SDL_Point mouseDeviceSamplePosition()
{
    SDL_Point point;
    if (mouseRelativeMode) {
        // Reading SDL's relative motion consumes it. Keep the sum for the update.
        SDL_GetRelativeMouseState(&point.x, &point.y);
        sampledMotion.x += point.x;
        sampledMotion.y += point.y;
        return sampledMotion;
    }

    SDL_GetMouseState(&point.x, &point.y);
    mouseDeviceMapWindowToLogicalPosition(&point.x, &point.y);
    return point;
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
    } else if (event->type == SDL_MOUSEBUTTONUP
        && (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)) {
        // A new press can arrive before the next device sample.
        canceledButtons &= ~SDL_BUTTON(event->button.button);
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

void mouseDeviceSetLogicalPosition(int x, int y)
{
    if (mouseDeviceUsesRelativeMode()
        || gSdlWindow == nullptr
        || mouseWindowMappingLogicalWidth <= 0
        || mouseWindowMappingLogicalHeight <= 0
        || mouseWindowMappingWindowWidth <= 0
        || mouseWindowMappingWindowHeight <= 0) {
        return;
    }

    int windowX = x * mouseWindowMappingWindowWidth / mouseWindowMappingLogicalWidth;
    int windowY = y * mouseWindowMappingWindowHeight / mouseWindowMappingLogicalHeight;
    SDL_WarpMouseInWindow(gSdlWindow, windowX, windowY);
}

} // namespace fallout
