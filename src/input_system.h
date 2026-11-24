#ifndef FALLOUT_INPUT_SYSTEM_H_
#define FALLOUT_INPUT_SYSTEM_H_

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
    void cancelKeyboard();
    void cancelPointer();
    void clear();

    Keyboard _keyboard;
    InputQueue<InputEvent, 40> _commands;
    std::optional<InputEvent> _pointerEdge;
    Point _lastPosition{ -1, -1 };
};

extern InputSystem inputSystem;

} // namespace fallout

#endif /* FALLOUT_INPUT_SYSTEM_H_ */
