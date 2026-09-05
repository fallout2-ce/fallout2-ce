#include "endgame.h"

#include <algorithm>
#include <cstdint>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "art.h"
#include "color.h"
#include "content_config.h"
#include "credits.h"
#include "cycle.h"
#include "db.h"
#include "dbox.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "game_vars.h"
#include "input.h"
#include "map.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "palette.h"
#include "pipboy.h"
#include "platform_compat.h"
#include "random.h"
#include "settings.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"
#include "word_wrap.h"
#include "worldmap.h"

namespace fallout {

// The maximum number of subtitle lines per slide.
#define ENDGAME_ENDING_MAX_SUBTITLES (50)

#define ENDGAME_ENDING_WINDOW_WIDTH 640
#define ENDGAME_ENDING_WINDOW_HEIGHT 480

typedef struct EndgameDeathEnding {
    GameGlobalVar gvar;
    int value;
    City worldAreaKnown;
    City worldAreaNotKnown;
    int min_level;
    int percentage;
    char voiceOverBaseName[16];

    // This flag denotes that the conditions for this ending is met and it was
    // selected as a candidate for final random selection.
    bool enabled;
} EndgameDeathEnding;

typedef struct EndgameEnding {
    GameGlobalVar gvar;
    int value;
    InterfaceFrameId art_num;
    char voiceOverBaseName[12];
    int direction;
} EndgameEnding;

static void endgameEndingRenderPanningScene(int direction, const char* narratorFileName);
static void endgameEndingRenderStaticScene(int fid, const char* narratorFileName);
static int endgameEndingHandleContinuePlaying();
static int endgameEndingSlideshowWindowInit();
static void endgameEndingSlideshowWindowFree();
static void endgameEndingRenderFrame(const unsigned char* data, int width, int height, int pitch);
static void endgameEndingVoiceOverInit(const char* fname);
static void endgameEndingVoiceOverReset();
static void endgameEndingVoiceOverFree();
static void endgameEndingLoadPalette(ObjectType type, int id);
static void _endgame_voiceover_callback();
static int endgameEndingSubtitlesLoad(const char* filePath);
static void endgameEndingRefreshSubtitles();
static void endgameEndingSubtitlesFree();
static void _endgame_movie_callback();
static void _endgame_movie_bk_process();
static void endgamePlayConfiguredMovie();
static int endgameEndingInit();
static void endgameEndingFree();
static int endgameDeathEndingValidate(int* percentage);
static void endgameEndingUpdateOverlay();

// The number of lines in current subtitles file.
//
// It's used as a length for two arrays:
// - [gEndgameEndingSubtitles]
// - [gEndgameEndingSubtitlesTimings]
//
// This value does not exceed [ENDGAME_ENDING_SUBTITLES_CAPACITY].
//
// 0x518668 endgame_subtitle_count
static int gEndgameEndingSubtitlesLength = 0;

// The number of characters in current subtitles file.
//
// This value is used to determine
//
// 0x51866C endgame_subtitle_characters
static int gEndgameEndingSubtitlesCharactersCount = 0;

// 0x518670 endgame_current_subtitle
static int gEndgameEndingSubtitlesCurrentLine = 0;

// 0x518674 endgame_maybe_done
static int _endgame_maybe_done = 0;

// enddeath.txt
//
// 0x518678 endDeathInfoList
static EndgameDeathEnding* gEndgameDeathEndings = nullptr;

// The number of death endings in [gEndgameDeathEndings] array.
//
// 0x51867C maxEndDeathInfo
static int gEndgameDeathEndingsLength = 0;

// Base file name for death ending.
//
// This value does not include extension.
//
// 0x570A90 endDeathSndChoice
static char gEndgameDeathEndingFileName[40];

// This flag denotes whether speech sound was successfully loaded for
// the current slide.
//
// 0x570AB8 endgame_voiceover_loaded
static bool gEndgameEndingVoiceOverSpeechLoaded;

// 0x570ABC endgame_subtitle_path
static char gEndgameEndingSubtitlesLocalizedPath[COMPAT_MAX_PATH];

// The flag used to denote voice over speech for current slide has ended.
//
// 0x570BC0 endgame_voiceover_done
static bool gEndgameEndingSpeechEnded;

// endgame.txt
//
// 0x570BC4 slides
static EndgameEnding* gEndgameEndings;

// The array of text lines in current subtitles file.
//
// The length is specified in [gEndgameEndingSubtitlesLength]. It's capacity
// is [ENDGAME_ENDING_SUBTITLES_CAPACITY].
//
// 0x570BC8 endgame_subtitle_text
static char** gEndgameEndingSubtitles;

// 0x570BCC endgame_do_subtitles
static bool gEndgameEndingSubtitlesEnabled;

// The flag used to denote voice over subtitles for current slide has ended.
//
// 0x570BD0 endgame_subtitle_done
static bool gEndgameEndingSubtitlesEnded;

// 0x570BD4 endgame_map_enabled
static bool _endgame_map_enabled;

// 0x570BD8 endgame_mouse_state
static bool _endgame_mouse_state;

// The number of endings in [gEndgameEndings] array.
//
// 0x570BDC num_slides
static int gEndgameEndingsLength = 0;

// This flag denotes whether subtitles was successfully loaded for
// the current slide.
//
// 0x570BE0 endgame_subtitle_loaded
static bool gEndgameEndingVoiceOverSubtitlesLoaded;

// Reference time is a timestamp when subtitle is first displayed.
//
// This value is used together with [gEndgameEndingSubtitlesTimings] array to
// determine when next line needs to be displayed.
//
// 0x570BE4 endgame_subtitle_start_time
static unsigned int gEndgameEndingSubtitlesReferenceTime;

// The array of timings for each line in current subtitles file.
//
// The length is specified in [gEndgameEndingSubtitlesLength]. It's capacity
// is [ENDGAME_ENDING_SUBTITLES_CAPACITY].
//
// 0x570BE8 endgame_subtitle_times
static unsigned int* gEndgameEndingSubtitlesTimings;

// Font that was current before endgame slideshow window was created.
//
// 0x570BEC endgame_old_font
static int gEndgameEndingSlideshowOldFont;

// 0x570BF0 endgame_window_buffer
static unsigned char* gEndgameEndingSlideshowWindowBuffer;

// 0x570BF4 endgame_window
static int gEndgameEndingSlideshowWindow;

static int gEndgameEndingOverlay;
static Rect gEndgameEndingFrameBounds;

// 0x43F788 endgame_slideshow
void endgamePlaySlideshow()
{
    if (endgameEndingSlideshowWindowInit() == -1) {
        return;
    }

    for (int index = 0; index < gEndgameEndingsLength; index++) {
        EndgameEnding* ending = &(gEndgameEndings[index]);
        int value = gameGetGlobalVar(ending->gvar);
        if (value == ending->value) {
            if (ending->art_num == InterfaceFrameId::PanningDesertImage) {
                endgameEndingRenderPanningScene(ending->direction, ending->voiceOverBaseName);
            } else {
                const FrmId frmId = FrmId(ending->art_num);
                endgameEndingRenderStaticScene(frmId.fid(), ending->voiceOverBaseName);
            }
        }
    }

    endgameEndingSlideshowWindowFree();
}

// 0x43F810 endgame_movie
void endgamePlayMovie()
{
    backgroundSoundDelete();
    isoDisable();
    paletteFadeTo(gPaletteBlack);
    _endgame_maybe_done = 0;
    tickersAdd(_endgame_movie_bk_process);
    backgroundSoundSetEndCallback(_endgame_movie_callback);
    backgroundSoundLoad(gameSoundGetMusicOverride("endgame_movie_music0", "akiss"), GSOUND_LIMIT_AFTER, GSOUND_STREAM, GSOUND_NO_LOOP);
    inputPauseForTocks(3000);

    endgamePlayConfiguredMovie();

    const char* creditsFilePath = "credits.txt";
    char* configuredCreditsFilePath;
    if (configGetString(&gContentConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_credits_file", &configuredCreditsFilePath)) {
        creditsFilePath = configuredCreditsFilePath;
    }
    if (creditsFilePath[0] != '\0') {
        creditsOpen(creditsFilePath, -1, false);
    }

    backgroundSoundDelete();
    backgroundSoundSetEndCallback(nullptr);
    tickersRemove(_endgame_movie_bk_process);
    backgroundSoundDelete();
    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);
    isoEnable();
    endgameEndingHandleContinuePlaying();
}

bool endgameShouldPlayMovieAfterSlideshow()
{
    bool enabled;
    configGetBool(&gContentConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_play_after_slideshow", &enabled, true);
    return enabled;
}

static void endgamePlayConfiguredMovie()
{
    int movie = -1;

    int gender = critterGetStat(gDude, STAT_GENDER);
    if (gender == GENDER_FEMALE) {
        configGetInt(&gContentConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_movie_female", &movie, -1);
    } else {
        configGetInt(&gContentConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_movie_male", &movie, -1);
    }

    if (movie >= 0 && movie < GAME_MOVIE_MAX_COUNT) {
        gameMoviePlay(movie, GAME_MOVIE_FADE_IN | GAME_MOVIE_PAUSE_MUSIC);
    }
}

// 0x43F8C4 gameOverConfim
static int endgameEndingHandleContinuePlaying()
{
    bool isoWasEnabled = isoDisable();

    bool gameMouseWasVisible;
    if (isoWasEnabled) {
        gameMouseWasVisible = gameMouseObjectsIsVisible();
    } else {
        gameMouseWasVisible = false;
    }

    if (gameMouseWasVisible) {
        gameMouseObjectsHide();
    }

    bool oldCursorIsHidden = cursorIsHidden();
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    int oldCursor = gameMouseGetCursor();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    int rc;

    MessageListItem messageListItem;
    messageListItem.num = 30;
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        rc = showDialogBox(messageListItem.text, nullptr, 0, 169, 117, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_YES_NO);
        if (rc == 0) {
            _game_user_wants_to_quit = GAME_QUIT_REQUEST_MAIN_MENU;
        }
    } else {
        rc = -1;
    }

    gameMouseSetCursor(oldCursor);
    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    if (gameMouseWasVisible) {
        gameMouseObjectsShow();
    }

    if (isoWasEnabled) {
        isoEnable();
    }

    return rc;
}

// 0x43FBDC endgame_pan_desert
static void endgameEndingRenderPanningScene(int direction, const char* narratorFileName)
{
    CacheEntry* backgroundHandle;
    Art* background = artLock(FrmId(InterfaceFrameId::PanningDesertImage), &backgroundHandle);
    if (background != nullptr) {
        int width = artGetWidth(background);
        int height = artGetHeight(background);
        unsigned char* backgroundData = artGetFrameData(background);
        if (width <= 0 || height <= 0 || backgroundData == nullptr) {
            artUnlock(backgroundHandle);
            return;
        }

        endgameEndingLoadPalette(OBJ_TYPE_INTERFACE, static_cast<int>(InterfaceFrameId::PanningDesertImage));
        bufferFill(gEndgameEndingSlideshowWindowBuffer, screenGetWidth(), screenGetHeight(), screenGetWidth(), COLOR_BLACK);

        // CE: Update overlay.
        endgameEndingUpdateOverlay();

        unsigned char palette[768];
        memcpy(palette, _cmap, 768);

        paletteSetEntries(gPaletteBlack);
        endgameEndingVoiceOverInit(narratorFileName);

        int sourceViewportWidth = std::min(width, static_cast<int>(static_cast<int64_t>(ENDGAME_ENDING_WINDOW_WIDTH) * height / ENDGAME_ENDING_WINDOW_HEIGHT));
        int panDistance = width - sourceViewportWidth;
        int fadeDistance = panDistance / 4;
        if (panDistance <= 0) {
            paletteSetEntries(palette);
            endgameEndingRenderFrame(backgroundData, sourceViewportWidth, height, width);
            windowRefresh(gEndgameEndingSlideshowWindow);
        }

        unsigned int frameDelay = 16;
        unsigned int baseAnimationTicks = 16 * panDistance;

        if (panDistance > 0 && gEndgameEndingVoiceOverSpeechLoaded) {
            unsigned int speechDurationMs = 1000 * speechGetDuration();
            if (speechDurationMs > baseAnimationTicks / 2) {
                frameDelay = (speechDurationMs + frameDelay * (panDistance / 2)) / panDistance;
            }
        }

        int start;
        int end;
        if (direction == -1) {
            start = width - sourceViewportWidth;
            end = 0;
        } else {
            start = 0;
            end = width - sourceViewportWidth;
        }

        tickersDisable();

        bool subtitlesLoaded = panDistance > 0 && fadeDistance == 0;
        if (subtitlesLoaded) {
            paletteSetEntries(palette);

            // NOTE: Uninline.
            endgameEndingVoiceOverReset();
        }

        unsigned int since = 0;
        while (start != end) {
            sharedFpsLimiter.mark();

            int fadeOutStartX = sourceViewportWidth - fadeDistance;

            // TODO: Complex math, setup scene in debugger.
            if (getTicksSince(since) >= frameDelay) {
                endgameEndingRenderFrame(backgroundData + start, sourceViewportWidth, height, width);

                if (subtitlesLoaded) {
                    endgameEndingRefreshSubtitles();
                }

                windowRefresh(gEndgameEndingSlideshowWindow);

                since = getTicks();

                if (fadeDistance > 0) {
                    bool shouldDarkenPalette;
                    double darkeningFactor;
                    if (start > fadeDistance) {
                        if (fadeOutStartX > start) {
                            shouldDarkenPalette = false;
                        } else {
                            int fadeOffset = fadeDistance - (start - fadeOutStartX);
                            darkeningFactor = (double)fadeOffset / (double)fadeDistance;
                            shouldDarkenPalette = true;
                        }
                    } else {
                        shouldDarkenPalette = true;
                        darkeningFactor = (double)start / (double)fadeDistance;
                    }

                    if (shouldDarkenPalette) {
                        unsigned char darkenedPalette[768];
                        for (int index = 0; index < 768; index++) {
                            darkenedPalette[index] = (unsigned char)trunc(palette[index] * darkeningFactor);
                        }
                        paletteSetEntries(darkenedPalette);
                    }
                }

                start += direction;

                if (fadeDistance > 0 && direction == 1 && (start == fadeDistance)) {
                    // NOTE: Uninline.
                    endgameEndingVoiceOverReset();
                    subtitlesLoaded = true;
                } else if (fadeDistance > 0 && direction == -1 && (start == fadeOutStartX)) {
                    // NOTE: Uninline.
                    endgameEndingVoiceOverReset();
                    subtitlesLoaded = true;
                }
            }

            soundContinueAll();

            if (inputGetInput() != -1) {
                // NOTE: Uninline.
                endgameEndingVoiceOverFree();
                break;
            }

            renderPresent();
            sharedFpsLimiter.throttle();
        }

        tickersEnable();
        artUnlock(backgroundHandle);

        paletteFadeTo(gPaletteBlack);
        bufferFill(gEndgameEndingSlideshowWindowBuffer, screenGetWidth(), screenGetHeight(), screenGetWidth(), COLOR_BLACK);
        windowRefresh(gEndgameEndingSlideshowWindow);
    }

    while (mouseGetEvent() != 0) {
        sharedFpsLimiter.mark();

        inputGetInput();

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// 0x440004 endgame_display_image
static void endgameEndingRenderStaticScene(int fid, const char* narratorFileName)
{
    CacheEntry* backgroundHandle;
    Art* background = artLock(fid, &backgroundHandle);
    if (background == nullptr) {
        return;
    }

    int width = artGetWidth(background);
    int height = artGetHeight(background);
    unsigned char* backgroundData = artGetFrameData(background);
    if (backgroundData != nullptr) {
        endgameEndingLoadPalette(objectTypeFromFid(fid), frameIdFromFid(fid));

        // CE: Update overlay.
        endgameEndingUpdateOverlay();

        endgameEndingRenderFrame(backgroundData, width, height, width);
        windowRefresh(gEndgameEndingSlideshowWindow);

        endgameEndingVoiceOverInit(narratorFileName);

        unsigned int delay;
        if (gEndgameEndingVoiceOverSubtitlesLoaded || gEndgameEndingVoiceOverSpeechLoaded) {
            delay = UINT_MAX;
        } else {
            delay = 3000;
        }

        paletteFadeTo(_cmap);

        inputPauseForTocks(500);

        // NOTE: Uninline.
        endgameEndingVoiceOverReset();

        unsigned int referenceTime = getTicks();
        tickersDisable();

        int keyCode;
        while (true) {
            sharedFpsLimiter.mark();

            keyCode = inputGetInput();
            if (keyCode != -1) {
                break;
            }

            if (gEndgameEndingSpeechEnded) {
                break;
            }

            if (gEndgameEndingSubtitlesEnded) {
                break;
            }

            if (getTicksSince(referenceTime) >= delay) {
                break;
            }

            endgameEndingRenderFrame(backgroundData, width, height, width);
            endgameEndingRefreshSubtitles();
            windowRefresh(gEndgameEndingSlideshowWindow);
            soundContinueAll();

            renderPresent();
            sharedFpsLimiter.throttle();
        }

        tickersEnable();
        speechDelete();
        endgameEndingSubtitlesFree();

        gEndgameEndingVoiceOverSpeechLoaded = false;
        gEndgameEndingVoiceOverSubtitlesLoaded = false;

        if (keyCode == -1) {
            inputPauseForTocks(500);
        }

        paletteFadeTo(gPaletteBlack);

        while (mouseGetEvent() != 0) {
            sharedFpsLimiter.mark();

            inputGetInput();

            renderPresent();
            sharedFpsLimiter.throttle();
        }
    }

    artUnlock(backgroundHandle);
}

// 0x43F99C endgame_init
static int endgameEndingSlideshowWindowInit()
{
    if (endgameEndingInit() != 0) {
        return -1;
    }

    backgroundSoundDelete();

    _endgame_map_enabled = isoDisable();

    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    bool oldCursorIsHidden = cursorIsHidden();
    _endgame_mouse_state = oldCursorIsHidden == 0;

    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    gEndgameEndingSlideshowOldFont = fontGetCurrent();
    fontSetCurrent(101);

    paletteFadeTo(gPaletteBlack);

    // CE: Every slide has a separate color palette which is incompatible with
    // main color palette. Setup overlay to hide everything.
    gEndgameEndingOverlay = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), COLOR_BLACK, WINDOW_MOVE_ON_TOP);
    if (gEndgameEndingOverlay == -1) {
        return -1;
    }

    int windowEndgameEndingX = 0;
    int windowEndgameEndingY = 0;
    gEndgameEndingSlideshowWindow = windowCreate(windowEndgameEndingX,
        windowEndgameEndingY,
        screenGetWidth(),
        screenGetHeight(),
        COLOR_BLACK,
        WINDOW_MOVE_ON_TOP);
    if (gEndgameEndingSlideshowWindow == -1) {
        return -1;
    }

    gEndgameEndingSlideshowWindowBuffer = windowGetBuffer(gEndgameEndingSlideshowWindow);
    if (gEndgameEndingSlideshowWindowBuffer == nullptr) {
        return -1;
    }

    colorCycleDisable();

    speechSetEndCallback(_endgame_voiceover_callback);

    gEndgameEndingSubtitlesEnabled = settings.preferences.subtitles;
    if (!gEndgameEndingSubtitlesEnabled) {
        return 0;
    }

    snprintf(gEndgameEndingSubtitlesLocalizedPath, sizeof(gEndgameEndingSubtitlesLocalizedPath), "text\\%s\\cuts\\", settings.system.language.c_str());

    gEndgameEndingSubtitles = (char**)internal_malloc(sizeof(*gEndgameEndingSubtitles) * ENDGAME_ENDING_MAX_SUBTITLES);
    if (gEndgameEndingSubtitles == nullptr) {
        gEndgameEndingSubtitlesEnabled = false;
        return 0;
    }

    for (int index = 0; index < ENDGAME_ENDING_MAX_SUBTITLES; index++) {
        gEndgameEndingSubtitles[index] = nullptr;
    }

    gEndgameEndingSubtitlesTimings = (unsigned int*)internal_malloc(sizeof(*gEndgameEndingSubtitlesTimings) * ENDGAME_ENDING_MAX_SUBTITLES);
    if (gEndgameEndingSubtitlesTimings == nullptr) {
        internal_free(gEndgameEndingSubtitles);
        gEndgameEndingSubtitlesEnabled = false;
        return 0;
    }

    return 0;
}

static void endgameEndingRenderFrame(const unsigned char* data, int width, int height, int pitch)
{
    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();
    gEndgameEndingFrameBounds = blitBuffer2DCenteredAspectFit(ConstBuffer2D(data, pitch, height),
        0,
        0,
        width,
        height,
        Buffer2D(gEndgameEndingSlideshowWindowBuffer, screenWidth, screenHeight),
        COLOR_BLACK,
        settings.ui.end_slide_size != 0);
}

// 0x43FB28 endgame_exit
static void endgameEndingSlideshowWindowFree()
{
    if (gEndgameEndingSubtitlesEnabled) {
        endgameEndingSubtitlesFree();

        internal_free(gEndgameEndingSubtitlesTimings);
        internal_free(gEndgameEndingSubtitles);

        gEndgameEndingSubtitles = nullptr;
        gEndgameEndingSubtitlesEnabled = false;
    }

    // NOTE: Uninline.
    endgameEndingFree();

    fontSetCurrent(gEndgameEndingSlideshowOldFont);

    speechSetEndCallback(nullptr);
    windowDestroy(gEndgameEndingSlideshowWindow);
    windowDestroy(gEndgameEndingOverlay);

    if (!_endgame_mouse_state) {
        mouseHideCursor();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);

    colorCycleEnable();

    if (_endgame_map_enabled) {
        isoEnable();
    }
}

// 0x4401A0 endgame_load_voiceover
static void endgameEndingVoiceOverInit(const char* fileBaseName)
{
    char path[COMPAT_MAX_PATH];

    // NOTE: Uninline.
    endgameEndingVoiceOverFree();

    gEndgameEndingVoiceOverSpeechLoaded = false;
    gEndgameEndingVoiceOverSubtitlesLoaded = false;

    // Build speech file path.
    snprintf(path, sizeof(path), "%s%s", "narrator\\", fileBaseName);

    if (speechLoad(path, GSOUND_LOAD_NO_PLAY, GSOUND_STREAM, GSOUND_NO_LOOP) != -1) {
        gEndgameEndingVoiceOverSpeechLoaded = true;
    }

    if (gEndgameEndingSubtitlesEnabled) {
        // Build subtitles file path.
        snprintf(path, sizeof(path), "%s%s.txt", gEndgameEndingSubtitlesLocalizedPath, fileBaseName);

        if (endgameEndingSubtitlesLoad(path) != 0) {
            return;
        }

        double durationPerCharacter;
        if (gEndgameEndingVoiceOverSpeechLoaded) {
            durationPerCharacter = (double)speechGetDuration() / (double)gEndgameEndingSubtitlesCharactersCount;
        } else {
            durationPerCharacter = 0.08;
        }

        unsigned int timing = 0;
        for (int index = 0; index < gEndgameEndingSubtitlesLength; index++) {
            double charactersCount = static_cast<double>(strlen(gEndgameEndingSubtitles[index]));
            // NOTE: There is floating point math at 0x4402E6 used to add
            // timing.
            timing += (unsigned int)trunc(charactersCount * durationPerCharacter * 1000.0);
            gEndgameEndingSubtitlesTimings[index] = timing;
        }

        gEndgameEndingVoiceOverSubtitlesLoaded = true;
    }
}

// NOTE: This function was inlined at every call site.
//
// 0x440324 endgame_play_voiceover
static void endgameEndingVoiceOverReset()
{
    gEndgameEndingSubtitlesEnded = false;
    gEndgameEndingSpeechEnded = false;

    if (gEndgameEndingVoiceOverSpeechLoaded) {
        _gsound_speech_play_preloaded();
    }

    if (gEndgameEndingVoiceOverSubtitlesLoaded) {
        gEndgameEndingSubtitlesReferenceTime = getTicks();
    }
}

// NOTE: This function was inlined at every call site.
//
// 0x44035C endgame_stop_voiceover
static void endgameEndingVoiceOverFree()
{
    speechDelete();
    endgameEndingSubtitlesFree();
    gEndgameEndingVoiceOverSpeechLoaded = false;
    gEndgameEndingVoiceOverSubtitlesLoaded = false;
}

// 0x440378 endgame_load_palette
static void endgameEndingLoadPalette(ObjectType type, int id)
{
    char fileName[13];
    if (artCopyFileName(type, id, fileName) != 0) {
        return;
    }

    // Remove extension from file name.
    char* pch = strrchr(fileName, '.');
    if (pch != nullptr) {
        *pch = '\0';
    }

    if (strlen(fileName) <= 8) {
        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s.pal", "art\\intrface", fileName);
        colorPaletteLoad(path);
    }
}

// 0x4403F0 endgame_voiceover_callback
static void _endgame_voiceover_callback()
{
    gEndgameEndingSpeechEnded = true;
}

// Loads subtitles file.
//
// 0x4403FC endgame_load_subtitles
static int endgameEndingSubtitlesLoad(const char* filePath)
{
    endgameEndingSubtitlesFree();

    File* stream = fileOpen(filePath, "rt");
    if (stream == nullptr) {
        return -1;
    }

    // FIXME: There is at least one subtitle for Arroyo ending (nar_ar1) that
    // does not fit into this buffer.
    char string[256];
    while (fileReadString(string, sizeof(string), stream)) {
        char* pch;

        // Find and clamp string at EOL.
        pch = strchr(string, '\n');
        if (pch != nullptr) {
            *pch = '\0';
        }

        // Find separator. The value before separator is ignored (as opposed to
        // movie subtitles, where the value before separator is a timing).
        pch = strchr(string, ':');
        if (pch != nullptr) {
            if (gEndgameEndingSubtitlesLength < ENDGAME_ENDING_MAX_SUBTITLES) {
                gEndgameEndingSubtitles[gEndgameEndingSubtitlesLength] = internal_strdup(pch + 1);
                gEndgameEndingSubtitlesLength++;
                gEndgameEndingSubtitlesCharactersCount += static_cast<int>(strlen(pch + 1));
            }
        }
    }

    fileClose(stream);

    return 0;
}

// Refreshes subtitles.
//
// 0x4404EC endgame_show_subtitles
static void endgameEndingRefreshSubtitles()
{
    if (gEndgameEndingSubtitlesLength <= gEndgameEndingSubtitlesCurrentLine) {
        if (gEndgameEndingVoiceOverSubtitlesLoaded) {
            gEndgameEndingSubtitlesEnded = true;
        }
        return;
    }

    if (getTicksSince(gEndgameEndingSubtitlesReferenceTime) > gEndgameEndingSubtitlesTimings[gEndgameEndingSubtitlesCurrentLine]) {
        gEndgameEndingSubtitlesCurrentLine++;
        return;
    }

    char* text = gEndgameEndingSubtitles[gEndgameEndingSubtitlesCurrentLine];
    if (text == nullptr) {
        return;
    }

    int screenWidth = screenGetWidth();
    int frameWidth = rectGetWidth(&gEndgameEndingFrameBounds);
    int frameHeight = rectGetHeight(&gEndgameEndingFrameBounds);
    int textMaxWidth = std::min(540, frameWidth - 20);

    short beginnings[WORD_WRAP_MAX_COUNT];
    short count;
    if (textMaxWidth <= 0 || wordWrap(text, textMaxWidth, beginnings, &count) != 0) {
        return;
    }

    int height = fontGetLineHeight();
    int lines = count - 1;
    int y = std::max(gEndgameEndingFrameBounds.top, gEndgameEndingFrameBounds.top + frameHeight - height * lines - 20);

    for (int index = 0; index < lines; index++) {
        char* beginning = text + beginnings[index];
        char* ending = text + beginnings[index + 1];

        if (ending[-1] == ' ') {
            ending--;
        }

        char c = *ending;
        *ending = '\0';

        int width = fontGetStringWidth(beginning);
        int x = std::max(0, gEndgameEndingFrameBounds.left + (frameWidth - width) / 2);
        bufferFill(gEndgameEndingSlideshowWindowBuffer + screenWidth * y + x, width, height, screenWidth, COLOR_BLACK);
        fontDrawText(gEndgameEndingSlideshowWindowBuffer + screenWidth * y + x, beginning, width, screenWidth, COLOR_WHITE);

        *ending = c;

        y += height;
    }
}

// 0x4406CC endgame_clear_subtitles
static void endgameEndingSubtitlesFree()
{
    for (int index = 0; index < gEndgameEndingSubtitlesLength; index++) {
        if (gEndgameEndingSubtitles[index] != nullptr) {
            internal_free(gEndgameEndingSubtitles[index]);
            gEndgameEndingSubtitles[index] = nullptr;
        }
    }

    gEndgameEndingSubtitlesCurrentLine = 0;
    gEndgameEndingSubtitlesCharactersCount = 0;
    gEndgameEndingSubtitlesLength = 0;
}

// 0x440728 endgame_movie_callback
static void _endgame_movie_callback()
{
    _endgame_maybe_done = 1;
}

// 0x440734 endgame_movie_bk_process
static void _endgame_movie_bk_process()
{
    if (_endgame_maybe_done) {
        backgroundSoundLoad(gameSoundGetMusicOverride("endgame_movie_music1", "10labone"), GSOUND_LIMIT_BEFORE, GSOUND_STREAM, GSOUND_LOOP);
        backgroundSoundSetEndCallback(nullptr);
        tickersRemove(_endgame_movie_bk_process);
    }
}

// 0x440770 endgame_load_slide_info
static int endgameEndingInit()
{
    File* stream;
    char str[256];
    char *ch, *tok;
    const char* delim = " \t,";
    EndgameEnding entry;
    EndgameEnding* entries;
    size_t narratorFileNameLength;

    if (gEndgameEndings != nullptr) {
        internal_free(gEndgameEndings);
        gEndgameEndings = nullptr;
    }

    gEndgameEndingsLength = 0;

    stream = fileOpen("data\\endgame.txt", "rt");
    if (stream == nullptr) {
        return -1;
    }

    while (fileReadString(str, sizeof(str), stream)) {
        ch = str;
        while (isspace(*ch)) {
            ch++;
        }

        if (*ch == '#') {
            continue;
        }

        tok = strtok(ch, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.gvar = static_cast<GameGlobalVar>(atoi(tok));

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.value = atoi(tok);

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.art_num = static_cast<InterfaceFrameId>(atoi(tok));

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        strcpy(entry.voiceOverBaseName, tok);

        narratorFileNameLength = strlen(entry.voiceOverBaseName);
        if (isspace(entry.voiceOverBaseName[narratorFileNameLength - 1])) {
            entry.voiceOverBaseName[narratorFileNameLength - 1] = '\0';
        }

        tok = strtok(nullptr, delim);
        if (tok != nullptr) {
            entry.direction = atoi(tok);
        } else {
            entry.direction = 1;
        }

        entries = (EndgameEnding*)internal_realloc(gEndgameEndings, sizeof(*entries) * (gEndgameEndingsLength + 1));
        if (entries == nullptr) {
            goto err;
        }

        memcpy(&(entries[gEndgameEndingsLength]), &entry, sizeof(entry));

        gEndgameEndings = entries;
        gEndgameEndingsLength++;
    }

    fileClose(stream);

    return 0;

err:

    fileClose(stream);

    return -1;
}

// NOTE: There are no references to this function. It was inlined.
//
// 0x44095C endgame_unload_slide_info
static void endgameEndingFree()
{
    if (gEndgameEndings != nullptr) {
        internal_free(gEndgameEndings);
        gEndgameEndings = nullptr;
    }

    gEndgameEndingsLength = 0;
}

// endgameDeathEndingInit
// 0x440984 endgameDeathEndingInit
int endgameDeathEndingInit()
{
    File* stream;
    char str[256];
    char* ch;
    const char* delim = " \t,";
    char* tok;
    EndgameDeathEnding entry;
    EndgameDeathEnding* entries;
    size_t narratorFileNameLength;

    strcpy(gEndgameDeathEndingFileName, "narrator\\nar_5");

    stream = fileOpen("data\\enddeath.txt", "rt");
    if (stream == nullptr) {
        return -1;
    }

    while (fileReadString(str, 256, stream)) {
        ch = str;
        while (isspace(*ch)) {
            ch++;
        }

        if (*ch == '#') {
            continue;
        }

        tok = strtok(ch, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.gvar = static_cast<GameGlobalVar>(atoi(tok));

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.value = atoi(tok);

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.worldAreaKnown = static_cast<City>(atoi(tok));

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.worldAreaNotKnown = static_cast<City>(atoi(tok));

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.min_level = atoi(tok);

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        entry.percentage = atoi(tok);

        tok = strtok(nullptr, delim);
        if (tok == nullptr) {
            continue;
        }

        // this code is slightly different from the original, but does the same thing
        narratorFileNameLength = strlen(tok);
        strncpy(entry.voiceOverBaseName, tok, narratorFileNameLength);

        entry.enabled = false;

        if (isspace(entry.voiceOverBaseName[narratorFileNameLength - 1])) {
            entry.voiceOverBaseName[narratorFileNameLength - 1] = '\0';
        }

        entries = (EndgameDeathEnding*)internal_realloc(gEndgameDeathEndings, sizeof(*entries) * (gEndgameDeathEndingsLength + 1));
        if (entries == nullptr) {
            goto err;
        }

        memcpy(&(entries[gEndgameDeathEndingsLength]), &entry, sizeof(entry));

        gEndgameDeathEndings = entries;
        gEndgameDeathEndingsLength++;
    }

    fileClose(stream);

    return 0;

err:

    fileClose(stream);

    return -1;
}

// 0x440BA8 endgameDeathEndingExit
int endgameDeathEndingExit()
{
    if (gEndgameDeathEndings != nullptr) {
        internal_free(gEndgameDeathEndings);
        gEndgameDeathEndings = nullptr;

        gEndgameDeathEndingsLength = 0;
    }

    return 0;
}

// endgameSetupDeathEnding
// 0x440BD0 endgameSetupDeathEnding
void endgameSetupDeathEnding(int reason)
{
    if (!gEndgameDeathEndingsLength) {
        debugPrint("\nError: endgameSetupDeathEnding: No endgame death info!");
        return;
    }

    // Build death ending file path.
    strcpy(gEndgameDeathEndingFileName, "narrator\\");

    int percentage = 0;
    endgameDeathEndingValidate(&percentage);

    int selectedEnding = 0;
    bool specialEndingSelected = false;

    switch (reason) {
    case ENDGAME_DEATH_ENDING_REASON_DEATH:
        if (gameGetGlobalVar(GVAR_MODOC_SHITTY_DEATH) != 0) {
            selectedEnding = 12;
            specialEndingSelected = true;
        }
        break;
    case ENDGAME_DEATH_ENDING_REASON_TIMEOUT:
        gameMoviePlay(MOVIE_TIMEOUT, GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC);
        break;
    }

    if (!specialEndingSelected) {
        int chance = randomBetween(0, percentage);

        int accum = 0;
        for (int index = 0; index < gEndgameDeathEndingsLength; index++) {
            EndgameDeathEnding* deathEnding = &(gEndgameDeathEndings[index]);

            if (deathEnding->enabled) {
                accum += deathEnding->percentage;
                if (accum >= chance) {
                    break;
                }
                selectedEnding++;
            }
        }
    }

    EndgameDeathEnding* deathEnding = &(gEndgameDeathEndings[selectedEnding]);

    strcat(gEndgameDeathEndingFileName, deathEnding->voiceOverBaseName);

    debugPrint("\nendgameSetupDeathEnding: Death Filename Picked: %s", gEndgameDeathEndingFileName);
}

// Validates conditions imposed by death endings.
//
// Upon return [percentage] is set as a sum of all valid endings' percentages.
// Always returns 0.
//
// 0x440CF4 endgameSetupInit
static int endgameDeathEndingValidate(int* percentage)
{
    *percentage = 0;

    for (int index = 0; index < gEndgameDeathEndingsLength; index++) {
        EndgameDeathEnding* deathEnding = &(gEndgameDeathEndings[index]);

        deathEnding->enabled = false;

        if (deathEnding->gvar != -1) {
            if (gameGetGlobalVar(deathEnding->gvar) >= deathEnding->value) {
                continue;
            }
        }

        if (deathEnding->worldAreaKnown != -1) {
            if (!wmAreaIsKnown(deathEnding->worldAreaKnown)) {
                continue;
            }
        }

        if (deathEnding->worldAreaNotKnown != -1) {
            if (wmAreaIsKnown(deathEnding->worldAreaNotKnown)) {
                continue;
            }
        }

        if (pcGetStat(PC_STAT_LEVEL) < deathEnding->min_level) {
            continue;
        }

        deathEnding->enabled = true;

        *percentage += deathEnding->percentage;
    }

    return 0;
}

// Returns file name for voice over for death ending.
//
// This path does not include extension.
//
// 0x440D8C endgameGetDeathEndingFileName
char* endgameDeathEndingGetFileName()
{
    if (gEndgameDeathEndingsLength == 0) {
        debugPrint("\nError: endgameSetupDeathEnding: No endgame death info!");
        strcpy(gEndgameDeathEndingFileName, "narrator\\nar_4");
    }

    debugPrint("\nendgameSetupDeathEnding: Death Filename: %s", gEndgameDeathEndingFileName);

    return gEndgameDeathEndingFileName;
}

void endgameEndingUpdateOverlay()
{
    bufferFill(windowGetBuffer(gEndgameEndingOverlay),
        windowGetWidth(gEndgameEndingOverlay),
        windowGetHeight(gEndgameEndingOverlay),
        windowGetWidth(gEndgameEndingOverlay),
        intensityColorTable[COLOR_BLACK][0]);
    windowRefresh(gEndgameEndingOverlay);
}

} // namespace fallout
