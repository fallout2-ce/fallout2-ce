#ifndef ENDGAME_H
#define ENDGAME_H

namespace fallout {

enum EndgameDeathEndingReason : int {
    // Dude died.
    ENDGAME_DEATH_ENDING_REASON_DEATH = 0,

    // 13 years passed.
    ENDGAME_DEATH_ENDING_REASON_TIMEOUT = 2,
};

extern char _aEnglish_2[];

void endgamePlaySlideshow();
void endgamePlayMovie();
bool endgameShouldPlayMovieAfterSlideshow();
int endgameDeathEndingInit();
int endgameDeathEndingExit();
void endgameSetupDeathEnding(EndgameDeathEndingReason reason);
char* endgameDeathEndingGetFileName();

} // namespace fallout

#endif /* ENDGAME_H */
