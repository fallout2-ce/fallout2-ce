#ifndef GAME_MOVIE_H
#define GAME_MOVIE_H

#include "db.h"

namespace fallout {

enum class GameMovieFlags : unsigned short {
    None = 0,
    FadeIn = 0x01,
    FadeOut = 0x02,
    StopMusic = 0x04,
    PauseMusic = 0x08,
};

constexpr inline GameMovieFlags operator&(GameMovieFlags lhs, GameMovieFlags rhs)
{
    return static_cast<GameMovieFlags>(static_cast<unsigned short>(lhs) & static_cast<unsigned short>(rhs));
}

constexpr inline GameMovieFlags operator|(GameMovieFlags lhs, GameMovieFlags rhs)
{
    return static_cast<GameMovieFlags>((static_cast<unsigned short>(lhs) | static_cast<unsigned short>(rhs)));
}

enum GameMovie : int {
    MOVIE_INVALID = -1,
    MOVIE_IPLOGO,
    MOVIE_INTRO,
    MOVIE_ELDER,
    MOVIE_VSUIT,
    MOVIE_AFAILED,
    MOVIE_ADESTROY,
    MOVIE_CAR,
    MOVIE_CARTUCCI,
    MOVIE_TIMEOUT,
    MOVIE_TANKER,
    MOVIE_ENCLAVE,
    MOVIE_DERRICK,
    MOVIE_ARTIMER1,
    MOVIE_ARTIMER2,
    MOVIE_ARTIMER3,
    MOVIE_ARTIMER4,
    MOVIE_CREDITS,
    MOVIE_COUNT,
    MOVIE_FIRST = MOVIE_IPLOGO
};

inline GameMovie operator++(GameMovie& e, int)
{
    GameMovie result = e;
    e = static_cast<GameMovie>(static_cast<int>(e) + 1);
    return result;
}

constexpr int GAME_MOVIE_MAX_COUNT = 32;

constexpr inline bool gameMovieIsValid(int movie)
{
    return movie >= MOVIE_FIRST && movie < MOVIE_COUNT;
}

constexpr inline bool gameMovieExtendedIsValid(int movie)
{
    return movie >= MOVIE_FIRST && movie < GAME_MOVIE_MAX_COUNT;
}

int gameMoviesInit();
void gameMoviesReset();
int gameMoviesLoad(File* stream);
int gameMoviesSave(File* stream);
int gameMoviePlay(GameMovie movie, GameMovieFlags flags);
const char* gameMovieGetDefaultFileName(GameMovie movie);
bool gameMovieSetPath(GameMovie movie, const char* fileName);
void gameMovieMarkSeen(GameMovie movie);
void gameMovieFadeOut();
bool gameMovieIsSeen(GameMovie movie);
bool gameMovieIsPlaying();

} // namespace fallout

#endif /* GAME_MOVIE_H */
