#ifndef FALLOUT_GAME_SPEED_H_
#define FALLOUT_GAME_SPEED_H_

namespace fallout {

// Gameplay clock. Remains continuous when speed or game mode changes.
unsigned int gameSpeedGetTicks();
int gameSpeedGetPercent();
void gameSpeedReset();

// Keep media playback and its surrounding transitions on real time.
class ScopedGameSpeedPause {
public:
    ScopedGameSpeedPause();
    ~ScopedGameSpeedPause();
    ScopedGameSpeedPause(const ScopedGameSpeedPause&) = delete;
    ScopedGameSpeedPause& operator=(const ScopedGameSpeedPause&) = delete;
};

} // namespace fallout

#endif /* FALLOUT_GAME_SPEED_H_ */
