#include "dbox.h"

#include <stdio.h>
#include <string>

#include <algorithm>

#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "debug.h"
#include "delay.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "message.h"
#include "mouse.h"
#include "platform_compat.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"
#include "word_wrap.h"

namespace fallout {

#define FILE_DIALOG_LINE_COUNT 12

#define FILE_DIALOG_DOUBLE_CLICK_DELAY 32

#define LOAD_FILE_DIALOG_DONE_BUTTON_X 58
#define LOAD_FILE_DIALOG_DONE_BUTTON_Y 187

#define LOAD_FILE_DIALOG_DONE_LABEL_X 79
#define LOAD_FILE_DIALOG_DONE_LABEL_Y 187

#define LOAD_FILE_DIALOG_CANCEL_BUTTON_X 163
#define LOAD_FILE_DIALOG_CANCEL_BUTTON_Y 187

#define LOAD_FILE_DIALOG_CANCEL_LABEL_X 182
#define LOAD_FILE_DIALOG_CANCEL_LABEL_Y 187

#define SAVE_FILE_DIALOG_DONE_BUTTON_X 58
#define SAVE_FILE_DIALOG_DONE_BUTTON_Y 214

#define SAVE_FILE_DIALOG_DONE_LABEL_X 79
#define SAVE_FILE_DIALOG_DONE_LABEL_Y 213

#define SAVE_FILE_DIALOG_CANCEL_BUTTON_X 163
#define SAVE_FILE_DIALOG_CANCEL_BUTTON_Y 214

#define SAVE_FILE_DIALOG_CANCEL_LABEL_X 182
#define SAVE_FILE_DIALOG_CANCEL_LABEL_Y 213

#define FILE_DIALOG_TITLE_X 49
#define FILE_DIALOG_TITLE_Y 16

#define FILE_DIALOG_SCROLL_BUTTON_X 36
#define FILE_DIALOG_SCROLL_BUTTON_Y 44

#define FILE_DIALOG_FILE_LIST_X 55
#define FILE_DIALOG_FILE_LIST_Y 49
#define FILE_DIALOG_FILE_LIST_WIDTH 190
#define FILE_DIALOG_FILE_LIST_HEIGHT 124

typedef enum DialogType {
    DIALOG_TYPE_MEDIUM,
    DIALOG_TYPE_LARGE,
    DIALOG_TYPE_COUNT,
} DialogType;

typedef enum FileDialogFrm {
    FILE_DIALOG_FRM_BACKGROUND,
    FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL,
    FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED,
    FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL,
    FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED,
    FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL,
    FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED,
    FILE_DIALOG_FRM_COUNT,
} FileDialogFrm;

typedef enum InputDialogFrm {
    INPUT_DIALOG_FRM_BACKGROUND,
    INPUT_DIALOG_FRM_NAME_BOX,
    INPUT_DIALOG_FRM_DONE_BOX,
    INPUT_DIALOG_FRM_LITTLE_RED_BUTTON_UP,
    INPUT_DIALOG_FRM_LITTLE_RED_BUTTON_DOWN,
    INPUT_DIALOG_FRM_COUNT
} InputDialogFrm;

typedef enum FileDialogScrollDirection {
    FILE_DIALOG_SCROLL_DIRECTION_NONE,
    FILE_DIALOG_SCROLL_DIRECTION_UP,
    FILE_DIALOG_SCROLL_DIRECTION_DOWN,
} FileDialogScrollDirection;

static void fileDialogRenderFileList(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch);

// 0x5108C8 dbox
static constexpr InterfaceFrmId kDialogBoxBackgroundFrmIds[DIALOG_TYPE_COUNT] = {
    InterfaceFrameId::MediumDialog,
    InterfaceFrameId::LargeDialog,
};

// 0x5108D0 ytable
static const int _ytable[DIALOG_TYPE_COUNT] = {
    23,
    27,
};

// 0x5108D8 xtable
static const int _xtable[DIALOG_TYPE_COUNT] = {
    29,
    29,
};

// 0x5108E0 doneY
static const int _doneY[DIALOG_TYPE_COUNT] = {
    81,
    98,
};

// 0x5108E8 doneX
static const int _doneX[DIALOG_TYPE_COUNT] = {
    51,
    37,
};

// 0x5108F0 dblines
static const int _dblines[DIALOG_TYPE_COUNT] = {
    5,
    6,
};

// 0x510900 flgids
static constexpr InterfaceFrmId kLoadFileDialogFrmIds[FILE_DIALOG_FRM_COUNT] = {
    InterfaceFrameId::LoadBox,
    InterfaceFrameId::LittleRedButtonUp,
    InterfaceFrameId::LittleRedButtonDown,
    InterfaceFrameId::CharacterEditorDownArrowOff,
    InterfaceFrameId::CharacterEditorDownArrowOn,
    InterfaceFrameId::CharacterEditorUpArrowOff,
    InterfaceFrameId::CharacterEditorUpArrowOn,
};

// 0x51091C flgids2
static constexpr InterfaceFrmId kSaveFileDialogFrmIds[FILE_DIALOG_FRM_COUNT] = {
    InterfaceFrameId::SaveBox,
    InterfaceFrameId::LittleRedButtonUp,
    InterfaceFrameId::LittleRedButtonDown,
    InterfaceFrameId::CharacterEditorDownArrowOff,
    InterfaceFrameId::CharacterEditorDownArrowOn,
    InterfaceFrameId::CharacterEditorUpArrowOff,
    InterfaceFrameId::CharacterEditorUpArrowOn,
};

static constexpr InterfaceFrameId kInputDialogFrmIds[INPUT_DIALOG_FRM_COUNT] = {
    InterfaceFrameId::CharacterWindow,
    InterfaceFrameId::CharacterEditorNameBox,
    InterfaceFrameId::DoneBox,
    InterfaceFrameId::LittleRedButtonUp,
    InterfaceFrameId::LittleRedButtonDown
};

// CE: extracted from character_editor.cc
// TODO: see if it could be used for `showSaveFileDialog`
static int _get_input_str(int win, int cancelKeyCode, std::string& text, int maxLength, int x, int y, ColorWithFlags textColor, Color backgroundColor, int flags)
{
    int cursorWidth = fontGetStringWidth("_") - 4;
    int windowWidth = windowGetWidth(win);
    int lineHeight = fontGetLineHeight();
    unsigned char* windowBuffer = windowGetBuffer(win);

    if (maxLength > 255) maxLength = 255;

    std::string copy = text + " ";

    int lastDrawnWidth = fontGetStringWidth(copy.c_str());

    auto redrawText = [&]() {
        int newWidth = fontGetStringWidth(copy.c_str());
        int clearWidth = std::max(lastDrawnWidth, newWidth);

        bufferFill(windowBuffer + windowWidth * y + x, clearWidth, lineHeight, windowWidth, backgroundColor);
        fontDrawText(windowBuffer + windowWidth * y + x, copy.c_str(), windowWidth, windowWidth, textColor);
        windowRefresh(win);

        lastDrawnWidth = newWidth;
    };
    redrawText();

    beginTextInput();

    int blinkingCounter = 3;
    bool blink = false;
    int rc = 1;

    while (rc == 1) {
        sharedFpsLimiter.mark();
        unsigned int frameTime = getTicks();

        int keyCode = inputGetInput();
        if (keyCode == cancelKeyCode) {
            rc = 0;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 0;
        } else if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            rc = -1;
        } else {
            // BACKSPACE / DELETE
            if ((keyCode == KEY_DELETE || keyCode == KEY_BACKSPACE) && !text.empty()) {
                text.pop_back();
                copy = text + " ";
                redrawText();
            }
            // Input
            else if ((keyCode >= KEY_FIRST_INPUT_CHARACTER && keyCode <= KEY_LAST_INPUT_CHARACTER) && text.size() < static_cast<size_t>(maxLength)) {
                if ((flags & 0x01) != 0 && !_isdoschar(keyCode)) {
                    continue;
                }

                text.push_back(static_cast<char>(keyCode & 0xFF));
                copy = text + " ";
                redrawText();
            }
        }

        blinkingCounter -= 1;
        if (blinkingCounter == 0) {
            blinkingCounter = 3;

            Color color = blink ? backgroundColor : static_cast<Color>(textColor & COLOR_LAST);
            blink = !blink;

            int currentTextWidth = fontGetStringWidth(copy.c_str());
            bufferFill(windowBuffer + windowWidth * y + x + currentTextWidth - cursorWidth, cursorWidth, lineHeight - 2, windowWidth, color);
        }

        windowRefresh(win);

        delay_ms(1000 / 24 - (getTicks() - frameTime));

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    endTextInput();
    return rc;
}

const char* showInputDialog(const char* currentInput, int windowX, int windowY, const char* doneText, int flags)
{
    static std::string result;
    FrmImage frms[INPUT_DIALOG_FRM_COUNT];

    ScopedFont mainFontGuard(101); // default font for input box

    for (int i = 0; i < INPUT_DIALOG_FRM_COUNT; ++i) {
        if (!frms[i].lock(kInputDialogFrmIds[i])) {
            return nullptr;
        }
    }

    const auto& bgFrm = frms[INPUT_DIALOG_FRM_BACKGROUND];
    const auto& nameBoxFrm = frms[INPUT_DIALOG_FRM_NAME_BOX];
    const auto& doneBoxFrm = frms[INPUT_DIALOG_FRM_DONE_BOX];
    const auto& btnUpFrm = frms[INPUT_DIALOG_FRM_LITTLE_RED_BUTTON_UP];
    const auto& btnDownFrm = frms[INPUT_DIALOG_FRM_LITTLE_RED_BUTTON_DOWN];

    int windowWidth = bgFrm.getWidth();
    int windowHeight = bgFrm.getHeight();

    UniqueWindow win(windowCreate(windowX, windowY, windowWidth, windowHeight, static_cast<ColorWithFlags>(256), flags));
    if (win.get() == -1) {
        return nullptr;
    }

    unsigned char* windowBuf = windowGetBuffer(win.get());

    memcpy(windowBuf, bgFrm.getData(), static_cast<size_t>(windowWidth) * windowHeight);

    blitBufferToBufferTrans(nameBoxFrm.getData(), nameBoxFrm.getWidth(), nameBoxFrm.getHeight(), nameBoxFrm.getWidth(), windowBuf + static_cast<size_t>(windowWidth) * 13 + 13, windowWidth);
    blitBufferToBufferTrans(doneBoxFrm.getData(), doneBoxFrm.getWidth(), doneBoxFrm.getHeight(), doneBoxFrm.getWidth(), windowBuf + windowWidth * 40 + 13, windowWidth);

    {
        ScopedFont buttonFontGuard(103); // "Done" button font
        fontDrawText(windowBuf + windowWidth * 44 + 50, doneText, windowWidth, windowWidth, COLOR_DARK_YELLOW);
    }

    UniqueButton doneBtn(buttonCreate(win.get(), 26, 44, btnUpFrm.getWidth(), btnUpFrm.getHeight(), -1, -1, -1, 500, btnUpFrm.getData(), btnDownFrm.getData(), nullptr, BUTTON_FLAG_TRANSPARENT));
    if (doneBtn.get() != -1) {
        buttonSetCallbacks(doneBtn.get(), _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(win.get());

    std::string editableText = (currentInput && strcmp(currentInput, "None") != 0) ? currentInput : "";
    int status = _get_input_str(win.get(), 500, editableText, 11, 23, 19, COLOR_GREEN | DRAW_TEXT_FLAG_NONE, Color(100), 0);

    if (status == 0 && !editableText.empty()) {
        result = std::move(editableText);
        return result.c_str();
    }

    return nullptr;
}

const char* showInputDialog(const char* currentInput, int x, int y, const char* doneText)
{
    return showInputDialog(currentInput, x, y, doneText, WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
}

// 0x41CF20 dialog_out
int showDialogBox(const char* title, const char** body, int bodyLength, int x, int y, ColorWithFlags titleColor, const char* secondaryButtonText, ColorWithFlags bodyColor, int flags)
{
    MessageList messageList;
    MessageListItem messageListItem;

    bool initializedButtons = false;

    bool hasTwoButtons = (secondaryButtonText != nullptr);
    const bool hasTitle = (title != nullptr);

    if ((flags & DIALOG_BOX_YES_NO) != 0) {
        hasTwoButtons = true;
        flags |= DIALOG_BOX_LARGE;
        flags &= ~DIALOG_BOX_NO_BUTTONS;
    }

    int maximumLineWidth = hasTitle ? fontGetStringWidth(title) : 0;
    for (int index = 0; index < bodyLength; index++) {
        maximumLineWidth = std::max(fontGetStringWidth(body[index]), maximumLineWidth);
    }

    int linesCount = bodyLength;
    int dialogType;

    if ((flags & DIALOG_BOX_LARGE) != 0 || hasTwoButtons) {
        dialogType = DIALOG_TYPE_LARGE;
    } else if ((flags & DIALOG_BOX_MEDIUM) != 0) {
        dialogType = DIALOG_TYPE_MEDIUM;
    } else {
        if (hasTitle) linesCount++;

        dialogType = (maximumLineWidth > 168 || linesCount > 5) ? DIALOG_TYPE_LARGE : DIALOG_TYPE_MEDIUM;
    }

    FrmImage backgroundFrmImage;
    const FrmId backgroundFid = kDialogBoxBackgroundFrmIds[dialogType];
    if (!backgroundFrmImage.lock(backgroundFid)) {
        return -1;
    }

    // Maintain original position in original resolution, otherwise center it.
    x += (screenGetWidth() - 640) / 2;
    y += (screenGetHeight() - 480) / 2;

    UniqueWindow win(windowCreate(x, y, backgroundFrmImage.getWidth(), backgroundFrmImage.getHeight(), static_cast<ColorWithFlags>(256), WINDOW_MODAL | WINDOW_MOVE_ON_TOP));
    if (win.get() == -1) return -1;

    unsigned char* windowBuf = windowGetBuffer(win.get());
    size_t bufferSize = static_cast<size_t>(backgroundFrmImage.getWidth()) * backgroundFrmImage.getHeight();
    memcpy(windowBuf, backgroundFrmImage.getData(), bufferSize);

    FrmImage doneBoxFrmImage;
    FrmImage buttonNormalFrmImage;
    FrmImage buttonPressedFrmImage;

    // Resources init
    const bool hasPrimaryButton = (flags & DIALOG_BOX_NO_BUTTONS) == 0;
    const bool hasSecondaryButton = hasTwoButtons && dialogType == DIALOG_TYPE_LARGE;

    if (hasPrimaryButton || hasSecondaryButton) {
        if (!doneBoxFrmImage.lock(InterfaceFrameId::DoneBox)
            || !buttonPressedFrmImage.lock(InterfaceFrameId::LittleRedButtonDown)
            || !buttonNormalFrmImage.lock(InterfaceFrameId::LittleRedButtonUp)
            || !messageListInit(&messageList)) return -1;

        std::string path = std::string(asc_5186C8) + "DBOX.MSG";
        if (!messageListLoad(&messageList, path.c_str())) {
            messageListFree(&messageList);
            return -1;
        }
    }

    const int bgWidth = backgroundFrmImage.getWidth();
    const int doneY = _doneY[dialogType];

    // Buttons
    {
        ScopedFont buttonFontGuard(103);

        // First button
        if (hasPrimaryButton) {
            const int doneBoxX = hasTwoButtons ? _doneX[dialogType] : (bgWidth - doneBoxFrmImage.getWidth()) / 2;

            blitBufferToBuffer(doneBoxFrmImage.getData(),
                doneBoxFrmImage.getWidth(), doneBoxFrmImage.getHeight(), doneBoxFrmImage.getWidth(),
                windowBuf + bgWidth * doneY + doneBoxX,
                bgWidth);

            messageListItem.num = ((flags & DIALOG_BOX_YES_NO) == 0) ? 100 : 101; // 100 - DONE, 101 - YES
            if (messageListGetItem(&messageList, &messageListItem)) {
                fontDrawText(windowBuf + bgWidth * (doneY + 3) + doneBoxX + 35,
                    messageListItem.text,
                    bgWidth, bgWidth, COLOR_DARK_YELLOW);
            }

            int btn = buttonCreate(win.get(),
                doneBoxX + 13, doneY + 4,
                buttonPressedFrmImage.getWidth(), buttonPressedFrmImage.getHeight(),
                -1, -1, -1, 500, // first button ID
                buttonNormalFrmImage.getData(), buttonPressedFrmImage.getData(),
                nullptr, BUTTON_FLAG_TRANSPARENT);

            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }

            initializedButtons = true;
        }

        // Second button
        if (hasSecondaryButton) {
            if ((flags & DIALOG_BOX_YES_NO) != 0) {
                secondaryButtonText = getmsg(&messageList, &messageListItem, 102); // 102 - NO
            }

            const int doneX = _doneX[dialogType];

            const int blitXOffset = hasPrimaryButton ? (doneX + doneBoxFrmImage.getWidth() + 24) : doneX;
            const int textXOffset = hasPrimaryButton ? (doneX + doneBoxFrmImage.getWidth() + 59) : (doneX + 35);
            const int buttonXOffset = hasPrimaryButton ? (doneX + doneBoxFrmImage.getWidth() + 37) : (doneX + 13);

            blitBufferToBufferTrans(doneBoxFrmImage.getData(),
                doneBoxFrmImage.getWidth(), doneBoxFrmImage.getHeight(), doneBoxFrmImage.getWidth(),
                windowBuf + bgWidth * doneY + blitXOffset,
                bgWidth);

            if (secondaryButtonText != nullptr) {
                fontDrawText(windowBuf + bgWidth * (doneY + 3) + textXOffset,
                    secondaryButtonText,
                    bgWidth, bgWidth, COLOR_DARK_YELLOW);
            }

            int btn = buttonCreate(win.get(),
                buttonXOffset, doneY + 4,
                buttonPressedFrmImage.getWidth(), buttonPressedFrmImage.getHeight(),
                -1, -1, -1, 501, // Second button ID
                buttonNormalFrmImage.getData(), buttonPressedFrmImage.getData(),
                nullptr, BUTTON_FLAG_TRANSPARENT);

            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }

            initializedButtons = true;
        }
    }

    ScopedFont mainFontGuard(101);

    int nextY = _ytable[dialogType];
    int maxY = _ytable[dialogType] + _dblines[dialogType] * fontGetLineHeight();
    int maxWidth = backgroundFrmImage.getWidth() - _xtable[dialogType] * 2;

    auto drawLine = [&](const char* text, int currentY, ColorWithFlags textColor) {
        int bgWidth = backgroundFrmImage.getWidth();
        int xOffset = 0;

        if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
            xOffset = _xtable[dialogType];
        } else {
            xOffset = (bgWidth - fontGetStringWidth(text)) / 2;
        }

        fontDrawText(windowBuf + bgWidth * currentY + xOffset, text, bgWidth, bgWidth, textColor);
    };

    // Vertical center
    if ((flags & DIALOG_BOX_NO_VERTICAL_CENTERING) == 0) {
        int numberOfLines = hasTitle ? 1 : 0;

        for (int index = 0; index < bodyLength; index++) {
            if (body[index] == nullptr) continue;

            const int maxWidth = backgroundFrmImage.getWidth() - _xtable[dialogType] * 2;

            short beginnings[WORD_WRAP_MAX_COUNT];
            short subLineCount = 0;

            if (wordWrap(body[index], maxWidth, beginnings, &subLineCount) == 0) {
                numberOfLines += subLineCount - 1;
            }
        }

        if (numberOfLines > _dblines[dialogType]) {
            numberOfLines = _dblines[dialogType];
        }

        nextY += (_dblines[dialogType] - numberOfLines) * fontGetLineHeight() / 2;
    }

    if (hasTitle && title != nullptr) {
        drawLine(title, nextY, titleColor);
        nextY += fontGetLineHeight();
    }

    for (int index = 0; index < bodyLength && nextY < maxY; index++) {
        if (body[index] == nullptr) continue;

        int width = fontGetStringWidth(body[index]);
        if (width <= maxWidth) {
            // Line's short
            drawLine(body[index], nextY, bodyColor);
            nextY += fontGetLineHeight();
        } else {
            // Line's long, use word wrap
            short beginnings[WORD_WRAP_MAX_COUNT];
            short count;
            if (wordWrap(body[index], maxWidth, beginnings, &count) != 0) {
                debugPrint("\nError: dialog_out");
            }

            std::string_view fullText(body[index]);

            for (int beginningIndex = 1; beginningIndex < count && nextY < maxY; beginningIndex++) {
                size_t start = beginnings[beginningIndex - 1];
                size_t length = beginnings[beginningIndex] - start;

                std::string_view subLine = fullText.substr(start, length);
                if (!subLine.empty() && subLine.back() == ' ') subLine.remove_suffix(1); // trim whitespace

                // null-terminator for fontDrawText
                std::string safeString(subLine);
                drawLine(safeString.c_str(), nextY, bodyColor);
                nextY += fontGetLineHeight();
            }
        }
    }

    windowRefresh(win.get());

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        if (keyCode == 500) { // First button (Done/Yes)
            rc = 1;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 1;
        } else if (keyCode == KEY_ESCAPE || keyCode == 501) { // Second button (NO) or ESC
            rc = 0;
        } else {
            if ((flags & DIALOG_BOX_YES_NO) != 0) {
                if (keyCode == KEY_UPPERCASE_Y || keyCode == KEY_LOWERCASE_Y) {
                    rc = 1;
                } else if (keyCode == KEY_UPPERCASE_N || keyCode == KEY_LOWERCASE_N) {
                    rc = 0;
                }
            }
        }

        if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            rc = 1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (initializedButtons) {
        messageListFree(&messageList);
    }

    return rc;
}

// 0x41DE90 file_dialog
int showLoadFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags)
{
    int oldFont = fontGetCurrent();

    bool isScrollable = false;
    if (fileListLength > FILE_DIALOG_LINE_COUNT) {
        isScrollable = true;
    }

    int selectedFileIndex = 0;
    int pageOffset = 0;
    int maxPageOffset = fileListLength - (FILE_DIALOG_LINE_COUNT + 1);
    if (maxPageOffset < 0) {
        maxPageOffset = fileListLength - 1;
        if (maxPageOffset < 0) {
            maxPageOffset = 0;
        }
    }

    FrmImage frmImages[FILE_DIALOG_FRM_COUNT];

    for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
        if (!frmImages[index].lock(kLoadFileDialogFrmIds[index])) {
            return -1;
        }
    }

    int backgroundWidth = frmImages[FILE_DIALOG_FRM_BACKGROUND].getWidth();
    int backgroundHeight = frmImages[FILE_DIALOG_FRM_BACKGROUND].getHeight();

    // Maintain original position in original resolution, otherwise center it.
    x += (screenGetWidth() - 640) / 2;
    y += (screenGetHeight() - 480) / 2;
    int win = windowCreate(x, y, backgroundWidth, backgroundHeight, static_cast<ColorWithFlags>(256), WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(win);
    memcpy(windowBuffer, frmImages[FILE_DIALOG_FRM_BACKGROUND].getData(), static_cast<size_t>(backgroundWidth) * backgroundHeight);

    MessageList messageList;
    MessageListItem messageListItem;

    if (!messageListInit(&messageList)) {
        windowDestroy(win);
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "DBOX.MSG");

    if (!messageListLoad(&messageList, path)) {
        windowDestroy(win);
        return -1;
    }

    fontSetCurrent(103);

    // DONE
    const char* done = getmsg(&messageList, &messageListItem, 100);
    fontDrawText(windowBuffer + LOAD_FILE_DIALOG_DONE_LABEL_Y * backgroundWidth + LOAD_FILE_DIALOG_DONE_LABEL_X, done, backgroundWidth, backgroundWidth, COLOR_DARK_YELLOW);

    // CANCEL
    const char* cancel = getmsg(&messageList, &messageListItem, 103);
    fontDrawText(windowBuffer + LOAD_FILE_DIALOG_CANCEL_LABEL_Y * backgroundWidth + LOAD_FILE_DIALOG_CANCEL_LABEL_X, cancel, backgroundWidth, backgroundWidth, COLOR_DARK_YELLOW);

    int doneBtn = buttonCreate(win,
        LOAD_FILE_DIALOG_DONE_BUTTON_X,
        LOAD_FILE_DIALOG_DONE_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        500,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int cancelBtn = buttonCreate(win,
        LOAD_FILE_DIALOG_CANCEL_BUTTON_X,
        LOAD_FILE_DIALOG_CANCEL_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        501,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollUpBtn = buttonCreate(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getHeight(),
        -1,
        505,
        506,
        505,
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        buttonSetCallbacks(scrollUpBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollDownButton = buttonCreate(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y + frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getHeight(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getHeight(),
        -1,
        503,
        504,
        503,
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollDownButton != -1) {
        buttonSetCallbacks(scrollDownButton, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    buttonCreate(
        win,
        FILE_DIALOG_FILE_LIST_X,
        FILE_DIALOG_FILE_LIST_Y,
        FILE_DIALOG_FILE_LIST_WIDTH,
        FILE_DIALOG_FILE_LIST_HEIGHT,
        -1,
        -1,
        -1,
        502,
        nullptr,
        nullptr,
        nullptr,
        0);

    if (title != nullptr) {
        fontDrawText(windowBuffer + backgroundWidth * FILE_DIALOG_TITLE_Y + FILE_DIALOG_TITLE_X, title, backgroundWidth, backgroundWidth, COLOR_DARK_YELLOW);
    }

    fontSetCurrent(101);

    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
    windowRefresh(win);

    int doubleClickSelectedFileIndex = -2;
    int doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int tick = getTicks();
        int keyCode = inputGetInput();
        int scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_NONE;
        int scrollCounter = 0;
        bool isScrolling = false;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == 500) {
            if (fileListLength != 0) {
                strncpy(dest, fileList[selectedFileIndex + pageOffset], 16);
                rc = 0;
            } else {
                rc = 1;
            }
        } else if (keyCode == 501 || keyCode == KEY_ESCAPE) {
            rc = 1;
        } else if (keyCode == 502 && fileListLength != 0) {
            int mouseX;
            int mouseY;
            mouseGetPosition(&mouseX, &mouseY);

            int selectedLine = (mouseY - y - FILE_DIALOG_FILE_LIST_Y) / fontGetLineHeight();
            if (selectedLine - 1 < 0) {
                selectedLine = 0;
            }

            if (isScrollable || selectedLine < fileListLength) {
                if (selectedLine >= FILE_DIALOG_LINE_COUNT) {
                    selectedLine = FILE_DIALOG_LINE_COUNT - 1;
                }
            } else {
                selectedLine = fileListLength - 1;
            }

            selectedFileIndex = selectedLine;
            if (selectedFileIndex == doubleClickSelectedFileIndex) {
                soundPlayFile("ib1p1xx1");
                strncpy(dest, fileList[selectedFileIndex + pageOffset], 16);
                rc = 0;
            }

            doubleClickSelectedFileIndex = selectedFileIndex;
            fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
        } else if (keyCode == 506) {
            scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_UP;
        } else if (keyCode == 504) {
            scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_DOWN;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                pageOffset--;
                if (pageOffset < 0) {
                    selectedFileIndex--;
                    if (selectedFileIndex < 0) {
                        selectedFileIndex = 0;
                    }
                    pageOffset = 0;
                }
                fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_ARROW_DOWN:
                if (isScrollable) {
                    pageOffset++;
                    // FIXME: Should be >= maxPageOffset (as in save dialog).
                    // Otherwise out of bounds index is considered selected.
                    if (pageOffset > maxPageOffset) {
                        selectedFileIndex++;
                        // FIXME: Should be >= FILE_DIALOG_LINE_COUNT (as in
                        // save dialog). Otherwise out of bounds index is
                        // considered selected.
                        if (selectedFileIndex > FILE_DIALOG_LINE_COUNT) {
                            selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                        }
                        pageOffset = maxPageOffset;
                    }
                } else {
                    selectedFileIndex++;
                    if (selectedFileIndex > maxPageOffset) {
                        selectedFileIndex = maxPageOffset;
                    }
                }
                fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_HOME:
                selectedFileIndex = 0;
                pageOffset = 0;
                fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_END:
                if (isScrollable) {
                    selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                    pageOffset = maxPageOffset;
                } else {
                    selectedFileIndex = maxPageOffset;
                    pageOffset = 0;
                }
                fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            }
        }

        if (scrollDirection != FILE_DIALOG_SCROLL_DIRECTION_NONE) {
            unsigned int scrollDelay = 4;
            doubleClickSelectedFileIndex = -2;
            while (1) {
                unsigned int scrollTick = getTicks();
                scrollCounter += 1;
                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollDelay += 1;
                        if (scrollDelay > 24) {
                            scrollDelay = 24;
                        }
                    }

                    if (scrollDirection == FILE_DIALOG_SCROLL_DIRECTION_UP) {
                        pageOffset--;
                        if (pageOffset < 0) {
                            selectedFileIndex--;
                            if (selectedFileIndex < 0) {
                                selectedFileIndex = 0;
                            }
                            pageOffset = 0;
                        }
                    } else {
                        if (isScrollable) {
                            pageOffset++;
                            if (pageOffset > maxPageOffset) {
                                selectedFileIndex++;
                                if (selectedFileIndex >= FILE_DIALOG_LINE_COUNT) {
                                    selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                                }
                                pageOffset = maxPageOffset;
                            }
                        } else {
                            selectedFileIndex++;
                            if (selectedFileIndex > maxPageOffset) {
                                selectedFileIndex = maxPageOffset;
                            }
                        }
                    }

                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    windowRefresh(win);
                }

                unsigned int delay = (scrollCounter > 14.4) ? 1000 / scrollDelay : 1000 / 24;

                delay_ms(delay - (getTicks() - scrollTick));

                if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                    rc = 1;
                    break;
                }

                int keyCode = inputGetInput();
                if (keyCode == 505 || keyCode == 503) {
                    break;
                }

                renderPresent();
            }
        } else {
            windowRefresh(win);

            doubleClickTimer--;
            if (doubleClickTimer == 0) {
                doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;
                doubleClickSelectedFileIndex = -2;
            }

            delay_ms(1000 / 24 - (getTicks() - tick));
        }

        if (_game_user_wants_to_quit) {
            rc = 1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);

    messageListFree(&messageList);
    fontSetCurrent(oldFont);

    return rc;
}

// 0x41EA78 save_file_dialog
int showSaveFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags)
{
    int oldFont = fontGetCurrent();

    bool isScrollable = false;
    if (fileListLength > FILE_DIALOG_LINE_COUNT) {
        isScrollable = true;
    }

    int selectedFileIndex = 0;
    int pageOffset = 0;
    int maxPageOffset = fileListLength - (FILE_DIALOG_LINE_COUNT + 1);
    if (maxPageOffset < 0) {
        maxPageOffset = fileListLength - 1;
        if (maxPageOffset < 0) {
            maxPageOffset = 0;
        }
    }

    FrmImage frmImages[FILE_DIALOG_FRM_COUNT];

    for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
        if (!frmImages[index].lock(kSaveFileDialogFrmIds[index])) {
            return -1;
        }
    }

    int backgroundWidth = frmImages[FILE_DIALOG_FRM_BACKGROUND].getWidth();
    int backgroundHeight = frmImages[FILE_DIALOG_FRM_BACKGROUND].getHeight();

    // Maintain original position in original resolution, otherwise center it.
    x += (screenGetWidth() - 640) / 2;
    y += (screenGetHeight() - 480) / 2;
    int win = windowCreate(x, y, backgroundWidth, backgroundHeight, static_cast<ColorWithFlags>(256), WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(win);
    memcpy(windowBuffer, frmImages[FILE_DIALOG_FRM_BACKGROUND].getData(), static_cast<size_t>(backgroundWidth) * backgroundHeight);

    MessageList messageList;
    MessageListItem messageListItem;

    if (!messageListInit(&messageList)) {
        windowDestroy(win);
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "DBOX.MSG");

    if (!messageListLoad(&messageList, path)) {
        windowDestroy(win);
        return -1;
    }

    fontSetCurrent(103);

    // DONE
    const char* done = getmsg(&messageList, &messageListItem, 100);
    fontDrawText(windowBuffer + backgroundWidth * SAVE_FILE_DIALOG_DONE_LABEL_Y + SAVE_FILE_DIALOG_DONE_LABEL_X, done, backgroundWidth, backgroundWidth, COLOR_DARK_YELLOW);

    // CANCEL
    const char* cancel = getmsg(&messageList, &messageListItem, 103);
    fontDrawText(windowBuffer + backgroundWidth * SAVE_FILE_DIALOG_CANCEL_LABEL_Y + SAVE_FILE_DIALOG_CANCEL_LABEL_X, cancel, backgroundWidth, backgroundWidth, COLOR_DARK_YELLOW);

    int doneBtn = buttonCreate(win,
        SAVE_FILE_DIALOG_DONE_BUTTON_X,
        SAVE_FILE_DIALOG_DONE_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        500,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int cancelBtn = buttonCreate(win,
        SAVE_FILE_DIALOG_CANCEL_BUTTON_X,
        SAVE_FILE_DIALOG_CANCEL_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        501,
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        buttonSetCallbacks(cancelBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollUpBtn = buttonCreate(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y,
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getHeight(),
        -1,
        505,
        506,
        505,
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        buttonSetCallbacks(scrollUpBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    int scrollDownButton = buttonCreate(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y + frmImages[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].getHeight(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getWidth(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getHeight(),
        -1,
        503,
        504,
        503,
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL].getData(),
        frmImages[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollDownButton != -1) {
        buttonSetCallbacks(scrollDownButton, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    buttonCreate(
        win,
        FILE_DIALOG_FILE_LIST_X,
        FILE_DIALOG_FILE_LIST_Y,
        FILE_DIALOG_FILE_LIST_WIDTH,
        FILE_DIALOG_FILE_LIST_HEIGHT,
        -1,
        -1,
        -1,
        502,
        nullptr,
        nullptr,
        nullptr,
        0);

    if (title != nullptr) {
        fontDrawText(windowBuffer + backgroundWidth * FILE_DIALOG_TITLE_Y + FILE_DIALOG_TITLE_X, title, backgroundWidth, backgroundWidth, COLOR_DARK_YELLOW);
    }

    fontSetCurrent(101);

    int cursorHeight = fontGetLineHeight();
    int cursorWidth = fontGetStringWidth("_") - 4;
    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);

    int fileNameLength = 0;
    char* pch = dest;
    while (*pch != '\0' && *pch != '.') {
        fileNameLength++;
        if (fileNameLength >= 12) {
            break;
        }

        pch++;
    }
    dest[fileNameLength] = '\0';

    char fileNameCopy[32];
    strncpy(fileNameCopy, dest, 32);

    size_t fileNameCopyLength = strlen(fileNameCopy);
    fileNameCopy[fileNameCopyLength + 1] = '\0';
    fileNameCopy[fileNameCopyLength] = ' ';

    unsigned char* fileNameBufferPtr = windowBuffer + backgroundWidth * 190 + 57;

    bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, backgroundWidth, Color(100));
    fontDrawText(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, COLOR_GREEN);

    windowRefresh(win);

    beginTextInput();

    int blinkingCounter = 3;
    bool blink = false;

    int doubleClickSelectedFileIndex = -2;
    int doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int tick = getTicks();
        int keyCode = inputGetInput();
        int scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_NONE;
        int scrollCounter = 0;
        bool isScrolling = false;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == 500) {
            rc = 0;
        } else if (keyCode == KEY_RETURN) {
            soundPlayFile("ib1p1xx1");
            rc = 0;
        } else if (keyCode == 501 || keyCode == KEY_ESCAPE) {
            rc = 1;
        } else if ((keyCode == KEY_DELETE || keyCode == KEY_BACKSPACE) && fileNameCopyLength > 0) {
            bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, backgroundWidth, Color(100));
            fileNameCopy[fileNameCopyLength - 1] = ' ';
            fileNameCopy[fileNameCopyLength] = '\0';
            fontDrawText(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, COLOR_GREEN);
            fileNameCopyLength--;
            windowRefresh(win);
        } else if (keyCode < KEY_FIRST_INPUT_CHARACTER || keyCode > KEY_LAST_INPUT_CHARACTER || fileNameCopyLength >= 8) {
            if (keyCode == 502 && fileListLength != 0) {
                int mouseX;
                int mouseY;
                mouseGetPosition(&mouseX, &mouseY);

                int selectedLine = (mouseY - y - FILE_DIALOG_FILE_LIST_Y) / fontGetLineHeight();
                if (selectedLine - 1 < 0) {
                    selectedLine = 0;
                }

                if (isScrollable || selectedLine < fileListLength) {
                    if (selectedLine >= FILE_DIALOG_LINE_COUNT) {
                        selectedLine = FILE_DIALOG_LINE_COUNT - 1;
                    }
                } else {
                    selectedLine = fileListLength - 1;
                }

                selectedFileIndex = selectedLine;
                if (selectedFileIndex == doubleClickSelectedFileIndex) {
                    soundPlayFile("ib1p1xx1");
                    strncpy(dest, fileList[selectedFileIndex + pageOffset], 16);

                    int index;
                    for (index = 0; index < 12; index++) {
                        if (dest[index] == '.' || dest[index] == '\0') {
                            break;
                        }
                    }

                    dest[index] = '\0';
                    rc = 2;
                } else {
                    doubleClickSelectedFileIndex = selectedFileIndex;
                    bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, backgroundWidth, Color(100));
                    strncpy(fileNameCopy, fileList[selectedFileIndex + pageOffset], 16);

                    int index;
                    for (index = 0; index < 12; index++) {
                        if (fileNameCopy[index] == '.' || fileNameCopy[index] == '\0') {
                            break;
                        }
                    }

                    fileNameCopy[index] = '\0';
                    fileNameCopyLength = strlen(fileNameCopy);
                    fileNameCopy[fileNameCopyLength] = ' ';
                    fileNameCopy[fileNameCopyLength + 1] = '\0';

                    fontDrawText(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, COLOR_GREEN);
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                }
            } else if (keyCode == 506) {
                scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_UP;
            } else if (keyCode == 504) {
                scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_DOWN;
            } else {
                switch (keyCode) {
                case KEY_ARROW_UP:
                    pageOffset--;
                    if (pageOffset < 0) {
                        selectedFileIndex--;
                        if (selectedFileIndex < 0) {
                            selectedFileIndex = 0;
                        }
                        pageOffset = 0;
                    }
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_ARROW_DOWN:
                    if (isScrollable) {
                        pageOffset++;
                        if (pageOffset >= maxPageOffset) {
                            selectedFileIndex++;
                            if (selectedFileIndex >= FILE_DIALOG_LINE_COUNT) {
                                selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                            }
                            pageOffset = maxPageOffset;
                        }
                    } else {
                        selectedFileIndex++;
                        if (selectedFileIndex > maxPageOffset) {
                            selectedFileIndex = maxPageOffset;
                        }
                    }
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_HOME:
                    selectedFileIndex = 0;
                    pageOffset = 0;
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_END:
                    if (isScrollable) {
                        selectedFileIndex = 11;
                        pageOffset = maxPageOffset;
                    } else {
                        selectedFileIndex = maxPageOffset;
                        pageOffset = 0;
                    }
                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                }
            }
        } else if (_isdoschar(keyCode)) {
            bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, backgroundWidth, Color(100));

            fileNameCopy[fileNameCopyLength] = keyCode & 0xFF;
            fileNameCopy[fileNameCopyLength + 1] = ' ';
            fileNameCopy[fileNameCopyLength + 2] = '\0';
            fontDrawText(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, COLOR_GREEN);
            fileNameCopyLength++;

            windowRefresh(win);
        }

        if (scrollDirection != FILE_DIALOG_SCROLL_DIRECTION_NONE) {
            unsigned int scrollDelay = 4;
            doubleClickSelectedFileIndex = -2;
            while (1) {
                unsigned int scrollTick = getTicks();
                scrollCounter += 1;
                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollDelay += 1;
                        if (scrollDelay > 24) {
                            scrollDelay = 24;
                        }
                    }

                    if (scrollDirection == FILE_DIALOG_SCROLL_DIRECTION_UP) {
                        pageOffset--;
                        if (pageOffset < 0) {
                            selectedFileIndex--;
                            if (selectedFileIndex < 0) {
                                selectedFileIndex = 0;
                            }
                            pageOffset = 0;
                        }
                    } else {
                        if (isScrollable) {
                            pageOffset++;
                            if (pageOffset > maxPageOffset) {
                                selectedFileIndex++;
                                if (selectedFileIndex >= FILE_DIALOG_LINE_COUNT) {
                                    selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                                }
                                pageOffset = maxPageOffset;
                            }
                        } else {
                            selectedFileIndex++;
                            if (selectedFileIndex > maxPageOffset) {
                                selectedFileIndex = maxPageOffset;
                            }
                        }
                    }

                    fileDialogRenderFileList(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    windowRefresh(win);
                }

                // NOTE: Original code is slightly different. For unknown reason
                // entire blinking stuff is placed into two different branches,
                // which only differs by amount of delay. Probably result of
                // using large blinking macro as there are no traces of inlined
                // function.
                blinkingCounter -= 1;
                if (blinkingCounter == 0) {
                    blinkingCounter = 3;

                    Color color = blink ? Color(100) : COLOR_GREEN;
                    blink = !blink;

                    bufferFill(fileNameBufferPtr + fontGetStringWidth(fileNameCopy) - cursorWidth, cursorWidth, cursorHeight - 2, backgroundWidth, color);
                }

                // FIXME: Missing windowRefresh makes blinking useless.

                unsigned int delay = (scrollCounter > 14.4) ? 1000 / scrollDelay : 1000 / 24;
                delay_ms(delay - (getTicks() - scrollTick));

                if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                    rc = 1;
                    break;
                }

                int key = inputGetInput();
                if (key == 505 || key == 503) {
                    break;
                }

                renderPresent();
            }
        } else {
            blinkingCounter -= 1;
            if (blinkingCounter == 0) {
                blinkingCounter = 3;

                Color color = blink ? Color(100) : COLOR_GREEN;
                blink = !blink;

                bufferFill(fileNameBufferPtr + fontGetStringWidth(fileNameCopy) - cursorWidth, cursorWidth, cursorHeight - 2, backgroundWidth, color);
            }

            windowRefresh(win);

            doubleClickTimer--;
            if (doubleClickTimer == 0) {
                doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;
                doubleClickSelectedFileIndex = -2;
            }

            delay_ms(1000 / 24 - (getTicks() - tick));
        }

        if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            rc = 1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    endTextInput();

    if (rc == 0) {
        if (fileNameCopyLength != 0) {
            fileNameCopy[fileNameCopyLength] = '\0';
            strcpy(dest, fileNameCopy);
        } else {
            rc = 1;
        }
    } else {
        if (rc == 2) {
            rc = 0;
        }
    }

    windowDestroy(win);
    messageListFree(&messageList);
    fontSetCurrent(oldFont);

    return rc;
}

// 0x41FBDC PrntFlist
static void fileDialogRenderFileList(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch)
{
    int lineHeight = fontGetLineHeight();
    int y = FILE_DIALOG_FILE_LIST_Y;
    bufferFill(buffer + y * pitch + FILE_DIALOG_FILE_LIST_X, FILE_DIALOG_FILE_LIST_WIDTH, FILE_DIALOG_FILE_LIST_HEIGHT, pitch, static_cast<Color>(100));
    if (fileListLength != 0) {
        if (fileListLength - pageOffset > FILE_DIALOG_LINE_COUNT) {
            fileListLength = FILE_DIALOG_LINE_COUNT;
        }

        for (int index = 0; index < fileListLength; index++) {
            Color color = index == selectedIndex ? COLOR_LIGHT_YELLOW : COLOR_GREEN;
            fontDrawText(buffer + pitch * y + FILE_DIALOG_FILE_LIST_X, fileList[pageOffset + index], FILE_DIALOG_FILE_LIST_WIDTH, pitch, color);
            y += lineHeight;
        }
    }
}

} // namespace fallout
