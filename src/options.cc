#include "options.h"

#include "art.h"
#include "color.h"
#include "cycle.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "graph_lib.h"
#include "input.h"
#include "kb.h"
#include "loadsave.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "preferences.h"
#include "settings.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

#define OPTIONS_MENU_BUTTON_SAVE 500
#define OPTIONS_MENU_BUTTON_LOAD 501
#define OPTIONS_MENU_BUTTON_PREFERENCES 502
#define OPTIONS_MENU_BUTTON_EXIT 503
#define OPTIONS_MENU_BUTTON_DONE 504
#define OPTIONS_MENU_BUTTON_HELP 505

typedef enum PauseWindowFrm {
    PAUSE_WINDOW_FRM_BACKGROUND,
    PAUSE_WINDOW_FRM_DONE_BOX,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN,
    PAUSE_WINDOW_FRM_COUNT,
} PauseWindowFrm;

typedef enum OptionsWindowFrm {
    OPTIONS_WINDOW_FRM_BACKGROUND,
    OPTIONS_WINDOW_FRM_BUTTON_ON,
    OPTIONS_WINDOW_FRM_BUTTON_OFF,
    OPTIONS_WINDOW_FRM_COUNT,
} OptionsWindowFrm;

static int optionsWindowInit();
static int optionsWindowFree();
static void optionsWindowCleanup(bool restoreWorldState);
static void _ShadeScreen(bool preserveWorldState);

struct OptionsMenuButtonSpec {
    int eventCode;
    int labelMessageId;
    bool isCeMessage;
};

// 0x48FC0C
static const int pauseWindowFrmIds[PAUSE_WINDOW_FRM_COUNT] = {
    208, // charwin.frm - character editor
    209, // donebox.frm - character editor
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
};

// 0x5197C0 opgrphs
static const int optionsWindowFrmIds[OPTIONS_WINDOW_FRM_COUNT] = {
    220, // opbase.frm - character editor
    222, // opbtnon.frm - character editor
    221, // opbtnoff.frm - character editor
};

// 0x6637E8 optn_msgfl
static MessageList preferencesMessageList;
static MessageList ceOptionsMessageList;
static bool optionsMenuHelpEnabled = false;

// 0x663840 optnmesg
static MessageListItem preferencesMessageListItem;

// 0x6638FC mouse_3d_was_on
static bool optionsWindowGameMouseObjectsWasVisible;
static bool optionsWindowCursorWasHidden;

// 0x663900 optnwin
static int optionsWindow = -1;
static int pauseWindow = -1;

// 0x663908 winbuf
static unsigned char* optionsWindowBuffer;

// 0x66398C fontsave_3
static int optionsWindowOldFont;

// 0x663994 bk_enable_4
static bool optionsWindowIsoWasEnabled;

static FrmImage _optionsFrmImages[OPTIONS_WINDOW_FRM_COUNT];

static const OptionsMenuButtonSpec optionsMenuButtonSpecs[] = {
    { OPTIONS_MENU_BUTTON_SAVE, 0, false },
    { OPTIONS_MENU_BUTTON_LOAD, 1, false },
    { OPTIONS_MENU_BUTTON_PREFERENCES, 2, false },
    { OPTIONS_MENU_BUTTON_HELP, 0, true },
    { OPTIONS_MENU_BUTTON_EXIT, 3, false },
    { OPTIONS_MENU_BUTTON_DONE, 4, false },
};

static constexpr int optionsWindowButtonCount = (sizeof(optionsMenuButtonSpecs) / sizeof(optionsMenuButtonSpecs[0])) * 2;

// 0x663878 opbtns
static unsigned char* _opbtns[optionsWindowButtonCount];

// 0x48FC50 do_optionsFunc
int showOptions()
{
    ScopedGameMode gm(GameMode::kOptions);

    if (optionsWindowInit() == -1) {
        debugPrint("\nOPTION MENU: Error loading option dialog data!\n");
        return -1;
    }

    touch_set_touchscreen_mode(true);

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        if (keyCode == KEY_ESCAPE || keyCode == OPTIONS_MENU_BUTTON_DONE || _game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_RETURN:
            case KEY_UPPERCASE_O:
            case KEY_LOWERCASE_O:
            case KEY_UPPERCASE_D:
            case KEY_LOWERCASE_D:
                soundPlayFile("ib1p1xx1");
                rc = 0;
                break;
            case KEY_UPPERCASE_S:
            case KEY_LOWERCASE_S:
            case OPTIONS_MENU_BUTTON_SAVE:
                if (lsgSaveGame(LOAD_SAVE_MODE_NORMAL) == 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_L:
            case KEY_LOWERCASE_L:
            case OPTIONS_MENU_BUTTON_LOAD:
                if (lsgLoadGame(LOAD_SAVE_MODE_NORMAL) == 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_P:
            case KEY_LOWERCASE_P:
                soundPlayFile("ib1p1xx1");
                // FALLTHROUGH
            case OPTIONS_MENU_BUTTON_PREFERENCES:
                // PREFERENCES
                doPreferences(false);
                break;
            case KEY_UPPERCASE_H:
            case KEY_LOWERCASE_H:
            case OPTIONS_MENU_BUTTON_HELP:
                if (optionsMenuHelpEnabled) {
                    soundPlayFile("ib1p1xx1");
                    showHelp();
                    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                    mouseShowCursor();
                    windowRefresh(optionsWindow);
                }
                break;
            case KEY_PLUS:
            case KEY_EQUAL:
                brightnessIncrease();
                break;
            case KEY_UNDERSCORE:
            case KEY_MINUS:
                brightnessDecrease();
                break;
            case KEY_F12:
                takeScreenshot();
                break;
            case KEY_UPPERCASE_E:
            case KEY_LOWERCASE_E:
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
            case OPTIONS_MENU_BUTTON_EXIT:
                showQuitConfirmationDialog();
                break;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    optionsWindowFree();

    return rc;
}

// 0x48FE14 OptnStart
static int optionsWindowInit()
{
    int optionsWindowX = 0;
    int optionsWindowY = 0;
    int buttonY = 0;
    int buttonBufferIndex = 0;
    char path[COMPAT_MAX_PATH];

    optionsWindowOldFont = fontGetCurrent();
    optionsWindow = -1;
    for (int index = 0; index < optionsWindowButtonCount; index++) {
        _opbtns[index] = nullptr;
    }

    messageListInit(&preferencesMessageList);
    messageListInit(&ceOptionsMessageList);

    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "options.msg");
    if (!messageListLoad(&preferencesMessageList, path)) {
        goto err;
    }

    optionsMenuHelpEnabled = settings.ui.in_game_menu_help;
    if (optionsMenuHelpEnabled) {
        if (!messageListLoad(&ceOptionsMessageList, "game\\ce.msg")) {
            optionsMenuHelpEnabled = false;
        }
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        bool loaded = false;
        if (index == OPTIONS_WINDOW_FRM_BACKGROUND && optionsMenuHelpEnabled) {
            loaded = _optionsFrmImages[index].lock(FrmId(OBJ_TYPE_INTERFACE, "opbase_help.png"));
            if (!loaded) {
                optionsMenuHelpEnabled = false;
            }
        }

        if (!loaded) {
            int fid = buildFid(OBJ_TYPE_INTERFACE, optionsWindowFrmIds[index], 0, 0, 0);
            loaded = _optionsFrmImages[index].lock(fid);
        }

        if (!loaded) {
            goto err;
        }
    }

    for (int index = 0; index < optionsWindowButtonCount; index++) {
        _opbtns[index] = (unsigned char*)internal_malloc(_optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight() + 1024);
        if (_opbtns[index] == nullptr) {
            goto err;
        }

        int buttonFrmIndex = (index % 2 == 0) ? OPTIONS_WINDOW_FRM_BUTTON_OFF : OPTIONS_WINDOW_FRM_BUTTON_ON;
        memcpy(_opbtns[index], _optionsFrmImages[buttonFrmIndex].getData(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight());
    }

    optionsWindowX = (screenGetWidth() - _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getWidth()) / 2;
    optionsWindowY = (screenGetHeight() - _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getHeight()) / 2 - 60;
    optionsWindow = windowCreate(optionsWindowX,
        optionsWindowY,
        _optionsFrmImages[0].getWidth(),
        _optionsFrmImages[0].getHeight(),
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);

    if (optionsWindow == -1) {
        goto err;
    }

    optionsWindowIsoWasEnabled = isoDisable();

    optionsWindowGameMouseObjectsWasVisible = gameMouseObjectsIsVisible();
    if (optionsWindowGameMouseObjectsWasVisible) {
        gameMouseObjectsHide();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    optionsWindowCursorWasHidden = cursorIsHidden();
    if (optionsWindowCursorWasHidden) {
        mouseShowCursor();
    }

    optionsWindowBuffer = windowGetBuffer(optionsWindow);
    memcpy(optionsWindowBuffer, _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getData(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getWidth() * _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getHeight());

    fontSetCurrent(103);

    buttonY = 17;

    buttonBufferIndex = 0;
    for (const OptionsMenuButtonSpec& buttonSpec : optionsMenuButtonSpecs) {
        if (buttonSpec.eventCode == OPTIONS_MENU_BUTTON_HELP && !optionsMenuHelpEnabled) {
            continue;
        }

        const char* msg = "ERROR";
        if (buttonSpec.isCeMessage) {
            msg = getmsg(&ceOptionsMessageList, &preferencesMessageListItem, buttonSpec.labelMessageId);
        } else {
            msg = getmsg(&preferencesMessageList, &preferencesMessageListItem, buttonSpec.labelMessageId);
        }

        int textX = (_optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() - fontGetStringWidth(msg)) / 2;
        if (textX < 0) {
            textX = 0;
        }
        int textY = (_optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight() - fontGetLineHeight()) / 2 + 1;

        fontDrawText(_opbtns[buttonBufferIndex] + _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * textY + textX, msg, _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), COLOR_DARK_YELLOW);
        fontDrawText(_opbtns[buttonBufferIndex + 1] + _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * (textY + 1) + textX, msg, _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), COLOR_DARK_YELLOW_2);

        int btn = buttonCreate(optionsWindow,
            13,
            buttonY,
            _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(),
            _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight(),
            -1,
            -1,
            -1,
            buttonSpec.eventCode,
            _opbtns[buttonBufferIndex],
            _opbtns[buttonBufferIndex + 1],
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_lrg_butt_press, _gsound_lrg_butt_release);
        }

        buttonY += _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight() + 3;
        buttonBufferIndex += 2;
    }

    fontSetCurrent(101);

    windowRefresh(optionsWindow);

    return 0;

err:
    optionsWindowCleanup(false);

    return -1;
}

// 0x490244 OptnEnd
static int optionsWindowFree()
{
    fontSetCurrent(optionsWindowOldFont);
    optionsWindowCleanup(true);

    return 0;
}

static void optionsWindowCleanup(bool restoreWorldState)
{
    if (optionsWindow != -1) {
        windowDestroy(optionsWindow);
        optionsWindow = -1;
    }

    messageListFree(&ceOptionsMessageList);
    messageListFree(&preferencesMessageList);

    for (int index = 0; index < optionsWindowButtonCount; index++) {
        if (_opbtns[index] != nullptr) {
            internal_free(_opbtns[index]);
            _opbtns[index] = nullptr;
        }
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        _optionsFrmImages[index].unlock();
    }

    if (!restoreWorldState) {
        return;
    }

    if (optionsWindowGameMouseObjectsWasVisible) {
        gameMouseObjectsShow();
    }

    if (optionsWindowIsoWasEnabled) {
        isoEnable();
    }

    if (optionsWindowCursorWasHidden) {
        mouseHideCursor();
    }

    touch_set_touchscreen_mode(false);
}

// 0x4902B0 PauseWindow
// preserveWorldState is always false
int showPause(bool preserveWorldState)
{
    bool gameMouseWasVisible;
    if (!preserveWorldState) {
        optionsWindowIsoWasEnabled = isoDisable();
        colorCycleDisable();

        gameMouseWasVisible = gameMouseObjectsIsVisible();
        if (gameMouseWasVisible) {
            gameMouseObjectsHide();
        }
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    _ShadeScreen(preserveWorldState);

    FrmImage frmImages[PAUSE_WINDOW_FRM_COUNT];
    for (int index = 0; index < PAUSE_WINDOW_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, pauseWindowFrmIds[index], 0, 0, 0);
        if (!frmImages[index].lock(fid)) {
            debugPrint("\n** Error loading pause window graphics! **\n");
            return -1;
        }
    }

    if (!messageListInit(&preferencesMessageList)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "options.msg");
    if (!messageListLoad(&preferencesMessageList, path)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    int pauseWindowX = (screenGetWidth() - frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth()) / 2;
    int pauseWindowY = (screenGetHeight() - frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getHeight()) / 2;

    if (preserveWorldState) {
        pauseWindowX -= 65;
        pauseWindowY -= 24;
    } else {
        pauseWindowY -= 54;
    }

    int window = windowCreate(pauseWindowX,
        pauseWindowY,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getHeight(),
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (window == -1) {
        messageListFree(&preferencesMessageList);

        debugPrint("\n** Error opening pause window! **\n");
        return -1;
    }

    pauseWindow = window;

    unsigned char* windowBuffer = windowGetBuffer(window);
    memcpy(windowBuffer,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getData(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getHeight());

    blitBufferToBufferTrans(frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getData(),
        frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getHeight(),
        frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getWidth(),
        windowBuffer + frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * 42 + 13,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth());

    optionsWindowOldFont = fontGetCurrent();
    fontSetCurrent(103);

    char* messageItemText;

    messageItemText = getmsg(&preferencesMessageList, &preferencesMessageListItem, 300);
    fontDrawText(windowBuffer + frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * 45 + 52,
        messageItemText,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        COLOR_DARK_YELLOW);

    fontSetCurrent(104);

    messageItemText = getmsg(&preferencesMessageList, &preferencesMessageListItem, 301);
    strcpy(path, messageItemText);

    int length = fontGetStringWidth(path);
    fontDrawText(windowBuffer + frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * 10 + 2 + (frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() - length) / 2,
        path,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        COLOR_DARK_YELLOW);

    int doneBtn = buttonCreate(window,
        26,
        46,
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        504,
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].getData(),
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(window);

    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        switch (keyCode) {
        case KEY_PLUS:
        case KEY_EQUAL:
            brightnessIncrease();
            break;
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            brightnessDecrease();
            break;
        default:
            if (keyCode != -1 && keyCode != -2) {
                done = true;
            }

            if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                done = true;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (!preserveWorldState) {
        tileWindowRefresh();
    }

    windowDestroy(window);
    pauseWindow = -1;
    messageListFree(&preferencesMessageList);

    if (!preserveWorldState) {
        if (gameMouseWasVisible) {
            gameMouseObjectsShow();
        }

        if (optionsWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    }

    fontSetCurrent(optionsWindowOldFont);

    return 0;
}

// 0x490748 ShadeScreen
static void _ShadeScreen(bool preserveWorldState)
{
    if (preserveWorldState) {
        mouseHideCursor();
    } else {
        mouseHideCursor();
        tileWindowRefresh();

        int windowWidth = windowGetWidth(gIsoWindow);
        int windowHeight = windowGetHeight(gIsoWindow);
        unsigned char* windowBuffer = windowGetBuffer(gIsoWindow);
        grayscalePaletteApply(windowBuffer, windowWidth, windowHeight, windowWidth);

        windowRefresh(gIsoWindow);
    }

    mouseShowCursor();
}

// init_options_menu
// 0x4928B8 init_options_menu
int _init_options_menu()
{
    preferencesInit();

    grayscalePaletteUpdate(0, 255);

    return 0;
}

int optionsGetWindow()
{
    if (windowGetWindow(optionsWindow) != nullptr) {
        return optionsWindow;
    }

    if (windowGetWindow(pauseWindow) != nullptr) {
        return pauseWindow;
    }

    return -1;
}

} // namespace fallout
