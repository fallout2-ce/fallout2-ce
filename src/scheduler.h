#ifndef FALLOUT_SCHEDULER_H_
#define FALLOUT_SCHEDULER_H_

#include <chrono>
#include <functional>

namespace fallout {

// Owns the deadlines for game updates and frame presentation. A top-level
// scene loop can use those deadlines to dispatch update and render separately.
class Scheduler {
public:
    // Legacy loops call this after one update. Present while waiting for the
    // next update to become due. Time-based effects can supply a callback
    // to prepare the scene immediately before each presentation.
    void finishUpdate(const std::function<void()>& draw = { });

    // Pace an authored sequence that presents one frame per update.
    void finishDiscreteUpdate();
    void presentNow();

    // Evaluate a visual effect on presentation deadlines for a fixed duration.
    // The callback draws the state at the supplied progress, from 0 to 1.
    void animate(std::chrono::milliseconds duration, const std::function<void(double)>& draw);

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point updateDeadline();
    void presentUntil(Clock::time_point end, const std::function<void()>& draw);

    Clock::time_point _nextUpdate { };
    Clock::time_point _nextPresent { };
};

extern Scheduler scheduler;

} // namespace fallout

#endif
