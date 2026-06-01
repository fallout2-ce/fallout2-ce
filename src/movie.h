#ifndef MOVIE_H
#define MOVIE_H

#include "geometry.h"

namespace fallout {

typedef enum MovieFlags {
    MOVIE_FLAG_SCALE = 0x01,
    MOVIE_FLAG_DIRECT = 0x02,
    MOVIE_FLAG_DIRECT_CENTERED = 0x04,
    MOVIE_FLAG_SUBTITLES = 0x08,
} MovieFlags;

typedef enum MovieExtendedFlags {
    MOVIE_EXTENDED_FLAG_ERROR = 0x01,
    MOVIE_EXTENDED_FLAG_STOP_REQUESTED = 0x02,
    MOVIE_EXTENDED_FLAG_DIRECT = 0x04,
    MOVIE_EXTENDED_FLAG_CENTERED = 0x08,
    MOVIE_EXTENDED_FLAG_SUBTITLES = 0x10,
} MovieExtendedFlags;

typedef char* MovieBuildSubtitleFilePathProc(char* movieFilePath);
typedef void MovieSetPaletteEntriesProc(unsigned char* palette, int start, int end);
typedef void MovieSetPaletteProc(int frame);
typedef int(MovieBlitFunc)(int win, unsigned char* data, int width, int height, int pitch);

void movieInit();
void movieExit();
void _movieStop();
int movieSetFlags(int flags);
void _movieSetPaletteFunc(MovieSetPaletteEntriesProc* proc);
void movieSetPaletteProc(MovieSetPaletteProc* proc);
int _movieRun(int win, char* filePath);
int _movieRunRect(int win, char* filePath, int x, int y, int w, int h);
void movieSetBuildSubtitleFilePathProc(MovieBuildSubtitleFilePathProc* proc);
void movieSetVolume(int volume);
void _movieUpdate();
int _moviePlaying();
void movieHandleRendererReset();
void movieRenderDirectOverlay();

} // namespace fallout

#endif /* MOVIE_H */
