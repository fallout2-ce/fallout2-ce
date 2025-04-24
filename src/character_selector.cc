#include "character_selector.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "critter.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "palette.h"
#include "platform_compat.h"
#include "preferences.h"
#include "proto.h"
#include "settings.h"
#include "sfall_config.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "trait.h"
#include "window_manager.h"

namespace fallout {

#define CS_WINDOW_WIDTH (640)
#define CS_WINDOW_HEIGHT (480)

#define CS_WINDOW_BACKGROUND_X (40)
#define CS_WINDOW_BACKGROUND_Y (30)
#define CS_WINDOW_BACKGROUND_WIDTH (560)
#define CS_WINDOW_BACKGROUND_HEIGHT (300)

#define CS_WINDOW_PREVIOUS_BUTTON_X (292)
#define CS_WINDOW_PREVIOUS_BUTTON_Y (320)

#define CS_WINDOW_NEXT_BUTTON_X (318)
#define CS_WINDOW_NEXT_BUTTON_Y (320)

#define CS_WINDOW_TAKE_BUTTON_X (81)
#define CS_WINDOW_TAKE_BUTTON_Y (323)

#define CS_WINDOW_MODIFY_BUTTON_X (435)
#define CS_WINDOW_MODIFY_BUTTON_Y (320)

#define CS_WINDOW_CREATE_BUTTON_X (80)
#define CS_WINDOW_CREATE_BUTTON_Y (425)

#define CS_WINDOW_BACK_BUTTON_X (461)
#define CS_WINDOW_BACK_BUTTON_Y (425)

#define CS_WINDOW_NAME_MID_X (318)
#define CS_WINDOW_PRIMARY_STAT_MID_X (362)
#define CS_WINDOW_SECONDARY_STAT_MID_X (379)
#define CS_WINDOW_BIO_X (438)

#define ORIGINAL_WIDTH 640
#define ORIGINAL_HEIGHT 480

// Stretching defines
#define SCALE_COORD_X(x) ( \
    (menuStretchMode == 2) ? \
        ((x) * windowGetWidth(gCharacterSelectorWindow) / ORIGINAL_WIDTH) : \
        ((x) * getLetterboxWidthScaleFactor()) + getLetterboxOffsetX())

#define SCALE_COORD_Y(y) ( \
    (menuStretchMode == 2) ? \
        ((y) * windowGetHeight(gCharacterSelectorWindow) / ORIGINAL_HEIGHT) : \
        ((y) * getLetterboxHeightScaleFactor()) + getLetterboxOffsetY())

static int gCharacterSelectorWindowWidth = 0;
static int gCharacterSelectorWindowHeight = 0;

typedef enum PremadeCharacter {
    PREMADE_CHARACTER_NARG,
    PREMADE_CHARACTER_CHITSA,
    PREMADE_CHARACTER_MINGUN,
    PREMADE_CHARACTER_COUNT,
} PremadeCharacter;

typedef struct PremadeCharacterDescription {
    char fileName[20];
    int face;
    char field_18[20];
} PremadeCharacterDescription;

static bool characterSelectorWindowInit();
static void characterSelectorWindowFree();
static bool characterSelectorWindowRefresh();
static bool characterSelectorWindowRenderBackground(uint8_t* destBuffer);
static bool characterSelectorWindowRenderFace(uint8_t* dest);
static bool characterSelectorWindowRenderStats(uint8_t* dest);
static bool characterSelectorWindowRenderBio(uint8_t* dest);
static bool characterSelectorWindowFatalError(bool result);

static void premadeCharactersLocalizePath(char* path);

// 0x51C84C
static int gCurrentPremadeCharacter = PREMADE_CHARACTER_NARG;

// 0x51C850
static PremadeCharacterDescription gPremadeCharacterDescriptions[PREMADE_CHARACTER_COUNT] = {
    { "premade\\combat", 201, "VID 208-197-88-125" },
    { "premade\\stealth", 202, "VID 208-206-49-229" },
    { "premade\\diplomat", 203, "VID 208-206-49-227" },
};

// 0x51C8D4
static int gPremadeCharacterCount = PREMADE_CHARACTER_COUNT;

// 0x51C7F8
static int gCharacterSelectorWindow = -1;

// 0x51C7FC
static unsigned char* gCharacterSelectorWindowBuffer = nullptr;

// 0x51C800
static unsigned char* gCharacterSelectorBackground = nullptr;

// 0x51C804
static int gCharacterSelectorWindowPreviousButton = -1;

// 0x51C810
static int gCharacterSelectorWindowNextButton = -1;

// 0x51C81C
static int gCharacterSelectorWindowTakeButton = -1;

// 0x51C828
static int gCharacterSelectorWindowModifyButton = -1;

// 0x51C834
static int gCharacterSelectorWindowCreateButton = -1;

// 0x51C840
static int gCharacterSelectorWindowBackButton = -1;

static FrmImage _takeButtonNormalFrmImage;
static FrmImage _takeButtonPressedFrmImage;
static FrmImage _modifyButtonNormalFrmImage;
static FrmImage _modifyButtonPressedFrmImage;
static FrmImage _createButtonNormalFrmImage;
static FrmImage _createButtonPressedFrmImage;
static FrmImage _backButtonNormalFrmImage;
static FrmImage _backButtonPressedFrmImage;
static FrmImage _nextButtonNormalFrmImage;
static FrmImage _nextButtonPressedFrmImage;
static FrmImage _previousButtonNormalFrmImage;
static FrmImage _previousButtonPressedFrmImage;

static std::vector<PremadeCharacterDescription> gCustomPremadeCharacterDescriptions;

float getLetterboxWidthScaleFactor() {
    float scaleX = (float)windowGetWidth(gCharacterSelectorWindow) / ORIGINAL_WIDTH;
    float scaleY = (float)windowGetHeight(gCharacterSelectorWindow) / ORIGINAL_HEIGHT;
    return (scaleX < scaleY) ? scaleX : scaleY;
}

float getLetterboxHeightScaleFactor() {
    float scaleX = (float)windowGetWidth(gCharacterSelectorWindow) / ORIGINAL_WIDTH;
    float scaleY = (float)windowGetHeight(gCharacterSelectorWindow) / ORIGINAL_HEIGHT;
    return (scaleX < scaleY) ? scaleX : scaleY;
}

int getLetterboxOffsetX() {
    float scale = getLetterboxWidthScaleFactor();
    int contentWidth = (int)(ORIGINAL_WIDTH * scale);
    return (windowGetWidth(gCharacterSelectorWindow) - contentWidth) / 2;
}

int getLetterboxOffsetY() {
    float scale = getLetterboxHeightScaleFactor();
    int contentHeight = (int)(ORIGINAL_HEIGHT * scale);
    return (windowGetHeight(gCharacterSelectorWindow) - contentHeight) / 2;
}

// 0x4A71D0
int characterSelectorOpen()
{
    if (!characterSelectorWindowInit()) {
        return 0;
    }

    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);

    int rc = 0;
    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        int keyCode = inputGetInput();

        switch (keyCode) {
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            brightnessDecrease();
            break;
        case KEY_EQUAL:
        case KEY_PLUS:
            brightnessIncrease();
            break;
        case KEY_UPPERCASE_B:
        case KEY_LOWERCASE_B:
        case KEY_ESCAPE:
            rc = 3;
            done = true;
            break;
        case KEY_UPPERCASE_C:
        case KEY_LOWERCASE_C:
            _ResetPlayer();
            if (characterEditorShow(1) == 0) {
                rc = 2;
                done = true;
            } else {
                characterSelectorWindowRefresh();
            }

            break;
        case KEY_UPPERCASE_M:
        case KEY_LOWERCASE_M:
            if (!characterEditorShow(1)) {
                rc = 2;
                done = true;
            } else {
                characterSelectorWindowRefresh();
            }

            break;
        case KEY_UPPERCASE_T:
        case KEY_LOWERCASE_T:
            rc = 2;
            done = true;

            break;
        case KEY_F10:
            showQuitConfirmationDialog();
            break;
        case KEY_ARROW_LEFT:
            soundPlayFile("ib2p1xx1");
            // FALLTHROUGH
        case 500:
            gCurrentPremadeCharacter -= 1;
            if (gCurrentPremadeCharacter < 0) {
                gCurrentPremadeCharacter = gPremadeCharacterCount - 1;
            }

            characterSelectorWindowRefresh();
            break;
        case KEY_ARROW_RIGHT:
            soundPlayFile("ib2p1xx1");
            // FALLTHROUGH
        case 501:
            gCurrentPremadeCharacter += 1;
            if (gCurrentPremadeCharacter >= gPremadeCharacterCount) {
                gCurrentPremadeCharacter = 0;
            }

            characterSelectorWindowRefresh();
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    paletteFadeTo(gPaletteBlack);
    characterSelectorWindowFree();

    if (cursorWasHidden) {
        mouseHideCursor();
    }

    return rc;
}

static FrmImage _charSelecBackgroundFrmImage;
static FrmImage _charSelecButtonNormalFrmImage;
static FrmImage _charSelecButtonPressedFrmImage;

// 0x4A7468
static bool characterSelectorWindowInit() {
    colorPaletteLoad("color.pal");

    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();

    // Load stretch mode from INI file
    int menuStretchMode = 0;  // Default to 0, no stretch
    Config config;
    if (configInit(&config)) {
        if (configRead(&config, "f2_res.ini", false)) {
            configGetInt(&config, "STATIC_SCREENS", "CHARACTER_SELECTOR_SIZE", &menuStretchMode);
        }
        configFree(&config);
    }
    
    // Get the original image dimensions
    int originalWidth = CS_WINDOW_WIDTH;
    int originalHeight = CS_WINDOW_HEIGHT;
    
    int scaledWidth = originalWidth;
    int scaledHeight = originalHeight;
    
    // Figure out how to stretch the character Selector depending on resolution + settings
    if (menuStretchMode != 0 || screenWidth < originalWidth || screenHeight < originalHeight) {
        if (menuStretchMode == 2) {
            // Fullscreen stretch
            scaledWidth = screenWidth;
            scaledHeight = screenHeight;
        } else {
            // Preserve aspect ratio
            if (screenHeight * originalWidth >= screenWidth * originalHeight) {
                scaledWidth = screenWidth;
                scaledHeight = screenWidth * originalHeight / originalWidth;
            } else {
                scaledWidth = screenHeight * originalWidth / originalHeight;
                scaledHeight = screenHeight;
            }
        }
    }
    
    // Center the menu window on screen
    int charSelecWindowX = (screenWidth - scaledWidth) / 2;
    int charSelecWindowY = (screenHeight - scaledHeight) / 2;
    
    // Create the character selector window after the stretching calculations
    gCharacterSelectorWindow = windowCreate(
        charSelecWindowX,
        charSelecWindowY,
        scaledWidth,
        scaledHeight,
        _colorTable[0],
        0
    );
    if (gCharacterSelectorWindow == -1) {
        return characterSelectorWindowFatalError(false);
    }
    
    gCharacterSelectorWindowWidth = windowGetWidth(gCharacterSelectorWindow);
    gCharacterSelectorWindowHeight = windowGetHeight(gCharacterSelectorWindow);
    gCharacterSelectorWindowBuffer = windowGetBuffer(gCharacterSelectorWindow);

    if (!characterSelectorWindowRefresh()) {
        return characterSelectorWindowFatalError(false);
    }

    // Load and scale character selector buttons
    int fid;

    // Load arrow button graphics (Next/Previous)
    fid = buildFid(OBJ_TYPE_INTERFACE, 122, 0, 0, 0);
    if (!_previousButtonNormalFrmImage.lock(fid)) return characterSelectorWindowFatalError(false);

    fid = buildFid(OBJ_TYPE_INTERFACE, 123, 0, 0, 0);
    if (!_previousButtonPressedFrmImage.lock(fid)) return characterSelectorWindowFatalError(false);

    fid = buildFid(OBJ_TYPE_INTERFACE, 124, 0, 0, 0);
    if (!_nextButtonNormalFrmImage.lock(fid)) return characterSelectorWindowFatalError(false);

    fid = buildFid(OBJ_TYPE_INTERFACE, 125, 0, 0, 0);
    if (!_nextButtonPressedFrmImage.lock(fid)) return characterSelectorWindowFatalError(false);

    // Load round button graphics (Done, Cancel, etc.)
    fid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    if (!_takeButtonNormalFrmImage.lock(fid)) return characterSelectorWindowFatalError(false);

    fid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    if (!_takeButtonPressedFrmImage.lock(fid)) return characterSelectorWindowFatalError(false);

    // Base dimensions
    int arrowBaseWidth = 20;
    int arrowBaseHeight = 18;
    int roundBaseWidth = 15;
    int roundBaseHeight = 16;

    // Scaling factors
    float scaleX = static_cast<float>(scaledWidth) / originalWidth;
    float scaleY = static_cast<float>(scaledHeight) / originalHeight;
    float buttonScale = scaleY;

    // Scaled sizes
    int arrowWidth = static_cast<int>(arrowBaseWidth * scaleX);
    int arrowHeight = static_cast<int>(arrowBaseHeight * scaleY);
    int roundWidth = static_cast<int>(roundBaseWidth * buttonScale);
    int roundHeight = static_cast<int>(roundBaseHeight * buttonScale);

    // Allocate memory for scaled buttons
    unsigned char* scaledPreviousNormal = reinterpret_cast<unsigned char*>(SDL_malloc(arrowWidth * arrowHeight));
    unsigned char* scaledPreviousPressed = reinterpret_cast<unsigned char*>(SDL_malloc(arrowWidth * arrowHeight));
    unsigned char* scaledNextNormal = reinterpret_cast<unsigned char*>(SDL_malloc(arrowWidth * arrowHeight));
    unsigned char* scaledNextPressed = reinterpret_cast<unsigned char*>(SDL_malloc(arrowWidth * arrowHeight));
    unsigned char* scaledRoundNormal = reinterpret_cast<unsigned char*>(SDL_malloc(roundWidth * roundHeight));
    unsigned char* scaledRoundPressed = reinterpret_cast<unsigned char*>(SDL_malloc(roundWidth * roundHeight));

    if (!scaledPreviousNormal || !scaledPreviousPressed || !scaledNextNormal || !scaledNextPressed || !scaledRoundNormal || !scaledRoundPressed) {
        return characterSelectorWindowFatalError(false);
    }

    // Stretch arrow buttons
    blitBufferToBufferStretch(
        _previousButtonNormalFrmImage.getData(), arrowBaseWidth, arrowBaseHeight, arrowBaseWidth,
        scaledPreviousNormal, arrowWidth, arrowHeight, arrowWidth);
    blitBufferToBufferStretch(
        _previousButtonPressedFrmImage.getData(), arrowBaseWidth, arrowBaseHeight, arrowBaseWidth,
        scaledPreviousPressed, arrowWidth, arrowHeight, arrowWidth);
    blitBufferToBufferStretch(
        _nextButtonNormalFrmImage.getData(), arrowBaseWidth, arrowBaseHeight, arrowBaseWidth,
        scaledNextNormal, arrowWidth, arrowHeight, arrowWidth);
    blitBufferToBufferStretch(
        _nextButtonPressedFrmImage.getData(), arrowBaseWidth, arrowBaseHeight, arrowBaseWidth,
        scaledNextPressed, arrowWidth, arrowHeight, arrowWidth);

    // Stretch round buttons
    blitBufferToBufferStretch(
        _takeButtonNormalFrmImage.getData(), roundBaseWidth, roundBaseHeight, roundBaseWidth,
        scaledRoundNormal, roundWidth, roundHeight, roundWidth);
    blitBufferToBufferStretch(
        _takeButtonPressedFrmImage.getData(), roundBaseWidth, roundBaseHeight, roundBaseWidth,
        scaledRoundPressed, roundWidth, roundHeight, roundWidth);

    // Fix edge glitches
    for (int y = 0; y < arrowHeight; y++) {
        scaledPreviousNormal[y * arrowWidth + (arrowWidth - 1)] = scaledPreviousNormal[y * arrowWidth + (arrowWidth - 2)];
        scaledPreviousPressed[y * arrowWidth + (arrowWidth - 1)] = scaledPreviousPressed[y * arrowWidth + (arrowWidth - 2)];
        scaledNextNormal[y * arrowWidth + (arrowWidth - 1)] = scaledNextNormal[y * arrowWidth + (arrowWidth - 2)];
        scaledNextPressed[y * arrowWidth + (arrowWidth - 1)] = scaledNextPressed[y * arrowWidth + (arrowWidth - 2)];
    }
    for (int x = 0; x < arrowWidth; x++) {
        scaledPreviousNormal[(arrowHeight - 1) * arrowWidth + x] = scaledPreviousNormal[(arrowHeight - 2) * arrowWidth + x];
        scaledPreviousPressed[(arrowHeight - 1) * arrowWidth + x] = scaledPreviousPressed[(arrowHeight - 2) * arrowWidth + x];
        scaledNextNormal[(arrowHeight - 1) * arrowWidth + x] = scaledNextNormal[(arrowHeight - 2) * arrowWidth + x];
        scaledNextPressed[(arrowHeight - 1) * arrowWidth + x] = scaledNextPressed[(arrowHeight - 2) * arrowWidth + x];
    }
    for (int y = 0; y < roundHeight; y++) {
        scaledRoundNormal[y * roundWidth + (roundWidth - 1)] = scaledRoundNormal[y * roundWidth + (roundWidth - 2)];
        scaledRoundPressed[y * roundWidth + (roundWidth - 1)] = scaledRoundPressed[y * roundWidth + (roundWidth - 2)];
    }
    for (int x = 0; x < roundWidth; x++) {
        scaledRoundNormal[(roundHeight - 1) * roundWidth + x] = scaledRoundNormal[(roundHeight - 2) * roundWidth + x];
        scaledRoundPressed[(roundHeight - 1) * roundWidth + x] = scaledRoundPressed[(roundHeight - 2) * roundWidth + x];
    }

    // Create Buttons
    gCharacterSelectorWindowPreviousButton = buttonCreate(gCharacterSelectorWindow,
        SCALE_COORD_X(CS_WINDOW_PREVIOUS_BUTTON_X),
        SCALE_COORD_Y(CS_WINDOW_PREVIOUS_BUTTON_Y),
        arrowWidth,
        arrowHeight,
        -1,
        -1,
        -1,
        500,
        scaledPreviousNormal,
        scaledPreviousPressed,
        nullptr,
        0);
    if (gCharacterSelectorWindowPreviousButton == -1) return characterSelectorWindowFatalError(false);
    buttonSetCallbacks(gCharacterSelectorWindowPreviousButton, _gsound_med_butt_press, _gsound_med_butt_release);

    gCharacterSelectorWindowNextButton = buttonCreate(gCharacterSelectorWindow,
        SCALE_COORD_X(CS_WINDOW_NEXT_BUTTON_X),
        SCALE_COORD_Y(CS_WINDOW_NEXT_BUTTON_Y),
        arrowWidth,
        arrowHeight,
        -1,
        -1,
        -1,
        501,
        scaledNextNormal,
        scaledNextPressed,
        nullptr,
        0);
    if (gCharacterSelectorWindowNextButton == -1) return characterSelectorWindowFatalError(false);
    buttonSetCallbacks(gCharacterSelectorWindowNextButton, _gsound_med_butt_press, _gsound_med_butt_release);

    gCharacterSelectorWindowTakeButton = buttonCreate(gCharacterSelectorWindow,
        SCALE_COORD_X(CS_WINDOW_TAKE_BUTTON_X),
        SCALE_COORD_Y(CS_WINDOW_TAKE_BUTTON_Y),
        roundWidth,
        roundHeight,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_T,
        scaledRoundNormal,
        scaledRoundPressed,
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowTakeButton == -1) return characterSelectorWindowFatalError(false);
    buttonSetCallbacks(gCharacterSelectorWindowTakeButton, _gsound_red_butt_press, _gsound_red_butt_release);

    gCharacterSelectorWindowModifyButton = buttonCreate(gCharacterSelectorWindow,
        SCALE_COORD_X(CS_WINDOW_MODIFY_BUTTON_X),
        SCALE_COORD_Y(CS_WINDOW_MODIFY_BUTTON_Y),
        roundWidth,
        roundHeight,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_M,
        scaledRoundNormal,
        scaledRoundPressed,
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowModifyButton == -1) return characterSelectorWindowFatalError(false);
    buttonSetCallbacks(gCharacterSelectorWindowModifyButton, _gsound_red_butt_press, _gsound_red_butt_release);

    gCharacterSelectorWindowCreateButton = buttonCreate(gCharacterSelectorWindow,
        SCALE_COORD_X(CS_WINDOW_CREATE_BUTTON_X),
        SCALE_COORD_Y(CS_WINDOW_CREATE_BUTTON_Y),
        roundWidth,
        roundHeight,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_C,
        scaledRoundNormal,
        scaledRoundPressed,
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowCreateButton == -1) return characterSelectorWindowFatalError(false);
    buttonSetCallbacks(gCharacterSelectorWindowCreateButton, _gsound_red_butt_press, _gsound_red_butt_release);

    gCharacterSelectorWindowBackButton = buttonCreate(gCharacterSelectorWindow,
        SCALE_COORD_X(CS_WINDOW_BACK_BUTTON_X),
        SCALE_COORD_Y(CS_WINDOW_BACK_BUTTON_Y),
        roundWidth,
        roundHeight,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        scaledRoundNormal,
        scaledRoundPressed,
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowBackButton == -1) return characterSelectorWindowFatalError(false);
    buttonSetCallbacks(gCharacterSelectorWindowBackButton, _gsound_red_butt_press, _gsound_red_butt_release);


    gCurrentPremadeCharacter = PREMADE_CHARACTER_NARG;

    windowRefresh(gCharacterSelectorWindow);

    if (!characterSelectorWindowRefresh()) {
        return characterSelectorWindowFatalError(false);
    }

    return true;
}

// 0x4A7AD4
static void characterSelectorWindowFree()
{
    if (gCharacterSelectorWindow == -1) {
        return;
    }

    if (gCharacterSelectorWindowPreviousButton != -1) {
        buttonDestroy(gCharacterSelectorWindowPreviousButton);
        gCharacterSelectorWindowPreviousButton = -1;
    }

    _previousButtonNormalFrmImage.unlock();
    _previousButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowNextButton != -1) {
        buttonDestroy(gCharacterSelectorWindowNextButton);
        gCharacterSelectorWindowNextButton = -1;
    }

    _nextButtonNormalFrmImage.unlock();
    _nextButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowTakeButton != -1) {
        buttonDestroy(gCharacterSelectorWindowTakeButton);
        gCharacterSelectorWindowTakeButton = -1;
    }

    _takeButtonNormalFrmImage.unlock();
    _takeButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowModifyButton != -1) {
        buttonDestroy(gCharacterSelectorWindowModifyButton);
        gCharacterSelectorWindowModifyButton = -1;
    }

    _modifyButtonNormalFrmImage.unlock();
    _modifyButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowCreateButton != -1) {
        buttonDestroy(gCharacterSelectorWindowCreateButton);
        gCharacterSelectorWindowCreateButton = -1;
    }

    _createButtonNormalFrmImage.unlock();
    _createButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowBackButton != -1) {
        buttonDestroy(gCharacterSelectorWindowBackButton);
        gCharacterSelectorWindowBackButton = -1;
    }

    _backButtonNormalFrmImage.unlock();
    _backButtonPressedFrmImage.unlock();

    if (gCharacterSelectorBackground != nullptr) {
        internal_free(gCharacterSelectorBackground);
        gCharacterSelectorBackground = nullptr;
    }

    windowDestroy(gCharacterSelectorWindow);
    gCharacterSelectorWindow = -1;
}

// 0x4A7D58
static bool characterSelectorWindowRefresh() {
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s.gcd", gCustomPremadeCharacterDescriptions[gCurrentPremadeCharacter].fileName);
    premadeCharactersLocalizePath(path);

    if (_proto_dude_init(path) == -1) {
        debugPrint("\n ** Error in dude init! **\n");
        return false;
    }

    // Allocate offscreen buffer at original resolution
    uint8_t* tempBuffer = (uint8_t*) SDL_malloc(CS_WINDOW_WIDTH * CS_WINDOW_HEIGHT);
    if (tempBuffer == NULL) {
        debugPrint("\n ** Failed to allocate temp buffer for character selector **\n");
        return false;
    }

    // Clear the tempBuffer to ensure it's initialized to zero
    memset(tempBuffer, 0, CS_WINDOW_WIDTH * CS_WINDOW_HEIGHT);

    bool success = false;

    // Step 1: Render the background first
    if (!characterSelectorWindowRenderBackground(tempBuffer)) {
        SDL_free(tempBuffer);
        return false;
    }

    // Step 2: Render the face, stats, and bio onto the temporary buffer
    if (characterSelectorWindowRenderFace(tempBuffer)) {
        if (characterSelectorWindowRenderStats(tempBuffer)) {
            success = characterSelectorWindowRenderBio(tempBuffer);  // Pass tempBuffer to render bio
        }
    }

    // Step 3: Stretch the final result (background + Bio text) into the main window buffer
    blitBufferToBufferStretch(
        tempBuffer,
        CS_WINDOW_WIDTH,
        CS_WINDOW_HEIGHT,
        CS_WINDOW_WIDTH,  // srcPitch
        gCharacterSelectorWindowBuffer,
        gCharacterSelectorWindowWidth,
        gCharacterSelectorWindowHeight,
        gCharacterSelectorWindowWidth  // destPitch
    );

    // Free the temporary buffer after use
    SDL_free(tempBuffer);

    // Refresh the window to display the updated content
    windowRefresh(gCharacterSelectorWindow);

    return success;
}

// Function to render the background of the character selector window
static bool characterSelectorWindowRenderBackground(uint8_t* destBuffer) {
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 174, 0, 0, 0);
    if (!_charSelecBackgroundFrmImage.lock(backgroundFid)) {
        return false;
    }

    const unsigned char* backgroundData = _charSelecBackgroundFrmImage.getData();
    if (!backgroundData) {
        _charSelecBackgroundFrmImage.unlock();
        return false;
    }

    memcpy(destBuffer, backgroundData, CS_WINDOW_WIDTH * CS_WINDOW_HEIGHT);
    _charSelecBackgroundFrmImage.unlock();
    return true;
}

// 0x4A7E08
static bool characterSelectorWindowRenderFace(uint8_t* tempBuffer)
{
    bool success = false;

    FrmImage faceFrmImage;
    int faceFid = buildFid(OBJ_TYPE_INTERFACE, gCustomPremadeCharacterDescriptions[gCurrentPremadeCharacter].face, 0, 0, 0);
    if (faceFrmImage.lock(faceFid)) {
        unsigned char* data = faceFrmImage.getData();
        if (data != nullptr) {
            int width = faceFrmImage.getWidth();
            int height = faceFrmImage.getHeight();
            blitBufferToBufferTrans(data, width, height, width, (tempBuffer + CS_WINDOW_WIDTH * 23 + 27), CS_WINDOW_WIDTH);
            success = true;
        }
        faceFrmImage.unlock();
    }

    return success;
}

// 0x4A7EA8
static bool characterSelectorWindowRenderStats(uint8_t* tempBuffer)
{
    char* str;
    char text[260];
    int length;
    int value;
    MessageListItem messageListItem;

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    fontGetCharacterWidth(0x20);

    int vh = fontGetLineHeight();
    int y = 40;

    // NAME
    str = objectGetName(gDude);
    strcpy(text, str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_NAME_MID_X - (length / 2), text, 160, CS_WINDOW_WIDTH, _colorTable[992]);

    // STRENGTH
    y += vh + vh + vh;

    value = critterGetStat(gDude, STAT_STRENGTH);
    str = statGetName(STAT_STRENGTH);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // PERCEPTION
    y += vh;

    value = critterGetStat(gDude, STAT_PERCEPTION);
    str = statGetName(STAT_PERCEPTION);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // ENDURANCE
    y += vh;

    value = critterGetStat(gDude, STAT_ENDURANCE);
    str = statGetName(STAT_ENDURANCE);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // CHARISMA
    y += vh;

    value = critterGetStat(gDude, STAT_CHARISMA);
    str = statGetName(STAT_CHARISMA);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // INTELLIGENCE
    y += vh;

    value = critterGetStat(gDude, STAT_INTELLIGENCE);
    str = statGetName(STAT_INTELLIGENCE);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // AGILITY
    y += vh;

    value = critterGetStat(gDude, STAT_AGILITY);
    str = statGetName(STAT_AGILITY);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // LUCK
    y += vh;

    value = critterGetStat(gDude, STAT_LUCK);
    str = statGetName(STAT_LUCK);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    y += vh; // blank line

    // HIT POINTS
    y += vh;

    messageListItem.num = 16;
    text[0] = '\0';
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        strcpy(text, messageListItem.text);
    }

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);
    snprintf(text, sizeof(text), " %d/%d", critterGetHitPoints(gDude), value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // ARMOR CLASS
    y += vh;

    str = statGetName(STAT_ARMOR_CLASS);
    strcpy(text, str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_ARMOR_CLASS);
    snprintf(text, sizeof(text), " %d", value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // ACTION POINTS
    y += vh;

    messageListItem.num = 15;
    text[0] = '\0';
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        strcpy(text, messageListItem.text);
    }

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS);
    snprintf(text, sizeof(text), " %d", value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // MELEE DAMAGE
    y += vh;

    str = statGetName(STAT_MELEE_DAMAGE);
    strcpy(text, str);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_MELEE_DAMAGE);
    snprintf(text, sizeof(text), " %d", value);

    length = fontGetStringWidth(text);
    fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    y += vh; // blank line

    // SKILLS
    int skills[DEFAULT_TAGGED_SKILLS];
    skillsGetTagged(skills, DEFAULT_TAGGED_SKILLS);

    for (int index = 0; index < DEFAULT_TAGGED_SKILLS; index++) {
        y += vh;

        str = skillGetName(skills[index]);
        strcpy(text, str);

        length = fontGetStringWidth(text);
        fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

        value = skillGetValue(gDude, skills[index]);
        snprintf(text, sizeof(text), " %d%%", value);

        length = fontGetStringWidth(text);
        fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);
    }

    // TRAITS
    int traits[TRAITS_MAX_SELECTED_COUNT];
    traitsGetSelected(&(traits[0]), &(traits[1]));

    for (int index = 0; index < TRAITS_MAX_SELECTED_COUNT; index++) {
        y += vh;

        str = traitGetName(traits[index]);
        strcpy(text, str);

        length = fontGetStringWidth(text);
        fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);
    }

    fontSetCurrent(oldFont);

    return true;
}

// 0x4A8AE4
static bool characterSelectorWindowRenderBio(uint8_t* tempBuffer)
{
    // Save current font and set to a custom font for the Bio text
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    // Construct the path to the bio file based on the current premade character
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s.bio", gCustomPremadeCharacterDescriptions[gCurrentPremadeCharacter].fileName);
    premadeCharactersLocalizePath(path);

    // Open the bio file
    File* stream = fileOpen(path, "rt");
    if (stream != nullptr) {
        int y = 40;  // Starting y position for Bio text
        int lineHeight = fontGetLineHeight();  // Get the line height of the font

        char string[256];
        while (fileReadString(string, 256, stream) && y < 260) {
            // Render each line of text into the tempBuffer
            fontDrawText(tempBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_BIO_X, string, CS_WINDOW_WIDTH - CS_WINDOW_BIO_X, CS_WINDOW_WIDTH, _colorTable[992]);
            y += lineHeight;  // Move to the next line
        }

        // Close the bio file after reading
        fileClose(stream);
    }

    // Restore the original font
    fontSetCurrent(oldFont);

    return true;
}

// NOTE: Inlined.
//
// 0x4A8BD0
static bool characterSelectorWindowFatalError(bool result)
{
    characterSelectorWindowFree();
    return result;
}

void premadeCharactersInit()
{
    char* fileNamesString;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PREMADE_CHARACTERS_FILE_NAMES_KEY, &fileNamesString);
    if (fileNamesString != nullptr && *fileNamesString == '\0') {
        fileNamesString = nullptr;
    }

    char* faceFidsString;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PREMADE_CHARACTERS_FACE_FIDS_KEY, &faceFidsString);
    if (faceFidsString != nullptr && *faceFidsString == '\0') {
        faceFidsString = nullptr;
    }

    if (fileNamesString != nullptr && faceFidsString != nullptr) {
        int fileNamesLength = 0;
        for (char* pch = fileNamesString; pch != nullptr; pch = strchr(pch + 1, ',')) {
            fileNamesLength++;
        }

        int faceFidsLength = 0;
        for (char* pch = faceFidsString; pch != nullptr; pch = strchr(pch + 1, ',')) {
            faceFidsLength++;
        }

        int premadeCharactersCount = std::min(fileNamesLength, faceFidsLength);
        gCustomPremadeCharacterDescriptions.resize(premadeCharactersCount);

        for (int index = 0; index < premadeCharactersCount; index++) {
            char* pch;

            pch = strchr(fileNamesString, ',');
            if (pch != nullptr) {
                *pch = '\0';
            }

            if (strlen(fileNamesString) > 11) {
                // Sfall fails here.
                continue;
            }

            snprintf(gCustomPremadeCharacterDescriptions[index].fileName, sizeof(gCustomPremadeCharacterDescriptions[index].fileName), "premade\\%s", fileNamesString);

            if (pch != nullptr) {
                *pch = ',';
            }

            fileNamesString = pch + 1;

            pch = strchr(faceFidsString, ',');
            if (pch != nullptr) {
                *pch = '\0';
            }

            gCustomPremadeCharacterDescriptions[index].face = atoi(faceFidsString);

            if (pch != nullptr) {
                *pch = ',';
            }

            faceFidsString = pch + 1;

            gCustomPremadeCharacterDescriptions[index].field_18[0] = '\0';
        }
    }

    if (gCustomPremadeCharacterDescriptions.empty()) {
        gCustomPremadeCharacterDescriptions.resize(PREMADE_CHARACTER_COUNT);

        for (int index = 0; index < PREMADE_CHARACTER_COUNT; index++) {
            strcpy(gCustomPremadeCharacterDescriptions[index].fileName, gPremadeCharacterDescriptions[index].fileName);
            gCustomPremadeCharacterDescriptions[index].face = gPremadeCharacterDescriptions[index].face;
            strcpy(gCustomPremadeCharacterDescriptions[index].field_18, gPremadeCharacterDescriptions[index].field_18);
        }
    }

    gPremadeCharacterCount = gCustomPremadeCharacterDescriptions.size();
}

void premadeCharactersExit()
{
    gCustomPremadeCharacterDescriptions.clear();
}

static void premadeCharactersLocalizePath(char* path)
{
    if (compat_strnicmp(path, "premade\\", 8) != 0) {
        return;
    }

    const char* language = settings.system.language.c_str();
    if (compat_stricmp(language, ENGLISH) == 0) {
        return;
    }

    char localizedPath[COMPAT_MAX_PATH];
    strncpy(localizedPath, path, 8);
    strcpy(localizedPath + 8, language);
    strcpy(localizedPath + 8 + strlen(language), path + 7);

    int fileSize;
    if (dbGetFileSize(localizedPath, &fileSize) == 0) {
        strcpy(path, localizedPath);
    }
}

} // namespace fallout
