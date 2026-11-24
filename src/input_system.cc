#include "input_system.h"

#include <utility>

#include "clock.h"
#include "dinput.h"
#include "key_codes.h"
#include "keyboard_layout.h"
#include "mouse.h"
#include "platform_input.h"
#include "screenshot.h"
#include "sfall_kb_helpers.h"
#include "touch.h"
#include "window_manager.h"

namespace fallout {

InputSystem inputSystem;

bool InputSystem::initialize()
{
    if (!directInputInit()) {
        return false;
    }
    if (mouseInit() == -1) {
        directInputFree();
        return false;
    }

    clear();
    _nativeHookKeys.reset();
    _keyboard.setEnabled(true);
    platformInputInit();
    platformFlushKeyboard();
    screenshotHandlerConfigure(KEY_ALT_C, nullptr);
    return true;
}

void InputSystem::shutdown()
{
    clear();
    mouseFree();
    directInputFree();
}

void InputSystem::reset()
{
    clear();
    platformFlushInput();
}

void InputSystem::resetKeyboard()
{
    cancelKeyboard();
    platformFlushKeyboard();
}

void InputSystem::flushCommands()
{
    _commands.clear();
    _pointerEdge.reset();
    platformFlushInput(true);
}

void InputSystem::post(int code, InputSource source, std::optional<KeyboardEvent> keyboardEvent)
{
    if (code == -1 || handleScreenshotShortcut(code)) {
        return;
    }

    Point position;
    mouseGetPosition(&position.x, &position.y);
    _commands.push({ code, position, source, keyboardEvent });
}

void InputSystem::sample()
{
    _pointerEdge.reset();
    _mouse_info();
    const int widgetCode = _win_check_all_buttons();
    if (widgetCode != -1) {
        post(widgetCode, InputSource::Widget);
    } else if (const auto event = _keyboard.take()) {
        post(decodeLegacyKey(*event), InputSource::Keyboard, event);
    }

    if (_commands.empty() && (mouseGetEvent() & (MouseButtonEvent::AnyButtonDown | MouseButtonEvent::AnyButtonUp)) != MouseButtonEvent::None) {
        Point position;
        mouseGetPosition(&position.x, &position.y);
        _pointerEdge = InputEvent{ -2, position, InputSource::Pointer, std::nullopt };
    }
}

InputEvent InputSystem::take()
{
    auto event = _commands.pop();
    if (!event) {
        event = std::exchange(_pointerEdge, std::nullopt);
    }
    if (event) {
        _lastPosition = event->position;
        return *event;
    }
    return {};
}

bool InputSystem::leftShiftHookHeld() const
{
    return _keyboard.touchShiftHeld() || _nativeHookKeys.test(SDL_SCANCODE_LSHIFT);
}

void InputSystem::syncLeftShiftHook(bool wasHeld)
{
    // The gesture and the native key both report DIK_LSHIFT to listeners.
    const bool held = leftShiftHookHeld();
    if (wasHeld != held) {
        _leftShiftHookOverride = sfall_kb_handle_key_pressed(SDL_SCANCODE_LSHIFT, held);
    }
}

void InputSystem::setTouchShift(bool held)
{
    const bool wasHeld = leftShiftHookHeld();
    _keyboard.setTouchShift(held);
    syncLeftShiftHook(wasHeld);
}

void InputSystem::dispatchNativeKey(SDL_Scancode physical, bool pressed)
{
    if (!_keyboard.enabled() || physical <= SDL_SCANCODE_UNKNOWN || physical >= SDL_NUM_SCANCODES) {
        return;
    }

    const bool isLeftShift = physical == SDL_SCANCODE_LSHIFT;
    const bool wasShiftHeld = leftShiftHookHeld();
    if (pressed) {
        _nativeHookKeys.set(physical);
        int override;
        if (isLeftShift) {
            syncLeftShiftHook(wasShiftHeld);
            override = _leftShiftHookOverride;
        } else {
            override = sfall_kb_handle_key_pressed(physical, true);
        }
        // A hook that enters a modal loop can suspend input before returning.
        if (_nativeHookKeys.test(physical)) {
            const auto effective = override == -1 ? physical : static_cast<SDL_Scancode>(override);
            _keyboard.press(physical, effective, getRealTicks());
        }
    } else if (_nativeHookKeys.test(physical)) {
        _nativeHookKeys.reset(physical);
        _keyboard.release(physical);
        if (isLeftShift) {
            syncLeftShiftHook(wasShiftHeld);
        } else {
            sfall_kb_handle_key_pressed(physical, false);
        }
    }
}

void InputSystem::cancelKeyboard(bool releaseNative)
{
    const bool wasShiftHeld = leftShiftHookHeld();
    // Snapshot and clear first so a nested update cannot release these twice.
    const auto held = releaseNative ? std::exchange(_nativeHookKeys, { }) : decltype(_nativeHookKeys) { };
    _keyboard.clear();
    _commands.discardIf([](const InputEvent& event) {
        return event.source == InputSource::Keyboard;
    });
    syncLeftShiftHook(wasShiftHeld);
    if (releaseNative) {
        for (int key = 1; key < SDL_NUM_SCANCODES; ++key) {
            if (key != SDL_SCANCODE_LSHIFT && held.test(key)) {
                sfall_kb_handle_key_pressed(key, false);
            }
        }
    }
}

void InputSystem::suspend()
{
    cancelPointer();
    _commands.discardIf([](const InputEvent& event) {
        return event.source == InputSource::Touch;
    });

    cancelKeyboard(true);
}

void InputSystem::cancelPointer()
{
    _pointerEdge.reset();
    cancelTouchGestures();
    cancelMouseButtons();
    cancelWindowButtonPresses();
}

void InputSystem::clear()
{
    cancelPointer();
    _commands.clear();
    cancelKeyboard();
}

} // namespace fallout
