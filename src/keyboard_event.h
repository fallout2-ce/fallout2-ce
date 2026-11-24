#ifndef FALLOUT_KEYBOARD_EVENT_H_
#define FALLOUT_KEYBOARD_EVENT_H_

#include <SDL_scancode.h>

#include <cstdint>

namespace fallout {

enum KeyModifier : std::uint16_t {
    CapsLock = 0x0001,
    NumLock = 0x0002,
    ScrollLock = 0x0004,
    LeftShift = 0x0008,
    RightShift = 0x0010,
    LeftAlt = 0x0020,
    RightAlt = 0x0040,
    LeftControl = 0x0080,
    RightControl = 0x0100,
    Shift = LeftShift | RightShift,
    Alt = LeftAlt | RightAlt,
    Control = LeftControl | RightControl,
    Locks = CapsLock | NumLock | ScrollLock,
};

enum class KeyStroke : std::uint8_t {
    Press,
    Repeat,
    ScriptTap,
};

struct KeyboardEvent {
    SDL_Scancode physical = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode effective = SDL_SCANCODE_UNKNOWN;
    std::uint16_t modifiers = 0;
    KeyStroke stroke = KeyStroke::Press;
};

} // namespace fallout

#endif /* FALLOUT_KEYBOARD_EVENT_H_ */
