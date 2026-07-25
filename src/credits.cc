#include "credits.h"

#include <string.h>

#include <algorithm>

#include "art.h"
#include "color.h"
#include "cycle.h"
#include "db.h"
#include "debug.h"
#include "delay.h"
#include "draw.h"
#include "game_mouse.h"
#include "input.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "palette.h"
#include "platform_compat.h"
#include "sound.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"

namespace fallout {

#define CREDITS_WINDOW_SCROLLING_DELAY (38)

static bool creditsFileParseNextLine(char* dest, int* font, int* color);

// 0x56D740 credits_file
static File* gCreditsFile;

// 0x56D744 name_color
static int gCreditsWindowNameColor;

// 0x56D748 title_font
static int gCreditsWindowTitleFont;

// 0x56D74C name_font
static int gCreditsWindowNameFont;

// 0x56D750 title_color
static int gCreditsWindowTitleColor;

// 0x42C860 credits
void creditsOpen(const char* filePath, int backgroundFid, bool useReversedStyle)
{
    int oldFont = fontGetCurrent();

    colorPaletteLoad("color.pal");

    if (useReversedStyle) {
        gCreditsWindowTitleColor = COLOR_DARK_YELLOW_3;
        gCreditsWindowNameFont = 103;
        gCreditsWindowTitleFont = 104;
        gCreditsWindowNameColor = COLOR_DULL_BROWN;
    } else {
        gCreditsWindowTitleColor = COLOR_DULL_BROWN;
        gCreditsWindowNameFont = 104;
        gCreditsWindowTitleFont = 103;
        gCreditsWindowNameColor = COLOR_DARK_YELLOW_3;
    }

    soundContinueAll();

    char localizedPath[COMPAT_MAX_PATH];
    if (_message_make_path(localizedPath, sizeof(localizedPath), filePath)) {
        gCreditsFile = fileOpen(localizedPath, "rt");
        if (gCreditsFile != nullptr) {
            soundContinueAll();

            colorCycleDisable();
            gameMouseSetCursor(MOUSE_CURSOR_NONE);

            bool cursorWasHidden = cursorIsHidden();
            if (cursorWasHidden) {
                mouseShowCursor();
            }

            int windowWidth = screenGetWidth();
            int windowHeight = screenGetHeight();
            int window = windowCreate(0, 0, windowWidth, windowHeight, COLOR_BLACK, 20);
            soundContinueAll();
            if (window != -1) {
                unsigned char* windowBuffer = windowGetBuffer(window);
                if (windowBuffer != nullptr) {
                    unsigned char* backgroundBuffer = (unsigned char*)internal_malloc(windowWidth * windowHeight);
                    if (backgroundBuffer) {
                        soundContinueAll();

                        memset(backgroundBuffer, COLOR_BLACK, windowWidth * windowHeight);

                        if (backgroundFid != -1) {
                            FrmImage backgroundFrmImage;
                            if (backgroundFrmImage.lock(backgroundFid)) {
                                blitBufferToBuffer(backgroundFrmImage.getData(),
                                    backgroundFrmImage.getWidth(),
                                    backgroundFrmImage.getHeight(),
                                    backgroundFrmImage.getWidth(),
                                    backgroundBuffer + windowWidth * ((windowHeight - backgroundFrmImage.getHeight()) / 2) + (windowWidth - backgroundFrmImage.getWidth()) / 2,
                                    windowWidth);
                                backgroundFrmImage.unlock();
                            }
                        }

                        unsigned char* intermediateBuffer = (unsigned char*)internal_malloc(windowWidth * windowHeight);
                        if (intermediateBuffer != nullptr) {
                            memset(intermediateBuffer, 0, windowWidth * windowHeight);

                            fontSetCurrent(gCreditsWindowTitleFont);
                            int titleFontLineHeight = fontGetLineHeight();

                            fontSetCurrent(gCreditsWindowNameFont);
                            int nameFontLineHeight = fontGetLineHeight();

                            int lineHeight = std::max(titleFontLineHeight, nameFontLineHeight);
                            int stringBufferSize = windowWidth * lineHeight;
                            unsigned char* stringBuffer = (unsigned char*)internal_malloc(stringBufferSize);
                            if (stringBuffer != nullptr) {
                                blitBufferToBuffer(backgroundBuffer,
                                    windowWidth,
                                    windowHeight,
                                    windowWidth,
                                    windowBuffer,
                                    windowWidth);

                                windowRefresh(window);

                                paletteFadeTo(_cmap);

                                char str[260];
                                int font;
                                int color;
                                unsigned int tick = 0;
                                bool stop = false;
                                while (creditsFileParseNextLine(str, &font, &color)) {
                                    fontSetCurrent(font);

                                    int stringWidth = fontGetStringWidth(str);
                                    if (stringWidth >= windowWidth) {
                                        continue;
                                    }

                                    memset(stringBuffer, 0, stringBufferSize);
                                    fontDrawText(stringBuffer, str, windowWidth, windowWidth, color);

                                    unsigned char* dest = intermediateBuffer + windowWidth * windowHeight - windowWidth + (windowWidth - stringWidth) / 2;
                                    unsigned char* src = stringBuffer;
                                    for (int index = 0; index < lineHeight; index++) {
                                        sharedFpsLimiter.mark();

                                        if (inputGetInput() != -1) {
                                            stop = true;
                                            break;
                                        }

                                        memmove(intermediateBuffer, intermediateBuffer + windowWidth, windowWidth * windowHeight - windowWidth);
                                        memcpy(dest, src, stringWidth);

                                        blitBufferToBuffer(backgroundBuffer,
                                            windowWidth,
                                            windowHeight,
                                            windowWidth,
                                            windowBuffer,
                                            windowWidth);

                                        blitBufferToBufferTrans(intermediateBuffer,
                                            windowWidth,
                                            windowHeight,
                                            windowWidth,
                                            windowBuffer,
                                            windowWidth);

                                        delay_ms(CREDITS_WINDOW_SCROLLING_DELAY - (getTicks() - tick));

                                        tick = getTicks();

                                        windowRefresh(window);

                                        src += windowWidth;

                                        sharedFpsLimiter.throttle();
                                        renderPresent();
                                    }

                                    if (stop) {
                                        break;
                                    }
                                }

                                if (!stop) {
                                    for (int index = 0; index < windowHeight; index++) {
                                        sharedFpsLimiter.mark();

                                        if (inputGetInput() != -1) {
                                            break;
                                        }

                                        memmove(intermediateBuffer, intermediateBuffer + windowWidth, windowWidth * windowHeight - windowWidth);
                                        memset(intermediateBuffer + windowWidth * windowHeight - windowWidth, 0, windowWidth);

                                        blitBufferToBuffer(backgroundBuffer,
                                            windowWidth,
                                            windowHeight,
                                            windowWidth,
                                            windowBuffer,
                                            windowWidth);

                                        blitBufferToBufferTrans(intermediateBuffer,
                                            windowWidth,
                                            windowHeight,
                                            windowWidth,
                                            windowBuffer,
                                            windowWidth);

                                        delay_ms(CREDITS_WINDOW_SCROLLING_DELAY - (getTicks() - tick));

                                        tick = getTicks();

                                        windowRefresh(window);

                                        sharedFpsLimiter.throttle();
                                        renderPresent();
                                    }
                                }

                                internal_free(stringBuffer);
                            }
                            internal_free(intermediateBuffer);
                        }
                        internal_free(backgroundBuffer);
                    }
                }

                soundContinueAll();
                paletteFadeTo(gPaletteBlack);
                soundContinueAll();
                windowDestroy(window);
            }

            if (cursorWasHidden) {
                mouseHideCursor();
            }

            gameMouseSetCursor(MOUSE_CURSOR_ARROW);
            colorCycleEnable();
            fileClose(gCreditsFile);
        }
    }

    fontSetCurrent(oldFont);
}

// 0x42CE6C credits_get_next_line
static bool creditsFileParseNextLine(char* dest, int* font, int* color)
{
    char string[256];
    while (fileReadString(string, 256, gCreditsFile)) {
        char* pch;
        if (string[0] == ';') {
            continue;
        } else if (string[0] == '@') {
            *font = gCreditsWindowTitleFont;
            *color = gCreditsWindowTitleColor;
            pch = string + 1;
        } else if (string[0] == '#') {
            *font = gCreditsWindowNameFont;
            *color = COLOR_GREY;
            pch = string + 1;
        } else {
            *font = gCreditsWindowNameFont;
            *color = gCreditsWindowNameColor;
            pch = string;
        }

        strcpy(dest, pch);

        return true;
    }

    return false;
}

} // namespace fallout
