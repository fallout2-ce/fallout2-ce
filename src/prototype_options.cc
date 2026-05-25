#include "prototype_options.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "art.h"
#include "color.h"
#include "cycle.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "palette.h"
#include "sfall_kb_helpers.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"
#include "window_manager_private.h"
#include "word_wrap.h"

namespace fallout {

namespace {

constexpr int kWindowWidth = 640;
constexpr int kWindowHeight = 480;

constexpr int kTabX = 18;
constexpr int kTabY = 24;

constexpr int kLeftPaneX = 18;
constexpr int kLeftPaneY = 68;
constexpr int kLeftPaneWidth = 292;
constexpr int kLeftPaneHeight = 336;
constexpr int kLeftPaneContentX = kLeftPaneX + 14;
constexpr int kLeftPaneContentY = kLeftPaneY + 12;
constexpr int kLeftPaneContentBottom = kLeftPaneY + kLeftPaneHeight - 12;
constexpr int kLeftPaneValueX = kLeftPaneContentX + 150;
constexpr int kLeftPaneValueWidth = 90;
constexpr int kLeftPaneRowWidth = 238;

constexpr int kInfoPanelX = 325;
constexpr int kInfoPanelY = 70;
constexpr int kInfoPanelWidth = 305;
constexpr int kInfoPanelHeight = 230;
constexpr int kInfoPanelSourceX = 268;
constexpr int kInfoPanelSourceY = 0;

constexpr int kButtonY = 430;

constexpr int kScrollUpButtonEventCode = 7000;
constexpr int kScrollDownButtonEventCode = 7001;
constexpr int kPrintButtonEventCode = 7002;
constexpr int kDoneButtonEventCode = 7003;
constexpr int kCancelButtonEventCode = 7004;

constexpr int kDoneBoxFrmId = 209;
constexpr int kLittleRedButtonUpFrmId = 8;
constexpr int kLittleRedButtonDownFrmId = 9;
constexpr int kScrollDownArrowOffFrmId = 181;
constexpr int kScrollDownArrowOnFrmId = 182;
constexpr int kScrollUpArrowOffFrmId = 199;
constexpr int kScrollUpArrowOnFrmId = 200;
constexpr int kTabFrmIds[] = { 180, 178, 179 };
constexpr int kTabIllustrationFrmIds[] = { 7, 11, 27 };
constexpr int kPerkWindowFrmId = 86;

constexpr int kTextColorIndex = 992;
constexpr int kHeadingColorIndex = 32747;
constexpr int kMutedColorIndex = 8804;
constexpr int kButtonTextColorIndex = 18979;
constexpr int kBorderColorIndex = 8456;
constexpr int kPanelFillColorIndex = 32328;
constexpr int kPanelShadowColorIndex = 17969;

enum class PrototypeOptionType {
    Section,
    Boolean,
    MultiSelect,
    Text,
    Control,
};

enum class PrototypePreset {
    Vanilla,
    Default,
};

struct PrototypeOptionValue {
    bool boolValue = false;
    int selectedChoice = 0;
    std::string textValue;
    int controlKeyCode = 23;
};

struct PrototypeOptionRow {
    PrototypeOptionType type;
    const char* label;
    const char* description;
    bool boolValue = false;
    std::array<const char*, 4> choices = {};
    int choiceCount = 0;
    int selectedChoice = 0;
    std::string textValue;
    int controlKeyCode = 23;
    PrototypeOptionValue vanillaValue;
    PrototypeOptionValue defaultValue;
};

struct PrototypeOptionsTab {
    const char* label;
    int illustrationFrmId;
    std::vector<PrototypeOptionRow> rows;
    int selectedRow = 0;
    int scrollOffset = 0;
};

struct VisibleRow {
    int rowIndex;
    int y;
    int height;
};

PrototypeOptionRow makeSection(const char* label, const char* description)
{
    PrototypeOptionRow row;
    row.type = PrototypeOptionType::Section;
    row.label = label;
    row.description = description;
    return row;
}

PrototypeOptionRow makeBoolean(const char* label, const char* description, bool currentValue, bool vanillaValue, bool defaultValue)
{
    PrototypeOptionRow row;
    row.type = PrototypeOptionType::Boolean;
    row.label = label;
    row.description = description;
    row.boolValue = currentValue;
    row.vanillaValue.boolValue = vanillaValue;
    row.defaultValue.boolValue = defaultValue;
    return row;
}

PrototypeOptionRow makeMultiSelect(const char* label,
    const char* description,
    std::array<const char*, 4> choices,
    int choiceCount,
    int currentValue,
    int vanillaValue,
    int defaultValue)
{
    PrototypeOptionRow row;
    row.type = PrototypeOptionType::MultiSelect;
    row.label = label;
    row.description = description;
    row.choices = choices;
    row.choiceCount = choiceCount;
    row.selectedChoice = currentValue;
    row.vanillaValue.selectedChoice = vanillaValue;
    row.defaultValue.selectedChoice = defaultValue;
    return row;
}

PrototypeOptionRow makeText(const char* label,
    const char* description,
    const char* currentValue,
    const char* vanillaValue,
    const char* defaultValue)
{
    PrototypeOptionRow row;
    row.type = PrototypeOptionType::Text;
    row.label = label;
    row.description = description;
    row.textValue = currentValue;
    row.vanillaValue.textValue = vanillaValue;
    row.defaultValue.textValue = defaultValue;
    return row;
}

PrototypeOptionRow makeControl(const char* label, const char* description, int currentValue, int vanillaValue, int defaultValue)
{
    PrototypeOptionRow row;
    row.type = PrototypeOptionType::Control;
    row.label = label;
    row.description = description;
    row.controlKeyCode = currentValue;
    row.vanillaValue.controlKeyCode = vanillaValue;
    row.defaultValue.controlKeyCode = defaultValue;
    return row;
}

class PrototypeOptionsMenu {
public:
    PrototypeOptionsMenu();
    int run(bool animated);

private:
    bool init();
    void render();
    void renderBackground();
    void renderTabs();
    void renderLeftPane();
    void renderInfoPanel();
    void renderButtons();
    void handleMouseInput(int mouseEvent);
    void handleKeyboardInput(int keyCode, int* rcPtr);
    void handleTabClick(int mouseX, int mouseY);
    void handleRowClick(int mouseX, int mouseY);
    void switchTab(int tabIndex);
    void moveSelection(int delta);
    void scrollRows(int delta, bool followSelection);
    void ensureSelectedRowVisible();
    void normalizeScrollOffset();
    bool canScrollDown() const;
    bool isRowVisible(int rowIndex) const;
    int findFirstSelectableRow(const PrototypeOptionsTab& tab) const;
    int findNextSelectableRow(const PrototypeOptionsTab& tab, int startRow, int delta) const;
    int rowHeight(const PrototypeOptionRow& row) const;
    PrototypeOptionRow* currentRow();
    const PrototypeOptionRow* currentRow() const;
    void activateCurrentRow();
    void updateSelectionAfterScroll();
    void editTextRow(PrototypeOptionRow* row, int rowY);
    void beginControlCapture();
    void completeControlCapture(int keyCode);
    void cancelControlCapture();
    void applyPreset(PrototypePreset preset);
    void applyPresetToRow(PrototypeOptionRow& row, PrototypePreset preset);
    void renderWrappedText(int x, int y, int width, const char* text, int color);
    std::string formatControlValue(int keyCode) const;
    const char* describeControlType(const PrototypeOptionRow& row) const;
    bool isCapturableKeyCode(int keyCode) const;

    int currentTabIndex = 0;
    int window = -1;
    unsigned char* windowBuffer = nullptr;
    int buttonPlateX = 0;
    int scrollButtonX = 0;
    int scrollDownButtonY = 0;
    int statusTick = 0;
    bool isCapturingControl = false;
    int capturingControlRowIndex = -1;
    std::string statusText;
    std::vector<PrototypeOptionsTab> tabs;
    std::vector<VisibleRow> visibleRows;

    FrmImage doneBoxImage;
    FrmImage perkWindowImage;
    FrmImage scrollUpArrowOffImage;
    FrmImage scrollDownArrowOffImage;
    FrmImage littleRedButtonUpImage;
    std::array<FrmImage, 3> tabImages;
};

PrototypeOptionsMenu::PrototypeOptionsMenu()
{
    tabs = {
        {
            "DISPLAY",
            kTabIllustrationFrmIds[0],
            {
                makeSection("INTERFACE", "Visual controls for this screen and the main interface."),
                makeBoolean("Show item highlights", "Toggles a prototype checkbox row with an immediate on or off state.", true, false, true),
                makeMultiSelect("Combat text density", "Prototype dropdown. Cycling it swaps between a few canned presets.", { "Minimal", "Standard", "Verbose" }, 3, 1, 0, 1),
                makeText("Profile label", "Prototype free-text row. Selecting it opens inline text input on the underline.", "Wasteland", "Vault Dweller", "Wasteland"),
                makeControl("Pip-Boy key", "Prototype control binding. Activate it, then press a key to store the HOOK_KEYPRESS code.", 25, 25, 23),
                makeSection("VIDEO", "Additional grouped rows to prove scrolling and section headers."),
                makeBoolean("Scanline overlay", "Another checkbox placeholder for the scrollable list.", false, true, false),
                makeMultiSelect("Window mode", "Placeholder dropdown row for display settings.", { "Classic", "Wide", "Fullscreen" }, 3, 0, 0, 1),
                makeBoolean("Monitor bloom", "A final filler row so this tab needs scrolling.", true, false, true),
                makeBoolean("CRT curvature", "Extra filler row to extend the scroll range.", false, true, false),
                makeMultiSelect("Gamma preset", "Another dropdown for scroll testing.", { "Low", "Normal", "High" }, 3, 1, 0, 1),
                makeText("HUD palette", "Text field placeholder near the middle of the long list.", "Amber", "Green", "Amber"),
                makeControl("Map key", "Second control row to test bindings mid-scroll.", 50, 50, 23),
                makeSection("OVERLAYS", "More grouped rows to keep the list longer than one page."),
                makeBoolean("Show grid markers", "Additional checkbox row for scrolling.", true, false, true),
                makeBoolean("Show cover icons", "Additional checkbox row for scrolling.", false, false, false),
                makeMultiSelect("Damage floaters", "Another repeated dropdown control.", { "Off", "Hits", "Full" }, 3, 2, 0, 1),
                makeText("Combat theme", "Longer list text field placeholder.", "Dustwind", "Classic", "Dustwind"),
                makeSection("ACCESSIBILITY", "Last section to guarantee substantial scroll distance."),
                makeBoolean("Large cursors", "Prototype accessibility toggle.", false, false, false),
                makeBoolean("High contrast text", "Prototype accessibility toggle.", true, false, true),
                makeMultiSelect("Subtitle scale", "Prototype dropdown near the bottom.", { "Small", "Medium", "Large" }, 3, 1, 0, 1),
                makeText("Colorblind profile", "Bottom text field to verify scrolling reaches the end.", "None", "None", "Protanopia"),
            },
        },
        {
            "AUDIO",
            kTabIllustrationFrmIds[1],
            {
                makeSection("MIX", "Audio rows using the same prototype controls."),
                makeBoolean("Music enabled", "Checkbox prototype for a master enable.", true, true, true),
                makeMultiSelect("Sound profile", "Dropdown prototype standing in for a future preset list.", { "Original", "Expanded", "Studio" }, 3, 0, 0, 1),
                makeText("Radio callsign", "Text prototype with an underline field you can edit.", "K-LAM", "K-LAM", "CE-RADIO"),
                makeControl("Push-to-talk key", "Audio tab control binding example.", 20, 20, 57),
                makeSection("VOICE", "Section break inside the scroll pane."),
                makeMultiSelect("NPC chatter", "Placeholder verbosity dropdown.", { "Low", "Normal", "High" }, 3, 1, 0, 1),
                makeBoolean("Combat taunts", "Prototype checkbox for supplemental voice lines.", false, false, true),
            },
        },
        {
            "PLAY",
            kTabIllustrationFrmIds[2],
            {
                makeSection("RULES", "Gameplay-facing rows for another tab state."),
                makeBoolean("Auto end turns", "Prototype checkbox on the gameplay tab.", false, false, true),
                makeMultiSelect("Difficulty preset", "Prototype dropdown for rule bundles.", { "Classic", "Rebalanced", "Hardcore" }, 3, 0, 0, 1),
                makeText("Mod pack name", "Prototype text field for future config-backed values.", "Vanilla+", "Vanilla", "Vanilla+"),
                makeControl("Inventory key", "Control binding example on the gameplay tab.", 23, 23, 34),
                makeSection("ASSISTS", "Extra rows to keep the pane scrollable."),
                makeBoolean("Quest pings", "Prototype checkbox for helper overlays.", true, false, true),
                makeBoolean("Loot hints", "Prototype checkbox for item prompts.", true, false, true),
                makeMultiSelect("Aim assist", "Prototype dropdown to demonstrate a repeated control type.", { "Off", "Low", "High" }, 3, 1, 0, 1),
            },
        },
    };

    for (auto& tab : tabs) {
        tab.selectedRow = findFirstSelectableRow(tab);
    }
}

int PrototypeOptionsMenu::run(bool animated)
{
    ScopedGameMode gm(GameMode::kOptions);

    if (!init()) {
        return -1;
    }

    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    touch_set_touchscreen_mode(true);

    if (animated) {
        colorPaletteLoad("color.pal");
        paletteFadeTo(_cmap);
    }

    render();

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        int mouseEvent = mouseGetEvent();

        handleMouseInput(mouseEvent);
        handleKeyboardInput(keyCode, &rc);

        if (statusTick != 0 && getTicksSince(statusTick) > 1500) {
            statusTick = 0;
            statusText.clear();
            render();
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (animated) {
        paletteFadeTo(gPaletteBlack);
    }

    if (cursorWasHidden) {
        mouseHideCursor();
    }

    windowDestroy(window);
    return rc;
}

bool PrototypeOptionsMenu::init()
{
    if (!doneBoxImage.lock(FrmId(OBJ_TYPE_INTERFACE, kDoneBoxFrmId))
        || !perkWindowImage.lock(FrmId(OBJ_TYPE_INTERFACE, kPerkWindowFrmId))
        || !scrollUpArrowOffImage.lock(FrmId(OBJ_TYPE_INTERFACE, kScrollUpArrowOffFrmId))
        || !scrollDownArrowOffImage.lock(FrmId(OBJ_TYPE_INTERFACE, kScrollDownArrowOffFrmId))
        || !littleRedButtonUpImage.lock(FrmId(OBJ_TYPE_INTERFACE, kLittleRedButtonUpFrmId))) {
        return false;
    }

    for (int index = 0; index < 3; index++) {
        if (!tabImages[index].lock(FrmId(OBJ_TYPE_INTERFACE, kTabFrmIds[index]))) {
            return false;
        }
    }

    int windowX = (screenGetWidth() - kWindowWidth) / 2;
    int windowY = (screenGetHeight() - kWindowHeight) / 2;
    window = windowCreate(windowX, windowY, kWindowWidth, kWindowHeight, _colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (window == -1) {
        return false;
    }

    windowBuffer = windowGetBuffer(window);

    scrollButtonX = kLeftPaneX + kLeftPaneWidth - scrollUpArrowOffImage.getWidth() - 8;
    scrollDownButtonY = kLeftPaneY + kLeftPaneHeight - scrollDownArrowOffImage.getHeight() - 8;

    int scrollUpButton = buttonCreateWithFrm(window,
        scrollButtonX,
        kLeftPaneY + 8,
        -1,
        -1,
        kScrollUpButtonEventCode,
        kScrollUpButtonEventCode,
        FrmId(OBJ_TYPE_INTERFACE, kScrollUpArrowOffFrmId),
        FrmId(OBJ_TYPE_INTERFACE, kScrollUpArrowOnFrmId),
        {},
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpButton == -1) {
        return false;
    }
    buttonSetCallbacks(scrollUpButton, _gsound_red_butt_press, nullptr);

    int scrollDownButton = buttonCreateWithFrm(window,
        scrollButtonX,
        scrollDownButtonY,
        -1,
        -1,
        kScrollDownButtonEventCode,
        kScrollDownButtonEventCode,
        FrmId(OBJ_TYPE_INTERFACE, kScrollDownArrowOffFrmId),
        FrmId(OBJ_TYPE_INTERFACE, kScrollDownArrowOnFrmId),
        {},
        BUTTON_FLAG_TRANSPARENT);
    if (scrollDownButton == -1) {
        return false;
    }
    buttonSetCallbacks(scrollDownButton, _gsound_red_butt_press, nullptr);

    buttonPlateX = kInfoPanelX + (kInfoPanelWidth - doneBoxImage.getWidth() * 3) / 2;
    const int buttonCodes[] = { kPrintButtonEventCode, kDoneButtonEventCode, kCancelButtonEventCode };
    for (int index = 0; index < 3; index++) {
        int buttonX = buttonPlateX + index * doneBoxImage.getWidth() + index * 2 + 13;
        int button = buttonCreateWithFrm(window,
            buttonX,
            kButtonY + 4,
            -1,
            -1,
            buttonCodes[index],
            buttonCodes[index],
            FrmId(OBJ_TYPE_INTERFACE, kLittleRedButtonUpFrmId),
            FrmId(OBJ_TYPE_INTERFACE, kLittleRedButtonDownFrmId),
            {},
            BUTTON_FLAG_TRANSPARENT);
        if (button == -1) {
            return false;
        }
        buttonSetCallbacks(button, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    return true;
}

void PrototypeOptionsMenu::render()
{
    renderBackground();
    renderTabs();
    renderLeftPane();
    renderInfoPanel();
    renderButtons();
    windowRefresh(window);
}

void PrototypeOptionsMenu::renderBackground()
{
    bufferFill(windowBuffer, kWindowWidth, kWindowHeight, kWindowWidth, _colorTable[0]);
    bufferDrawRect(windowBuffer, kWindowWidth, 0, 0, kWindowWidth - 1, kWindowHeight - 1, _colorTable[kBorderColorIndex]);
    bufferDrawRect(windowBuffer, kWindowWidth, 1, 1, kWindowWidth - 2, kWindowHeight - 2, _colorTable[kMutedColorIndex]);

    windowFill(window, kLeftPaneX, kLeftPaneY, kLeftPaneWidth, kLeftPaneHeight, _colorTable[0]);
    bufferDrawRect(windowBuffer, kWindowWidth, kLeftPaneX, kLeftPaneY, kLeftPaneX + kLeftPaneWidth, kLeftPaneY + kLeftPaneHeight, _colorTable[kHeadingColorIndex]);
    bufferDrawRect(windowBuffer, kWindowWidth, kLeftPaneX + 1, kLeftPaneY + 1, kLeftPaneX + kLeftPaneWidth - 1, kLeftPaneY + kLeftPaneHeight - 1, _colorTable[kMutedColorIndex]);

    if (!statusText.empty()) {
        fontSetCurrent(101);
        fontDrawText(windowBuffer + kWindowWidth * (kWindowHeight - 18) + 20,
            statusText.c_str(),
            kWindowWidth - 40,
            kWindowWidth,
            _colorTable[kHeadingColorIndex]);
    }
}

void PrototypeOptionsMenu::renderTabs()
{
    const FrmImage& tabImage = tabImages[currentTabIndex];
    blitBuffer2D(tabImage.getBuffer(), windowGetBuffer2D(window), kTabX, kTabY);

    fontSetCurrent(103);
    for (int tabIndex = 0; tabIndex < 3; tabIndex++) {
        int centerX = kTabX + tabImage.getWidth() * (2 * tabIndex + 1) / 6;
        int width = fontGetStringWidth(tabs[tabIndex].label);
        int color = _colorTable[tabIndex == currentTabIndex ? kMutedColorIndex : kButtonTextColorIndex];
        fontDrawText(windowBuffer + kWindowWidth * (kTabY + 6) + centerX - width / 2,
            tabs[tabIndex].label,
            tabImage.getWidth() / 3,
            kWindowWidth,
            color);
    }
}

void PrototypeOptionsMenu::renderLeftPane()
{
    PrototypeOptionsTab& tab = tabs[currentTabIndex];
    normalizeScrollOffset();
    ensureSelectedRowVisible();

    visibleRows.clear();
    fontSetCurrent(101);

    int y = kLeftPaneContentY;
    for (int rowIndex = tab.scrollOffset; rowIndex < static_cast<int>(tab.rows.size()); rowIndex++) {
        const PrototypeOptionRow& row = tab.rows[rowIndex];
        int height = rowHeight(row);
        if (y + height > kLeftPaneContentBottom) {
            break;
        }

        visibleRows.push_back({ rowIndex, y, height });

        if (row.type == PrototypeOptionType::Section) {
            fontDrawText(windowBuffer + kWindowWidth * y + kLeftPaneContentX,
                row.label,
                kLeftPaneRowWidth,
                kWindowWidth,
                _colorTable[kTextColorIndex]);
            bufferDrawLine(windowBuffer,
                kWindowWidth,
                kLeftPaneContentX,
                y + fontGetLineHeight() + 1,
                kLeftPaneContentX + kLeftPaneRowWidth - 8,
                y + fontGetLineHeight() + 1,
                _colorTable[kMutedColorIndex]);
        } else {
            bool isSelected = rowIndex == tab.selectedRow;
            int color = _colorTable[isSelected ? kHeadingColorIndex : kTextColorIndex];
            if (isSelected) {
                fontDrawText(windowBuffer + kWindowWidth * y + kLeftPaneContentX - 10, ">", 8, kWindowWidth, color);
            }

            fontDrawText(windowBuffer + kWindowWidth * y + kLeftPaneContentX, row.label, 148, kWindowWidth, color);

            switch (row.type) {
            case PrototypeOptionType::Boolean:
                fontDrawText(windowBuffer + kWindowWidth * y + kLeftPaneValueX,
                    row.boolValue ? "[ On ]" : "[ Off ]",
                    kLeftPaneValueWidth,
                    kWindowWidth,
                    color);
                break;
            case PrototypeOptionType::MultiSelect: {
                char buffer[64];
                std::snprintf(buffer, sizeof(buffer), "[ %s ]", row.choices[row.selectedChoice]);
                fontDrawText(windowBuffer + kWindowWidth * y + kLeftPaneValueX, buffer, kLeftPaneValueWidth, kWindowWidth, color);
                break;
            }
            case PrototypeOptionType::Text:
                fontDrawText(windowBuffer + kWindowWidth * y + kLeftPaneValueX,
                    row.textValue.c_str(),
                    kLeftPaneValueWidth,
                    kWindowWidth,
                    color);
                bufferDrawLine(windowBuffer,
                    kWindowWidth,
                    kLeftPaneValueX,
                    y + fontGetLineHeight() + 1,
                    kLeftPaneValueX + kLeftPaneValueWidth,
                    y + fontGetLineHeight() + 1,
                    color);
                break;
            case PrototypeOptionType::Control: {
                std::string value = formatControlValue(row.controlKeyCode);
                fontDrawText(windowBuffer + kWindowWidth * y + kLeftPaneValueX,
                    value.c_str(),
                    kLeftPaneValueWidth,
                    kWindowWidth,
                    color);
                break;
            }
            case PrototypeOptionType::Section:
                break;
            }
        }

        y += height;
    }
}

void PrototypeOptionsMenu::renderInfoPanel()
{
    blitBuffer2D(perkWindowImage.getBuffer(),
        kInfoPanelSourceX,
        kInfoPanelSourceY,
        kInfoPanelWidth,
        kInfoPanelHeight,
        windowGetBuffer2D(window),
        kInfoPanelX,
        kInfoPanelY);

    const PrototypeOptionRow* row = currentRow();
    if (row == nullptr) {
        return;
    }

    fontSetCurrent(102);
    fontDrawText(windowBuffer + kWindowWidth * (kInfoPanelY + 27) + kInfoPanelX + 12,
        row->label,
        kInfoPanelWidth - 24,
        kWindowWidth,
        _colorTable[0]);

    int nameHeight = fontGetLineHeight();
    windowDrawLine(window,
        kInfoPanelX + 12,
        kInfoPanelY + 27 + nameHeight,
        kInfoPanelX + 277,
        kInfoPanelY + 27 + nameHeight,
        _colorTable[0]);
    windowDrawLine(window,
        kInfoPanelX + 12,
        kInfoPanelY + 28 + nameHeight,
        kInfoPanelX + 277,
        kInfoPanelY + 28 + nameHeight,
        _colorTable[0]);

    fontSetCurrent(101);
    const char* kind = describeControlType(*row);

    fontDrawText(windowBuffer + kWindowWidth * (kInfoPanelY + 66) + kInfoPanelX + 12,
        kind,
        120,
        kWindowWidth,
        _colorTable[0]);

    renderWrappedText(kInfoPanelX + 12, kInfoPanelY + 92, 145, row->description, _colorTable[0]);

    FrmImage illustration;
    if (illustration.lock(FrmId(OBJ_TYPE_SKILLDEX, tabs[currentTabIndex].illustrationFrmId))) {
        int illustrationDestWidth = 104;
        int illustrationDestHeight = 120;
        int illustrationDestX = kInfoPanelX + 145;
        int illustrationDestY = kInfoPanelY + 64;
        blitBufferToBufferStretchTrans(illustration.getData(),
            illustration.getWidth(),
            illustration.getHeight(),
            illustration.getWidth(),
            windowBuffer + kWindowWidth * illustrationDestY + illustrationDestX,
            illustrationDestWidth,
            illustrationDestHeight,
            kWindowWidth);
    }
}

void PrototypeOptionsMenu::renderButtons()
{
    const char* labels[] = { "VANILLA", "DEFAULT", "DONE" };
    Buffer2D windowBuffer2D = windowGetBuffer2D(window);

    fontSetCurrent(103);
    for (int index = 0; index < 3; index++) {
        int x = buttonPlateX + index * doneBoxImage.getWidth() + index * 2;
        blitBuffer2D(doneBoxImage.getBuffer(), windowBuffer2D, x, kButtonY);

        int width = fontGetStringWidth(labels[index]);
        fontDrawText(windowBuffer + kWindowWidth * (kButtonY + 4) + x + 38 + (doneBoxImage.getWidth() - 42 - width) / 2,
            labels[index],
            doneBoxImage.getWidth(),
            kWindowWidth,
            _colorTable[kButtonTextColorIndex]);
    }
}

void PrototypeOptionsMenu::handleMouseInput(int mouseEvent)
{
    if (isCapturingControl) {
        return;
    }

    if ((mouseEvent & MOUSE_EVENT_WHEEL) != 0
        && mouseHitTestInWindow(window, kLeftPaneX, kLeftPaneY, kLeftPaneX + kLeftPaneWidth, kLeftPaneY + kLeftPaneHeight)) {
        int wheelX;
        int wheelY;
        mouseGetWheel(&wheelX, &wheelY);
        if (wheelY > 0) {
            scrollRows(-1, false);
        } else if (wheelY < 0) {
            scrollRows(1, false);
        }
        return;
    }

    if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
        return;
    }

    int mouseX;
    int mouseY;
    mouseGetPositionInWindow(window, &mouseX, &mouseY);

    handleTabClick(mouseX, mouseY);
    handleRowClick(mouseX, mouseY);
}

void PrototypeOptionsMenu::handleKeyboardInput(int keyCode, int* rcPtr)
{
    if (isCapturingControl) {
        if (keyCode == -1) {
            int scanCode = keyboardGetLastScanCode();
            int hookKeyCode = sfall_kb_get_key_from_scancode(scanCode);
            if (scanCode >= 0
                && scanCode < SDL_NUM_SCANCODES
                && hookKeyCode > 0
                && sfall_kb_get_scancode_from_key(hookKeyCode) == scanCode
                && gPressedPhysicalKeys[scanCode] != KEY_STATE_UP) {
                completeControlCapture(hookKeyCode);
            }
            return;
        }

        if (keyCode == KEY_ESCAPE) {
            cancelControlCapture();
        } else if (isCapturableKeyCode(keyCode)) {
            int hookKeyCode = sfall_kb_get_key_from_scancode(keyboardGetLastScanCode());
            if (hookKeyCode > 0) {
                completeControlCapture(hookKeyCode);
            }
        }
        return;
    }

    switch (keyCode) {
    case KEY_ESCAPE:
        *rcPtr = 0;
        break;
    case KEY_TAB:
        switchTab((currentTabIndex + 1) % static_cast<int>(tabs.size()));
        break;
    case KEY_ARROW_LEFT:
        switchTab((currentTabIndex + static_cast<int>(tabs.size()) - 1) % static_cast<int>(tabs.size()));
        break;
    case KEY_ARROW_RIGHT:
        switchTab((currentTabIndex + 1) % static_cast<int>(tabs.size()));
        break;
    case KEY_ARROW_UP:
        moveSelection(-1);
        break;
    case KEY_ARROW_DOWN:
        moveSelection(1);
        break;
    case KEY_RETURN:
    case KEY_SPACE:
        activateCurrentRow();
        break;
    case kScrollUpButtonEventCode:
        scrollRows(-1, false);
        break;
    case kScrollDownButtonEventCode:
        scrollRows(1, false);
        break;
    case kPrintButtonEventCode:
        applyPreset(PrototypePreset::Vanilla);
        break;
    case kDoneButtonEventCode:
        applyPreset(PrototypePreset::Default);
        break;
    case KEY_UPPERCASE_D:
    case KEY_LOWERCASE_D:
        *rcPtr = 0;
        break;
    case kCancelButtonEventCode:
        *rcPtr = 1;
        break;
    default:
        break;
    }
}

void PrototypeOptionsMenu::handleTabClick(int mouseX, int mouseY)
{
    const FrmImage& tabImage = tabImages[currentTabIndex];
    if (mouseX < kTabX || mouseX >= kTabX + tabImage.getWidth() || mouseY < kTabY || mouseY >= kTabY + tabImage.getHeight()) {
        return;
    }

    int tabWidth = tabImage.getWidth() / 3;
    int tabIndex = std::clamp((mouseX - kTabX) / tabWidth, 0, 2);
    switchTab(tabIndex);
}

void PrototypeOptionsMenu::handleRowClick(int mouseX, int mouseY)
{
    if (mouseX < kLeftPaneX || mouseX >= kLeftPaneX + kLeftPaneWidth) {
        return;
    }

    for (const VisibleRow& visibleRow : visibleRows) {
        if (mouseY >= visibleRow.y && mouseY < visibleRow.y + visibleRow.height) {
            PrototypeOptionsTab& tab = tabs[currentTabIndex];
            PrototypeOptionRow& row = tab.rows[visibleRow.rowIndex];
            if (row.type == PrototypeOptionType::Section) {
                return;
            }

            if (tab.selectedRow == visibleRow.rowIndex) {
                activateCurrentRow();
            } else {
                tab.selectedRow = visibleRow.rowIndex;
                render();
            }
            return;
        }
    }
}

void PrototypeOptionsMenu::switchTab(int tabIndex)
{
    if (tabIndex == currentTabIndex) {
        return;
    }

    currentTabIndex = tabIndex;
    ensureSelectedRowVisible();
    render();
}

void PrototypeOptionsMenu::moveSelection(int delta)
{
    PrototypeOptionsTab& tab = tabs[currentTabIndex];
    int nextRow = findNextSelectableRow(tab, tab.selectedRow + delta, delta);
    if (nextRow == -1) {
        return;
    }

    tab.selectedRow = nextRow;
    ensureSelectedRowVisible();
    render();
}

void PrototypeOptionsMenu::scrollRows(int delta, bool followSelection)
{
    PrototypeOptionsTab& tab = tabs[currentTabIndex];
    if (delta < 0) {
        tab.scrollOffset = std::max(0, tab.scrollOffset + delta);
    } else {
        while (delta > 0 && canScrollDown()) {
            tab.scrollOffset++;
            delta--;
        }
    }

    if (followSelection) {
        ensureSelectedRowVisible();
    } else {
        updateSelectionAfterScroll();
    }

    render();
}

void PrototypeOptionsMenu::ensureSelectedRowVisible()
{
    PrototypeOptionsTab& tab = tabs[currentTabIndex];
    if (tab.selectedRow < tab.scrollOffset) {
        tab.scrollOffset = tab.selectedRow;
    }

    while (!isRowVisible(tab.selectedRow) && canScrollDown()) {
        tab.scrollOffset++;
    }

    normalizeScrollOffset();
}

void PrototypeOptionsMenu::normalizeScrollOffset()
{
    PrototypeOptionsTab& tab = tabs[currentTabIndex];
    tab.scrollOffset = std::clamp(tab.scrollOffset, 0, std::max(0, static_cast<int>(tab.rows.size()) - 1));
}

bool PrototypeOptionsMenu::canScrollDown() const
{
    const PrototypeOptionsTab& tab = tabs[currentTabIndex];
    int y = kLeftPaneContentY;
    int rowIndex = tab.scrollOffset;
    for (; rowIndex < static_cast<int>(tab.rows.size()); rowIndex++) {
        int height = rowHeight(tab.rows[rowIndex]);
        if (y + height > kLeftPaneContentBottom) {
            break;
        }
        y += height;
    }
    return rowIndex < static_cast<int>(tab.rows.size());
}

bool PrototypeOptionsMenu::isRowVisible(int rowIndex) const
{
    const PrototypeOptionsTab& tab = tabs[currentTabIndex];
    int y = kLeftPaneContentY;
    for (int index = tab.scrollOffset; index < static_cast<int>(tab.rows.size()); index++) {
        int height = rowHeight(tab.rows[index]);
        if (y + height > kLeftPaneContentBottom) {
            return false;
        }
        if (index == rowIndex) {
            return true;
        }
        y += height;
    }
    return false;
}

int PrototypeOptionsMenu::findFirstSelectableRow(const PrototypeOptionsTab& tab) const
{
    return findNextSelectableRow(tab, 0, 1);
}

int PrototypeOptionsMenu::findNextSelectableRow(const PrototypeOptionsTab& tab, int startRow, int delta) const
{
    for (int rowIndex = startRow; rowIndex >= 0 && rowIndex < static_cast<int>(tab.rows.size()); rowIndex += delta) {
        if (tab.rows[rowIndex].type != PrototypeOptionType::Section) {
            return rowIndex;
        }
    }
    return -1;
}

int PrototypeOptionsMenu::rowHeight(const PrototypeOptionRow& row) const
{
    return row.type == PrototypeOptionType::Section ? 18 : 24;
}

PrototypeOptionRow* PrototypeOptionsMenu::currentRow()
{
    PrototypeOptionsTab& tab = tabs[currentTabIndex];
    if (tab.selectedRow < 0 || tab.selectedRow >= static_cast<int>(tab.rows.size())) {
        return nullptr;
    }
    return &(tab.rows[tab.selectedRow]);
}

const PrototypeOptionRow* PrototypeOptionsMenu::currentRow() const
{
    const PrototypeOptionsTab& tab = tabs[currentTabIndex];
    if (tab.selectedRow < 0 || tab.selectedRow >= static_cast<int>(tab.rows.size())) {
        return nullptr;
    }
    return &(tab.rows[tab.selectedRow]);
}

void PrototypeOptionsMenu::activateCurrentRow()
{
    PrototypeOptionRow* row = currentRow();
    if (row == nullptr) {
        return;
    }

    switch (row->type) {
    case PrototypeOptionType::Boolean:
        row->boolValue = !row->boolValue;
        soundPlayFile("ib1p1xx1");
        render();
        break;
    case PrototypeOptionType::MultiSelect:
        row->selectedChoice = (row->selectedChoice + 1) % row->choiceCount;
        soundPlayFile("ib1p1xx1");
        render();
        break;
    case PrototypeOptionType::Text:
        for (const VisibleRow& visibleRow : visibleRows) {
            if (visibleRow.rowIndex == tabs[currentTabIndex].selectedRow) {
                editTextRow(row, visibleRow.y);
                break;
            }
        }
        break;
    case PrototypeOptionType::Control:
        beginControlCapture();
        break;
    case PrototypeOptionType::Section:
        break;
    }
}

void PrototypeOptionsMenu::updateSelectionAfterScroll()
{
    PrototypeOptionsTab& tab = tabs[currentTabIndex];
    if (isRowVisible(tab.selectedRow)) {
        return;
    }

    for (int rowIndex = tab.scrollOffset; rowIndex < static_cast<int>(tab.rows.size()); rowIndex++) {
        if (tab.rows[rowIndex].type != PrototypeOptionType::Section) {
            tab.selectedRow = rowIndex;
            return;
        }
    }
}

void PrototypeOptionsMenu::editTextRow(PrototypeOptionRow* row, int rowY)
{
    char buffer[32];
    std::strncpy(buffer, row->textValue.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);
    if (_win_input_str(window, buffer, 20, kLeftPaneValueX, rowY, _colorTable[kHeadingColorIndex], _colorTable[0]) == 0) {
        row->textValue = buffer;
        soundPlayFile("ib1p1xx1");
    }
    fontSetCurrent(oldFont);

    render();
}

void PrototypeOptionsMenu::beginControlCapture()
{
    PrototypeOptionRow* row = currentRow();
    if (row == nullptr || row->type != PrototypeOptionType::Control) {
        return;
    }

    isCapturingControl = true;
    capturingControlRowIndex = tabs[currentTabIndex].selectedRow;
    statusTick = 0;
    statusText = "Press a key to bind this control. Esc cancels.";
    soundPlayFile("ib1p1xx1");
    render();
}

void PrototypeOptionsMenu::completeControlCapture(int keyCode)
{
    PrototypeOptionsTab& tab = tabs[currentTabIndex];
    if (capturingControlRowIndex < 0 || capturingControlRowIndex >= static_cast<int>(tab.rows.size())) {
        cancelControlCapture();
        return;
    }

    PrototypeOptionRow& row = tab.rows[capturingControlRowIndex];
    row.controlKeyCode = keyCode;

    isCapturingControl = false;
    capturingControlRowIndex = -1;
    statusTick = getTicks();
    statusText = "Bound to HOOK_KEYPRESS code " + formatControlValue(keyCode) + ".";
    soundPlayFile("ib1p1xx1");
    render();
}

void PrototypeOptionsMenu::cancelControlCapture()
{
    isCapturingControl = false;
    capturingControlRowIndex = -1;
    statusTick = getTicks();
    statusText = "Control binding unchanged.";
    render();
}

void PrototypeOptionsMenu::applyPreset(PrototypePreset preset)
{
    for (auto& tab : tabs) {
        for (auto& row : tab.rows) {
            applyPresetToRow(row, preset);
        }
    }

    statusTick = getTicks();
    statusText = preset == PrototypePreset::Vanilla
        ? "Applied vanilla values to all options."
        : "Applied default values to all options.";
    soundPlayFile("ib1p1xx1");
    render();
}

void PrototypeOptionsMenu::applyPresetToRow(PrototypeOptionRow& row, PrototypePreset preset)
{
    if (row.type == PrototypeOptionType::Section) {
        return;
    }

    const PrototypeOptionValue& source = preset == PrototypePreset::Vanilla
        ? row.vanillaValue
        : row.defaultValue;

    switch (row.type) {
    case PrototypeOptionType::Boolean:
        row.boolValue = source.boolValue;
        break;
    case PrototypeOptionType::MultiSelect:
        row.selectedChoice = std::clamp(source.selectedChoice, 0, std::max(0, row.choiceCount - 1));
        break;
    case PrototypeOptionType::Text:
        row.textValue = source.textValue;
        break;
    case PrototypeOptionType::Control:
        row.controlKeyCode = source.controlKeyCode;
        break;
    case PrototypeOptionType::Section:
        break;
    }
}

std::string PrototypeOptionsMenu::formatControlValue(int keyCode) const
{
    int sdlScanCode = sfall_kb_get_scancode_from_key(keyCode);
    if (sdlScanCode > 0) {
        const char* scancodeName = SDL_GetScancodeName(static_cast<SDL_Scancode>(sdlScanCode));
        if (scancodeName != nullptr && scancodeName[0] != '\0') {
            return std::string(scancodeName) + " [" + std::to_string(keyCode) + "]";
        }
    }

    return "[" + std::to_string(keyCode) + "]";
}

const char* PrototypeOptionsMenu::describeControlType(const PrototypeOptionRow& row) const
{
    switch (row.type) {
    case PrototypeOptionType::Boolean:
        return "Checkbox";
    case PrototypeOptionType::MultiSelect:
        return "Dropdown";
    case PrototypeOptionType::Text:
        return "Text field";
    case PrototypeOptionType::Control:
        return "Control bind";
    case PrototypeOptionType::Section:
        break;
    }

    return "";
}

bool PrototypeOptionsMenu::isCapturableKeyCode(int keyCode) const
{
    if (keyCode < 0) {
        return false;
    }

    return keyCode < kScrollUpButtonEventCode || keyCode > kCancelButtonEventCode;
}

void PrototypeOptionsMenu::renderWrappedText(int x, int y, int width, const char* text, int color)
{
    char description[512];
    std::strncpy(description, text, sizeof(description) - 1);
    description[sizeof(description) - 1] = '\0';

    short breakpoints[WORD_WRAP_MAX_COUNT];
    short breakpointsLength = 0;
    if (wordWrap(description, width, breakpoints, &breakpointsLength) != 0) {
        return;
    }

    for (short index = 0; index < breakpointsLength - 1; index++) {
        char saved = description[breakpoints[index + 1]];
        description[breakpoints[index + 1]] = '\0';
        fontDrawText(windowBuffer + kWindowWidth * y + x, description + breakpoints[index], width, kWindowWidth, color);
        description[breakpoints[index + 1]] = saved;
        y += fontGetLineHeight();
    }
}

} // namespace

int showPrototypeOptionsMenu(bool animated)
{
    PrototypeOptionsMenu menu;
    return menu.run(animated);
}

} // namespace fallout
