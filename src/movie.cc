#include "movie.h"

#include <SDL.h>
#include <string.h>
#include <vector>

#include "color.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "geometry.h"
#include "input.h"
#include "memory_manager.h"
#include "movie_effect.h"
#include "movie_lib.h"
#include "platform_compat.h"
#include "settings.h"
#include "sound.h"
#include "svga.h"
#include "text_font.h"
#include "window.h"
#include "window_manager.h"

namespace fallout {

typedef struct MovieSubtitleListNode {
    int num;
    char* text;
    struct MovieSubtitleListNode* next;
} MovieSubtitleListNode;

static SDL_Rect movieComputeDirectRect(int srcWidth, int srcHeight);
static void movieDirectOverlayDestroy();
static bool movieDirectOverlayEnsureTexture(int width, int height);
static void movieDirectOverlayUpload(unsigned char* pixels, int srcWidth, int srcHeight);
static void* movieMallocImpl(size_t size);
static void movieFreeImpl(void* ptr);
static bool movieReadImpl(void* handle, void* buf, int count);
static void movieDirectImpl(unsigned char* pixels, int src_width, int src_height, int, int, int, int, int, int);
static void movieBufferedImpl(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y);
static int _movieScaleSubRect(int win, unsigned char* data, int width, int height, int pitch);
static int _movieScaleWindow(int win, unsigned char* data, int width, int height, int pitch);
static int _blitNormal(int win, unsigned char* data, int width, int height, int pitch);
static void movieSetPaletteEntriesImpl(unsigned char* palette, int start, int end);
static void _cleanupMovie(bool shouldEndMovie);
static File* movieOpen(char* filePath);
static void movieLoadSubtitles(char* filePath);
static void movieRenderSubtitles();
static int _movieStart(int win, char* filePath);
static int _stepMovie();

// 0x5195B8 GNWWin
static int gMovieWindow = -1;

struct MovieDirectOverlay {
    SDL_Texture* texture = nullptr;
    int srcWidth = 0;
    int srcHeight = 0;
    SDL_Rect dstRect = { 0, 0, 0, 0 };
    bool active = false;
    std::vector<uint32_t> uploadBuffer;
};

static MovieDirectOverlay movieDirectOverlay;

static void movieDirectOverlayDestroy()
{
    if (movieDirectOverlay.texture != nullptr) {
        SDL_DestroyTexture(movieDirectOverlay.texture);
        movieDirectOverlay.texture = nullptr;
    }

    movieDirectOverlay.srcWidth = 0;
    movieDirectOverlay.srcHeight = 0;
    movieDirectOverlay.dstRect = { 0, 0, 0, 0 };
    movieDirectOverlay.active = false;
    movieDirectOverlay.uploadBuffer.clear();
}

static bool movieDirectOverlayEnsureTexture(int width, int height)
{
    if (movieDirectOverlay.texture != nullptr
        && movieDirectOverlay.srcWidth == width
        && movieDirectOverlay.srcHeight == height) {
        return true;
    }

    movieDirectOverlayDestroy();

    movieDirectOverlay.texture = SDL_CreateTexture(gSdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (movieDirectOverlay.texture == nullptr) {
        return false;
    }

    SDL_SetTextureBlendMode(movieDirectOverlay.texture, SDL_BLENDMODE_NONE);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(movieDirectOverlay.texture, SDL_ScaleModeNearest);
#endif

    movieDirectOverlay.srcWidth = width;
    movieDirectOverlay.srcHeight = height;
    return true;
}

static void movieDirectOverlayUpload(unsigned char* pixels, int srcWidth, int srcHeight)
{
    if (movieDirectOverlay.texture == nullptr || gSdlSurface == nullptr || gSdlSurface->format == nullptr || gSdlSurface->format->palette == nullptr) {
        return;
    }

    int width = srcWidth;
    int height = srcHeight;
    if (width <= 0 || height <= 0) {
        return;
    }

    int pixelCount = width * height;
    if (static_cast<int>(movieDirectOverlay.uploadBuffer.size()) < pixelCount) {
        movieDirectOverlay.uploadBuffer.resize(pixelCount);
    }

    SDL_Color* colors = gSdlSurface->format->palette->colors;
    for (int y = 0; y < height; y++) {
        const unsigned char* srcRow = pixels + y * srcWidth;
        uint32_t* dstRow = movieDirectOverlay.uploadBuffer.data() + y * width;
        for (int x = 0; x < width; x++) {
            SDL_Color color = colors[srcRow[x]];
            dstRow[x] = (static_cast<uint32_t>(color.a) << 24)
                | (static_cast<uint32_t>(color.r) << 16)
                | (static_cast<uint32_t>(color.g) << 8)
                | static_cast<uint32_t>(color.b);
        }
    }

    SDL_Rect rect = { 0, 0, width, height };
    SDL_UpdateTexture(movieDirectOverlay.texture, &rect, movieDirectOverlay.uploadBuffer.data(), width * static_cast<int>(sizeof(uint32_t)));
}

// 0x5195BC subtitleFont
static int gMovieSubtitlesFont = -1;

// 0x5195C0 showFrameFuncs
static MovieBlitFunc* gMovieBlitFuncs[2][2] = {
    {
        _blitNormal,
        _blitNormal,
    },
    {
        _movieScaleWindow,
        _movieScaleSubRect,
    },
};

// 0x5195E0 paletteFunc
static MovieSetPaletteEntriesProc* gMovieSetPaletteEntriesProc = _setSystemPaletteEntries;

// 0x5195E4 subtitleR
static int gMovieSubtitlesColorR = 31;

// 0x5195E8 subtitleG
static int gMovieSubtitlesColorG = 31;

// 0x5195EC subtitleB
static int gMovieSubtitlesColorB = 31;

// 0x638E10 mve_win_rect
static Rect gMovieWindowRect;

// 0x638E38 updateCallbackFunc
static MovieSetPaletteProc* gMoviePaletteProc;

// 0x638E40 subtitleFilenameFunc
static MovieBuildSubtitleFilePathProc* gMovieBuildSubtitleFilePathProc;

// 0x638E48 subtitleW
static int _subtitleW;

// 0x638E5C movieScaleFlag
static int _movieScaleFlag;

// similar to 0x638E64 lastMovieRect
static SDL_Rect movieOutputRect;

// 0x638E74 subtitleList
static MovieSubtitleListNode* gMovieSubtitleHead;

// 0x638E78 movieFlags
static unsigned int gMovieFlags;

// 0x638E80 movieSubRectFlag
static bool _movieSubRectFlag;

// 0x638E84 movieH
static int _movieH;

// 0x638E94 movieW
static int _movieW;

// 0x638EA0 subtitleH
static int _subtitleH;

// 0x638EA4 mve_running
static int _running;

// 0x638EA8 MVE_handle_file
static File* gMovieFileStream;

// 0x638EB0 movieX
static int _movieX;

// 0x638EB4 movieY
static int _movieY;

static SDL_Rect movieComputeDirectRect(int srcWidth, int srcHeight)
{
    int availableX = _movieX;
    int availableY = _movieY;
    int availableWidth = _movieW;
    int availableHeight = _movieH;
    bool centered = (gMovieFlags & MOVIE_EXTENDED_FLAG_CENTERED) != 0;

    if (!settings.ui.movie_aspect_fit) {
        if (!centered) {
            return {
                availableX,
                availableY,
                srcWidth,
                srcHeight,
            };
        }

        return {
            availableX + (availableWidth - srcWidth) / 2,
            availableY + (availableHeight - srcHeight) / 2,
            srcWidth,
            srcHeight,
        };
    }

    int subtitleReserve = 0;
    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_SUBTITLES) != 0) {
        subtitleReserve = _subtitleH + 4;
    }

    auto fitInto = [&](int fitHeight) {
        int movieWidth = availableWidth;
        int movieHeight = movieWidth * srcHeight / srcWidth;
        if (movieHeight > fitHeight) {
            movieHeight = fitHeight;
            movieWidth = fitHeight * srcWidth / srcHeight;
        }

        return SDL_Rect {
            centered ? availableX + (availableWidth - movieWidth) / 2 : availableX,
            centered ? availableY + (fitHeight - movieHeight) / 2 : availableY,
            movieWidth,
            movieHeight,
        };
    };

    SDL_Rect movieRect = fitInto(availableHeight);

    int marginHeight = (availableHeight - movieRect.h) / 2;
    if (subtitleReserve > 0 && marginHeight < subtitleReserve && availableHeight > subtitleReserve) {
        movieRect = fitInto(availableHeight - subtitleReserve);
    }

    return movieRect;
}

// 0x4865FC movieMalloc
static void* movieMallocImpl(size_t size)
{
    return internal_malloc_safe(size, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 209
}

// 0x486614 movieFree
static void movieFreeImpl(void* ptr)
{
    internal_free_safe(ptr, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 213
}

// 0x48662C movieRead
static bool movieReadImpl(void* handle, void* buf, int count)
{
    return fileRead(buf, 1, count, (File*)handle) == count;
}

// 0x486654 movie_MVE_ShowFrame
static void movieDirectImpl(unsigned char* pixels, int src_width, int src_height, int, int, int, int, int, int)
{
    if (gMovieWindow == -1) {
        return;
    }

    if (movieOutputRect.w == 0) {
        // We don't know the movie dimensions until we have the first frame, at which point we can compute the output rect
        movieOutputRect = movieComputeDirectRect(src_width, src_height);
        debugPrint("Movie output: x=%d, y=%d, w=%d, h=%d", movieOutputRect.x, movieOutputRect.y, movieOutputRect.w, movieOutputRect.h);
    }

    if (movieOutputRect.w == src_width && movieOutputRect.h == src_height) {
        movieDirectOverlay.active = false;
        _scr_blit(pixels, src_width, src_height, 0, 0, src_width, src_height, movieOutputRect.x, movieOutputRect.y);
    } else {
        // we're scaling, so use an SDL overlay to render
        if (!movieDirectOverlayEnsureTexture(src_width, src_height)) {
            gMovieFlags |= MOVIE_EXTENDED_FLAG_ERROR;
            return;
        }

        movieDirectOverlayUpload(pixels, src_width, src_height);
        movieDirectOverlay.dstRect = movieOutputRect;
        movieDirectOverlay.active = true;
    }
}

void movieRenderDirectOverlay()
{
    if (!movieDirectOverlay.active || movieDirectOverlay.texture == nullptr) {
        return;
    }

    SDL_RenderCopy(gSdlRenderer, movieDirectOverlay.texture, nullptr, &movieDirectOverlay.dstRect);
}

void movieHandleRendererReset()
{
    movieDirectOverlayDestroy();
}

// 0x486900 movieShowFrame
static void movieBufferedImpl(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y)
{
    if (gMovieWindow == -1) {
        return;
    }

    movieOutputRect = { src_x, src_y, dst_width, dst_height };

    MovieBlitFunc* func = gMovieBlitFuncs[_movieScaleFlag][_movieSubRectFlag];
    if (func(gMovieWindow, pixels, src_width, src_height, src_width) != 0) {
        Rect movieRect = { _movieX, _movieY, _movieX + _movieW, _movieY + _movieH };
        windowRefreshRect(gMovieWindow, &movieRect);
    }
}

// 0x486B68 movieScaleSubRect
int _movieScaleSubRect(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    unsigned char* windowBuffer = windowGetBuffer(win) + windowWidth * _movieY + _movieX;
    if (width * 4 / 3 > _movieW) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_ERROR;
        return 0;
    }

    int tripletCount = width / 3;
    for (int y = 0; y < height; y++) {
        int x;
        for (x = 0; x < tripletCount; x++) {
            unsigned int value = data[0];
            value |= data[1] << 8;
            value |= data[2] << 16;
            value |= data[2] << 24;

            *(unsigned int*)windowBuffer = value;

            windowBuffer += 4;
            data += 3;
        }

        for (x = x * 3; x < width; x++) {
            *windowBuffer++ = *data++;
        }

        data += pitch - width;
        windowBuffer += windowWidth - _movieW;
    }

    return 1;
}

// 0x486CD4 movieScaleWindow
int _movieScaleWindow(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    if (width != 3 * windowWidth / 4) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_ERROR;
        return 0;
    }

    unsigned char* windowBuffer = windowGetBuffer(win);
    for (int y = 0; y < height; y++) {
        int scaledWidth = width / 3;
        for (int x = 0; x < scaledWidth; x++) {
            unsigned int value = data[0];
            value |= data[1] << 8;
            value |= data[2] << 16;
            value |= data[3] << 24;

            *(unsigned int*)windowBuffer = value;

            windowBuffer += 4;
            data += 3;
        }
        data += pitch - width;
    }

    return 1;
}

// 0x486D84 blitNormal
int _blitNormal(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    unsigned char* windowBuffer = windowGetBuffer(win);
    _drawScaled(windowBuffer + windowWidth * _movieY + _movieX, _movieW, _movieH, windowWidth, data, width, height, pitch);
    return 1;
}

// 0x486DDC movieSetPalette
static void movieSetPaletteEntriesImpl(unsigned char* palette, int start, int end)
{
    if (end != 0) {
        gMovieSetPaletteEntriesProc(palette + start * 3, start, end + start - 1);
    }
}

// initMovie
// 0x486E0C initMovie
void movieInit()
{
    movieDirectOverlayDestroy();
    MveSetMemory(movieMallocImpl, movieFreeImpl);
    MveSetPalette(movieSetPaletteEntriesImpl);
    MveSetScreenSize(screenGetWidth(), screenGetHeight());
    MveSetIO(movieReadImpl);
}

// 0x486E98 cleanupMovie
static void _cleanupMovie(bool shouldEndMovie)
{
    if (!_running) {
        return;
    }

    int frame;
    int dropped;
    MVE_rmFrameCounts(&frame, &dropped);
    debugPrint("Frames %d, dropped %d\n", frame, dropped);

    if (shouldEndMovie) {
        MVE_rmEndMovie();
    }

    MVE_ReleaseMem();

    fileClose(gMovieFileStream);
    gMovieFileStream = nullptr;
    movieDirectOverlayDestroy();

    while (gMovieSubtitleHead != nullptr) {
        MovieSubtitleListNode* next = gMovieSubtitleHead->next;
        internal_free_safe(gMovieSubtitleHead->text, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 851
        internal_free_safe(gMovieSubtitleHead, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 852
        gMovieSubtitleHead = next;
    }

    _running = 0;
    _movieSubRectFlag = 0;
    _movieScaleFlag = 0;
    gMovieFlags = 0;
    gMovieWindow = -1;
}

// 0x48711C movieClose
void movieExit()
{
    _cleanupMovie(true);
}

// 0x487150 movieStop
void _movieStop()
{
    if (_running) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_STOP_REQUESTED;
    }
}

// 0x487164 movieSetFlags
int movieSetFlags(int flags)
{
    if ((flags & MOVIE_FLAG_DIRECT_CENTERED) != 0) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_DIRECT | MOVIE_EXTENDED_FLAG_CENTERED;
    } else {
        gMovieFlags &= ~MOVIE_EXTENDED_FLAG_CENTERED;
        if ((flags & MOVIE_FLAG_DIRECT) != 0) {
            gMovieFlags |= MOVIE_EXTENDED_FLAG_DIRECT;
        } else {
            gMovieFlags &= ~MOVIE_EXTENDED_FLAG_DIRECT;
        }
    }

    if ((flags & MOVIE_FLAG_SCALE) != 0) {
        _movieScaleFlag = 1;
    } else {
        _movieScaleFlag = 0;

        if ((gMovieFlags & MOVIE_EXTENDED_FLAG_DIRECT) == 0) {
            gMovieFlags &= ~MOVIE_EXTENDED_FLAG_CENTERED;
        }
    }

    if ((flags & MOVIE_FLAG_SUBTITLES) != 0) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_SUBTITLES;
    } else {
        gMovieFlags &= ~MOVIE_EXTENDED_FLAG_SUBTITLES;
    }

    return 0;
}

// 0x48725C movieSetPaletteFunc
void _movieSetPaletteFunc(MovieSetPaletteEntriesProc* proc)
{
    gMovieSetPaletteEntriesProc = proc != nullptr ? proc : _setSystemPaletteEntries;
}

// 0x487274 movieSetCallback
void movieSetPaletteProc(MovieSetPaletteProc* proc)
{
    gMoviePaletteProc = proc;
}

// 0x48731C openFile
static File* movieOpen(char* filePath)
{
    gMovieFileStream = fileOpen(filePath, "rb");
    if (gMovieFileStream == nullptr) {
        debugPrint("Couldn't find movie file %s\n", filePath);
        return nullptr;
    }
    return gMovieFileStream;
}

// 0x487380 openSubtitle
static void movieLoadSubtitles(char* filePath)
{
    _subtitleW = _movieW;
    _subtitleH = fontGetLineHeight() + 4;

    if (gMovieBuildSubtitleFilePathProc != nullptr) {
        filePath = gMovieBuildSubtitleFilePathProc(filePath);
    }

    char path[COMPAT_MAX_PATH];
    strcpy(path, filePath);

    debugPrint("Opening subtitle file %s\n", path);
    File* stream = fileOpen(path, "r");
    if (stream == nullptr) {
        debugPrint("Couldn't open subtitle file %s\n", path);
        gMovieFlags &= ~MOVIE_EXTENDED_FLAG_SUBTITLES;
        return;
    }

    MovieSubtitleListNode* prev = nullptr;
    int subtitleCount = 0;
    while (!fileEof(stream)) {
        char string[260];
        string[0] = '\0';
        fileReadString(string, 259, stream);
        if (*string == '\0') {
            break;
        }

        MovieSubtitleListNode* subtitle = (MovieSubtitleListNode*)internal_malloc_safe(sizeof(*subtitle), __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1050
        subtitle->next = nullptr;

        subtitleCount++;

        char* pch;

        pch = strchr(string, '\n');
        if (pch != nullptr) {
            *pch = '\0';
        }

        pch = strchr(string, '\r');
        if (pch != nullptr) {
            *pch = '\0';
        }

        pch = strchr(string, ':');
        if (pch != nullptr) {
            *pch = '\0';
            subtitle->num = atoi(string);
            subtitle->text = strdup_safe(pch + 1, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1058

            if (prev != nullptr) {
                prev->next = subtitle;
            } else {
                gMovieSubtitleHead = subtitle;
            }

            prev = subtitle;
        } else {
            debugPrint("subtitle: couldn't parse %s\n", string);
        }
    }

    fileClose(stream);

    debugPrint("Read %d subtitles\n", subtitleCount);
}

// 0x48755C doSubtitle
static void movieRenderSubtitles()
{
    if (gMovieSubtitleHead == nullptr) {
        return;
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_SUBTITLES) == 0) {
        return;
    }

    int subtitleX = _movieX;
    int subtitleW = _subtitleW;
    int movieY = _movieY;
    int movieHeight = _movieH;
    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_DIRECT) != 0 && movieOutputRect.w > 0 && movieOutputRect.h > 0) {
        subtitleX = movieOutputRect.x;
        subtitleW = movieOutputRect.w;
        movieY = movieOutputRect.y;
        movieHeight = movieOutputRect.h;
    }

    int lineHeight = fontGetLineHeight();
    int subtitleY = (windowGetHeight(gMovieWindow) - movieHeight - movieY - lineHeight) / 2 + movieHeight + movieY;

    int subtitleHeight = _subtitleH;
    int movieWindowHeight = windowGetHeight(gMovieWindow);
    if (subtitleHeight + subtitleY > movieWindowHeight) {
        subtitleHeight = movieWindowHeight - subtitleY;
    }
    if (subtitleHeight <= 0) {
        return;
    }

    int frame;
    int dropped;
    MVE_rmFrameCounts(&frame, &dropped);

    while (gMovieSubtitleHead != nullptr) {
        if (frame < gMovieSubtitleHead->num) {
            break;
        }

        MovieSubtitleListNode* next = gMovieSubtitleHead->next;

        windowFill(gMovieWindow, subtitleX, subtitleY, subtitleW, subtitleHeight, 0);

        int oldFont;
        if (gMovieSubtitlesFont != -1) {
            oldFont = fontGetCurrent();
            fontSetCurrent(gMovieSubtitlesFont);
        }

        int colorIndex = (gMovieSubtitlesColorR << 10) | (gMovieSubtitlesColorG << 5) | gMovieSubtitlesColorB;
        windowWrapLine(gMovieWindow, gMovieSubtitleHead->text, subtitleW, subtitleHeight, subtitleX, subtitleY, _colorTable[colorIndex] | 0x2000000, TEXT_ALIGNMENT_CENTER);

        Rect rect;
        rect.right = subtitleX + subtitleW - 1;
        rect.top = subtitleY;
        rect.bottom = subtitleY + subtitleHeight - 1;
        rect.left = subtitleX;
        windowRefreshRect(gMovieWindow, &rect);

        internal_free_safe(gMovieSubtitleHead->text, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1108
        internal_free_safe(gMovieSubtitleHead, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1109

        gMovieSubtitleHead = next;

        if (gMovieSubtitlesFont != -1) {
            fontSetCurrent(oldFont);
        }
    }
}

// 0x487710 movieStart
static int _movieStart(int win, char* filePath)
{
    if (_running) {
        return 1;
    }

    gMovieFileStream = movieOpen(filePath);
    if (gMovieFileStream == nullptr) {
        return 1;
    }

    gMovieWindow = win;
    _running = 1;
    gMovieFlags &= ~MOVIE_EXTENDED_FLAG_ERROR;
    movieOutputRect = { 0, 0, 0, 0 };

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_SUBTITLES) != 0) {
        movieLoadSubtitles(filePath);
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_DIRECT) != 0) {
        debugPrint("Direct ");
        windowGetRect(gMovieWindow, &gMovieWindowRect);
        debugPrint("Playing at (%d, %d)  ", _movieX + gMovieWindowRect.left, _movieY + gMovieWindowRect.top);
        MveSetShowFrame(movieDirectImpl);
        MVE_rmPrepMovie(gMovieFileStream, _movieX + gMovieWindowRect.left, _movieY + gMovieWindowRect.top, 0);
    } else {
        debugPrint("Buffered ");
        MveSetShowFrame(movieBufferedImpl);
        MVE_rmPrepMovie(gMovieFileStream, 0, 0, 0);
    }

    if (_movieScaleFlag) {
        debugPrint("scaled\n");
    } else {
        debugPrint("not scaled\n");
    }

    return 0;
}

// 0x487AC8 movieRun
int _movieRun(int win, char* filePath)
{
    if (_running) {
        return 1;
    }

    _movieX = 0;
    _movieY = 0;
    _movieW = windowGetWidth(win);
    _movieH = windowGetHeight(win);
    _movieSubRectFlag = 0;
    return _movieStart(win, filePath);
}

// 0x487B1C movieRunRect
int _movieRunRect(int win, char* filePath, int x, int y, int w, int h)
{
    if (_running) {
        return 1;
    }

    _movieX = x;
    _movieY = y;
    _movieW = w;
    _movieH = h;
    _movieSubRectFlag = 1;

    return _movieStart(win, filePath);
}

// 0x487B7C stepMovie
static int _stepMovie()
{
    int stepResult = _MVE_rmStepMovie();
    if (stepResult != -1) {
        movieRenderSubtitles();
        renderPresent();
    }

    return stepResult;
}

// 0x487BC8 movieSetSubtitleFunc
void movieSetBuildSubtitleFilePathProc(MovieBuildSubtitleFilePathProc* proc)
{
    gMovieBuildSubtitleFilePathProc = proc;
}

// 0x487BD0 movieSetVolume
void movieSetVolume(int volume)
{
    int normalizedVolume = _soundVolumeHMItoDirectSound(volume);
    MveSetVolume(normalizedVolume);
}

// 0x487BEC movieUpdate
void _movieUpdate()
{
    if (!_running) {
        return;
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_STOP_REQUESTED) != 0) {
        debugPrint("Movie aborted\n");
        _cleanupMovie(true);
        return;
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_ERROR) != 0) {
        debugPrint("Movie error\n");
        _cleanupMovie(true);
        return;
    }

    if (_stepMovie() == -1) {
        _cleanupMovie(true);
        return;
    }

    if (gMoviePaletteProc != nullptr) {
        int frame;
        int dropped;
        MVE_rmFrameCounts(&frame, &dropped);
        gMoviePaletteProc(frame);
    }
}

// 0x487C88 moviePlaying
int _moviePlaying()
{
    return _running;
}

} // namespace fallout
