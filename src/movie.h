#ifndef MOVIE_H
#define MOVIE_H

#include "geometry.h"

namespace fallout {

enum class MovieFlags : unsigned int {
    None = 0x00,
    Scale = 0x01,
    Direct = 0x02,
    DirectCentered = 0x04,
    Subtitles = 0x08,
};

constexpr inline MovieFlags operator&(MovieFlags lhs, MovieFlags rhs)
{
    return static_cast<MovieFlags>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline MovieFlags operator|(MovieFlags lhs, MovieFlags rhs)
{
    return static_cast<MovieFlags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

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
int movieSetFlags(MovieFlags flags);
void _movieSetPaletteFunc(MovieSetPaletteEntriesProc* proc);
void movieSetPaletteProc(MovieSetPaletteProc* proc);
int _movieRun(int win, char* filePath);
int _movieRunRect(int win, char* filePath, int x, int y, int w, int h);
void movieSetBuildSubtitleFilePathProc(MovieBuildSubtitleFilePathProc* proc);
void movieSetVolume(int volume);
void _movieUpdate();
int _moviePlaying();
void movieHandleRendererReset();
void movieHandleFocusGained();
void movieRenderDirectOverlay();

} // namespace fallout

#endif /* MOVIE_H */
