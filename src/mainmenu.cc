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
#include "geometry.h"
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

static FrmImage mainMenuBackgroundFrmImage;
static FrmImage mainMenuButtonPanelFrmImage;
static FrmImage mainMenuButtonNormalFrmImage;
static FrmImage mainMenuButtonPressedFrmImage;
static std::vector<unsigned char> mainMenuScaledButtonData;

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

struct MainMenuOffsets {
    int menuX;
    int menuY;
    int creditsX;
    int creditsY;
};

static void mainMenuComputeAspectFit(int srcWidth, int srcHeight, int& outX, int& outY, int& outWidth, int& outHeight);
static bool mainMenuShouldAspectFit(int backgroundWidth, int backgroundHeight);
static bool mainMenuShouldUseVanillaArtForLayout(int backgroundWidth, int backgroundHeight);
static bool mainMenuLoadArt();
static MainMenuLayout mainMenuBuildLayout();
static void mainMenuDrawBackground(const MainMenuLayout& layout);
static MainMenuOffsets mainMenuReadOffsets(const MainMenuLayout& layout);
static void mainMenuDrawPanel(const MainMenuLayout& layout, const MainMenuOffsets& offsets);
static Point mainMenuTransformPoint(const MainMenuLayout& layout, int x, int y);
static Size mainMenuTransformSize(const MainMenuLayout& layout, int width, int height);
static int mainMenuScaleX(const MainMenuLayout& layout, int value);
static int mainMenuScaleY(const MainMenuLayout& layout, int value);
static int mainMenuGetAnchoredY(const MainMenuLayout& layout, int value);
static int mainMenuGetAnchoredRightX(const MainMenuLayout& layout, int rightMargin, int width);
static void mainMenuDrawBuildInfo(const MainMenuLayout& layout, const MainMenuOffsets& offsets);
static bool mainMenuCreateButtons(const MainMenuLayout& layout, const MainMenuOffsets& offsets);
static void mainMenuDrawButtonLabels(const MainMenuLayout& layout, const MainMenuOffsets& offsets);

static void mainMenuComputeAspectFit(int srcWidth, int srcHeight, int& outX, int& outY, int& outWidth, int& outHeight)
{
    int dstWidth = screenGetWidth();
    int dstHeight = screenGetHeight();
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

static bool mainMenuShouldAspectFit(int backgroundWidth, int backgroundHeight)
{
    return settings.ui.main_menu_scale_mode != 0 || backgroundWidth > screenGetWidth() || backgroundHeight > screenGetHeight();
}

static bool mainMenuShouldUseVanillaArtForLayout(int backgroundWidth, int backgroundHeight)
{
    int x, y, width, height;
    if (!mainMenuShouldAspectFit(backgroundWidth, backgroundHeight)) {
        return false;
    }

    mainMenuComputeAspectFit(backgroundWidth, backgroundHeight, x, y, width, height);
    return width == MAIN_MENU_LOGICAL_WIDTH && height == MAIN_MENU_LOGICAL_HEIGHT;
}

static bool mainMenuLoadArt()
{
    bool canUseHiresArt = screenGetWidth() != MAIN_MENU_LOGICAL_WIDTH || screenGetHeight() != MAIN_MENU_LOGICAL_HEIGHT;
    if (canUseHiresArt && mainMenuBackgroundFrmImage.lock(FrmId(OBJ_TYPE_INTERFACE, "HR_MAINMENU.FRM"))) {
        if (mainMenuShouldUseVanillaArtForLayout(mainMenuBackgroundFrmImage.getWidth(), mainMenuBackgroundFrmImage.getHeight())) {
            // use Vanilla art if not scaling to reduce artifacts
            mainMenuBackgroundFrmImage.unlock();
        } else {
            // for highres main menu art, use separate panel art
            if (!mainMenuButtonPanelFrmImage.lock(FrmId(OBJ_TYPE_INTERFACE, "HR_MENU_BG.FRM"))) {
                debugPrint("MAINMENU: failed to load hires HR_MENU_BG.FRM, falling back to vanilla mainmenu.frm\n");
                mainMenuBackgroundFrmImage.unlock();
            }
        }
    }

    if (!mainMenuBackgroundFrmImage.isLocked()) {
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 140, 0, 0, 0);
        if (!mainMenuBackgroundFrmImage.lock(backgroundFid)) {
            debugPrint("MAINMENU: failed to load vanilla mainmenu.frm\n");
            return false;
        }
    }

    int fid = buildFid(OBJ_TYPE_INTERFACE, 299, 0, 0, 0);
    if (!mainMenuButtonNormalFrmImage.lock(fid)) {
        return false;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 300, 0, 0, 0);
    if (!mainMenuButtonPressedFrmImage.lock(fid)) {
        return false;
    }

    return true;
}

static MainMenuLayout mainMenuBuildLayout()
{
    MainMenuLayout layout {};
    layout.screenWidth = screenGetWidth();
    layout.screenHeight = screenGetHeight();
    layout.art = mainMenuBackgroundFrmImage.getWidth() != MAIN_MENU_LOGICAL_WIDTH || mainMenuBackgroundFrmImage.getHeight() != MAIN_MENU_LOGICAL_HEIGHT
        ? MenuArt::Hires
        : MenuArt::Vanilla;

    int backgroundWidth = mainMenuBackgroundFrmImage.getWidth();
    int backgroundHeight = mainMenuBackgroundFrmImage.getHeight();

    bool aspectFit = mainMenuShouldAspectFit(backgroundWidth, backgroundHeight);
    if (aspectFit) {
        mainMenuComputeAspectFit(backgroundWidth, backgroundHeight, layout.backgroundX, layout.backgroundY, layout.backgroundWidth, layout.backgroundHeight);
    } else {
        layout.backgroundWidth = backgroundWidth;
        layout.backgroundHeight = backgroundHeight;
        layout.backgroundX = (layout.screenWidth - layout.backgroundWidth) / 2;
        layout.backgroundY = (layout.screenHeight - layout.backgroundHeight) / 2;
    }

    layout.scaleX = layout.backgroundWidth / static_cast<float>(MAIN_MENU_LOGICAL_WIDTH);
    layout.scaleY = layout.backgroundHeight / static_cast<float>(MAIN_MENU_LOGICAL_HEIGHT);
    layout.scaleControls = layout.art == MenuArt::Vanilla || settings.ui.main_menu_scale_mode >= 2;
    layout.scale = layout.scaleControls ? layout.scaleY : 1.0f;
    return layout;
}

static void mainMenuDrawBackground(const MainMenuLayout& layout)
{
    Buffer2D dest(gMainMenuWindowBuffer, layout.screenWidth, layout.screenHeight);
    bufferFill2D(dest, 0);

    if (layout.backgroundWidth == mainMenuBackgroundFrmImage.getWidth()
        && layout.backgroundHeight == mainMenuBackgroundFrmImage.getHeight()) {
        blitBuffer2D(mainMenuBackgroundFrmImage.getBuffer(), dest, layout.backgroundX, layout.backgroundY);
    } else {
        blitBuffer2DScaled(mainMenuBackgroundFrmImage.getBuffer(),
            0,
            0,
            mainMenuBackgroundFrmImage.getWidth(),
            mainMenuBackgroundFrmImage.getHeight(),
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

    offsets.menuX = mainMenuScaleX(layout, offsets.menuX);
    offsets.menuY = mainMenuScaleY(layout, offsets.menuY);

    // Match HRP's extra compensation when the hires background is scaled but
    // the buttons/panel stay at their original size.  It uses this funny quadratic scaling to place the menu
    if (layout.art == MenuArt::Hires && !layout.scaleControls) {
        if (layout.screenWidth != MAIN_MENU_LOGICAL_WIDTH) {
            float diff = layout.screenWidth / static_cast<float>(MAIN_MENU_LOGICAL_WIDTH);
            offsets.menuX += static_cast<int>(lround(8.0f * diff * diff));
        }
        if (layout.screenHeight != MAIN_MENU_LOGICAL_HEIGHT) {
            float diff = layout.screenHeight / static_cast<float>(MAIN_MENU_LOGICAL_HEIGHT);
            offsets.menuY += static_cast<int>(lround(6.0f * diff * diff));
        }
    }

    return offsets;
}

static void mainMenuDrawPanel(const MainMenuLayout& layout, const MainMenuOffsets& offsets)
{
    if (layout.art != MenuArt::Hires || !mainMenuButtonPanelFrmImage.isLocked()) {
        return;
    }

    Size panelSize = mainMenuTransformSize(layout, mainMenuButtonPanelFrmImage.getWidth(), mainMenuButtonPanelFrmImage.getHeight());
    Point panelOrigin = mainMenuTransformPoint(layout, MAIN_MENU_PANEL_OFFSET_X, MAIN_MENU_PANEL_OFFSET_Y);

    blitBuffer2DScaledTrans(mainMenuButtonPanelFrmImage.getBuffer(),
        Buffer2D(gMainMenuWindowBuffer, layout.screenWidth, layout.screenHeight),
        panelOrigin.x + offsets.menuX,
        panelOrigin.y + offsets.menuY,
        panelSize.width,
        panelSize.height);
}

static Point mainMenuTransformPoint(const MainMenuLayout& layout, int x, int y)
{
    return {
        layout.backgroundX + (layout.scaleControls ? mainMenuScaleX(layout, x) : x),
        layout.backgroundY + (layout.scaleControls ? mainMenuScaleY(layout, y) : y),
    };
}

static Size mainMenuTransformSize(const MainMenuLayout& layout, int width, int height)
{
    return {
        std::max(1, static_cast<int>(lround(width * layout.scale))),
        std::max(1, static_cast<int>(lround(height * layout.scale))),
    };
}

static int mainMenuScaleX(const MainMenuLayout& layout, int value)
{
    return static_cast<int>(lround(value * layout.scaleX));
}

static int mainMenuScaleY(const MainMenuLayout& layout, int value)
{
    return static_cast<int>(lround(value * layout.scaleY));
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
    int fontSettings = COLOR_YELLOW_2;
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

static bool mainMenuCreateButtons(const MainMenuLayout& layout, const MainMenuOffsets& offsets)
{
    Size buttonSize = mainMenuTransformSize(layout, MAIN_MENU_BUTTON_WIDTH, MAIN_MENU_BUTTON_HEIGHT);
    int buttonWidth = buttonSize.width;
    int buttonHeight = buttonSize.height;
    unsigned char* buttonNormalData;
    unsigned char* buttonPressedData;
    if (buttonWidth == MAIN_MENU_BUTTON_WIDTH && buttonHeight == MAIN_MENU_BUTTON_HEIGHT) {
        buttonNormalData = mainMenuButtonNormalFrmImage.getData();
        buttonPressedData = mainMenuButtonPressedFrmImage.getData();
    } else {
        mainMenuScaledButtonData.assign(buttonWidth * buttonHeight * 2, 0);
        buttonNormalData = mainMenuScaledButtonData.data();
        buttonPressedData = buttonNormalData + buttonWidth * buttonHeight;

        blitBuffer2DScaledTrans(mainMenuButtonNormalFrmImage.getBuffer(), Buffer2D(buttonNormalData, buttonWidth, buttonHeight), 0, 0, buttonWidth, buttonHeight);
        blitBuffer2DScaledTrans(mainMenuButtonPressedFrmImage.getBuffer(), Buffer2D(buttonPressedData, buttonWidth, buttonHeight), 0, 0, buttonWidth, buttonHeight);
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        Point buttonPosition = mainMenuTransformPoint(layout, MAIN_MENU_BUTTON_X, MAIN_MENU_BUTTON_Y + index * MAIN_MENU_BUTTON_Y_STEP);
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
    int fontSettings = COLOR_YELLOW_2;
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
        Point labelPosition = mainMenuTransformPoint(layout, MAIN_MENU_BUTTON_LABEL_X, MAIN_MENU_BUTTON_LABEL_Y + index * MAIN_MENU_BUTTON_Y_STEP);
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

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = -1;
    }

    colorPaletteLoad("color.pal");

    gMainMenuWindow = windowCreate(0,
        0,
        screenGetWidth(),
        screenGetHeight(),
        0,
        WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP);
    if (gMainMenuWindow == -1) {
        return main_menu_fatal_error();
    }

    gMainMenuWindowBuffer = windowGetBuffer(gMainMenuWindow);

    if (!mainMenuLoadArt()) {
        return main_menu_fatal_error();
    }

    MainMenuLayout layout = mainMenuBuildLayout();
    MainMenuOffsets offsets = mainMenuReadOffsets(layout);

    mainMenuDrawBackground(layout);
    mainMenuDrawPanel(layout, offsets);

    int oldFont = fontGetCurrent();
    fontSetCurrent(100);
    mainMenuDrawBuildInfo(layout, offsets);

    if (!mainMenuCreateButtons(layout, offsets)) {
        return main_menu_fatal_error();
    }

    fontSetCurrent(104);
    mainMenuDrawButtonLabels(layout, offsets);

    fontSetCurrent(oldFont);
    mainMenuBackgroundFrmImage.unlock();
    mainMenuButtonPanelFrmImage.unlock();

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

    mainMenuScaledButtonData.clear();
    mainMenuButtonPanelFrmImage.unlock();
    mainMenuButtonPressedFrmImage.unlock();
    mainMenuButtonNormalFrmImage.unlock();
    mainMenuBackgroundFrmImage.unlock();

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
