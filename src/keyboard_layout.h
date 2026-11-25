#ifndef FALLOUT_KEYBOARD_LAYOUT_H_
#define FALLOUT_KEYBOARD_LAYOUT_H_

#include "keyboard_event.h"

namespace fallout {

// Translate a keyboard stroke into the numeric code used by legacy callers.
int decodeLegacyKey(const KeyboardEvent& event);

} // namespace fallout

#endif /* FALLOUT_KEYBOARD_LAYOUT_H_ */
