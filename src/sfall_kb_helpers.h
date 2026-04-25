#ifndef FALLOUT_SFALL_KB_HELPERS_H_
#define FALLOUT_SFALL_KB_HELPERS_H_

namespace fallout {

/// Returns `true` if given key is pressed.
bool sfall_kb_is_key_pressed(int key);

/// Simulates pressing `key`.
void sfall_kb_press_key(int key);

/// Returns `true` when the next matching SDL key event was injected by `tap_key`.
bool sfall_kb_consume_synthetic_key_event(int sdlScanCode, bool pressed);

int sfall_kb_handle_key_pressed(int sdlScanCode, bool pressed);

} // namespace fallout

#endif /* FALLOUT_SFALL_KB_HELPERS_H_ */
