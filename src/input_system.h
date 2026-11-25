#ifndef FALLOUT_INPUT_SYSTEM_H_
#define FALLOUT_INPUT_SYSTEM_H_

#include <bitset>

#include "input_event.h"
#include "input_queue.h"
#include "keyboard.h"

namespace fallout {

class InputSystem {
public:
    bool initialize();
    void shutdown();
    void reset();
    void resetKeyboard();
    // Discard posted commands and queued native presses while preserving
    // sampled device state and the releases needed to clear it.
    void flushCommands();
    void suspend();

    // SDL dispatch supplies device events. Sampling prepares commands for the
    // active game update, which can consume them independently.
    void sample();
    InputEvent take();
    Point lastPosition() const { return _lastPosition; }

    void postWidget(int code) { post(code, InputSource::Widget); }
    void postTouchCommand(int code) { post(code, InputSource::Touch); }

    bool isKeyDown(SDL_Scancode key) const { return _keyboard.held(key); }
    bool keyboardEnabled() const { return _keyboard.enabled(); }
    void setKeyboardEnabled(bool enabled) { _keyboard.setEnabled(enabled); }
    void setTouchShift(bool held);
    bool touchShiftHeld() const { return _keyboard.touchShiftHeld(); }

private:
    friend void platformInputInit();
    friend void dispatchPlatformEvents();

    void post(int code, InputSource source, std::optional<KeyboardEvent> keyboardEvent = std::nullopt);
    void dispatchNativeKey(SDL_Scancode physical, bool pressed);
    bool leftShiftHookHeld() const;
    void syncLeftShiftHook(bool wasHeld);
    void cancelKeyboard(bool releaseNative = false);
    void cancelPointer();
    void clear();

    Keyboard _keyboard;
    // A script may consume a press, so hook releases track physical keys
    // independently of the keyboard's effective held keys.
    std::bitset<SDL_NUM_SCANCODES> _nativeHookKeys;
    int _leftShiftHookOverride = -1;
    InputQueue<InputEvent, 40> _commands;
    std::optional<InputEvent> _pointerEdge;
    Point _lastPosition { -1, -1 };
};

extern InputSystem inputSystem;

} // namespace fallout

#endif /* FALLOUT_INPUT_SYSTEM_H_ */
