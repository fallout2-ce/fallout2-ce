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
#define MAIN_MENU_BUTTON_X 30
#define MAIN_MENU_BUTTON_Y 19
#define MAIN_MENU_BUTTON_Y_STEP 41
#define MAIN_MENU_BUTTON_LABEL_X 126
#define MAIN_MENU_BUTTON_LABEL_Y 20
#define MAIN_MENU_BUTTON_PANEL_COMPENSATION_X 8.0f
#define MAIN_MENU_BUTTON_PANEL_COMPENSATION_Y 6.0f
#define MAIN_MENU_FOOTER_LEFT_X 15
#define MAIN_MENU_FOOTER_RIGHT_MARGIN 25
#define MAIN_MENU_COPYRIGHT_Y 460
#define MAIN_MENU_VERSION_Y 440
#define MAIN_MENU_BUILD_HASH_Y 450
#define MAIN_MENU_BUILD_DATE_Y 460

typedef enum MainMenuButton {
    MAIN_MENU_BUTTON_INTRO,
    MAIN_MENU_BUTTON_NEW_GAME,
    MAIN_MENU_BUTTON_LOAD_GAME,
    MAIN_MENU_BUTTON_OPTIONS,
    MAIN_MENU_BUTTON_CREDITS,
    MAIN_MENU_BUTTON_EXIT,
    MAIN_MENU_BUTTON_COUNT,
} MainMenuButton;

enum class MenuArt {
    Vanilla,
    Hires,
};

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

struct MainMenuButtonSpec {
    int keyCode;
    int returnValue;
    int labelMessage;
};

// 0x519510 button_values / 0x519528 return_values
static const MainMenuButtonSpec mainMenuButtonSpecs[MAIN_MENU_BUTTON_COUNT] = {
    { KEY_LOWERCASE_I, MAIN_MENU_INTRO, 9 },
    { KEY_LOWERCASE_N, MAIN_MENU_NEW_GAME, 10 },
    { KEY_LOWERCASE_L, MAIN_MENU_LOAD_GAME, 11 },
    { KEY_LOWERCASE_O, MAIN_MENU_OPTIONS, 12 },
    { KEY_LOWERCASE_C, MAIN_MENU_CREDITS, 13 },
    { KEY_LOWERCASE_E, MAIN_MENU_EXIT, 14 },
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
    float scaleX;
    float scaleY;
    float scale;
    MenuArt art;
    bool scaleControls;
};

struct MainMenuPoint {
    int x;
    int y;
};

struct MainMenuSize {
    int width;
    int height;
};

struct MainMenuOffsets {
    int menuX;
    int menuY;
    int creditsX;
    int creditsY;
};

static void mainMenuComputeAspectFit(int srcWidth, int srcHeight, int dstWidth, int dstHeight, int& outX, int& outY, int& outWidth, int& outHeight);
static bool mainMenuShouldAspectFit(int backgroundWidth, int backgroundHeight, int screenWidth, int screenHeight);
static bool mainMenuShouldUseVanillaArtForLayout(int backgroundWidth, int backgroundHeight);
static bool mainMenuLoadArt();
static MainMenuLayout mainMenuBuildLayout();
static void mainMenuDrawBackground(const MainMenuLayout& layout);
static MainMenuOffsets mainMenuReadOffsets(const MainMenuLayout& layout);
static void mainMenuDrawPanel(const MainMenuLayout& layout, const MainMenuOffsets& offsets);
static MainMenuPoint mainMenuTransformPoint(const MainMenuLayout& layout, int x, int y);
static MainMenuPoint mainMenuTransformOffset(const MainMenuLayout& layout, int x, int y);
static MainMenuSize mainMenuTransformSize(const MainMenuLayout& layout, int width, int height);
static int mainMenuScaleX(const MainMenuLayout& layout, int value);
static int mainMenuScaleY(const MainMenuLayout& layout, int value);
static int mainMenuScaleUniform(const MainMenuLayout& layout, int value);
static int mainMenuGetAnchoredY(const MainMenuLayout& layout, int value);
static int mainMenuGetAnchoredRightX(const MainMenuLayout& layout, int rightMargin, int width);
static void mainMenuDrawBuildInfo(const MainMenuLayout& layout, const MainMenuOffsets& offsets);
static void mainMenuGetButtonBuffers(const MainMenuLayout& layout, unsigned char*& normalData, unsigned char*& pressedData, int& width, int& height);
static bool mainMenuCreateButtons(const MainMenuLayout& layout, const MainMenuOffsets& offsets);
static void mainMenuDrawButtonLabels(const MainMenuLayout& layout, const MainMenuOffsets& offsets);

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

static bool mainMenuShouldAspectFit(int backgroundWidth, int backgroundHeight, int screenWidth, int screenHeight)
{
    return settings.ui.main_menu_scale_mode != 0 || backgroundWidth > screenWidth || backgroundHeight > screenHeight;
}

static bool mainMenuShouldUseVanillaArtForLayout(int backgroundWidth, int backgroundHeight)
{
    int backgroundX;
    int backgroundY;
    int scaledWidth;
    int scaledHeight;
    if (!mainMenuShouldAspectFit(backgroundWidth, backgroundHeight, screenGetWidth(), screenGetHeight())) {
        return false;
    }

    mainMenuComputeAspectFit(backgroundWidth, backgroundHeight, screenGetWidth(), screenGetHeight(), backgroundX, backgroundY, scaledWidth, scaledHeight);

    return scaledWidth == MAIN_MENU_LOGICAL_WIDTH && scaledHeight == MAIN_MENU_LOGICAL_HEIGHT;
}

static bool mainMenuLoadArt()
{
    bool canUseHiresArt = screenGetWidth() != MAIN_MENU_LOGICAL_WIDTH || screenGetHeight() != MAIN_MENU_LOGICAL_HEIGHT;
    if (canUseHiresArt && _mainMenuBackgroundFrmImage.lock(FrmId(OBJ_TYPE_INTERFACE, "HR_MAINMENU.FRM"))) {
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
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 140, 0, 0, 0);
        if (!_mainMenuBackgroundFrmImage.lock(backgroundFid)) {
            debugPrint("MAINMENU: failed to load vanilla mainmenu.frm\n");
            return false;
        }
        debugPrint("MAINMENU: loaded vanilla mainmenu.frm (%dx%d)\n",
            _mainMenuBackgroundFrmImage.getWidth(),
            _mainMenuBackgroundFrmImage.getHeight());
    }

    int fid = buildFid(OBJ_TYPE_INTERFACE, 299, 0, 0, 0);
    if (!_mainMenuButtonNormalFrmImage.lock(fid)) {
        return false;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 300, 0, 0, 0);
    if (!_mainMenuButtonPressedFrmImage.lock(fid)) {
        return false;
    }

    return true;
}

static MainMenuLayout mainMenuBuildLayout()
{
    MainMenuLayout layout {};
    layout.screenWidth = screenGetWidth();
    layout.screenHeight = screenGetHeight();
    layout.art = _mainMenuBackgroundFrmImage.getWidth() != MAIN_MENU_LOGICAL_WIDTH || _mainMenuBackgroundFrmImage.getHeight() != MAIN_MENU_LOGICAL_HEIGHT
        ? MenuArt::Hires
        : MenuArt::Vanilla;

    int backgroundWidth = _mainMenuBackgroundFrmImage.getWidth();
    int backgroundHeight = _mainMenuBackgroundFrmImage.getHeight();

    bool aspectFit = mainMenuShouldAspectFit(backgroundWidth, backgroundHeight, layout.screenWidth, layout.screenHeight);
    if (aspectFit) {
        mainMenuComputeAspectFit(backgroundWidth, backgroundHeight, layout.screenWidth, layout.screenHeight, layout.backgroundX, layout.backgroundY, layout.backgroundWidth, layout.backgroundHeight);
    } else {
        layout.backgroundWidth = backgroundWidth;
        layout.backgroundHeight = backgroundHeight;
        layout.backgroundX = (layout.screenWidth - layout.backgroundWidth) / 2;
        layout.backgroundY = (layout.screenHeight - layout.backgroundHeight) / 2;
    }

    layout.scaleX = layout.backgroundWidth / 640.0f;
    layout.scaleY = layout.backgroundHeight / 480.0f;
    layout.scale = layout.scaleY;
    layout.scaleControls = layout.art == MenuArt::Vanilla || settings.ui.main_menu_scale_buttons_and_text;
    return layout;
}

static void mainMenuDrawBackground(const MainMenuLayout& layout)
{
    Buffer2D dest(gMainMenuWindowBuffer, layout.screenWidth, layout.screenHeight);
    bufferFill2D(dest, 0);

    if (layout.backgroundWidth == _mainMenuBackgroundFrmImage.getWidth()
        && layout.backgroundHeight == _mainMenuBackgroundFrmImage.getHeight()) {
        blitBuffer2D(_mainMenuBackgroundFrmImage.getBuffer(), dest, layout.backgroundX, layout.backgroundY);
    } else {
        blitBuffer2DScaled(_mainMenuBackgroundFrmImage.getBuffer(),
            0,
            0,
            _mainMenuBackgroundFrmImage.getWidth(),
            _mainMenuBackgroundFrmImage.getHeight(),
            dest,
            layout.backgroundX,
            layout.backgroundY,
            layout.backgroundWidth,
            layout.backgroundHeight);
    }
}

static MainMenuOffsets mainMenuReadOffsets(const MainMenuLayout& layout)
{
    MainMenuOffsets offsets {};
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_x", &offsets.menuX, 0);
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_y", &offsets.menuY, 0);
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_x", &offsets.creditsX, 0);
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_y", &offsets.creditsY, 0);

    MainMenuPoint menuOffset = mainMenuTransformOffset(layout, offsets.menuX, offsets.menuY);
    offsets.menuX = menuOffset.x;
    offsets.menuY = menuOffset.y;
    return offsets;
}

static void mainMenuDrawPanel(const MainMenuLayout& layout, const MainMenuOffsets& offsets)
{
    if (layout.art != MenuArt::Hires || !gMainMenuButtonPanelFrmImage.isLocked()) {
        return;
    }

    MainMenuSize panelSize = layout.scaleControls
        ? mainMenuTransformSize(layout, gMainMenuButtonPanelFrmImage.getWidth(), gMainMenuButtonPanelFrmImage.getHeight())
        : MainMenuSize { gMainMenuButtonPanelFrmImage.getWidth(), gMainMenuButtonPanelFrmImage.getHeight() };
    MainMenuPoint panelOrigin = mainMenuTransformPoint(layout, MAIN_MENU_PANEL_OFFSET_X, MAIN_MENU_PANEL_OFFSET_Y);
    int x = panelOrigin.x + offsets.menuX;
    int y = panelOrigin.y + offsets.menuY;

    blitBuffer2DScaledTrans(gMainMenuButtonPanelFrmImage.getBuffer(),
        Buffer2D(gMainMenuWindowBuffer, layout.screenWidth, layout.screenHeight),
        x,
        y,
        panelSize.width,
        panelSize.height);
}

static MainMenuPoint mainMenuTransformPoint(const MainMenuLayout& layout, int x, int y)
{
    if (layout.scaleControls) {
        return {
            layout.backgroundX + mainMenuScaleX(layout, x),
            layout.backgroundY + mainMenuScaleY(layout, y),
        };
    }

    return {
        layout.backgroundX + x,
        layout.backgroundY + y,
    };
}

static MainMenuPoint mainMenuTransformOffset(const MainMenuLayout& layout, int x, int y)
{
    MainMenuPoint offset {
        mainMenuScaleX(layout, x),
        mainMenuScaleY(layout, y),
    };

    // Match HRP's extra compensation when the hires background is scaled but
    // the buttons/panel stay at their original size.
    if (layout.art == MenuArt::Hires && !layout.scaleControls) {
        if (layout.screenWidth != MAIN_MENU_LOGICAL_WIDTH) {
            float diff = layout.screenWidth / static_cast<float>(MAIN_MENU_LOGICAL_WIDTH);
            offset.x += static_cast<int>(lround(MAIN_MENU_BUTTON_PANEL_COMPENSATION_X * diff * diff));
        }
        if (layout.screenHeight != MAIN_MENU_LOGICAL_HEIGHT) {
            float diff = layout.screenHeight / static_cast<float>(MAIN_MENU_LOGICAL_HEIGHT);
            offset.y += static_cast<int>(lround(MAIN_MENU_BUTTON_PANEL_COMPENSATION_Y * diff * diff));
        }
    }

    return offset;
}

static MainMenuSize mainMenuTransformSize(const MainMenuLayout& layout, int width, int height)
{
    if (layout.scaleControls) {
        return {
            std::max(1, mainMenuScaleUniform(layout, width)),
            std::max(1, mainMenuScaleUniform(layout, height)),
        };
    }

    return { width, height };
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

static int mainMenuGetAnchoredY(const MainMenuLayout& layout, int value)
{
    return layout.backgroundY + layout.backgroundHeight + value - MAIN_MENU_LOGICAL_HEIGHT;
}

static int mainMenuGetAnchoredRightX(const MainMenuLayout& layout, int rightMargin, int width)
{
    return layout.backgroundX + layout.backgroundWidth - rightMargin - width;
}

static void mainMenuDrawBuildInfo(const MainMenuLayout& layout, const MainMenuOffsets& offsets)
{
    MessageListItem msg;

    // SFALL: Allow to change font color/flags of copyright/version text
    //        It's the last byte ('3C' by default) that picks the colour used. The first byte supplies additional flags for this option
    //        0x010000 - change the color for version string only
    //        0x020000 - underline text (only for the version string)
    //        0x040000 - monospace font (only for the version string)
    int fontSettings = _colorTable[21091];
    int fontSettingsSFall = 0;
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "font_color", &fontSettingsSFall, 0);
    if (fontSettingsSFall && !(fontSettingsSFall & 0x010000)) {
        fontSettings = fontSettingsSFall & 0xFF;
    }

    msg.num = 20;
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        windowDrawText(gMainMenuWindow,
            msg.text,
            0,
            layout.backgroundX + offsets.creditsX + MAIN_MENU_FOOTER_LEFT_X,
            mainMenuGetAnchoredY(layout, offsets.creditsY + MAIN_MENU_COPYRIGHT_Y),
            fontSettings | 0x06000000);
    }

    if (fontSettingsSFall) {
        fontSettings = fontSettingsSFall;
    }

    int versionFontSettings = fontSettings & ~0x010000;
    char version[VERSION_MAX];
    versionGetVersion(version, sizeof(version));
    int len = fontGetStringWidth(version);
    windowDrawText(gMainMenuWindow, version, 0, mainMenuGetAnchoredRightX(layout, MAIN_MENU_FOOTER_RIGHT_MARGIN, len), mainMenuGetAnchoredY(layout, MAIN_MENU_VERSION_Y), versionFontSettings | 0x06000000);

    char commitHash[VERSION_MAX] = "BUILD HASH: ";
    strcat(commitHash, _BUILD_HASH);
    len = fontGetStringWidth(commitHash);
    windowDrawText(gMainMenuWindow, commitHash, 0, mainMenuGetAnchoredRightX(layout, MAIN_MENU_FOOTER_RIGHT_MARGIN, len), mainMenuGetAnchoredY(layout, MAIN_MENU_BUILD_HASH_Y), versionFontSettings | 0x06000000);

    char buildDate[VERSION_MAX] = "DATE: ";
    strcat(buildDate, _BUILD_DATE);
    len = fontGetStringWidth(buildDate);
    windowDrawText(gMainMenuWindow, buildDate, 0, mainMenuGetAnchoredRightX(layout, MAIN_MENU_FOOTER_RIGHT_MARGIN, len), mainMenuGetAnchoredY(layout, MAIN_MENU_BUILD_DATE_Y), versionFontSettings | 0x06000000);
}

static void mainMenuGetButtonBuffers(const MainMenuLayout& layout, unsigned char*& normalData, unsigned char*& pressedData, int& width, int& height)
{
    MainMenuSize buttonSize = mainMenuTransformSize(layout, MAIN_MENU_BUTTON_WIDTH, MAIN_MENU_BUTTON_HEIGHT);
    width = buttonSize.width;
    height = buttonSize.height;

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

static bool mainMenuCreateButtons(const MainMenuLayout& layout, const MainMenuOffsets& offsets)
{
    unsigned char* buttonNormalData;
    unsigned char* buttonPressedData;
    int buttonWidth;
    int buttonHeight;
    mainMenuGetButtonBuffers(layout, buttonNormalData, buttonPressedData, buttonWidth, buttonHeight);

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        MainMenuPoint buttonPosition = mainMenuTransformPoint(layout, MAIN_MENU_BUTTON_X, MAIN_MENU_BUTTON_Y + index * MAIN_MENU_BUTTON_Y_STEP);
        gMainMenuButtons[index] = buttonCreate(gMainMenuWindow,
            buttonPosition.x + offsets.menuX,
            buttonPosition.y + offsets.menuY,
            buttonWidth,
            buttonHeight,
            -1,
            -1,
            1111,
            mainMenuButtonSpecs[index].keyCode,
            buttonNormalData,
            buttonPressedData,
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (gMainMenuButtons[index] == -1) {
            return false;
        }

        buttonSetMask(gMainMenuButtons[index], buttonNormalData);
    }

    return true;
}

static void mainMenuDrawButtonLabels(const MainMenuLayout& layout, const MainMenuOffsets& offsets)
{
    MessageListItem msg;
    int fontSettings = _colorTable[21091];
    int fontSettingsSFall = 0;
    configGetInt(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "big_font_color", &fontSettingsSFall, 0);
    if (fontSettingsSFall) {
        fontSettings = fontSettingsSFall & 0xFF;
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        msg.num = mainMenuButtonSpecs[index].labelMessage;
        if (!messageListGetItem(&gMiscMessageList, &msg)) {
            continue;
        }

        int len = fontGetStringWidth(msg.text);
        MainMenuPoint labelPosition = mainMenuTransformPoint(layout, MAIN_MENU_BUTTON_LABEL_X, MAIN_MENU_BUTTON_LABEL_Y + index * MAIN_MENU_BUTTON_Y_STEP);
        if (!layout.scaleControls) {
            fontDrawText(gMainMenuWindowBuffer + (labelPosition.y + offsets.menuY) * layout.screenWidth + labelPosition.x + offsets.menuX - (len / 2),
                msg.text,
                layout.screenWidth - (labelPosition.x + offsets.menuX - (len / 2)) - 1,
                layout.screenWidth,
                fontSettings);
        } else {
            int scaledLen = interfaceFontGetStringWidthScaled(msg.text, fontSettings, layout.scale);
            interfaceFontDrawTextScaled2D(Buffer2D(gMainMenuWindowBuffer, layout.screenWidth, layout.screenHeight),
                labelPosition.x + offsets.menuX - scaledLen / 2,
                labelPosition.y + offsets.menuY,
                msg.text,
                fontSettings,
                layout.scale);
        }
    }
}

// 0x481650 main_menu_create
int mainMenuWindowInit()
{
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
    MainMenuOffsets offsets = mainMenuReadOffsets(layout);

    mainMenuDrawBackground(layout);
    mainMenuDrawPanel(layout, offsets);

    int oldFont = fontGetCurrent();
    fontSetCurrent(100);
    mainMenuDrawBuildInfo(layout, offsets);

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = -1;
    }

    if (!mainMenuCreateButtons(layout, offsets)) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    fontSetCurrent(104);
    mainMenuDrawButtonLabels(layout, offsets);

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
    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        if (gMainMenuButtons[index] != -1) {
            buttonDestroy(gMainMenuButtons[index]);
            gMainMenuButtons[index] = -1;
        }
    }

    gMainMenuScaledButtonData.clear();
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
    gMainMenuWindowHidden = true;
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
            int buttonKeyCode = mainMenuButtonSpecs[buttonIndex].keyCode;
            if (keyCode == buttonKeyCode || keyCode == toupper(buttonKeyCode)) {
                // NOTE: Uninline.
                main_menu_play_sound("nmselec1");

                rc = mainMenuButtonSpecs[buttonIndex].returnValue;

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
