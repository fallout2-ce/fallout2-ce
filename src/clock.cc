#include "clock.h"

#include <SDL.h>

#include <climits>

#include "game_speed.h"

namespace fallout {

// 0x4C9370
unsigned int getTicks()
{
    return gameSpeedGetTicks();
}

unsigned int getRealTicks()
{
    return SDL_GetTicks();
}

// 0x4C93E0
unsigned int getTicksSince(unsigned int start)
{
    return getTicksBetween(getTicks(), start);
}

// 0x4C9400
unsigned int getTicksBetween(unsigned int end, unsigned int start)
{
    if (start > end) {
        return INT_MAX;
    }
    return end - start;
}

} // namespace fallout
