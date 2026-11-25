#include "scheduler.h"

#include <algorithm>
#include <thread>

#include "game_speed.h"
#include "mouse.h"
#include "settings.h"
#include "svga.h"

namespace fallout {

namespace {

    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    constexpr int kUpdateHz = 60;
    constexpr int kMinPresentationFps = 60;
    constexpr int kMaxPresentationFps = 1000;

    auto presentationPeriod()
    {
        const int requestedFps = settings.screen.max_fps == 0 ? screenGetRefreshRate() : settings.screen.max_fps;
        const int fps = std::clamp(requestedFps, kMinPresentationFps, kMaxPresentationFps);
        return std::chrono::duration_cast<Clock::duration>(1s) / fps;
    }

    void waitUntil(Clock::time_point deadline)
    {
#ifdef __EMSCRIPTEN__
        const auto delay = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
        SDL_Delay(static_cast<Uint32>(std::max(delay, 1ms).count()));
#else
        std::this_thread::sleep_until(deadline);
#endif
    }

} // namespace

Scheduler scheduler;

Scheduler::Clock::time_point Scheduler::updateDeadline()
{
    // Keep the nominal 60 Hz rate and allow accelerated gameplay to update more often.
    const auto interval = std::chrono::duration_cast<Clock::duration>(100s)
        / (kUpdateHz * std::max(100, gameSpeedGetPercent()));
    const auto deadline = std::max(Clock::now(), _nextUpdate);
    // A slow update or a return from a modal screen starts a fresh interval.
    _nextUpdate = deadline + interval;
    return deadline;
}

void Scheduler::finishUpdate(const std::function<void()>& draw)
{
    presentUntil(updateDeadline(), draw);
}

void Scheduler::finishDiscreteUpdate()
{
    waitUntil(updateDeadline());
    presentNow();
}

void Scheduler::presentNow()
{
    renderFrame(mouseGetCursorDrawPosition());
    _nextPresent = Clock::now() + presentationPeriod();
}

void Scheduler::presentFor(std::chrono::milliseconds duration)
{
    presentUntil(Clock::now() + duration, {});
}

void Scheduler::presentUntil(Clock::time_point end, const std::function<void()>& draw)
{
    const auto period = presentationPeriod();
    auto now = Clock::now();
    if (_nextPresent == Clock::time_point {}) {
        _nextPresent = now;
    }

    bool yielded = false;
    for (;;) {
        if (now >= _nextPresent) {
            if (draw) {
                draw();
            }
            const auto cursorPosition = mouseSampleCursorDrawPosition();
            renderFrame(cursorPosition);
            now = Clock::now();
            // Keep phase even when a render is late. Missed frames have no work to replay.
            _nextPresent = now + period - (now - _nextPresent) % period;
        }
        if (now >= end) {
            break;
        }
        waitUntil(std::min(end, _nextPresent));
        yielded = true;
        now = Clock::now();
    }

#ifdef __EMSCRIPTEN__
    if (!yielded) {
        SDL_Delay(1);
    }
#else
    (void)yielded;
#endif
}

void Scheduler::animate(std::chrono::milliseconds duration, const std::function<void(double)>& draw)
{
    double progress = 0.0;
    if (duration.count() > 0) {
        const auto start = Clock::now();
        presentUntil(start + duration, [&] {
            const auto elapsed = std::chrono::duration<double>(Clock::now() - start);
            progress = std::min(1.0, elapsed / duration);
            draw(progress);
        });
    }

    // The final state is visible before the calling scene continues.
    if (progress < 1.0) {
        draw(1.0);
        presentNow();
    }
}

} // namespace fallout
