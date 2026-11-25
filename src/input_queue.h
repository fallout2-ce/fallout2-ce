#ifndef FALLOUT_INPUT_QUEUE_H_
#define FALLOUT_INPUT_QUEUE_H_

#include <array>
#include <optional>

namespace fallout {

template <typename Event, std::size_t Capacity>
class InputQueue {
    static_assert(Capacity > 0);

public:
    void push(const Event& event)
    {
        // Preserve pending input when the queue fills.
        if (_size < Capacity) {
            at(_size++) = event;
        }
    }

    std::optional<Event> pop()
    {
        if (empty()) {
            return std::nullopt;
        }

        const Event event = at(0);
        _head = (_head + 1) % Capacity;
        --_size;
        return event;
    }

    bool empty() const { return _size == 0; }

    void clear()
    {
        _head = 0;
        _size = 0;
    }

    template <typename Predicate>
    void discardIf(Predicate discard)
    {
        std::size_t kept = 0;
        for (std::size_t index = 0; index < _size; ++index) {
            if (!discard(at(index))) {
                at(kept++) = at(index);
            }
        }
        _size = kept;
    }

private:
    Event& at(std::size_t index) { return _events[(_head + index) % Capacity]; }

    std::array<Event, Capacity> _events {};
    std::size_t _head = 0;
    std::size_t _size = 0;
};

} // namespace fallout

#endif /* FALLOUT_INPUT_QUEUE_H_ */
