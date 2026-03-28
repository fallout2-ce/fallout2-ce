#include "display_monitor.h"

#include <string.h>

#include <fstream>
#include <string>

#include "art.h"
#include "color.h"
#include "combat.h"
#include "draw.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "input.h"
#include "interface.h"
#include "memory.h"
#include "sfall_config.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"

namespace fallout {

// The maximum number of lines display monitor can hold. Once this value
// is reached earlier messages are thrown away.
#define DISPLAY_MONITOR_LINES_CAPACITY (100)

// The maximum length of a string in display monitor (in characters).
#define DISPLAY_MONITOR_LINE_LENGTH (80)

#define DISPLAY_MONITOR_X (23)
#define DISPLAY_MONITOR_Y (24)
#define DISPLAY_MONITOR_WIDTH (167 + gInterfaceBarContentOffset)
#define DISPLAY_MONITOR_HEIGHT (60)

#define DISPLAY_MONITOR_HALF_HEIGHT (DISPLAY_MONITOR_HEIGHT / 2)

#define DISPLAY_MONITOR_FONT (101)

#define DISPLAY_MONITOR_BEEP_DELAY (500U)

static void display_clear();
static void displayMonitorRefresh();
static void displayMonitorScrollUpOnMouseDown(int btn, int keyCode);
static void displayMonitorScrollDownOnMouseDown(int btn, int keyCode);
static void displayMonitorScrollUpOnMouseEnter(int btn, int keyCode);
static void displayMonitorScrollDownOnMouseEnter(int btn, int keyCode);
static void displayMonitorOnMouseExit(int btn, int keyCode);

static void consoleFileInit();
static void consoleFileReset();
static void consoleFileExit();
static void consoleFileAddMessage(const char* message);
static void consoleFileFlush();

// 0x51850C
static bool gDisplayMonitorInitialized = false;

// The rectangle that display monitor occupies in the main interface window.
//
// 0x518510
static Rect gDisplayMonitorRect;

// 0x518520
static int gDisplayMonitorScrollDownButton = -1;

// 0x518524
static int gDisplayMonitorScrollUpButton = -1;

// 0x56DBFC
static char gDisplayMonitorLines[DISPLAY_MONITOR_LINES_CAPACITY][DISPLAY_MONITOR_LINE_LENGTH];

// 0x56FB3C
static unsigned char* gDisplayMonitorBackgroundFrmData;

// 0x56FB40
static int _max_disp;

// 0x56FB44
static bool gDisplayMonitorEnabled;

// 0x56FB48
static int _disp_curr;

// 0x56FB4C
static int _intface_full_width;

// 0x56FB50
static int gDisplayMonitorLinesCapacity;

// 0x56FB54
static int _disp_start;

// 0x56FB58
static unsigned int gDisplayMonitorLastBeepTimestamp;

static std::ofstream gConsoleFileStream;
static int gConsoleFilePrintCount = 0;

// 0x431610 display_init_
int displayMonitorInit()
{
    if (!gDisplayMonitorInitialized) {
        gDisplayMonitorRect = {
            DISPLAY_MONITOR_X,
            DISPLAY_MONITOR_Y,
            DISPLAY_MONITOR_X + DISPLAY_MONITOR_WIDTH - 1,
            DISPLAY_MONITOR_Y + DISPLAY_MONITOR_HEIGHT - 1,
        };

        int oldFont = fontGetCurrent();
        fontSetCurrent(DISPLAY_MONITOR_FONT);

        gDisplayMonitorLinesCapacity = DISPLAY_MONITOR_LINES_CAPACITY;
        _max_disp = DISPLAY_MONITOR_HEIGHT / fontGetLineHeight();
        _disp_start = 0;
        _disp_curr = 0;
        fontSetCurrent(oldFont);

        gDisplayMonitorBackgroundFrmData = (unsigned char*)internal_malloc(DISPLAY_MONITOR_WIDTH * DISPLAY_MONITOR_HEIGHT);
        if (gDisplayMonitorBackgroundFrmData == nullptr) {
            return -1;
        }

        if (gInterfaceBarIsCustom) {
            _intface_full_width = gInterfaceBarWidth;
            blitBufferToBuffer(customInterfaceBarGetBackgroundImageData() + gInterfaceBarWidth * DISPLAY_MONITOR_Y + DISPLAY_MONITOR_X,
                DISPLAY_MONITOR_WIDTH,
                DISPLAY_MONITOR_HEIGHT,
                gInterfaceBarWidth,
                gDisplayMonitorBackgroundFrmData,
                DISPLAY_MONITOR_WIDTH);
        } else {
            FrmImage backgroundFrmImage;
            int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 16, 0, 0, 0);
            if (!backgroundFrmImage.lock(backgroundFid)) {
                internal_free(gDisplayMonitorBackgroundFrmData);
                return -1;
            }

            unsigned char* backgroundFrmData = backgroundFrmImage.getData();
            _intface_full_width = backgroundFrmImage.getWidth();

            blitBufferToBuffer(backgroundFrmData + _intface_full_width * DISPLAY_MONITOR_Y + DISPLAY_MONITOR_X,
                DISPLAY_MONITOR_WIDTH,
                DISPLAY_MONITOR_HEIGHT,
                _intface_full_width,
                gDisplayMonitorBackgroundFrmData,
                DISPLAY_MONITOR_WIDTH);
        }

        gDisplayMonitorScrollUpButton = buttonCreate(gInterfaceBarWindow,
            DISPLAY_MONITOR_X,
            DISPLAY_MONITOR_Y,
            DISPLAY_MONITOR_WIDTH,
            DISPLAY_MONITOR_HALF_HEIGHT,
            -1,
            -1,
            -1,
            -1,
            nullptr,
            nullptr,
            nullptr,
            0);
        if (gDisplayMonitorScrollUpButton != -1) {
            buttonSetMouseCallbacks(gDisplayMonitorScrollUpButton,
                displayMonitorScrollUpOnMouseEnter,
                displayMonitorOnMouseExit,
                displayMonitorScrollUpOnMouseDown,
                nullptr);
        }

        gDisplayMonitorScrollDownButton = buttonCreate(gInterfaceBarWindow,
            DISPLAY_MONITOR_X,
            DISPLAY_MONITOR_Y + DISPLAY_MONITOR_HALF_HEIGHT,
            DISPLAY_MONITOR_WIDTH,
            DISPLAY_MONITOR_HEIGHT - DISPLAY_MONITOR_HALF_HEIGHT,
            -1,
            -1,
            -1,
            -1,
            nullptr,
            nullptr,
            nullptr,
            0);
        if (gDisplayMonitorScrollDownButton != -1) {
            buttonSetMouseCallbacks(gDisplayMonitorScrollDownButton,
                displayMonitorScrollDownOnMouseEnter,
                displayMonitorOnMouseExit,
                displayMonitorScrollDownOnMouseDown,
                nullptr);
        }

        gDisplayMonitorEnabled = true;
        gDisplayMonitorInitialized = true;

        // NOTE: Uninline.
        display_clear();

        // SFALL
        consoleFileInit();
    }

    return 0;
}

// 0x431800 display_reset_
int displayMonitorReset()
{
    // NOTE: Uninline.
    display_clear();

    // SFALL
    consoleFileReset();

    return 0;
}

// 0x43184C display_exit_
void displayMonitorExit()
{
    if (gDisplayMonitorInitialized) {
        // SFALL
        consoleFileExit();

        internal_free(gDisplayMonitorBackgroundFrmData);
        gDisplayMonitorInitialized = false;
    }
}

// 0x43186C display_print_
void displayMonitorAddMessage(const char* str)
{
    if (!gDisplayMonitorInitialized || str == nullptr) {
        return;
    }

    // SFALL
    consoleFileAddMessage(str);

    int oldFont = fontGetCurrent();
    fontSetCurrent(DISPLAY_MONITOR_FONT);

    char knob = '\x95';

    char knobString[2];
    knobString[0] = knob;
    knobString[1] = '\0';
    int knobWidth = fontGetStringWidth(knobString);

    if (!isInCombat()) {
        unsigned int now = _get_bk_time();
        if (getTicksBetween(now, gDisplayMonitorLastBeepTimestamp) >= DISPLAY_MONITOR_BEEP_DELAY) {
            gDisplayMonitorLastBeepTimestamp = now;
            soundPlayFile("monitor");
        }
    }

    std::string mutableMessage(str);
    char* mutableStr = mutableMessage.data();

    // TODO: Refactor these two loops.
    char* splitPos = nullptr;
    while (true) {
        while (fontGetStringWidth(mutableStr) < DISPLAY_MONITOR_WIDTH - _max_disp - knobWidth) {
            char* temp = gDisplayMonitorLines[_disp_start];
            int length;
            if (knob != '\0') {
                *temp++ = knob;
                length = DISPLAY_MONITOR_LINE_LENGTH - 2;
                knob = '\0';
                knobWidth = 0;
            } else {
                length = DISPLAY_MONITOR_LINE_LENGTH - 1;
            }
            strncpy(temp, mutableStr, length);
            gDisplayMonitorLines[_disp_start][DISPLAY_MONITOR_LINE_LENGTH - 1] = '\0';
            _disp_start = (_disp_start + 1) % gDisplayMonitorLinesCapacity;

            if (splitPos == nullptr) {
                fontSetCurrent(oldFont);
                _disp_curr = _disp_start;
                displayMonitorRefresh();
                return;
            }

            mutableStr = splitPos + 1;
            *splitPos = ' ';
            splitPos = nullptr;
        }

        char* space = strrchr(mutableStr, ' ');
        if (space == nullptr) {
            break;
        }

        if (splitPos != nullptr) {
            *splitPos = ' ';
        }

        splitPos = space;
        if (space != nullptr) {
            *space = '\0';
        }
    }

    char* temp = gDisplayMonitorLines[_disp_start];
    int length;
    if (knob != '\0') {
        temp++;
        gDisplayMonitorLines[_disp_start][0] = knob;
        length = DISPLAY_MONITOR_LINE_LENGTH - 2;
        knob = '\0';
    } else {
        length = DISPLAY_MONITOR_LINE_LENGTH - 1;
    }
    strncpy(temp, mutableStr, length);

    gDisplayMonitorLines[_disp_start][DISPLAY_MONITOR_LINE_LENGTH - 1] = '\0';
    _disp_start = (_disp_start + 1) % gDisplayMonitorLinesCapacity;

    fontSetCurrent(oldFont);
    _disp_curr = _disp_start;
    displayMonitorRefresh();
}

// NOTE: Inlined.
//
// 0x431A2C display_clear_
static void display_clear()
{
    int index;

    if (gDisplayMonitorInitialized) {
        for (index = 0; index < gDisplayMonitorLinesCapacity; index++) {
            gDisplayMonitorLines[index][0] = '\0';
        }

        _disp_start = 0;
        _disp_curr = 0;
        displayMonitorRefresh();
    }
}

// 0x431A78 display_redraw_
static void displayMonitorRefresh()
{
    if (!gDisplayMonitorInitialized) {
        return;
    }

    unsigned char* buf = windowGetBuffer(gInterfaceBarWindow);
    if (buf == nullptr) {
        return;
    }

    buf += _intface_full_width * DISPLAY_MONITOR_Y + DISPLAY_MONITOR_X;
    blitBufferToBuffer(gDisplayMonitorBackgroundFrmData,
        DISPLAY_MONITOR_WIDTH,
        DISPLAY_MONITOR_HEIGHT,
        DISPLAY_MONITOR_WIDTH,
        buf,
        _intface_full_width);

    int oldFont = fontGetCurrent();
    fontSetCurrent(DISPLAY_MONITOR_FONT);

    for (int index = 0; index < _max_disp; index++) {
        int stringIndex = (_disp_curr + gDisplayMonitorLinesCapacity + index - _max_disp) % gDisplayMonitorLinesCapacity;
        fontDrawText(buf + index * _intface_full_width * fontGetLineHeight(), gDisplayMonitorLines[stringIndex], DISPLAY_MONITOR_WIDTH, _intface_full_width, _colorTable[992]);

        // Even though the display monitor is rectangular, it's graphic is not.
        // To give a feel of depth it's covered by some metal canopy and
        // considered inclined outwards. This way earlier messages appear a
        // little bit far from player's perspective. To implement this small
        // detail the destination buffer is incremented by 1.
        buf++;
    }

    windowRefreshRect(gInterfaceBarWindow, &gDisplayMonitorRect);
    fontSetCurrent(oldFont);
}

// 0x431B70 display_scroll_up_
static void displayMonitorScrollUpOnMouseDown(int btn, int keyCode)
{
    if ((gDisplayMonitorLinesCapacity + _disp_curr - 1) % gDisplayMonitorLinesCapacity != _disp_start) {
        _disp_curr = (gDisplayMonitorLinesCapacity + _disp_curr - 1) % gDisplayMonitorLinesCapacity;
        displayMonitorRefresh();
    }
}

// 0x431B9C display_scroll_down_
static void displayMonitorScrollDownOnMouseDown(int btn, int keyCode)
{
    if (_disp_curr != _disp_start) {
        _disp_curr = (_disp_curr + 1) % gDisplayMonitorLinesCapacity;
        displayMonitorRefresh();
    }
}

// 0x431BC8 display_arrow_up_
static void displayMonitorScrollUpOnMouseEnter(int btn, int keyCode)
{
    gameMouseSetCursor(MOUSE_CURSOR_SMALL_ARROW_UP);
}

// 0x431BD4 display_arrow_down_
static void displayMonitorScrollDownOnMouseEnter(int btn, int keyCode)
{
    gameMouseSetCursor(MOUSE_CURSOR_SMALL_ARROW_DOWN);
}

// 0x431BE0 display_arrow_restore_
static void displayMonitorOnMouseExit(int btn, int keyCode)
{
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
}

// 0x431BEC display_disable_
void displayMonitorDisable()
{
    if (gDisplayMonitorEnabled) {
        buttonDisable(gDisplayMonitorScrollDownButton);
        buttonDisable(gDisplayMonitorScrollUpButton);
        gDisplayMonitorEnabled = false;
    }
}

// 0x431C14 display_enable_
void displayMonitorEnable()
{
    if (!gDisplayMonitorEnabled) {
        buttonEnable(gDisplayMonitorScrollDownButton);
        buttonEnable(gDisplayMonitorScrollUpButton);
        gDisplayMonitorEnabled = true;
    }
}

static void consoleFileInit()
{
    char* consoleFilePath;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_CONSOLE_OUTPUT_FILE_KEY, &consoleFilePath);
    if (consoleFilePath != nullptr && *consoleFilePath == '\0') {
        consoleFilePath = nullptr;
    }

    if (consoleFilePath != nullptr) {
        gConsoleFileStream.open(consoleFilePath);
    }
}

static void consoleFileReset()
{
    if (gConsoleFileStream.is_open()) {
        gConsoleFilePrintCount = 0;
        gConsoleFileStream.flush();
    }
}

static void consoleFileExit()
{
    if (gConsoleFileStream.is_open()) {
        gConsoleFileStream.close();
    }
}

static void consoleFileAddMessage(const char* message)
{
    if (gConsoleFileStream.is_open()) {
        gConsoleFileStream << message << '\n';

        gConsoleFilePrintCount++;
        if (gConsoleFilePrintCount >= 20) {
            consoleFileFlush();
        }
    }
}

static void consoleFileFlush()
{
    if (gConsoleFileStream.is_open()) {
        gConsoleFilePrintCount = 0;
        gConsoleFileStream.flush();
    }
}

} // namespace fallout
