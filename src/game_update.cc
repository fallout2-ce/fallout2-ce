#include "game_update.h"

#include <list>

#include "clock.h"
#include "delay.h"
#include "input_system.h"
#include "platform_input.h"
#include "window_manager_private.h"

namespace fallout {

namespace {

class UpdateCallbacks {
public:
    void add(UpdateCallback* callback)
    {
        for (auto& entry : _callbacks) {
            if (entry.callback == callback && entry.removed) {
                entry.removed = false;
                return;
            }
        }
        _callbacks.push_front({ callback });
    }

    void remove(UpdateCallback* callback)
    {
        for (auto& entry : _callbacks) {
            if (entry.callback == callback) {
                // A callback can remove itself or another callback mid-pass.
                entry.removed = true;
                return;
            }
        }
    }

    void run()
    {
        if (!_enabled) {
            return;
        }

        _timestamp = getTicks();
        for (auto entry = _callbacks.begin(); entry != _callbacks.end();) {
            if (entry->removed) {
                entry = _callbacks.erase(entry);
            } else {
                // Additions go to the front and start on the next pass.
                const auto callback = entry++->callback;
                callback();
            }
        }
    }

    void reset()
    {
        _callbacks.clear();
        _enabled = true;
    }

    void setEnabled(bool enabled) { _enabled = enabled; }
    unsigned int timestamp() const { return _timestamp; }

private:
    struct Registration {
        UpdateCallback* callback;
        bool removed = false;
    };

    std::list<Registration> _callbacks;
    unsigned int _timestamp = 0;
    bool _enabled = true;
};

UpdateCallbacks updateCallbacks;

} // namespace

InputEvent beginInteractiveUpdate()
{
    dispatchPlatformEvents();
    waitForPlatformFocus();
    runBackgroundUpdate();
    return windowDispatchMenu(inputSystem.take());
}

void runBackgroundUpdate()
{
    runUpdateCallbacks();
    inputSystem.sample();
}

void waitForGameTime(unsigned int durationMs, UpdateCallback* update)
{
    const auto start = getTicks();
    while (getTicksSince(start) < durationMs) {
        if (update != nullptr) {
            update();
        }
        delay_ms(1);
    }
}

void addUpdateCallback(UpdateCallback* callback)
{
    updateCallbacks.add(callback);
}

void removeUpdateCallback(UpdateCallback* callback)
{
    updateCallbacks.remove(callback);
}

void runUpdateCallbacks()
{
    updateCallbacks.run();
}

void setUpdateCallbacksEnabled(bool enabled)
{
    updateCallbacks.setEnabled(enabled);
}

void resetUpdateCallbacks()
{
    updateCallbacks.reset();
}

unsigned int lastBackgroundUpdateTime()
{
    return updateCallbacks.timestamp();
}

} // namespace fallout
