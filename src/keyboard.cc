#include "keyboard.h"

namespace fallout {

namespace {

    constexpr std::uint32_t kRepeatDelay = 500;
    constexpr std::uint32_t kRepeatInterval = 80;

    bool valid(SDL_Scancode key)
    {
        return key > SDL_SCANCODE_UNKNOWN && key < SDL_NUM_SCANCODES;
    }

    SDL_Scancode canonicalScancode(SDL_Scancode key)
    {
        return key == SDL_SCANCODE_KP_PERIOD ? SDL_SCANCODE_KP_DECIMAL : key;
    }

    KeyModifier lockFor(SDL_Scancode key)
    {
        switch (key) {
        case SDL_SCANCODE_CAPSLOCK:
            return KeyModifier::CapsLock;
        case SDL_SCANCODE_NUMLOCKCLEAR:
            return KeyModifier::NumLock;
        case SDL_SCANCODE_SCROLLLOCK:
            return KeyModifier::ScrollLock;
        default:
            return static_cast<KeyModifier>(0);
        }
    }

    bool repeats(SDL_Scancode key)
    {
        return lockFor(key) == 0 && (key < SDL_SCANCODE_LCTRL || key > SDL_SCANCODE_RGUI);
    }

} // namespace

void Keyboard::press(SDL_Scancode physical, SDL_Scancode effective, std::uint32_t now)
{
    effective = canonicalScancode(effective);
    if (!_enabled || !valid(physical) || !valid(effective)) {
        return;
    }

    auto& press = _presses[physical];
    if (press.effective != SDL_SCANCODE_UNKNOWN) {
        return;
    }

    press = { effective, now, _count };
    _active[_count++] = physical;
    if (++_owners[effective] == 1) {
        toggleLock(effective);
    }
    emit(physical, effective, KeyStroke::Press);
}

void Keyboard::release(SDL_Scancode physical)
{
    if (!valid(physical)) {
        return;
    }

    auto& press = _presses[physical];
    if (press.effective == SDL_SCANCODE_UNKNOWN) {
        return;
    }

    --_owners[press.effective];
    // Fill the vacated slot so an update only visits keys that are held.
    const auto last = _active[--_count];
    _active[press.slot] = last;
    _presses[last].slot = press.slot;
    press = {};
}

void Keyboard::tap(SDL_Scancode effective)
{
    effective = canonicalScancode(effective);
    if (_enabled && valid(effective)) {
        toggleLock(effective);
        emit(SDL_SCANCODE_UNKNOWN, effective, KeyStroke::ScriptTap);
    }
}

void Keyboard::repeat(std::uint32_t now)
{
    if (!_enabled) {
        return;
    }

    for (std::size_t index = 0; index < _count; ++index) {
        const auto physical = _active[index];
        auto& press = _presses[physical];
        const auto delay = press.repeating ? kRepeatInterval : kRepeatDelay;
        if (now - press.lastStroke >= delay && repeats(press.effective)) {
            press.lastStroke = now;
            press.repeating = true;
            emit(physical, press.effective, KeyStroke::Repeat);
        }
    }
}

void Keyboard::clear()
{
    _presses.fill({});
    _owners.fill(0);
    _count = 0;
    _touchShift = false;
    _events.clear();
}

void Keyboard::syncLocks(std::uint16_t modifiers)
{
    _locks = modifiers & KeyModifier::Locks;
}

bool Keyboard::held(SDL_Scancode effective) const
{
    return (effective == SDL_SCANCODE_LSHIFT && _touchShift)
        || (valid(effective) && _owners[effective] != 0);
}

void Keyboard::toggleLock(SDL_Scancode key)
{
    if (!held(SDL_SCANCODE_LCTRL) && !held(SDL_SCANCODE_RCTRL)) {
        _locks ^= lockFor(key);
    }
}

std::uint16_t Keyboard::modifiers() const
{
    static constexpr struct {
        SDL_Scancode key;
        KeyModifier modifier;
    } keys[] = {
        { SDL_SCANCODE_LSHIFT, KeyModifier::LeftShift },
        { SDL_SCANCODE_RSHIFT, KeyModifier::RightShift },
        { SDL_SCANCODE_LALT, KeyModifier::LeftAlt },
        { SDL_SCANCODE_RALT, KeyModifier::RightAlt },
        { SDL_SCANCODE_LCTRL, KeyModifier::LeftControl },
        { SDL_SCANCODE_RCTRL, KeyModifier::RightControl },
    };

    auto modifiers = _locks;
    for (const auto& key : keys) {
        if (held(key.key)) {
            modifiers |= key.modifier;
        }
    }
    return modifiers;
}

void Keyboard::emit(SDL_Scancode physical, SDL_Scancode effective, KeyStroke stroke)
{
    _events.push({ physical, effective, modifiers(), stroke });
}

} // namespace fallout
