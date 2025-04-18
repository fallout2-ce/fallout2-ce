#include "movie.h"

#include <string.h>

#include <SDL.h>

#include "color.h"
#include "config.h" // add for video stretching config
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

// Added for movie stretching
#define MOVIE_SIZE_ASPECT 1
#define MOVIE_SIZE_FULLSCREEN 2

namespace fallout {

typedef struct MovieSubtitleListNode {
    int num;
    char* text;
    struct MovieSubtitleListNode* next;
} MovieSubtitleListNode;

static void* movieMallocImpl(size_t size);
static void movieFreeImpl(void* ptr);
static bool movieReadImpl(void* handle, void* buf, int count);
static void movieDirectImpl(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y);
static int _movieScaleSubRect(int win, unsigned char* data, int width, int height, int pitch);
static int _movieScaleSubRectAlpha(int win, unsigned char* data, int width, int height, int pitch);
static int _movieScaleWindowAlpha(int win, unsigned char* data, int width, int height, int pitch);
static int _blitAlpha(int win, unsigned char* data, int width, int height, int pitch);
static int _movieScaleWindow(int win, unsigned char* data, int width, int height, int pitch);
static int _blitNormal(int win, unsigned char* data, int width, int height, int pitch);
static void movieSetPaletteEntriesImpl(unsigned char* palette, int start, int end);
static void _cleanupMovie(int a1);
static void _cleanupLast();
static File* movieOpen(char* filePath);
static void movieLoadSubtitles(char* filePath);
static void movieRenderSubtitles();
static int _movieStart(int win, char* filePath);
static bool _localMovieCallback();
static int _stepMovie();

// Added for movie stretching
static bool _movieHasSubtitles = false;

// 0x5195B8
static int gMovieWindow = -1;

// 0x5195BC
static int gMovieSubtitlesFont = -1;

// 0x5195E0
static MovieSetPaletteEntriesProc* gMovieSetPaletteEntriesProc = _setSystemPaletteEntries;

// 0x5195E4
static int gMovieSubtitlesColorR = 31;

// 0x5195E8
static int gMovieSubtitlesColorG = 31;

// 0x5195EC
static int gMovieSubtitlesColorB = 31;

// 0x638E10
static Rect gMovieWindowRect;

// 0x638E20
static Rect _movieRect;

// 0x638E30
static void (*_movieCallback)();

// 0x638E38
static MovieSetPaletteProc* gMoviePaletteProc;

// 0x638E40
static MovieBuildSubtitleFilePathProc* gMovieBuildSubtitleFilePathProc;

// 0x638E48
static int _subtitleW;

// 0x638E4C
static int _lastMovieBH;

// 0x638E50
static int _lastMovieBW;

// 0x638E54
static int _lastMovieSX;

// 0x638E58
static int _lastMovieSY;

// 0x638E64
static int _lastMovieH;

// 0x638E68
static int _lastMovieW;

// 0x638E6C
static int _lastMovieX;

// 0x638E70
static int _lastMovieY;

// 0x638E74
static MovieSubtitleListNode* gMovieSubtitleHead;

// 0x638E78
static unsigned int gMovieFlags;

// 0x638E7C
static int _movieAlphaFlag;

// 0x638E80
static bool _movieSubRectFlag;

// 0x638E84
static int _movieH;

// 0x638E88
static int _movieOffset;

// 0x638E90
static unsigned char* _lastMovieBuffer;

// 0x638E94
static int _movieW;

// 0x638EA0
static int _subtitleH;

// 0x638EA4
static int _running;

// 0x638EA8
static File* gMovieFileStream;

// 0x638EAC
static unsigned char* _alphaWindowBuf;

// 0x638EB0
static int _movieX;

// 0x638EB4
static int _movieY;

// 0x638EBC
static File* _alphaHandle;

// 0x638EC0
static unsigned char* _alphaBuf;

static unsigned char* MVE_lastBuffer = nullptr;

// Added for movie stretching
static int _movieSizeFlag = 0; // private to movie.cc

// Added for movie stretching
void readMovieSettings()
{
    int size = 0;
    Config config;
    if (configInit(&config)) {
        if (configRead(&config, "f2_res.ini", false)) {
            configGetInt(&config, "STATIC_SCREENS", "MOVIE_SIZE", &size);
        }
        configFree(&config);
    }

    _movieSizeFlag = size;
}

// Added for movie stretching
int movieGetSizeFlag()
{
    return _movieSizeFlag;
}

// 0x4865FC
static void* movieMallocImpl(size_t size)
{
    return internal_malloc_safe(size, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 209
}

// 0x486614
static void movieFreeImpl(void* ptr)
{
    internal_free_safe(ptr, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 213
}

// 0x48662C
static bool movieReadImpl(void* handle, void* buf, int count)
{
    return fileRead(buf, 1, count, (File*)handle) == count;
}

// 0x486654
// Renders the movie frame directly to the screen, with optional scaling and subtitle margins.
static void movieDirectImpl(
    unsigned char* pixels, int srcWidth, int srcHeight,
    int srcX, int srcY, int dstWidth, int dstHeight,
    int dstX, int dstY)
{
    int windowWidth = screenGetWidth();
    int windowHeight = screenGetHeight();

    int finalWidth = srcWidth;
    int finalHeight = srcHeight;
    int sizeMode = movieGetSizeFlag(); // Gets scaling mode (original, aspect, fullscreen) from f2_res.ini
    bool subtitlesEnabled = settings.preferences.subtitles;

    // Figure out if we should reserve vertical margin for subtitles
    float subtitleMarginRatio = (_movieHasSubtitles && subtitlesEnabled) ? 0.15f : 0.0f;

    // Adjust final size based on scaling mode
    if (sizeMode == MOVIE_SIZE_ASPECT) {
        // Reserve margin for subtitles even in aspect mode (margin may be zero)
        float availableHeight = (float)windowHeight * (1.0f - subtitleMarginRatio);
        float scale = fminf((float)windowWidth / srcWidth, availableHeight / srcHeight);
        finalWidth = (int)(srcWidth * scale);
        finalHeight = (int)(srcHeight * scale);
    } else if (sizeMode == MOVIE_SIZE_FULLSCREEN) {
        // Fullscreen with margin for subtitles (margin may be zero)
        finalWidth = windowWidth;
        finalHeight = (int)(windowHeight * (1.0f - subtitleMarginRatio));
    }


    // buffer for rescaled image
    unsigned char* scaledFrame = (unsigned char*)internal_malloc_safe(finalWidth * finalHeight, __FILE__, __LINE__);

    // Nearest-neighbor scaling using fixed-point math
    const int fpShift = 16;
    int xStep = (srcWidth << fpShift) / finalWidth;
    int yStep = (srcHeight << fpShift) / finalHeight;

    for (int y = 0, ySrcFP = 0; y < finalHeight; y++, ySrcFP += yStep) {
        int srcY = ySrcFP >> fpShift;
        if (srcY >= srcHeight) srcY = srcHeight - 1;

        for (int x = 0, xSrcFP = 0; x < finalWidth; x++, xSrcFP += xStep) {
            int srcX = xSrcFP >> fpShift;
            if (srcX >= srcWidth) srcX = srcWidth - 1;

            scaledFrame[y * finalWidth + x] = pixels[srcY * srcWidth + srcX];
        }
    }

    // Determine blit position on screen
    int posX = (windowWidth - finalWidth) / 2;
    int posY = (windowHeight - finalHeight) / 2;

    if (sizeMode == MOVIE_SIZE_FULLSCREEN) {
        // Fullscreen movies are normally top-left aligned unless subtitles are shown
        if (_movieHasSubtitles) {
            posY = (windowHeight - finalHeight) / 2; // center vertically with margin
        } else {
            posY = 0;
        }
        posX = 0;
    }

    // Blit scaled frame to screen at final position
    _scr_blit(
        scaledFrame, finalWidth, finalHeight,
        0, 0, finalWidth, finalHeight,
        posX, posY);

    renderPresent();
    internal_free_safe(scaledFrame, __FILE__, __LINE__);

    // Store last frame properties (used elsewhere for subtitles/positioning)
    _lastMovieSX = srcX;
        _lastMovieSY = srcY;
        _lastMovieX = posX;
        _lastMovieY = posY;
        _lastMovieBH = srcHeight;
        _lastMovieBW = srcWidth;
        _lastMovieW = finalWidth;
        _lastMovieH = finalHeight;
        MVE_lastBuffer = pixels;
}

// 0x486B68
int _movieScaleSubRect(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    unsigned char* windowBuffer = windowGetBuffer(win) + windowWidth * _movieY + _movieX;
    if (width * 4 / 3 > _movieW) {
        gMovieFlags |= 0x01;
        return 0;
    }

    int v1 = width / 3;
    for (int y = 0; y < height; y++) {
        int x;
        for (x = 0; x < v1; x++) {
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

// 0x486C74
int _movieScaleSubRectAlpha(int win, unsigned char* data, int width, int height, int pitch)
{
    gMovieFlags |= 1;
    return 0;
}

// NOTE: Uncollapsed 0x486C74.
int _movieScaleWindowAlpha(int win, unsigned char* data, int width, int height, int pitch)
{
    gMovieFlags |= 1;
    return 0;
}

// 0x486C80
int _blitAlpha(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    unsigned char* windowBuffer = windowGetBuffer(win);
    _alphaBltBuf(data, width, height, pitch, _alphaWindowBuf, _alphaBuf, windowBuffer + windowWidth * _movieY + _movieX, windowWidth);
    return 1;
}

// 0x486CD4
int _movieScaleWindow(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    if (width != 3 * windowWidth / 4) {
        gMovieFlags |= 1;
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

// 0x486D84
int _blitNormal(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = windowGetWidth(win);
    unsigned char* windowBuffer = windowGetBuffer(win);
    _drawScaled(windowBuffer + windowWidth * _movieY + _movieX, _movieW, _movieH, windowWidth, data, width, height, pitch);
    return 1;
}

// 0x486DDC
static void movieSetPaletteEntriesImpl(unsigned char* palette, int start, int end)
{
    if (end != 0) {
        gMovieSetPaletteEntriesProc(palette + start * 3, start, end + start - 1);
    }
}

// initMovie
// 0x486E0C
void movieInit()
{
    MveSetMemory(movieMallocImpl, movieFreeImpl);
    MveSetPalette(movieSetPaletteEntriesImpl);
    MveSetScreenSize(screenGetWidth(), screenGetHeight());
    MveSetIO(movieReadImpl);
}

// 0x486E98
static void _cleanupMovie(int a1)
{
    if (!_running) {
        return;
    }

    int frame;
    int dropped;
    MVE_rmFrameCounts(&frame, &dropped);
    debugPrint("Frames %d, dropped %d\n", frame, dropped);

    if (_lastMovieBuffer != nullptr) {
        internal_free_safe(_lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 787
        _lastMovieBuffer = nullptr;
    }

    if (MVE_lastBuffer != nullptr) {
        _lastMovieBuffer = (unsigned char*)internal_malloc_safe(_lastMovieBH * _lastMovieBW, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 802
        memcpy(_lastMovieBuffer, MVE_lastBuffer, _lastMovieBW * _lastMovieBH);
        MVE_lastBuffer = nullptr;
    }

    if (a1) {
        MVE_rmEndMovie();
    }

    MVE_ReleaseMem();

    fileClose(gMovieFileStream);

    if (_alphaWindowBuf != nullptr) {
        blitBufferToBuffer(_alphaWindowBuf, _movieW, _movieH, _movieW, windowGetBuffer(gMovieWindow) + _movieY * windowGetWidth(gMovieWindow) + _movieX, windowGetWidth(gMovieWindow));
        windowRefreshRect(gMovieWindow, &_movieRect);
    }

    if (_alphaHandle != nullptr) {
        fileClose(_alphaHandle);
        _alphaHandle = nullptr;
    }

    if (_alphaBuf != nullptr) {
        internal_free_safe(_alphaBuf, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 840
        _alphaBuf = nullptr;
    }

    if (_alphaWindowBuf != nullptr) {
        internal_free_safe(_alphaWindowBuf, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 845
        _alphaWindowBuf = nullptr;
    }

    while (gMovieSubtitleHead != nullptr) {
        MovieSubtitleListNode* next = gMovieSubtitleHead->next;
        internal_free_safe(gMovieSubtitleHead->text, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 851
        internal_free_safe(gMovieSubtitleHead, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 852
        gMovieSubtitleHead = next;
    }

    _running = 0;
    _movieSubRectFlag = 0;
    _movieAlphaFlag = 0;
    gMovieFlags = 0;
    gMovieWindow = -1;
}

// 0x48711C
void movieExit()
{
    _cleanupMovie(1);

    if (_lastMovieBuffer) {
        internal_free_safe(_lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 869
        _lastMovieBuffer = nullptr;
    }
}

// 0x487150
void _movieStop()
{
    if (_running) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_SUBTITLES_LOADED;
    }
}

// 0x487164
int movieSetFlags(int flags)
{
    // MOVIE_FLAG_0x04 and MOVIE_FLAG_0x02 no longer needed
    // Stripped this down to critical only
    if ((flags & MOVIE_FLAG_ENABLE_SUBTITLES) != 0) {
        gMovieFlags |= MOVIE_EXTENDED_FLAG_SUBTITLES_ENABLED;
    } else {
        gMovieFlags &= ~MOVIE_EXTENDED_FLAG_SUBTITLES_ENABLED;
    }

    return 0;
}


// 0x48725C
void _movieSetPaletteFunc(MovieSetPaletteEntriesProc* proc)
{
    gMovieSetPaletteEntriesProc = proc != nullptr ? proc : _setSystemPaletteEntries;
}

// 0x487274
void movieSetPaletteProc(MovieSetPaletteProc* proc)
{
    gMoviePaletteProc = proc;
}

// 0x4872E8
static void _cleanupLast()
{
    if (_lastMovieBuffer != nullptr) {
        internal_free_safe(_lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 981
        _lastMovieBuffer = nullptr;
    }

    MVE_lastBuffer = nullptr;
}

// 0x48731C
static File* movieOpen(char* filePath)
{
    gMovieFileStream = fileOpen(filePath, "rb");
    if (gMovieFileStream == nullptr) {
        debugPrint("Couldn't find movie file %s\n", filePath);
        return nullptr;
    }
    return gMovieFileStream;
}

// 0x487380
static void movieLoadSubtitles(char* filePath)
{
    _subtitleW = windowGetWidth(gMovieWindow);
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
        gMovieFlags &= ~MOVIE_EXTENDED_FLAG_SUBTITLES_ENABLED;
        return;
    }
    
    _movieHasSubtitles = true;
    
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

// 0x48755C
static void movieRenderSubtitles()
{
    if (gMovieSubtitleHead == nullptr) {
        return;
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_SUBTITLES_ENABLED) == 0) {
        return;
    }

    int frame;
    int dropped;
    MVE_rmFrameCounts(&frame, &dropped);

    // Height of subtitle text
    int lineHeight = fontGetLineHeight();

    // Total subtitle box height
    int subtitleHeight = _subtitleH;

    // Figure out Y position: place subtitle box centered in space below the video
    int subtitleY = _lastMovieY + _lastMovieH + ((screenGetHeight() - (_lastMovieY + _lastMovieH)) - subtitleHeight) / 2;

    // If the subtitle box would go off-screen, nudge it up
    if (subtitleY + subtitleHeight > screenGetHeight()) {
        subtitleY = screenGetHeight() - subtitleHeight;
    }

    // Don't allow subtitles to overlap the video itself
    if (subtitleY < _lastMovieY + _lastMovieH) {
        subtitleY = _lastMovieY + _lastMovieH;
    }

    while (gMovieSubtitleHead != nullptr) {
        if (frame < gMovieSubtitleHead->num) {
            break;
        }

        MovieSubtitleListNode* next = gMovieSubtitleHead->next;

        // Clear the subtitle region before drawing
        windowFill(gMovieWindow, 0, subtitleY, _subtitleW, subtitleHeight, 0);

        int previousFont;
        if (gMovieSubtitlesFont != -1) {
            previousFont = fontGetCurrent();
            fontSetCurrent(gMovieSubtitlesFont);
        }

        int colorIndex = (gMovieSubtitlesColorR << 10) | (gMovieSubtitlesColorG << 5) | gMovieSubtitlesColorB;

        // Render subtitle text, centered within the subtitle box
        _windowWrapLine(
            gMovieWindow,
            gMovieSubtitleHead->text,
            _subtitleW,
            subtitleHeight,
            0,
            subtitleY,
            _colorTable[colorIndex] | 0x2000000,
            TEXT_ALIGNMENT_CENTER);

        // Refresh the window region to make subtitle visible
        Rect subtitleRect;
        subtitleRect.left = 0;
        subtitleRect.top = subtitleY;
        subtitleRect.right = _subtitleW;
        subtitleRect.bottom = subtitleY + subtitleHeight;
        windowRefreshRect(gMovieWindow, &subtitleRect);

        // Clean up used subtitle
        internal_free_safe(gMovieSubtitleHead->text, __FILE__, __LINE__);
        internal_free_safe(gMovieSubtitleHead, __FILE__, __LINE__);
        gMovieSubtitleHead = next;

        if (gMovieSubtitlesFont != -1) {
            fontSetCurrent(previousFont);
        }
    }
}


// 0x487710
static int _movieStart(int win, char* filePath)
{
    if (_running) {
        return 1;
    }

    _cleanupLast();
    
    _movieHasSubtitles = false; // reset has subtitles flag

    gMovieFileStream = movieOpen(filePath);
    if (gMovieFileStream == nullptr) {
        return 1;
    }

    gMovieWindow = win;
    _running = 1;
    gMovieFlags &= ~MOVIE_EXTENDED_FLAG_PLAYING;

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_SUBTITLES_ENABLED) != 0) {
        movieLoadSubtitles(filePath);
    }

    // Always use direct rendering
    debugPrint("Direct ");
    windowGetRect(gMovieWindow, &gMovieWindowRect);
    debugPrint("Playing at (%d, %d)  ", _movieX + gMovieWindowRect.left, _movieY + gMovieWindowRect.top);
    MveSetShowFrame(movieDirectImpl);
    MVE_rmPrepMovie(gMovieFileStream, _movieX + gMovieWindowRect.left, _movieY + gMovieWindowRect.top, 0);

    if (_alphaHandle != nullptr) {
        int size;
        fileReadInt32(_alphaHandle, &size);

        short tmp;
        fileReadInt16(_alphaHandle, &tmp);
        fileReadInt16(_alphaHandle, &tmp);

        _alphaBuf = (unsigned char*)internal_malloc_safe(size, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1178
        _alphaWindowBuf = (unsigned char*)internal_malloc_safe(_movieH * _movieW, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1179

        unsigned char* windowBuffer = windowGetBuffer(gMovieWindow);
        blitBufferToBuffer(windowBuffer + windowGetWidth(gMovieWindow) * _movieY + _movieX,
            _movieW,
            _movieH,
            windowGetWidth(gMovieWindow),
            _alphaWindowBuf,
            _movieW);
    }

    _movieRect.left = _movieX;
    _movieRect.top = _movieY;
    _movieRect.right = _movieW + _movieX;
    _movieRect.bottom = _movieH + _movieY;

    return 0;
}

// 0x487964
static bool _localMovieCallback()
{
    movieRenderSubtitles();

    if (_movieCallback != nullptr) {
        _movieCallback();
    }

    return inputGetInput() != -1;
}

// 0x487AC8
int _movieRun(int win, char* filePath)
{
    if (_running) {
        return 1;
    }

    _movieX = 0;
    _movieY = 0;
    _movieOffset = 0;
    _movieW = windowGetWidth(win);
    _movieH = windowGetHeight(win);
    _movieSubRectFlag = 0;
    return _movieStart(win, filePath);
}

// 0x487B1C
int _movieRunRect(int win, char* filePath, int a3, int a4, int a5, int a6)
{
    if (_running) {
        return 1;
    }

    _movieX = a3;
    _movieY = a4;
    _movieOffset = a3 + a4 * windowGetWidth(win);
    _movieW = a5;
    _movieH = a6;
    _movieSubRectFlag = 1;

    return _movieStart(win, filePath);
}

// 0x487B7C
static int _stepMovie()
{
    if (_alphaHandle != nullptr) {
        int size;
        fileReadInt32(_alphaHandle, &size);
        fileRead(_alphaBuf, 1, size, _alphaHandle);
    }

    int v1 = _MVE_rmStepMovie();
    if (v1 != -1) {
        movieRenderSubtitles();
    }

    return v1;
}

// 0x487BC8
void movieSetBuildSubtitleFilePathProc(MovieBuildSubtitleFilePathProc* proc)
{
    gMovieBuildSubtitleFilePathProc = proc;
}

// 0x487BD0
void movieSetVolume(int volume)
{
    int normalizedVolume = _soundVolumeHMItoDirectSound(volume);
    MveSetVolume(normalizedVolume);
}

// 0x487BEC
void _movieUpdate()
{
    if (!_running) {
        return;
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_SUBTITLES_LOADED) != 0) {
        debugPrint("Movie aborted\n");
        _cleanupMovie(1);
        return;
    }

    if ((gMovieFlags & MOVIE_EXTENDED_FLAG_PLAYING) != 0) {
        debugPrint("Movie error\n");
        _cleanupMovie(1);
        return;
    }

    if (_stepMovie() == -1) {
        _cleanupMovie(1);
        return;
    }

    if (gMoviePaletteProc != nullptr) {
        int frame;
        int dropped;
        MVE_rmFrameCounts(&frame, &dropped);
        gMoviePaletteProc(frame);
    }
}

// 0x487C88
int _moviePlaying()
{
    return _running;
}

} // namespace fallout
