#include "sfall_kb_helpers.h"

#include <SDL.h>

#include "game.h"
#include "sfall_script_hooks.h"
#include "svga.h"

#include <deque>
#include <unordered_map>

namespace fallout {

constexpr size_t DIK_MAP_COUNT = 256;

/// Maps DirectInput DIK constants to SDL scancodes.
static constexpr SDL_Scancode kDiks[DIK_MAP_COUNT] = {
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_ESCAPE, // DIK_ESCAPE
    SDL_SCANCODE_1, // DIK_1
    SDL_SCANCODE_2, // DIK_2
    SDL_SCANCODE_3, // DIK_3
    SDL_SCANCODE_4, // DIK_4
    SDL_SCANCODE_5, // DIK_5
    SDL_SCANCODE_6, // DIK_6
    SDL_SCANCODE_7, // DIK_7
    SDL_SCANCODE_8, // DIK_8
    SDL_SCANCODE_9, // DIK_9
    SDL_SCANCODE_0, // DIK_0
    SDL_SCANCODE_MINUS, // DIK_MINUS
    SDL_SCANCODE_EQUALS, // DIK_EQUALS
    SDL_SCANCODE_BACKSPACE, // DIK_BACK
    SDL_SCANCODE_TAB, // DIK_TAB
    SDL_SCANCODE_Q, // DIK_Q
    SDL_SCANCODE_W, // DIK_W
    SDL_SCANCODE_E, // DIK_E
    SDL_SCANCODE_R, // DIK_R
    SDL_SCANCODE_T, // DIK_T
    SDL_SCANCODE_Y, // DIK_Y
    SDL_SCANCODE_U, // DIK_U
    SDL_SCANCODE_I, // DIK_I
    SDL_SCANCODE_O, // DIK_O
    SDL_SCANCODE_P, // DIK_P
    SDL_SCANCODE_LEFTBRACKET, // DIK_LBRACKET
    SDL_SCANCODE_RIGHTBRACKET, // DIK_RBRACKET
    SDL_SCANCODE_RETURN, // DIK_RETURN
    SDL_SCANCODE_LCTRL, // DIK_LCONTROL
    SDL_SCANCODE_A, // DIK_A
    SDL_SCANCODE_S, // DIK_S
    SDL_SCANCODE_D, // DIK_D
    SDL_SCANCODE_F, // DIK_F
    SDL_SCANCODE_G, // DIK_G
    SDL_SCANCODE_H, // DIK_H
    SDL_SCANCODE_J, // DIK_J
    SDL_SCANCODE_K, // DIK_K
    SDL_SCANCODE_L, // DIK_L
    SDL_SCANCODE_SEMICOLON, // DIK_SEMICOLON
    SDL_SCANCODE_APOSTROPHE, // DIK_APOSTROPHE
    SDL_SCANCODE_GRAVE, // DIK_GRAVE
    SDL_SCANCODE_LSHIFT, // DIK_LSHIFT
    SDL_SCANCODE_BACKSLASH, // DIK_BACKSLASH
    SDL_SCANCODE_Z, // DIK_Z
    SDL_SCANCODE_X, // DIK_X
    SDL_SCANCODE_C, // DIK_C
    SDL_SCANCODE_V, // DIK_V
    SDL_SCANCODE_B, // DIK_B
    SDL_SCANCODE_N, // DIK_N
    SDL_SCANCODE_M, // DIK_M
    SDL_SCANCODE_COMMA, // DIK_COMMA
    SDL_SCANCODE_PERIOD, // DIK_PERIOD
    SDL_SCANCODE_SLASH, // DIK_SLASH
    SDL_SCANCODE_RSHIFT, // DIK_RSHIFT
    SDL_SCANCODE_KP_MULTIPLY, // DIK_MULTIPLY
    SDL_SCANCODE_LALT, // DIK_LMENU
    SDL_SCANCODE_SPACE, // DIK_SPACE
    SDL_SCANCODE_CAPSLOCK, // DIK_CAPITAL
    SDL_SCANCODE_F1, // DIK_F1
    SDL_SCANCODE_F2, // DIK_F2
    SDL_SCANCODE_F3, // DIK_F3
    SDL_SCANCODE_F4, // DIK_F4
    SDL_SCANCODE_F5, // DIK_F5
    SDL_SCANCODE_F6, // DIK_F6
    SDL_SCANCODE_F7, // DIK_F7
    SDL_SCANCODE_F8, // DIK_F8
    SDL_SCANCODE_F9, // DIK_F9
    SDL_SCANCODE_F10, // DIK_F10
    SDL_SCANCODE_NUMLOCKCLEAR, // DIK_NUMLOCK
    SDL_SCANCODE_SCROLLLOCK, // DIK_SCROLL
    SDL_SCANCODE_KP_7, // DIK_NUMPAD7
    SDL_SCANCODE_KP_8, // DIK_NUMPAD8
    SDL_SCANCODE_KP_9, // DIK_NUMPAD9
    SDL_SCANCODE_KP_MINUS, // DIK_SUBTRACT
    SDL_SCANCODE_KP_4, // DIK_NUMPAD4
    SDL_SCANCODE_KP_5, // DIK_NUMPAD5
    SDL_SCANCODE_KP_6, // DIK_NUMPAD6
    SDL_SCANCODE_KP_PLUS, // DIK_ADD
    SDL_SCANCODE_KP_1, // DIK_NUMPAD1
    SDL_SCANCODE_KP_2, // DIK_NUMPAD2
    SDL_SCANCODE_KP_3, // DIK_NUMPAD3
    SDL_SCANCODE_KP_0, // DIK_NUMPAD0
    SDL_SCANCODE_KP_PERIOD, // DIK_DECIMAL
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_F11, // DIK_F11
    SDL_SCANCODE_F12, // DIK_F12
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_KP_EQUALS, // DIK_NUMPADEQUALS
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN, // DIK_AT
    SDL_SCANCODE_UNKNOWN, // DIK_COLON
    SDL_SCANCODE_UNKNOWN, // DIK_UNDERLINE
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN, // DIK_STOP
    SDL_SCANCODE_UNKNOWN, // DIK_AX
    SDL_SCANCODE_UNKNOWN, // DIK_UNLABELED
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_KP_ENTER, // DIK_NUMPADENTER
    SDL_SCANCODE_RCTRL, // DIK_RCONTROL
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_KP_COMMA, // DIK_NUMPADCOMMA
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_SLASH, // DIK_DIVIDE
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_SYSREQ, // DIK_SYSRQ
    SDL_SCANCODE_RALT, // DIK_RMENU
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_HOME, // DIK_HOME
    SDL_SCANCODE_UP, // DIK_UP
    SDL_SCANCODE_PAGEUP, // DIK_PRIOR
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_LEFT, // DIK_LEFT
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_RIGHT, // DIK_RIGHT
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_END, // DIK_END
    SDL_SCANCODE_DOWN, // DIK_DOWN
    SDL_SCANCODE_PAGEDOWN, // DIK_NEXT
    SDL_SCANCODE_INSERT, // DIK_INSERT
    SDL_SCANCODE_DELETE, // DIK_DELETE
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_LGUI, // DIK_LWIN
    SDL_SCANCODE_RGUI, // DIK_RWIN
    SDL_SCANCODE_APPLICATION, // DIK_APPS
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
};

std::unordered_map<SDL_Scancode, int> kScanCodeToDik;
std::deque<std::pair<SDL_Scancode, bool>> syntheticKeyEvents;

/// Translates Sfall key code (DIK or VK constant) to SDL scancode.
static SDL_Scancode get_scancode_from_key(int key)
{
    return kDiks[key & 0xFF];
}

/// Translates SDL scancode into DIK key constant, used by sfall.
static int get_key_from_scancode(SDL_Scancode scanCode)
{
    if (kScanCodeToDik.empty()) {
        for (int dik = DIK_MAP_COUNT - 1; dik >= 1; --dik) {
            if (kDiks[dik] == SDL_SCANCODE_UNKNOWN) continue;

            kScanCodeToDik[kDiks[dik]] = dik;
        }
    }
    auto dikIt = kScanCodeToDik.find(scanCode);
    if (dikIt == kScanCodeToDik.end()) {
        return SDL_SCANCODE_UNKNOWN;
    }
    return dikIt->second;
}

bool sfall_kb_is_key_pressed(int key)
{
    // todo: sfall uses this condition to check for VK key instead of DIK:
    /* if ((key & 0x80000000) > 0) { // special flag to check by VK code directly
        return GetAsyncKeyState(key & 0xFFFF) & 0x8000;
    }*/
    SDL_Scancode scancode = get_scancode_from_key(key);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return false;
    }

    const Uint8* state = SDL_GetKeyboardState(nullptr);
    return state[scancode] != 0;
}

void sfall_kb_press_key(int key)
{
    SDL_Scancode scancode = get_scancode_from_key(key);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }

    SDL_Event event;
    SDL_zero(event);

    event.type = SDL_KEYDOWN;
    event.key.timestamp = SDL_GetTicks();
    event.key.windowID = gSdlWindow != nullptr ? SDL_GetWindowID(gSdlWindow) : 0;
    event.key.state = SDL_PRESSED;
    event.key.repeat = 0;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
    event.key.keysym.mod = SDL_GetModState();
    if (SDL_PushEvent(&event) == 1) {
        syntheticKeyEvents.emplace_back(scancode, true);
    }

    event.type = SDL_KEYUP;
    event.key.timestamp = SDL_GetTicks();
    event.key.state = SDL_RELEASED;
    if (SDL_PushEvent(&event) == 1) {
        syntheticKeyEvents.emplace_back(scancode, false);
    }
}

bool sfall_kb_consume_synthetic_key_event(int sdlScanCode, bool pressed)
{
    if (syntheticKeyEvents.empty()) {
        return false;
    }

    const auto& [expectedScanCode, expectedPressed] = syntheticKeyEvents.front();
    if (expectedScanCode != static_cast<SDL_Scancode>(sdlScanCode) || expectedPressed != pressed) {
        return false;
    }

    syntheticKeyEvents.pop_front();
    return true;
}

int sfall_kb_handle_key_pressed(int sdlScanCode, bool pressed)
{
    if (!gGameLoaded) return SDL_SCANCODE_UNKNOWN;

    ScriptHookCall hook(HOOK_KEYPRESS, 1, {
                                              pressed ? 1 : 0, get_key_from_scancode(static_cast<SDL_Scancode>(sdlScanCode)),
                                              0 // TODO: sfall uses VK_ codes here; not sure any mod actually used it. If so, maybe it is better to use Key values from kb.h?
                                          });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return SDL_SCANCODE_UNKNOWN;
    }

    int overrideDxCode = hook.getReturnValueAt(0).asInt();
    return get_scancode_from_key(overrideDxCode);
}

} // namespace fallout
