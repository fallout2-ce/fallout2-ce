#include "platform_input.h"

#include <SDL.h>

#include <cstdlib>
#include <iterator>
#include <vector>

#include "audio_engine.h"
#include "clock.h"
#include "debug.h"
#include "delay.h"
#include "dinput.h"
#include "game.h"
#include "input_system.h"
#include "movie.h"
#include "sfall_kb_helpers.h"
#include "sfall_script_hooks.h"
#include "svga.h"
#include "touch.h"
#include "win32.h"
#include "window_manager.h"

namespace fallout {

namespace {

Uint32 scriptTapType;

using VideoHandler = void (*)();

VideoHandler videoHandler(const SDL_Event& event)
{
    if (event.type == SDL_RENDER_TARGETS_RESET) {
        return handleRenderTargetsReset;
    }
    if (event.type == SDL_RENDER_DEVICE_RESET) {
        return handleRenderDeviceReset;
    }
    if (event.type == SDL_WINDOWEVENT) {
#if SDL_VERSION_ATLEAST(2, 0, 18)
        if (event.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
#else
        if (event.window.event == SDL_WINDOWEVENT_MOVED) {
#endif
            return handleWindowDisplayChanged;
        }
    }
    return nullptr;
}

int SDLCALL collectVideoEvents(void* data, SDL_Event* event)
{
    if (const auto handle = videoHandler(*event)) {
        static_cast<std::vector<VideoHandler>*>(data)->push_back(handle);
        return 0;
    }
    return 1;
}

SDL_Scancode canonical(SDL_Scancode key)
{
    return key == SDL_SCANCODE_KP_PERIOD ? SDL_SCANCODE_KP_DECIMAL : key;
}

void dispatchKey(Keyboard& keyboard, const SDL_KeyboardEvent& event)
{
    const auto physical = event.keysym.scancode;
    if (!gProgramIsActive || !keyboard.enabled() || event.repeat
        || physical <= SDL_SCANCODE_UNKNOWN || physical >= SDL_NUM_SCANCODES) {
        return;
    }

    const bool pressed = event.type == SDL_KEYDOWN;
    const int override = sfall_kb_handle_key_pressed(physical, pressed);
    if (pressed) {
        const auto effective = override == -1 ? physical : static_cast<SDL_Scancode>(override);
        keyboard.press(physical, canonical(effective), getRealTicks());
    } else {
        // Scripts see the release. State follows the key selected on press.
        keyboard.release(physical);
    }
}

void notifyMouseHook(const SDL_MouseButtonEvent& event)
{
    // sfall numbers right before middle, unlike SDL.
    constexpr int hookButtons[] = { -1, 0, 2, 1, 3, 4, 5, 6, 7 };
    if (gGameLoaded && event.button >= SDL_BUTTON_LEFT && event.button < std::size(hookButtons)) {
        ScriptHookCall(HOOK_MOUSECLICK, 0, { event.type == SDL_MOUSEBUTTONDOWN ? 1 : 0, hookButtons[event.button] }).call();
    }
}

void dispatchWindowEvent(const SDL_WindowEvent& event)
{
    switch (event.event) {
    case SDL_WINDOWEVENT_EXPOSED:
        windowRefreshAll(&_scr_size);
        break;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        handleWindowSizeChanged();
        break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
        gProgramIsActive = true;
        if (gSdlRenderer == nullptr) {
            handleRenderDeviceReset();
        }
        movieHandleFocusGained();
        if (!mouseDeviceInitMode()) {
            debugPrint("Failed to initialize mouse mode on focus gained: %s\n", SDL_GetError());
        }
        windowRefreshAll(&_scr_size);
        audioEngineResume();
        break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
        gProgramIsActive = false;
        cancelTouchGestures();
        inputSystem.suspend();
        mouseDeviceInitMode();
        audioEnginePause();
        break;
    }
}

} // namespace

void platformInputInit()
{
    if (scriptTapType == 0) {
        const Uint32 type = SDL_RegisterEvents(1);
        if (type == static_cast<Uint32>(-1)) {
            debugPrint("Could not register scripted key taps: %s\n", SDL_GetError());
        } else {
            scriptTapType = type;
        }
    }

    const auto native = SDL_GetModState();
    std::uint16_t locks = 0;
    if (native & KMOD_CAPS) {
        locks |= KeyModifier::CapsLock;
    }
    if (native & KMOD_NUM) {
        locks |= KeyModifier::NumLock;
    }
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (native & KMOD_SCROLL) {
        locks |= KeyModifier::ScrollLock;
    }
#endif
    inputSystem._keyboard.syncLocks(locks);
}

void enqueueScriptKey(SDL_Scancode key)
{
    if (scriptTapType != 0) {
        // A tap is ordered with native events and has no held-key release.
        SDL_Event event{};
        event.type = scriptTapType;
        event.user.code = key;
        SDL_PushEvent(&event);
    }
}

void platformFlushKeyboard()
{
    SDL_FlushEvents(SDL_KEYDOWN, SDL_TEXTINPUT);
    if (scriptTapType != 0) {
        SDL_FlushEvent(scriptTapType);
    }
}

void platformFlushInput()
{
    SDL_PumpEvents();
    platformFlushKeyboard();
    SDL_FlushEvents(SDL_MOUSEMOTION, SDL_MOUSEWHEEL);
    SDL_FlushEvents(SDL_FINGERDOWN, SDL_FINGERMOTION);
    cancelTouchGestures();

    // A reset caller can present immediately. Dispatch recovery in queue order
    // after filtering has finished, with the remaining SDL events still queued.
    std::vector<VideoHandler> pending;
    SDL_FilterEvents(collectVideoEvents, &pending);
    for (auto handle : pending) {
        handle();
    }
}

void dispatchPlatformEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (const auto handle = videoHandler(event)) {
            handle();
            continue;
        }
        if (event.type == scriptTapType && scriptTapType != 0) {
            if (gProgramIsActive) {
                inputSystem._keyboard.tap(canonical(static_cast<SDL_Scancode>(event.user.code)));
            }
            continue;
        }

        switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            notifyMouseHook(event.button);
            handleMouseEvent(&event);
            break;
        case SDL_MOUSEMOTION:
        case SDL_MOUSEWHEEL:
            handleMouseEvent(&event);
            break;
        case SDL_FINGERDOWN:
            touch_handle_start(&event.tfinger);
            break;
        case SDL_FINGERMOTION:
            touch_handle_move(&event.tfinger);
            break;
        case SDL_FINGERUP:
            touch_handle_end(&event.tfinger);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            dispatchKey(inputSystem._keyboard, event.key);
            break;
        case SDL_WINDOWEVENT:
            dispatchWindowEvent(event.window);
            break;
        case SDL_APP_WILLENTERBACKGROUND:
            cancelTouchGestures();
            inputSystem.suspend();
            break;
        case SDL_QUIT:
            std::exit(EXIT_SUCCESS);
        }
    }

    touch_process_gesture();
    if (gProgramIsActive) {
        inputSystem._keyboard.repeat(getRealTicks());
    }
}

void waitForPlatformFocus()
{
    // A suspended window still needs events to receive focus or quit.
    while (!gProgramIsActive) {
        dispatchPlatformEvents();
        delay_ms(125);
    }
}

void beginTextInput()
{
    SDL_StartTextInput();
}

void endTextInput()
{
    SDL_StopTextInput();
}

} // namespace fallout
