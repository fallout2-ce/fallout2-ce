#ifndef MOVIE_H
#define MOVIE_H

#include "geometry.h"

namespace fallout {

typedef enum MovieFlags {
    MOVIE_FLAG_ENABLE_SUBTITLES    = 0x08, // triggers subtitle loading
} MovieFlags;

typedef enum MovieExtendedFlags {
    MOVIE_EXTENDED_FLAG_PLAYING              = 0x01, // marks that a movie is currently playing (?)
    MOVIE_EXTENDED_FLAG_SUBTITLES_LOADED     = 0x02, // internal state: subtitles were loaded
    MOVIE_EXTENDED_FLAG_SUBTITLES_ENABLED    = 0x10, // external request: subtitles should be loaded
} MovieExtendedFlags;

typedef char* MovieBuildSubtitleFilePathProc(char* movieFilePath);
typedef void MovieSetPaletteEntriesProc(unsigned char* palette, int start, int end);
typedef void MovieSetPaletteProc(int frame);

void movieInit();
void movieExit();
void _movieStop();
int movieSetFlags(int a1);
void _movieSetPaletteFunc(MovieSetPaletteEntriesProc* proc);
void movieSetPaletteProc(MovieSetPaletteProc* proc);
int _movieRun(int win, char* filePath);
int _movieRunRect(int win, char* filePath, int a3, int a4, int a5, int a6);
void movieSetBuildSubtitleFilePathProc(MovieBuildSubtitleFilePathProc* proc);
void movieSetVolume(int volume);
void _movieUpdate();
int _moviePlaying();

void readMovieSettings();
int movieGetSizeFlag();

} // namespace fallout

#endif /* MOVIE_H */
