#include "quick_toolbar.h"

#if defined(__APPLE__) && TARGET_OS_IOS

#include <string.h>

#include "../../art.h"
#include "../../color.h"
#include "../../draw.h"
#include "../../game.h"
#include "../../interface.h"
#include "../../skilldex.h"
#include "../../svga.h"
#include "../../text_font.h"
#include "../../window_manager.h"

namespace fallout {

namespace {

    constexpr int kSkillButtonCount = 8;
    constexpr int kButtonWidth = 36;
    constexpr int kButtonHeight = 24;
    constexpr int kToolbarBottomMargin = 10;
    constexpr int kToolbarWidth = kSkillButtonCount * kButtonWidth;
    // Window is exactly the button row — no outer padding rows, so there are no
    // pixels outside the buttons that could bleed as window background.
    constexpr int kToolbarHeight = kButtonHeight;

    struct SkillEntry {
        int skilldexRc;
        const char* label;
    };

    constexpr SkillEntry kSkills[kSkillButtonCount] = {
        { SKILLDEX_RC_SNEAK, "SNK" },
        { SKILLDEX_RC_LOCKPICK, "LCK" },
        { SKILLDEX_RC_STEAL, "STL" },
        { SKILLDEX_RC_TRAPS, "TRP" },
        { SKILLDEX_RC_FIRST_AID, "F/A" },
        { SKILLDEX_RC_DOCTOR, "DOC" },
        { SKILLDEX_RC_SCIENCE, "SCI" },
        { SKILLDEX_RC_REPAIR, "RPR" },
    };

    int gToolbarWindow = -1;
    int gToolbarX = 0;
    int gToolbarY = 0;
    bool gShown = false;
    bool gEnabled = true;

    void fillRect(unsigned char* buffer, int pitch, int x, int y, int w, int h, unsigned char color)
    {
        for (int row = 0; row < h; row++) {
            memset(buffer + (y + row) * pitch + x, color, static_cast<size_t>(w));
        }
    }

    void drawCenteredLabel(unsigned char* buffer, int pitch, int x, int y, int w, int h, const char* text, unsigned char color)
    {
        int textWidth = fontGetStringWidth(text);
        int lineHeight = fontGetLineHeight();
        int tx = x + (w - textWidth) / 2;
        // Font metrics report the full line box, but glyphs sit high within it —
        // +2 nudges the optical center down to match the panel's visual middle.
        int ty = y + (h - lineHeight) / 2 + 2;
        if (tx < x) tx = x;
        if (ty < y) ty = y;
        // Clip text to the button's right edge so a label wider than the panel
        // (e.g. localized or longer-named skill) can't overdraw adjacent buttons.
        int maxDrawWidth = x + w - tx;
        if (maxDrawWidth <= 0) {
            return;
        }
        fontDrawText(buffer + ty * pitch + tx, text, maxDrawWidth, pitch, color);
    }

    // Muted panel tuned to sit inside the same tonal range as the belt: very dim
    // fill, thin soft border, no sharp highlight/shadow. Label uses the dimmed
    // yellow of the belt's HUD text so it doesn't compete with the interface.
    void paintPanelButton(unsigned char* buffer, int pitch, int x, int y, int w, int h, const char* label)
    {
        unsigned char panel = intensityColorTable[_colorTable[32767]][8];
        unsigned char border = intensityColorTable[_colorTable[32767]][28];

        fillRect(buffer, pitch, x, y, w, h, panel);
        fillRect(buffer, pitch, x, y, w, 1, border);
        fillRect(buffer, pitch, x, y + h - 1, w, 1, border);
        fillRect(buffer, pitch, x, y, 1, h, border);
        fillRect(buffer, pitch, x + w - 1, y, 1, h, border);

        drawCenteredLabel(buffer, pitch, x, y, w, h, label, intensityColorTable[_colorTable[32747]][48]);
    }

    void paintAll()
    {
        unsigned char* buffer = windowGetBuffer(gToolbarWindow);
        if (buffer == nullptr) {
            return;
        }

        fillRect(buffer, kToolbarWidth, 0, 0, kToolbarWidth, kToolbarHeight, _colorTable[0]);

        int oldFont = fontGetCurrent();
        fontSetCurrent(101);

        int buttonY = (kToolbarHeight - kButtonHeight) / 2;
        for (int i = 0; i < kSkillButtonCount; i++) {
            paintPanelButton(buffer, kToolbarWidth, i * kButtonWidth, buttonY, kButtonWidth, kButtonHeight, kSkills[i].label);
        }

        fontSetCurrent(oldFont);
    }

    // WINDOW_TRANSPARENT makes palette-0 (black) pixels composite away, so the
    // empty space around and between buttons is see-through. The button panels
    // themselves use a non-black dim gray so they still render as raised tiles.
    void createWindow()
    {
        gToolbarX = (screenGetWidth() - kToolbarWidth) / 2;
        gToolbarY = screenGetHeight() - INTERFACE_BAR_HEIGHT - kToolbarHeight - kToolbarBottomMargin;

        gToolbarWindow = windowCreate(gToolbarX, gToolbarY, kToolbarWidth, kToolbarHeight, _colorTable[0], WINDOW_HIDDEN | WINDOW_TRANSPARENT);
        if (gToolbarWindow == -1) {
            return;
        }

        paintAll();
    }

    void destroyWindow()
    {
        if (gToolbarWindow == -1) {
            return;
        }
        windowDestroy(gToolbarWindow);
        gToolbarWindow = -1;
    }

} // namespace

void quickToolbarInit()
{
    if (gToolbarWindow != -1) {
        return;
    }
    createWindow();
}

void quickToolbarFree()
{
    destroyWindow();
    gShown = false;
}

void quickToolbarShow()
{
    if (gToolbarWindow == -1 || gShown || !gEnabled) {
        return;
    }
    windowShow(gToolbarWindow);
    gShown = true;
}

void quickToolbarSetEnabled(bool enabled)
{
    gEnabled = enabled;
    if (!enabled && gShown) {
        quickToolbarHide();
    }
}

void quickToolbarHide()
{
    if (gToolbarWindow == -1 || !gShown) {
        return;
    }
    windowHide(gToolbarWindow);
    gShown = false;
}

bool quickToolbarIsWindow(int windowId)
{
    return gToolbarWindow != -1 && windowId == gToolbarWindow;
}

bool quickToolbarContainsPoint(int x, int y)
{
    if (gToolbarWindow == -1 || !gShown) {
        return false;
    }
    return x >= gToolbarX && x < gToolbarX + kToolbarWidth
        && y >= gToolbarY && y < gToolbarY + kToolbarHeight;
}

bool quickToolbarHandleTap(int x, int y)
{
    if (!quickToolbarContainsPoint(x, y)) {
        return false;
    }

    // Prevent skill activation when the interface bar is disabled (e.g., during cutscenes).
    if (!interfaceBarEnabled() || gameUiIsDisabled()) {
        return true;
    }

    int localX = x - gToolbarX;
    int index = localX / kButtonWidth;
    if (index < 0 || index >= kSkillButtonCount) {
        return true;
    }

    gameHandleSkilldexResult(kSkills[index].skilldexRc);

    return true;
}

} // namespace fallout

#endif // defined(__APPLE__) && TARGET_OS_IOS
