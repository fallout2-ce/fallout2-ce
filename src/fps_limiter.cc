#include "fps_limiter.h"

#include <SDL.h>

#include <algorithm>

#include "game_speed.h"

namespace fallout {

FpsLimiter::FpsLimiter(unsigned int fps)
    : _fps(fps)
    , _ticks(0)
{
}

void FpsLimiter::mark()
{
    _ticks = SDL_GetTicks();
}

void FpsLimiter::throttle() const
{
    // Allow accelerated animations to advance more often, but keep input and
    // rendering responsive during slow motion. Travel has its own scaled timer.
    unsigned int frameDuration = 100000 / (_fps * std::max(100, gameSpeedGetPercent()));
    unsigned int elapsed = SDL_GetTicks() - _ticks;
    if (frameDuration > elapsed) {
        SDL_Delay(frameDuration - elapsed);
    }
}

} // namespace fallout
