#ifndef FALLOUT_INPUT_EVENT_H_
#define FALLOUT_INPUT_EVENT_H_

#include <optional>

#include "geometry.h"
#include "keyboard_event.h"

namespace fallout {

enum class InputSource {
    None,
    Keyboard,
    Pointer,
    Widget,
    Touch,
};

struct InputEvent {
    int code = -1;
    Point position{ -1, -1 };
    InputSource source = InputSource::None;
    std::optional<KeyboardEvent> keyboard;
};

} // namespace fallout

#endif /* FALLOUT_INPUT_EVENT_H_ */
