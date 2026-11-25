#include "game_update.h"

#include <list>
#include <utility>

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
                if (entry.callback == callback && !entry.removed) {
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
            Pass pass(_callbacks.begin(), _activePass);
            while (pass.next != _callbacks.end()) {
                // Additions go to the front and start on the next pass.
                const auto entry = pass.next++;
                if (entry->removed) {
                    // A nested pass can remove the next entry of a suspended pass.
                    for (auto* outer = pass.outer; outer != nullptr; outer = outer->outer) {
                        if (outer->next == entry) {
                            ++outer->next;
                        }
                    }
                    _callbacks.erase(entry);
                } else {
                    entry->callback();
                }
            }
        }

        void reset()
        {
            for (auto* pass = _activePass; pass != nullptr; pass = pass->outer) {
                pass->next = _callbacks.end();
            }
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

        struct Pass {
            Pass(std::list<Registration>::iterator first, Pass*& active)
                : next(first)
                , outer(std::exchange(active, this))
                , _active(active)
            {
            }

            ~Pass() { _active = outer; }

            std::list<Registration>::iterator next;
            Pass* outer;

        private:
            Pass*& _active;
        };

        std::list<Registration> _callbacks;
        Pass* _activePass = nullptr;
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
