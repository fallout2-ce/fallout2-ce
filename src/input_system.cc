#include "input_system.h"

#include <utility>

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

void InputSystem::setTouchShift(bool held)
{
    const bool wasHeld = _keyboard.touchShiftHeld();
    _keyboard.setTouchShift(held);
    if (_keyboard.touchShiftHeld() != wasHeld) {
        sfall_kb_handle_key_pressed(SDL_SCANCODE_LSHIFT, _keyboard.touchShiftHeld());
    }
}

void InputSystem::cancelKeyboard()
{
    setTouchShift(false);
    _keyboard.clear();
    _commands.discardIf([](const InputEvent& event) {
        return event.source == InputSource::Keyboard;
    });
}

void InputSystem::suspend()
{
    cancelPointer();
    cancelKeyboard();
    _commands.discardIf([](const InputEvent& event) {
        return event.source == InputSource::Touch;
    });
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
    setTouchShift(false);
    _keyboard.clear();
    _commands.clear();
}

} // namespace fallout
