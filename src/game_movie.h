#ifndef GAME_MOVIE_H
#define GAME_MOVIE_H

#include "db.h"

namespace fallout {

enum GameMovieFlags : unsigned short {
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

typedef enum GameMovie {
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
} GameMovie;

constexpr int GAME_MOVIE_MAX_COUNT = 32;

int gameMoviesInit();
void gameMoviesReset();
int gameMoviesLoad(File* stream);
int gameMoviesSave(File* stream);
int gameMoviePlay(int movie, GameMovieFlags flags);
const char* gameMovieGetDefaultFileName(int movie);
bool gameMovieSetPath(int movie, const char* fileName);
void gameMovieMarkSeen(int movie);
void gameMovieFadeOut();
bool gameMovieIsSeen(int movie);
bool gameMovieIsPlaying();

} // namespace fallout

#endif /* GAME_MOVIE_H */
