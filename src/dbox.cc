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

namespace {
    namespace dbox {
        struct Point {
            int x, y;
        };

        int createRedButton(int windowId, Point pos, int buttonId, const FrmImage& upFrm, const FrmImage& downFrm)
        {
            int btn = buttonCreate(windowId, pos.x, pos.y, downFrm.getWidth(), downFrm.getHeight(),
                -1, -1, -1, buttonId, upFrm.getData(), downFrm.getData(), nullptr, BUTTON_FLAG_TRANSPARENT);

            if (btn != -1) buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            return btn;
        }

        namespace input {
            typedef enum InputDialogFrm {
                INPUT_DIALOG_FRM_BACKGROUND,
                INPUT_DIALOG_FRM_NAME_BOX,
                INPUT_DIALOG_FRM_DONE_BOX,
                INPUT_DIALOG_FRM_LITTLE_RED_BUTTON_UP,
                INPUT_DIALOG_FRM_LITTLE_RED_BUTTON_DOWN,
                INPUT_DIALOG_FRM_COUNT
            } InputDialogFrm;

            constexpr InterfaceFrameId kInputDialogFrmIds[INPUT_DIALOG_FRM_COUNT] = {
                InterfaceFrameId::CharacterWindow,
                InterfaceFrameId::CharacterEditorNameBox,
                InterfaceFrameId::DoneBox,
                InterfaceFrameId::LittleRedButtonUp,
                InterfaceFrameId::LittleRedButtonDown
            };

            constexpr Point nameBoxPos { 13, 13 };
            constexpr Point doneBoxPos { 13, 40 };
            constexpr Point doneLabelPos { 50, 44 };
            constexpr Point doneButtonPos { 26, 44 };

            struct DialogFrms {
                FrmImage background, nameBox, doneBox, buttonUp, buttonDown;
            };

            bool loadImages(DialogFrms& images)
            {
                return images.background.lock(kInputDialogFrmIds[0])
                    && images.nameBox.lock(kInputDialogFrmIds[1])
                    && images.doneBox.lock(kInputDialogFrmIds[2])
                    && images.buttonUp.lock(kInputDialogFrmIds[3])
                    && images.buttonDown.lock(kInputDialogFrmIds[4]);
            }

            void drawBox(unsigned char* buffer, int width, Point position, const FrmImage& frm)
            {
                size_t offset = static_cast<size_t>(width) * position.y + position.x;
                blitBufferToBufferTrans(frm.getData(), frm.getWidth(), frm.getHeight(), frm.getWidth(), buffer + offset, width);
            }
        } // namespace input

        namespace file {
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

            // 0x510900 flgids
            constexpr InterfaceFrmId kLoadFileDialogFrmIds[FILE_DIALOG_FRM_COUNT] = {
                InterfaceFrameId::LoadBox,
                InterfaceFrameId::LittleRedButtonUp,
                InterfaceFrameId::LittleRedButtonDown,
                InterfaceFrameId::CharacterEditorDownArrowOff,
                InterfaceFrameId::CharacterEditorDownArrowOn,
                InterfaceFrameId::CharacterEditorUpArrowOff,
                InterfaceFrameId::CharacterEditorUpArrowOn,
            };

            // 0x51091C flgids2
            constexpr InterfaceFrmId kSaveFileDialogFrmIds[FILE_DIALOG_FRM_COUNT] = {
                InterfaceFrameId::SaveBox,
                InterfaceFrameId::LittleRedButtonUp,
                InterfaceFrameId::LittleRedButtonDown,
                InterfaceFrameId::CharacterEditorDownArrowOff,
                InterfaceFrameId::CharacterEditorDownArrowOn,
                InterfaceFrameId::CharacterEditorUpArrowOff,
                InterfaceFrameId::CharacterEditorUpArrowOn,
            };

            struct DialogFrms {
                FrmImage background;
                FrmImage buttonNormal;
                FrmImage buttonPressed;
                FrmImage scrollDownNormal;
                FrmImage scrollDownPressed;
                FrmImage scrollUpNormal;
                FrmImage scrollUpPressed;
            };

            bool loadImages(DialogFrms& images, const InterfaceFrmId* frmIds)
            {
                auto* frms = reinterpret_cast<FrmImage*>(&images);

                for (int i = 0; i < FILE_DIALOG_FRM_COUNT; ++i) {
                    if (!frms[i].lock(frmIds[i])) {
                        return false;
                    }
                }
                return true;
            }

            constexpr int lineCount = 12;
            constexpr int doubleClickDelay = 32;

            constexpr Point title { 49, 16 };
            constexpr Point scrollButton { 36, 44 };

            struct ListRect {
                int x, y, width, height;
            };
            constexpr ListRect list { 55, 49, 190, 124 };

            namespace load {
                constexpr Point doneButton { 58, 187 };
                constexpr Point doneLabel { 79, 187 };
                constexpr Point cancelButton { 163, 187 };
                constexpr Point cancelLabel { 182, 187 };
            } // namespace load

            namespace save {
                constexpr Point doneButton { 58, 214 };
                constexpr Point doneLabel { 79, 213 };
                constexpr Point cancelButton { 163, 214 };
                constexpr Point cancelLabel { 182, 213 };
            } // namespace save

            int createScrollButton(int windowId, Point pos, int buttonId, int shortKey, int arrowKey, int hoverKey,
                const FrmImage& upFrm, const FrmImage& downFrm)
            {
                int btn = buttonCreate(windowId, pos.x, pos.y, downFrm.getWidth(), downFrm.getHeight(),
                    shortKey, arrowKey, hoverKey, buttonId,
                    upFrm.getData(), downFrm.getData(), nullptr, BUTTON_FLAG_TRANSPARENT);

                if (btn != -1) buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
                return btn;
            }

            struct ModeConfig {
                Point doneButton;
                Point doneLabel;
                Point cancelButton;
                Point cancelLabel;
            };

            struct Context {
                UniqueWindow window;
                unsigned char* windowBuffer;
                MessageList messageList;
            };

            bool initInterface(Context& ctx, DialogFrms& frms, const InterfaceFrmId* frmIds, const ModeConfig& cfg,
                const char* title, int& x, int& y)
            {
                if (!loadImages(frms, frmIds)) return false;

                int bgWidth = frms.background.getWidth(), bgHeight = frms.background.getHeight();
                x += (screenGetWidth() - 640) / 2, y += (screenGetHeight() - 480) / 2;

                ctx.window.reset(windowCreate(x, y, bgWidth, bgHeight, static_cast<ColorWithFlags>(256), WINDOW_MODAL | WINDOW_MOVE_ON_TOP));
                if (ctx.window.get() == -1) return false;

                ctx.windowBuffer = windowGetBuffer(ctx.window.get());
                memcpy(ctx.windowBuffer, frms.background.getData(), static_cast<size_t>(bgWidth) * bgHeight);

                MessageListItem messageListItem;
                if (!messageListInit(&ctx.messageList)) return false;

                char path[COMPAT_MAX_PATH];
                snprintf(path, sizeof(path), "%s%s", asc_5186C8, "DBOX.MSG");
                if (!messageListLoad(&ctx.messageList, path)) return false;

                fontSetCurrent(103);

                const char* doneText = getmsg(&ctx.messageList, &messageListItem, 100);
                fontDrawText(ctx.windowBuffer + bgWidth * cfg.doneLabel.y + cfg.doneLabel.x, doneText, bgWidth, bgWidth, COLOR_DARK_YELLOW);

                const char* cancelText = getmsg(&ctx.messageList, &messageListItem, 103);
                fontDrawText(ctx.windowBuffer + bgWidth * cfg.cancelLabel.y + cfg.cancelLabel.x, cancelText, bgWidth, bgWidth, COLOR_DARK_YELLOW);

                int doneBtn = createRedButton(ctx.window.get(), cfg.doneButton, 500, frms.buttonNormal, frms.buttonPressed);
                int cancelBtn = createRedButton(ctx.window.get(), cfg.cancelButton, 501, frms.buttonNormal, frms.buttonPressed);

                int scrollUpBtn = createScrollButton(ctx.window.get(), scrollButton, 505, -1, 505, 506, frms.scrollUpNormal, frms.scrollUpPressed);

                Point scrollDownPos { scrollButton.x, scrollButton.y + frms.scrollUpPressed.getHeight() };
                int scrollDownBtn = createScrollButton(ctx.window.get(), scrollDownPos, 503, -1, 503, 504, frms.scrollDownNormal, frms.scrollDownPressed);

                buttonCreate(ctx.window.get(), list.x, list.y, list.width, list.height, -1, -1, -1, 502, nullptr, nullptr, nullptr, 0);

                if (title != nullptr) {
                    fontDrawText(ctx.windowBuffer + bgWidth * file::title.y + file::title.x, title, bgWidth, bgWidth, COLOR_DARK_YELLOW);
                }

                return true;
            }

        } // namespace file

        namespace alert {
            typedef enum DialogType {
                DIALOG_TYPE_MEDIUM,
                DIALOG_TYPE_LARGE,
                DIALOG_TYPE_COUNT,
            } DialogType;

            // 0x5108C8 dbox
            static constexpr InterfaceFrmId kDialogBoxBackgroundFrmIds[DIALOG_TYPE_COUNT] = {
                InterfaceFrameId::MediumDialog,
                InterfaceFrameId::LargeDialog,
            };

            struct DialogConfig {
                InterfaceFrmId backgroundFrameId;
                int x, y, doneX, doneY, lines;
            };

            constexpr DialogConfig kDialogConfigs[DIALOG_TYPE_COUNT] = {
                { InterfaceFrameId::MediumDialog, 29, 23, 51, 81, 5 },
                { InterfaceFrameId::LargeDialog, 29, 27, 37, 98, 6 }
            };

            struct DialogFrms {
                FrmImage done;
                FrmImage buttonUp;
                FrmImage buttonDown;
            };

            bool loadImages(DialogFrms& images)
            {
                return images.done.lock(InterfaceFrameId::DoneBox)
                    && images.buttonUp.lock(InterfaceFrameId::LittleRedButtonUp)
                    && images.buttonDown.lock(InterfaceFrameId::LittleRedButtonDown);
            };
        } // namespace alert
    } // namespace dbox
} // namespace

typedef enum FileDialogScrollDirection {
    FILE_DIALOG_SCROLL_DIRECTION_NONE,
    FILE_DIALOG_SCROLL_DIRECTION_UP,
    FILE_DIALOG_SCROLL_DIRECTION_DOWN,
} FileDialogScrollDirection;

static void fileDialogRenderFileList(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch);

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
    using namespace fallout::dbox;

    ScopedFont mainFontGuard(101); // default font for input box
    static std::string result;

    input::DialogFrms frms;
    if (!input::loadImages(frms)) return nullptr;

    int windowWidth = frms.background.getWidth();
    int windowHeight = frms.background.getHeight();

    UniqueWindow window(windowCreate(windowX, windowY, windowWidth, windowHeight, static_cast<ColorWithFlags>(256), flags));
    if (window.get() == -1) return nullptr;

    unsigned char* windowBuf = windowGetBuffer(window.get());
    memcpy(windowBuf, frms.background.getData(), static_cast<size_t>(windowWidth) * windowHeight);

    input::drawBox(windowBuf, windowWidth, input::nameBoxPos, frms.nameBox);
    input::drawBox(windowBuf, windowWidth, input::doneBoxPos, frms.doneBox);

    {
        ScopedFont buttonFontGuard(103); // "Done" button font
        fontDrawText(windowBuf + windowWidth * input::doneLabelPos.y + input::doneLabelPos.x,
            doneText, windowWidth, windowWidth, COLOR_DARK_YELLOW);
    }

    int doneBtn = dbox::createRedButton(window.get(), input::doneButtonPos, 500, frms.buttonUp, frms.buttonDown);

    windowRefresh(window.get());

    std::string editableText = (currentInput && strcmp(currentInput, "None") != 0) ? currentInput : "";
    int status = _get_input_str(window.get(), 500, editableText, 11, 23, 19, COLOR_GREEN | DRAW_TEXT_FLAG_NONE, Color(100), 0);

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
    using namespace fallout::dbox;

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
        dialogType = alert::DIALOG_TYPE_LARGE;
    } else if ((flags & DIALOG_BOX_MEDIUM) != 0) {
        dialogType = alert::DIALOG_TYPE_MEDIUM;
    } else {
        if (hasTitle) linesCount++;

        dialogType = (maximumLineWidth > 168 || linesCount > 5) ? alert::DIALOG_TYPE_LARGE : alert::DIALOG_TYPE_MEDIUM;
    }

    alert::DialogConfig config = alert::kDialogConfigs[dialogType];

    FrmImage bg;
    if (!bg.lock(alert::kDialogBoxBackgroundFrmIds[dialogType])) return -1;

    // Maintain original position in original resolution, otherwise center it.
    x += (screenGetWidth() - 640) / 2;
    y += (screenGetHeight() - 480) / 2;

    UniqueWindow window(windowCreate(x, y, bg.getWidth(), bg.getHeight(), static_cast<ColorWithFlags>(256), WINDOW_MODAL | WINDOW_MOVE_ON_TOP));
    if (window.get() == -1) return -1;

    unsigned char* windowBuf = windowGetBuffer(window.get());
    size_t bufferSize = static_cast<size_t>(bg.getWidth()) * bg.getHeight();
    memcpy(windowBuf, bg.getData(), bufferSize);

    alert::DialogFrms frms;

    // Resources init
    const bool hasPrimaryButton = (flags & DIALOG_BOX_NO_BUTTONS) == 0;
    const bool hasSecondaryButton = hasTwoButtons && dialogType == alert::DIALOG_TYPE_LARGE;

    if (hasPrimaryButton || hasSecondaryButton) {
        if (!alert::loadImages(frms) || !messageListInit(&messageList)) return -1;

        std::string path = std::string(asc_5186C8) + "DBOX.MSG";
        if (!messageListLoad(&messageList, path.c_str())) {
            messageListFree(&messageList);
            return -1;
        }
    }

    const int bgWidth = bg.getWidth();

    // Buttons
    {
        ScopedFont buttonFontGuard(103);

        // First button
        if (hasPrimaryButton) {
            const int doneBoxX = hasTwoButtons ? config.doneX : (bgWidth - frms.done.getWidth()) / 2;

            blitBufferToBuffer(frms.done.getData(), frms.done.getWidth(), frms.done.getHeight(), frms.done.getWidth(),
                windowBuf + bgWidth * config.doneY + doneBoxX, bgWidth);

            messageListItem.num = ((flags & DIALOG_BOX_YES_NO) == 0) ? 100 : 101; // 100 - DONE, 101 - YES
            if (messageListGetItem(&messageList, &messageListItem)) {
                fontDrawText(windowBuf + bgWidth * (config.doneY + 3) + doneBoxX + 35,
                    messageListItem.text, bgWidth, bgWidth, COLOR_DARK_YELLOW);
            }

            dbox::Point btnPos = { doneBoxX + 13, config.doneY + 4 };
            int btn = createRedButton(window.get(), btnPos, 500, frms.buttonUp, frms.buttonDown);

            initializedButtons = true;
        }

        // Second button
        if (hasSecondaryButton) {
            if ((flags & DIALOG_BOX_YES_NO) != 0) {
                secondaryButtonText = getmsg(&messageList, &messageListItem, 102); // 102 - NO
            }

            const int blitXOffset = hasPrimaryButton ? (config.doneX + frms.done.getWidth() + 24) : config.doneX;
            const int textXOffset = hasPrimaryButton ? (config.doneX + frms.done.getWidth() + 59) : (config.doneX + 35);
            const int buttonXOffset = hasPrimaryButton ? (config.doneX + frms.done.getWidth() + 37) : (config.doneX + 13);

            blitBufferToBufferTrans(frms.done.getData(), frms.done.getWidth(), frms.done.getHeight(), frms.done.getWidth(),
                windowBuf + bgWidth * config.doneY + blitXOffset, bgWidth);

            if (secondaryButtonText != nullptr) {
                fontDrawText(windowBuf + bgWidth * (config.doneY + 3) + textXOffset,
                    secondaryButtonText, bgWidth, bgWidth, COLOR_DARK_YELLOW);
            }

            dbox::Point btnPos = { buttonXOffset, config.doneY + 4 };
            int btn = createRedButton(window.get(), btnPos, 501, frms.buttonUp, frms.buttonDown);

            initializedButtons = true;
        }
    }

    ScopedFont mainFontGuard(101);

    int nextY = config.y;
    int maxY = config.y + config.lines * fontGetLineHeight();
    int maxWidth = bg.getWidth() - config.x * 2;

    auto drawLine = [&](const char* text, int currentY, ColorWithFlags textColor) {
        int bgWidth = bg.getWidth();
        int xOffset = 0;

        if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
            xOffset = config.x;
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

            const int maxWidth = bg.getWidth() - config.x * 2;

            short beginnings[WORD_WRAP_MAX_COUNT];
            short subLineCount = 0;

            if (wordWrap(body[index], maxWidth, beginnings, &subLineCount) == 0) {
                numberOfLines += subLineCount - 1;
            }
        }

        if (numberOfLines > config.lines) {
            numberOfLines = config.lines;
        }

        nextY += (config.lines - numberOfLines) * fontGetLineHeight() / 2;
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

    windowRefresh(window.get());

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
    using namespace fallout::dbox;
    ScopedFont mainFont(103);

    bool isScrollable = (fileListLength > file::lineCount);

    int selectedFileIndex = 0, pageOffset = 0;
    int maxPageOffset = fileListLength - (file::lineCount + 1);
    if (maxPageOffset < 0) {
        maxPageOffset = std::max(0, fileListLength - 1);
    }

    constexpr file::ModeConfig saveConfig {
        file::load::doneButton,
        file::load::doneLabel,
        file::load::cancelButton,
        file::load::cancelLabel
    };

    file::DialogFrms frms;
    file::Context ctx;

    if (!file::initInterface(ctx, frms, file::kLoadFileDialogFrmIds, saveConfig, title, x, y)) {
        return -1;
    }

    fontSetCurrent(101);

    int bgWidth = frms.background.getWidth();

    fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
    windowRefresh(ctx.window.get());

    int doubleClickSelectedFileIndex = -2;
    int doubleClickTimer = file::doubleClickDelay;

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

            int selectedLine = (mouseY - y - file::list.y) / fontGetLineHeight();
            if (selectedLine - 1 < 0) {
                selectedLine = 0;
            }

            if (isScrollable || selectedLine < fileListLength) {
                if (selectedLine >= file::lineCount) {
                    selectedLine = file::lineCount - 1;
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
            fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
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
                fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
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
                        if (selectedFileIndex > file::lineCount) {
                            selectedFileIndex = file::lineCount - 1;
                        }
                        pageOffset = maxPageOffset;
                    }
                } else {
                    selectedFileIndex++;
                    if (selectedFileIndex > maxPageOffset) {
                        selectedFileIndex = maxPageOffset;
                    }
                }
                fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_HOME:
                selectedFileIndex = 0;
                pageOffset = 0;
                fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_END:
                if (isScrollable) {
                    selectedFileIndex = file::lineCount - 1;
                    pageOffset = maxPageOffset;
                } else {
                    selectedFileIndex = maxPageOffset;
                    pageOffset = 0;
                }
                fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
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
                                if (selectedFileIndex >= file::lineCount) {
                                    selectedFileIndex = file::lineCount - 1;
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

                    fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
                    windowRefresh(ctx.window.get());
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
            windowRefresh(ctx.window.get());

            doubleClickTimer--;
            if (doubleClickTimer == 0) {
                doubleClickTimer = file::doubleClickDelay;
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

    messageListFree(&ctx.messageList);

    return rc;
}

// 0x41EA78 save_file_dialog
int showSaveFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags)
{
    using namespace fallout::dbox;
    ScopedFont mainFont(103);

    bool isScrollable = (fileListLength > file::lineCount);

    int selectedFileIndex = 0, pageOffset = 0;
    int maxPageOffset = fileListLength - (file::lineCount + 1);
    if (maxPageOffset < 0) {
        maxPageOffset = std::max(0, fileListLength - 1);
    }

    constexpr file::ModeConfig saveConfig {
        file::save::doneButton,
        file::save::doneLabel,
        file::save::cancelButton,
        file::save::cancelLabel
    };

    file::DialogFrms frms;
    file::Context ctx;

    if (!file::initInterface(ctx, frms, file::kSaveFileDialogFrmIds, saveConfig, title, x, y)) {
        return -1;
    }

    int bgWidth = frms.background.getWidth();

    fontSetCurrent(101);

    int cursorHeight = fontGetLineHeight();
    int cursorWidth = fontGetStringWidth("_") - 4;
    fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);

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

    unsigned char* fileNameBufferPtr = ctx.windowBuffer + bgWidth * 190 + 57;

    bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, bgWidth, Color(100));
    fontDrawText(fileNameBufferPtr, fileNameCopy, bgWidth, bgWidth, COLOR_GREEN);

    windowRefresh(ctx.window.get());

    beginTextInput();

    int blinkingCounter = 3;
    bool blink = false;

    int doubleClickSelectedFileIndex = -2;
    int doubleClickTimer = file::doubleClickDelay;

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
            bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, bgWidth, Color(100));
            fileNameCopy[fileNameCopyLength - 1] = ' ';
            fileNameCopy[fileNameCopyLength] = '\0';
            fontDrawText(fileNameBufferPtr, fileNameCopy, bgWidth, bgWidth, COLOR_GREEN);
            fileNameCopyLength--;
            windowRefresh(ctx.window.get());
        } else if (keyCode < KEY_FIRST_INPUT_CHARACTER || keyCode > KEY_LAST_INPUT_CHARACTER || fileNameCopyLength >= 8) {
            if (keyCode == 502 && fileListLength != 0) {
                int mouseX;
                int mouseY;
                mouseGetPosition(&mouseX, &mouseY);

                int selectedLine = (mouseY - y - file::list.x) / fontGetLineHeight();
                if (selectedLine - 1 < 0) {
                    selectedLine = 0;
                }

                if (isScrollable || selectedLine < fileListLength) {
                    if (selectedLine >= file::lineCount) {
                        selectedLine = file::lineCount - 1;
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
                    bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, bgWidth, Color(100));
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

                    fontDrawText(fileNameBufferPtr, fileNameCopy, bgWidth, bgWidth, COLOR_GREEN);
                    fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
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
                    fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_ARROW_DOWN:
                    if (isScrollable) {
                        pageOffset++;
                        if (pageOffset >= maxPageOffset) {
                            selectedFileIndex++;
                            if (selectedFileIndex >= file::lineCount) {
                                selectedFileIndex = file::lineCount - 1;
                            }
                            pageOffset = maxPageOffset;
                        }
                    } else {
                        selectedFileIndex++;
                        if (selectedFileIndex > maxPageOffset) {
                            selectedFileIndex = maxPageOffset;
                        }
                    }
                    fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_HOME:
                    selectedFileIndex = 0;
                    pageOffset = 0;
                    fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
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
                    fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                }
            }
        } else if (_isdoschar(keyCode)) {
            bufferFill(fileNameBufferPtr, fontGetStringWidth(fileNameCopy), cursorHeight, bgWidth, Color(100));

            fileNameCopy[fileNameCopyLength] = keyCode & 0xFF;
            fileNameCopy[fileNameCopyLength + 1] = ' ';
            fileNameCopy[fileNameCopyLength + 2] = '\0';
            fontDrawText(fileNameBufferPtr, fileNameCopy, bgWidth, bgWidth, COLOR_GREEN);
            fileNameCopyLength++;

            windowRefresh(ctx.window.get());
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
                                if (selectedFileIndex >= file::lineCount) {
                                    selectedFileIndex = file::lineCount - 1;
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

                    fileDialogRenderFileList(ctx.windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, bgWidth);
                    windowRefresh(ctx.window.get());
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

                    bufferFill(fileNameBufferPtr + fontGetStringWidth(fileNameCopy) - cursorWidth, cursorWidth, cursorHeight - 2, bgWidth, color);
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

                bufferFill(fileNameBufferPtr + fontGetStringWidth(fileNameCopy) - cursorWidth, cursorWidth, cursorHeight - 2, bgWidth, color);
            }

            windowRefresh(ctx.window.get());

            doubleClickTimer--;
            if (doubleClickTimer == 0) {
                doubleClickTimer = file::doubleClickDelay;
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

    messageListFree(&ctx.messageList);

    return rc;
}

// 0x41FBDC PrntFlist
static void fileDialogRenderFileList(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch)
{
    using namespace fallout::dbox;

    int lineHeight = fontGetLineHeight();
    int y = file::list.y;
    bufferFill(buffer + y * pitch + file::list.x, file::list.width, file::list.height, pitch, static_cast<Color>(100));
    if (fileListLength != 0) {
        if (fileListLength - pageOffset > file::lineCount) {
            fileListLength = file::lineCount;
        }

        for (int index = 0; index < fileListLength; index++) {
            Color color = index == selectedIndex ? COLOR_LIGHT_YELLOW : COLOR_GREEN;
            fontDrawText(buffer + pitch * y + file::list.x, fileList[pageOffset + index], file::list.width, pitch, color);
            y += lineHeight;
        }
    }
}

} // namespace fallout
