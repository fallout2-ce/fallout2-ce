#ifndef FALLOUT_KEYBOARD_H_
#define FALLOUT_KEYBOARD_H_

#include "input_queue.h"
#include "keyboard_event.h"

namespace fallout {

class Keyboard {
public:
    void press(SDL_Scancode physical, SDL_Scancode effective, std::uint32_t now);
    void release(SDL_Scancode physical);
    void tap(SDL_Scancode effective);
    void repeat(std::uint32_t now);
    void clear();
    void syncLocks(std::uint16_t modifiers);
    void setTouchShift(bool held) { _touchShift = _enabled && held; }
    void setEnabled(bool enabled) { _enabled = enabled; }

    bool enabled() const { return _enabled; }
    bool touchShiftHeld() const { return _touchShift; }
    bool held(SDL_Scancode effective) const;
    std::optional<KeyboardEvent> take() { return _events.pop(); }

private:
    struct Press {
        SDL_Scancode effective = SDL_SCANCODE_UNKNOWN;
        std::uint32_t lastStroke = 0;
        std::size_t slot = 0;
        bool repeating = false;
    };

    void emit(SDL_Scancode physical, SDL_Scancode effective, KeyStroke stroke);
    void toggleLock(SDL_Scancode key);
    std::uint16_t modifiers() const;

    std::array<Press, SDL_NUM_SCANCODES> _presses {};
    std::array<SDL_Scancode, SDL_NUM_SCANCODES> _active {};
    // An effective key stays held for as long as any physical key owns it.
    std::array<unsigned int, SDL_NUM_SCANCODES> _owners {};
    std::size_t _count = 0;
    InputQueue<KeyboardEvent, 63> _events;
    std::uint16_t _locks = 0;
    bool _touchShift = false;
    bool _enabled = true;
};

} // namespace fallout

#endif /* FALLOUT_KEYBOARD_H_ */
