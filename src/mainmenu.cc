#include "mainmenu.h"

#include <ctype.h>

#include "art.h"
#include "color.h"
#include "content_config.h"
#include "debug.h"
#include "draw.h"
#include "font_manager.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "palette.h"
#include "platform_compat.h"
#include "preferences.h"
#include "settings.h"
#include "svga.h"
#include "text_font.h"
#include "version.h"
#include "window_manager.h"

#include "platform/git_version.h"

#include <math.h>

#include <vector>

namespace fallout {

#define MAIN_MENU_LOGICAL_WIDTH 640
#define MAIN_MENU_LOGICAL_HEIGHT 480
#define MAIN_MENU_BUTTON_WIDTH 26
#define MAIN_MENU_BUTTON_HEIGHT 26
#define MAIN_MENU_PANEL_OFFSET_X 16
#define MAIN_MENU_PANEL_OFFSET_Y 15

typedef enum MainMenuButton {
    MAIN_MENU_BUTTON_INTRO,
    MAIN_MENU_BUTTON_NEW_GAME,
    MAIN_MENU_BUTTON_LOAD_GAME,
    MAIN_MENU_BUTTON_OPTIONS,
    MAIN_MENU_BUTTON_CREDITS,
    MAIN_MENU_BUTTON_EXIT,
    MAIN_MENU_BUTTON_COUNT,
} MainMenuButton;

static int main_menu_fatal_error();
static void main_menu_play_sound(const char* fileName);

// 0x5194F0 main_window
static int gMainMenuWindow = -1;

// 0x5194F4 main_window_buf
static unsigned char* gMainMenuWindowBuffer = nullptr;

// 0x519504 in_main_menu
static bool _in_main_menu = false;

// 0x519508 main_menu_created
static bool gMainMenuWindowInitialized = false;

static bool gMainMenuExitRequested = false;

// 0x51950C main_menu_timeout
static unsigned int gMainMenuScreensaverDelay = 120000;

// 0x519510 button_values
static const int gMainMenuButtonKeyBindings[MAIN_MENU_BUTTON_COUNT] = {
    KEY_LOWERCASE_I, // intro
    KEY_LOWERCASE_N, // new game
    KEY_LOWERCASE_L, // load game
    KEY_LOWERCASE_O, // options
    KEY_LOWERCASE_C, // credits
    KEY_LOWERCASE_E, // exit
};

// 0x519528 return_values
static const int _return_values[MAIN_MENU_BUTTON_COUNT] = {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_OPTIONS,
    MAIN_MENU_CREDITS,
    MAIN_MENU_EXIT,
};

// 0x614840 buttons
static int gMainMenuButtons[MAIN_MENU_BUTTON_COUNT];

// 0x614858 main_menu_is_hidden
static bool gMainMenuWindowHidden;

static FrmImage _mainMenuBackgroundFrmImage;
static FrmImage gMainMenuButtonPanelFrmImage;
static FrmImage _mainMenuButtonNormalFrmImage;
static FrmImage _mainMenuButtonPressedFrmImage;
static std::vector<unsigned char> gMainMenuScaledButtonData;

struct MainMenuLayout {
    int screenWidth;
    int screenHeight;
    int backgroundX;
    int backgroundY;
    int backgroundWidth;
    int backgroundHeight;
    int backgroundSrcX;
    int backgroundSrcY;
    int backgroundSrcWidth;
    int backgroundSrcHeight;
    float scaleX;
    float scaleY;
    float scale;
    bool useHiresArt;
    bool drawHiresPanel;
};

static void mainMenuComputeAspectFit(int srcWidth, int srcHeight, int dstWidth, int dstHeight, int& outX, int& outY, int& outWidth, int& outHeight);
static bool mainMenuShouldUseVanillaArtForLayout(int backgroundWidth, int backgroundHeight);
static bool mainMenuLoadArt();
static MainMenuLayout mainMenuBuildLayout();
static void mainMenuDrawBackground(const MainMenuLayout& layout);
static void mainMenuDrawPanel(const MainMenuLayout& layout);
static int mainMenuScaleX(const MainMenuLayout& layout, int value);
static int mainMenuScaleY(const MainMenuLayout& layout, int value);
static int mainMenuScaleUniform(const MainMenuLayout& layout, int value);
static void mainMenuDrawScaledText(const MainMenuLayout& layout, int x, int y, const char* text, int color);
static void mainMenuGetButtonBuffers(const MainMenuLayout& layout, unsigned char*& normalData, unsigned char*& pressedData, int& width, int& height);

static void mainMenuComputeAspectFit(int srcWidth, int srcHeight, int dstWidth, int dstHeight, int& outX, int& outY, int& outWidth, int& outHeight)
{
    outWidth = dstWidth;
    outHeight = dstHeight;

    if (dstHeight * srcWidth > dstWidth * srcHeight) {
        outHeight = dstWidth * srcHeight / srcWidth;
    } else {
        outWidth = dstHeight * srcWidth / srcHeight;
    }

    outX = (dstWidth - outWidth) / 2;
    outY = (dstHeight - outHeight) / 2;
}

static bool mainMenuShouldUseVanillaArtForLayout(int backgroundWidth, int backgroundHeight)
{
    bool aspectFit = settings.ui.main_menu_scale_mode != 0 || backgroundWidth > screenGetWidth() || backgroundHeight > screenGetHeight();
    if (!aspectFit) {
        return false;
    }

    int backgroundX;
    int backgroundY;
    int scaledWidth;
    int scaledHeight;
    mainMenuComputeAspectFit(backgroundWidth, backgroundHeight, screenGetWidth(), screenGetHeight(), backgroundX, backgroundY, scaledWidth, scaledHeight);

    return scaledWidth == MAIN_MENU_LOGICAL_WIDTH && scaledHeight == MAIN_MENU_LOGICAL_HEIGHT;
}

static bool mainMenuLoadArt()
{
    bool useHiresArt = screenGetWidth() != MAIN_MENU_LOGICAL_WIDTH || screenGetHeight() != MAIN_MENU_LOGICAL_HEIGHT;
    if (useHiresArt && _mainMenuBackgroundFrmImage.lock(FrmId(OBJ_TYPE_INTERFACE, "HR_MAINMENU.FRM"))) {
        if (mainMenuShouldUseVanillaArtForLayout(_mainMenuBackgroundFrmImage.getWidth(), _mainMenuBackgroundFrmImage.getHeight())) {
            debugPrint("MAINMENU: using vanilla art because hires layout resolves to 640x480 on %dx%d (mode=%d)\n",
                screenGetWidth(),
                screenGetHeight(),
                settings.ui.main_menu_scale_mode);
            _mainMenuBackgroundFrmImage.unlock();
        } else {
            gMainMenuButtonPanelFrmImage.lock(FrmId(OBJ_TYPE_INTERFACE, "HR_MENU_BG.FRM"));
            debugPrint("MAINMENU: loaded hires art HR_MAINMENU.FRM (%dx%d), panel=%s\n",
                _mainMenuBackgroundFrmImage.getWidth(),
                _mainMenuBackgroundFrmImage.getHeight(),
                gMainMenuButtonPanelFrmImage.isLocked() ? "yes" : "no");
        }
    }

    if (!_mainMenuBackgroundFrmImage.isLocked()) {
        useHiresArt = false;
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 140, 0, 0, 0);
        if (!_mainMenuBackgroundFrmImage.lock(backgroundFid)) {
            debugPrint("MAINMENU: failed to load vanilla mainmenu.frm\n");
            return false;
        }
        debugPrint("MAINMENU: loaded vanilla mainmenu.frm (%dx%d)\n",
            _mainMenuBackgroundFrmImage.getWidth(),
            _mainMenuBackgroundFrmImage.getHeight());
    } else {
        useHiresArt = true;
    }

    int fid = buildFid(OBJ_TYPE_INTERFACE, 299, 0, 0, 0);
    if (!_mainMenuButtonNormalFrmImage.lock(fid)) {
        debugPrint("MAINMENU: failed to load menuup.frm\n");
        return false;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 300, 0, 0, 0);
    if (!_mainMenuButtonPressedFrmImage.lock(fid)) {
        debugPrint("MAINMENU: failed to load menudown.frm\n");
        return false;
    }

    return true;
}

static MainMenuLayout mainMenuBuildLayout()
{
    MainMenuLayout layout{};
    layout.screenWidth = screenGetWidth();
    layout.screenHeight = screenGetHeight();
    layout.useHiresArt = _mainMenuBackgroundFrmImage.getWidth() != MAIN_MENU_LOGICAL_WIDTH || _mainMenuBackgroundFrmImage.getHeight() != MAIN_MENU_LOGICAL_HEIGHT;
    layout.drawHiresPanel = layout.useHiresArt && gMainMenuButtonPanelFrmImage.isLocked();

    int backgroundWidth = _mainMenuBackgroundFrmImage.getWidth();
    int backgroundHeight = _mainMenuBackgroundFrmImage.getHeight();
    layout.backgroundSrcX = 0;
    layout.backgroundSrcY = 0;
    layout.backgroundSrcWidth = backgroundWidth;
    layout.backgroundSrcHeight = backgroundHeight;

    bool aspectFit = settings.ui.main_menu_scale_mode != 0 || backgroundWidth > layout.screenWidth || backgroundHeight > layout.screenHeight;
    if (aspectFit) {
        mainMenuComputeAspectFit(backgroundWidth, backgroundHeight, layout.screenWidth, layout.screenHeight, layout.backgroundX, layout.backgroundY, layout.backgroundWidth, layout.backgroundHeight);
    } else {
        layout.backgroundWidth = backgroundWidth;
        layout.backgroundHeight = backgroundHeight;
        layout.backgroundX = (layout.screenWidth - layout.backgroundWidth) / 2;
        layout.backgroundY = (layout.screenHeight - layout.backgroundHeight) / 2;
    }

    debugPrint("MAINMENU: %s layout screen=%dx%d src=%d,%d %dx%d dst=%d,%d %dx%d scale=(%.3f,%.3f) mode=%d\n",
        aspectFit ? "aspect-fit" : "native",
        layout.screenWidth,
        layout.screenHeight,
        layout.backgroundSrcX,
        layout.backgroundSrcY,
        layout.backgroundSrcWidth,
        layout.backgroundSrcHeight,
        layout.backgroundX,
        layout.backgroundY,
        layout.backgroundWidth,
        layout.backgroundHeight,
        layout.backgroundWidth / 640.0f,
        layout.backgroundHeight / 480.0f,
        settings.ui.main_menu_scale_mode);

    layout.scaleX = layout.backgroundWidth / 640.0f;
    layout.scaleY = layout.backgroundHeight / 480.0f;
    layout.scale = layout.scaleY;
    return layout;
}

static void mainMenuDrawBackground(const MainMenuLayout& layout)
{
    Buffer2D dest(gMainMenuWindowBuffer, layout.screenWidth, layout.screenHeight);
    bufferFill2D(dest, 0);

    if (layout.backgroundSrcX == 0
        && layout.backgroundSrcY == 0
        && layout.backgroundSrcWidth == _mainMenuBackgroundFrmImage.getWidth()
        && layout.backgroundSrcHeight == _mainMenuBackgroundFrmImage.getHeight()
        && layout.backgroundWidth == _mainMenuBackgroundFrmImage.getWidth()
        && layout.backgroundHeight == _mainMenuBackgroundFrmImage.getHeight()) {
        blitBuffer2D(_mainMenuBackgroundFrmImage.getBuffer(), dest, layout.backgroundX, layout.backgroundY);
    } else {
        blitBuffer2DScaled(_mainMenuBackgroundFrmImage.getBuffer(),
            layout.backgroundSrcX,
            layout.backgroundSrcY,
            layout.backgroundSrcWidth,
            layout.backgroundSrcHeight,
            dest,
            layout.backgroundX,
            layout.backgroundY,
            layout.backgroundWidth,
            layout.backgroundHeight);
    }
}

static void mainMenuDrawPanel(const MainMenuLayout& layout)
{
    if (!layout.drawHiresPanel) {
        return;
    }

    int width = std::max(1, mainMenuScaleUniform(layout, gMainMenuButtonPanelFrmImage.getWidth()));
    int height = std::max(1, mainMenuScaleUniform(layout, gMainMenuButtonPanelFrmImage.getHeight()));
    int x = layout.backgroundX + mainMenuScaleX(layout, MAIN_MENU_PANEL_OFFSET_X);
    int y = layout.backgroundY + mainMenuScaleY(layout, MAIN_MENU_PANEL_OFFSET_Y);

    blitBuffer2DScaledTrans(gMainMenuButtonPanelFrmImage.getBuffer(),
        Buffer2D(gMainMenuWindowBuffer, layout.screenWidth, layout.screenHeight),
        x,
        y,
        width,
        height);
}

static int mainMenuScaleX(const MainMenuLayout& layout, int value)
{
    return static_cast<int>(lround(value * layout.scaleX));
}

static int mainMenuScaleY(const MainMenuLayout& layout, int value)
{
    return static_cast<int>(lround(value * layout.scaleY));
}

static int mainMenuScaleUniform(const MainMenuLayout& layout, int value)
{
    return static_cast<int>(lround(value * layout.scale));
}

static void mainMenuDrawScaledText(const MainMenuLayout& layout, int x, int y, const char* text, int color)
{
    Buffer2D dest(gMainMenuWindowBuffer, layout.screenWidth, layout.screenHeight);
    interfaceFontDrawTextScaled2D(dest, x, y, text, color, layout.scale);
}

static void mainMenuGetButtonBuffers(const MainMenuLayout& layout, unsigned char*& normalData, unsigned char*& pressedData, int& width, int& height)
{
    width = std::max(1, mainMenuScaleUniform(layout, MAIN_MENU_BUTTON_WIDTH));
    height = std::max(1, mainMenuScaleUniform(layout, MAIN_MENU_BUTTON_HEIGHT));

    if (width == MAIN_MENU_BUTTON_WIDTH && height == MAIN_MENU_BUTTON_HEIGHT) {
        normalData = _mainMenuButtonNormalFrmImage.getData();
        pressedData = _mainMenuButtonPressedFrmImage.getData();
        return;
    }

    gMainMenuScaledButtonData.assign(width * height * 2, 0);
    normalData = gMainMenuScaledButtonData.data();
    pressedData = normalData + width * height;

    blitBuffer2DScaledTrans(_mainMenuButtonNormalFrmImage.getBuffer(), Buffer2D(normalData, width, height), 0, 0, width, height);
    blitBuffer2DScaledTrans(_mainMenuButtonPressedFrmImage.getBuffer(), Buffer2D(pressedData, width, height), 0, 0, width, height);
}

// 0x481650 main_menu_create
int mainMenuWindowInit()
{
    MessageListItem msg;
    int len;

    if (gMainMenuWindowInitialized) {
        return 0;
    }

    colorPaletteLoad("color.pal");

    gMainMenuWindow = windowCreate(0,
        0,
        screenGetWidth(),
        screenGetHeight(),
        0,
        WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP);
    if (gMainMenuWindow == -1) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    gMainMenuWindowBuffer = windowGetBuffer(gMainMenuWindow);

    if (!mainMenuLoadArt()) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    MainMenuLayout layout = mainMenuBuildLayout();
    mainMenuDrawBackground(layout);
    mainMenuDrawPanel(layout);

    int oldFont = fontGetCurrent();
    fontSetCurrent(100);

    // SFALL: Allow to change font color/flags of copyright/version text
    //        It's the last byte ('3C' by default) that picks the colour used. The first byte supplies additional flags for this option
    //        0x010000 - change the color for version string only
    //        0x020000 - underline text (only for the version string)
    //        0x040000 - monospace font (only for the version string)
    int fontSettings = _colorTable[21091], fontSettingsSFall = 0;
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "font_color", &fontSettingsSFall, 0);
    if (fontSettingsSFall && !(fontSettingsSFall & 0x010000))
        fontSettings = fontSettingsSFall & 0xFF;

    // SFALL: Allow to move copyright text
    int offsetX = 0, offsetY = 0;
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_x", &offsetX, 0);
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_y", &offsetY, 0);

    // Copyright.
    msg.num = 20;
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        if (layout.scale == 1.0f) {
            windowDrawText(gMainMenuWindow, msg.text, 0, layout.backgroundX + offsetX + 15, layout.backgroundY + offsetY + 460, fontSettings | 0x06000000);
        } else {
            mainMenuDrawScaledText(layout,
                layout.backgroundX + mainMenuScaleX(layout, offsetX + 15),
                layout.backgroundY + mainMenuScaleY(layout, offsetY + 460),
                msg.text,
                fontSettings);
        }
    }

    // SFALL: Make sure font settings are applied when using 0x010000 flag
    if (fontSettingsSFall)
        fontSettings = fontSettingsSFall;
    int versionFontSettings = fontSettings & ~0x010000;

    // TODO: Allow to move version text
    // Version.
    char version[VERSION_MAX];
    versionGetVersion(version, sizeof(version));
    len = fontGetStringWidth(version);
    if (layout.scale == 1.0f) {
        windowDrawText(gMainMenuWindow, version, 0, layout.backgroundX + 615 - len, layout.backgroundY + 440, versionFontSettings | 0x06000000);
    } else {
        int scaledLen = interfaceFontGetStringWidthScaled(version, versionFontSettings, layout.scale);
        mainMenuDrawScaledText(layout,
            layout.backgroundX + mainMenuScaleX(layout, 615) - scaledLen,
            layout.backgroundY + mainMenuScaleY(layout, 440),
            version,
            versionFontSettings);
    }

    char commitHash[VERSION_MAX] = "BUILD HASH: ";
    strcat(commitHash, _BUILD_HASH);
    len = fontGetStringWidth(commitHash);
    if (layout.scale == 1.0f) {
        windowDrawText(gMainMenuWindow, commitHash, 0, layout.backgroundX + 615 - len, layout.backgroundY + 450, versionFontSettings | 0x06000000);
    } else {
        int scaledLen = interfaceFontGetStringWidthScaled(commitHash, versionFontSettings, layout.scale);
        mainMenuDrawScaledText(layout,
            layout.backgroundX + mainMenuScaleX(layout, 615) - scaledLen,
            layout.backgroundY + mainMenuScaleY(layout, 450),
            commitHash,
            versionFontSettings);
    }

    char buildDate[VERSION_MAX] = "DATE: ";
    strcat(buildDate, _BUILD_DATE);
    len = fontGetStringWidth(buildDate);
    if (layout.scale == 1.0f) {
        windowDrawText(gMainMenuWindow, buildDate, 0, layout.backgroundX + 615 - len, layout.backgroundY + 460, versionFontSettings | 0x06000000);
    } else {
        int scaledLen = interfaceFontGetStringWidthScaled(buildDate, versionFontSettings, layout.scale);
        mainMenuDrawScaledText(layout,
            layout.backgroundX + mainMenuScaleX(layout, 615) - scaledLen,
            layout.backgroundY + mainMenuScaleY(layout, 460),
            buildDate,
            versionFontSettings);
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = -1;
    }

    // SFALL: Allow to move menu buttons
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_x", &offsetX, 0);
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_y", &offsetY, 0);
    unsigned char* buttonNormalData;
    unsigned char* buttonPressedData;
    int buttonWidth;
    int buttonHeight;
    mainMenuGetButtonBuffers(layout, buttonNormalData, buttonPressedData, buttonWidth, buttonHeight);

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = buttonCreate(gMainMenuWindow,
            layout.backgroundX + mainMenuScaleX(layout, offsetX + 30),
            layout.backgroundY + mainMenuScaleY(layout, offsetY + 19 + index * 42 - index),
            buttonWidth,
            buttonHeight,
            -1,
            -1,
            1111,
            gMainMenuButtonKeyBindings[index],
            buttonNormalData,
            buttonPressedData,
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (gMainMenuButtons[index] == -1) {
            // NOTE: Uninline.
            return main_menu_fatal_error();
        }

        buttonSetMask(gMainMenuButtons[index], buttonNormalData);
    }

    fontSetCurrent(104);

    // SFALL: Allow to change font color of buttons
    fontSettings = _colorTable[21091];
    fontSettingsSFall = 0;
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "big_font_color", &fontSettingsSFall, 0);
    if (fontSettingsSFall)
        fontSettings = fontSettingsSFall & 0xFF;

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        msg.num = 9 + index;
        if (messageListGetItem(&gMiscMessageList, &msg)) {
            len = fontGetStringWidth(msg.text);
            if (layout.scale == 1.0f) {
                fontDrawText(gMainMenuWindowBuffer + (layout.backgroundY + offsetY + 42 * index - index + 20) * layout.screenWidth + layout.backgroundX + offsetX + 126 - (len / 2),
                    msg.text,
                    layout.screenWidth - (layout.backgroundX + offsetX + 126 - (len / 2)) - 1,
                    layout.screenWidth,
                    fontSettings);
            } else {
                int scaledLen = interfaceFontGetStringWidthScaled(msg.text, fontSettings, layout.scale);
                mainMenuDrawScaledText(layout,
                    layout.backgroundX + mainMenuScaleX(layout, offsetX + 126) - scaledLen / 2,
                    layout.backgroundY + mainMenuScaleY(layout, offsetY + 42 * index - index + 20),
                    msg.text,
                    fontSettings);
            }
        }
    }

    fontSetCurrent(oldFont);
    _mainMenuBackgroundFrmImage.unlock();
    gMainMenuButtonPanelFrmImage.unlock();

    gMainMenuWindowInitialized = true;
    gMainMenuWindowHidden = true;

    return 0;
}

// 0x481968 main_menu_destroy
void mainMenuWindowFree()
{
    if (!gMainMenuWindowInitialized
        && gMainMenuWindow == -1
        && !_mainMenuBackgroundFrmImage.isLocked()
        && !gMainMenuButtonPanelFrmImage.isLocked()
        && !_mainMenuButtonNormalFrmImage.isLocked()
        && !_mainMenuButtonPressedFrmImage.isLocked()
        && gMainMenuScaledButtonData.empty()) {
        return;
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        if (gMainMenuButtons[index] != -1) {
            buttonDestroy(gMainMenuButtons[index]);
            gMainMenuButtons[index] = -1;
        }
    }

    gMainMenuScaledButtonData.clear();
    gMainMenuScaledButtonData.shrink_to_fit();
    gMainMenuButtonPanelFrmImage.unlock();
    _mainMenuButtonPressedFrmImage.unlock();
    _mainMenuButtonNormalFrmImage.unlock();
    _mainMenuBackgroundFrmImage.unlock();

    if (gMainMenuWindow != -1) {
        windowDestroy(gMainMenuWindow);
    }

    gMainMenuWindowInitialized = false;
    gMainMenuWindow = -1;
    gMainMenuWindowBuffer = nullptr;
}

// 0x481A00 main_menu_hide
void mainMenuWindowHide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (gMainMenuWindowHidden) {
        return;
    }

    soundContinueAll();

    if (animate) {
        paletteFadeTo(gPaletteBlack);
        soundContinueAll();
    }

    windowHide(gMainMenuWindow);
    touch_set_touchscreen_mode(false);

    gMainMenuWindowHidden = true;
}

// 0x481A48 main_menu_show
void mainMenuWindowUnhide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (!gMainMenuWindowHidden) {
        return;
    }

    windowShow(gMainMenuWindow);
    touch_set_touchscreen_mode(true);

    if (animate) {
        colorPaletteLoad("color.pal");
        paletteFadeTo(_cmap);
    }

    gMainMenuWindowHidden = false;
}

// 0x481AA8 main_menu_is_enabled
int _main_menu_is_enabled()
{
    return 1;
}

// 0x481AEC main_menu_loop
int mainMenuWindowHandleEvents()
{
    _in_main_menu = true;

    bool oldCursorIsHidden = cursorIsHidden();
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    unsigned int tick = getTicks();

    int rc = -1;
    while (rc == -1) {
        if (gMainMenuExitRequested) {
            gMainMenuExitRequested = false;
            _game_user_wants_to_quit = GAME_QUIT_REQUEST_EXIT;
        }

        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        for (int buttonIndex = 0; buttonIndex < MAIN_MENU_BUTTON_COUNT; buttonIndex++) {
            if (keyCode == gMainMenuButtonKeyBindings[buttonIndex] || keyCode == toupper(gMainMenuButtonKeyBindings[buttonIndex])) {
                // NOTE: Uninline.
                main_menu_play_sound("nmselec1");

                rc = _return_values[buttonIndex];

                if (buttonIndex == MAIN_MENU_BUTTON_CREDITS && (gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT] != KEY_STATE_UP || gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT] != KEY_STATE_UP)) {
                    rc = MAIN_MENU_QUOTES;
                }

                break;
            }
        }

        if (rc == -1) {
            if (keyCode == KEY_CTRL_R) {
                rc = MAIN_MENU_SELFRUN;
                continue;
            } else if (keyCode == KEY_PLUS || keyCode == KEY_EQUAL) {
                brightnessIncrease();
            } else if (keyCode == KEY_MINUS || keyCode == KEY_UNDERSCORE) {
                brightnessDecrease();
            } else if (keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
                rc = MAIN_MENU_SCREENSAVER;
                continue;
            } else if (keyCode == 1111) {
                if (!(mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT)) {
                    // NOTE: Uninline.
                    main_menu_play_sound("nmselec0");
                }
                continue;
            }
        }

        if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit == GAME_QUIT_REQUEST_EXIT) {
            rc = MAIN_MENU_EXIT;

            // NOTE: Uninline.
            main_menu_play_sound("nmselec1");
            break;
        } else if (_game_user_wants_to_quit == GAME_QUIT_REQUEST_MAIN_MENU) {
            _game_user_wants_to_quit = GAME_QUIT_REQUEST_NONE;
        } else {
            if (getTicksSince(tick) >= gMainMenuScreensaverDelay) {
                rc = MAIN_MENU_TIMEOUT;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    _in_main_menu = false;

    return rc;
}

void mainMenuRequestExit()
{
    gMainMenuExitRequested = true;
}

// NOTE: Inlined.
//
// 0x481C88 main_menu_fatal_error
static int main_menu_fatal_error()
{
    mainMenuWindowFree();

    return -1;
}

// NOTE: Inlined.
//
// 0x481C94 main_menu_play_sound
static void main_menu_play_sound(const char* fileName)
{
    soundPlayFile(fileName);
}

} // namespace fallout
