#include "mapper/map_edge_setup.h"

#include <algorithm>
#include <vector>

#include "color.h"
#include "draw.h"
#include "geometry.h"
#include "input.h"
#include "kb.h"
#include "map.h"
#include "map_defs.h"
#include "map_edge.h"
#include "mouse.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

// Palette indices for the iso overlay (15-bit RGB lookups into _colorTable).
constexpr int kActiveRectColor = 30720; // red
constexpr int kMoveSideColor = 783; // green

// A side of the active edge rect that can be moved.
enum class EdgeSide {
    None,
    Left,
    Top,
    Right,
    Bottom,
};

static bool active = false; // dialog open → draw the overlay
static bool showAllRects = false; // persistent overlay toggle (Settings menu)
static int workingElevation = 0;
static int activeRect = -1;
static EdgeSide moveSide = EdgeSide::None;
static int moveOrigValue = 0; // side value to restore on Cancel move

// Snapshot of all edge data taken when the dialog opens, restored on Cancel.
static EdgeElevationData backup[ELEVATION_COUNT];

static std::vector<EdgeZone>& currentZones()
{
    return mapEdgeGetElevationData(workingElevation).zones;
}

// Computes the tileRect (corners as tile indices) that covers the whole hex grid.
static Rect fullGridTileRect()
{
    const int corners[4] = {
        0,
        HEX_GRID_WIDTH - 1,
        (HEX_GRID_HEIGHT - 1) * HEX_GRID_WIDTH,
        HEX_GRID_SIZE - 1,
    };

    Rect result { };
    int maxPx = 0;
    int minPx = 0;
    int minPy = 0;
    int maxPy = 0;
    for (int i = 0; i < 4; i++) {
        int px;
        int py;
        tileToPixelOffset(corners[i], px, py);
        if (i == 0 || px > maxPx) {
            maxPx = px;
            result.left = corners[i]; // X inverted: left has the larger pixel X
        }
        if (i == 0 || px < minPx) {
            minPx = px;
            result.right = corners[i];
        }
        if (i == 0 || py < minPy) {
            minPy = py;
            result.top = corners[i];
        }
        if (i == 0 || py > maxPy) {
            maxPy = py;
            result.bottom = corners[i];
        }
    }
    return result;
}

static void editorOpen(int elevation)
{
    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        backup[elev] = mapEdgeGetElevationData(elev);
    }

    workingElevation = elevation;
    active = true;
    moveSide = EdgeSide::None;
    activeRect = currentZones().empty() ? -1 : 0;
}

static void editorClose(bool save)
{
    if (!save) {
        for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
            mapEdgeGetElevationData(elev) = backup[elev];
        }
    }
    active = false;
    moveSide = EdgeSide::None;
}

static void editorAddRect()
{
    currentZones().push_back(EdgeZone { fullGridTileRect() });
    activeRect = static_cast<int>(currentZones().size()) - 1;
}

static void editorDeleteRect()
{
    std::vector<EdgeZone>& zones = currentZones();
    if (activeRect < 0 || activeRect >= static_cast<int>(zones.size())) {
        return;
    }

    zones.erase(zones.begin() + activeRect);
    if (zones.empty()) {
        activeRect = -1;
    } else if (activeRect > 0) {
        activeRect--; // previous one
    }
    // else (was first): keep index 0, now pointing at the next one.
}

static void editorNextRect()
{
    int count = static_cast<int>(currentZones().size());
    if (count > 0 && activeRect < count - 1) {
        activeRect++;
    }
}

static void editorPrevRect()
{
    if (activeRect > 0) {
        activeRect--;
    }
}

// Returns a writable pointer to the active rect's side value, or nullptr.
static int* activeSideValue(EdgeSide side)
{
    std::vector<EdgeZone>& zones = currentZones();
    if (activeRect < 0 || activeRect >= static_cast<int>(zones.size())) {
        return nullptr;
    }
    Rect& r = zones[activeRect].tileRect;
    switch (side) {
    case EdgeSide::Left:
        return &r.left;
    case EdgeSide::Top:
        return &r.top;
    case EdgeSide::Right:
        return &r.right;
    case EdgeSide::Bottom:
        return &r.bottom;
    default:
        return nullptr;
    }
}

static void editorBeginMove(EdgeSide side)
{
    int* value = activeSideValue(side);
    if (value == nullptr) {
        return;
    }
    moveSide = side;
    moveOrigValue = *value;
}

static void editorCommitMove()
{
    moveSide = EdgeSide::None;
}

static void editorCancelMove()
{
    int* value = activeSideValue(moveSide);
    if (value != nullptr) {
        *value = moveOrigValue;
    }
    moveSide = EdgeSide::None;
}

// Moves the selected side to match a screen position over the iso grid (live preview).
// Returns true if the value changed and the views should refresh.
static bool editorUpdateMoveFromScreen(int screenX, int screenY)
{
    int* value = activeSideValue(moveSide);
    if (value == nullptr) {
        return false;
    }

    int tile = tileFromScreenXY(screenX, screenY, true);
    if (!hexGridTileIsValid(tile) || tile == *value) {
        return false;
    }

    *value = tile;
    return true;
}

// Clips an axis-aligned line to clip and draws it; skips lines fully outside.
static void drawClippedHLine(unsigned char* buffer, int pitch, const Rect* clip, int y, int x0, int x1, int color)
{
    if (y < clip->top || y > clip->bottom) return;
    int left = std::max(std::min(x0, x1), clip->left);
    int right = std::min(std::max(x0, x1), clip->right);
    if (left > right) return;
    bufferDrawLine(buffer, pitch, left, y, right, y, color);
}

static void drawClippedVLine(unsigned char* buffer, int pitch, const Rect* clip, int x, int y0, int y1, int color)
{
    if (x < clip->left || x > clip->right) return;
    int top = std::max(std::min(y0, y1), clip->top);
    int bottom = std::min(std::max(y0, y1), clip->bottom);
    if (top > bottom) return;
    bufferDrawLine(buffer, pitch, x, top, x, bottom, color);
}

// Screen-space rect (normalized left<=right, top<=bottom) of a zone's tile corners.
static Rect tileRectToScreenRect(const Rect& tileRect)
{
    int lx, ly, rx, ry, tx, ty, bx, by;
    tileToScreenXY(tileRect.left, &lx, &ly);
    tileToScreenXY(tileRect.right, &rx, &ry);
    tileToScreenXY(tileRect.top, &tx, &ty);
    tileToScreenXY(tileRect.bottom, &bx, &by);
    return Rect { std::min(lx, rx), std::min(ty, by), std::max(lx, rx), std::max(ty, by) };
}

static void drawScreenRectOutline(unsigned char* buffer, int pitch, const Rect* clip, const Rect& s, int color)
{
    drawClippedHLine(buffer, pitch, clip, s.top, s.left, s.right, color);
    drawClippedHLine(buffer, pitch, clip, s.bottom, s.left, s.right, color);
    drawClippedVLine(buffer, pitch, clip, s.left, s.top, s.bottom, color);
    drawClippedVLine(buffer, pitch, clip, s.right, s.top, s.bottom, color);
}

// Registered as the tile renderer's overlay hook. While the dialog is open, only the active
// rect is drawn (plus the moving side in green). Otherwise the persistent toggle outlines
// every rect. Never drawn in play mode (edges enabled).
static void renderOverlay(unsigned char* buffer, int pitch, int elevation, const Rect* clip)
{
    if (mapEdgeIsEnabled()) {
        return;
    }

    const int red = _colorTable[kActiveRectColor];

    if (!active) {
        if (showAllRects) {
            for (const EdgeZone& zone : mapEdgeGetElevationData(elevation).zones) {
                drawScreenRectOutline(buffer, pitch, clip, tileRectToScreenRect(zone.tileRect), red);
            }
        }
        return;
    }

    if (elevation != workingElevation) {
        return;
    }

    std::vector<EdgeZone>& zones = currentZones();
    if (activeRect < 0 || activeRect >= static_cast<int>(zones.size())) {
        return;
    }

    const Rect s = tileRectToScreenRect(zones[activeRect].tileRect);
    drawScreenRectOutline(buffer, pitch, clip, s, red);

    if (moveSide != EdgeSide::None) {
        const int green = _colorTable[kMoveSideColor];
        switch (moveSide) {
        case EdgeSide::Left:
            drawClippedVLine(buffer, pitch, clip, s.left, s.top, s.bottom, green);
            break;
        case EdgeSide::Right:
            drawClippedVLine(buffer, pitch, clip, s.right, s.top, s.bottom, green);
            break;
        case EdgeSide::Top:
            drawClippedHLine(buffer, pitch, clip, s.top, s.left, s.right, green);
            break;
        case EdgeSide::Bottom:
            drawClippedHLine(buffer, pitch, clip, s.bottom, s.left, s.right, green);
            break;
        default:
            break;
        }
    }
}

void mapEdgeSetupInit()
{
    tileSetMapperOverlayProc(renderOverlay);
}

void mapEdgeSetupToggleOverlay()
{
    showAllRects = !showAllRects;
    tileWindowRefresh();
}

void mapEdgeSetupDialog()
{
    constexpr int kWinWidth = 230;
    constexpr int kWinHeight = 250;

    // Button event codes (kept clear of real key/mouse codes).
    constexpr int kEvAddRect = 0x3201;
    constexpr int kEvDelRect = 0x3202;
    constexpr int kEvPrevRect = 0x3203;
    constexpr int kEvNextRect = 0x3204;
    constexpr int kEvSideTop = 0x3205;
    constexpr int kEvSideBottom = 0x3206;
    constexpr int kEvSideLeft = 0x3207;
    constexpr int kEvSideRight = 0x3208;
    constexpr int kEvCancel = 0x3209;
    constexpr int kEvSave = 0x320A;

    // Reference diagram box; sides are highlighted green while being moved.
    constexpr int kBoxLeft = 60;
    constexpr int kBoxTop = 128;
    constexpr int kBoxRight = 170;
    constexpr int kBoxBottom = 186;

    const int winBgColor = _colorTable[3082]; // dark blue
    const int textColor = _colorTable[16344]; // lighter gray
    // const int buttonTextColor = _colorTable[20052] | FONT_SHADOW | FONT_MONO; // grey
    const int titleColor = _colorTable[32767]; // white
    const int rectColor = _colorTable[kActiveRectColor]; // red
    const int highlightColor = _colorTable[kMoveSideColor]; // green

    const int winX = (rectGetWidth(&_scr_size) - kWinWidth) / 2;
    const int winY = (rectGetHeight(&_scr_size) - kWinHeight) / 2;

    int win = windowCreate(winX, winY, kWinWidth, kWinHeight, winBgColor, WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return;
    }

    unsigned char* buf = windowGetBuffer(win);
    const int pitch = windowGetWidth(win);
    const int lineHeight = fontGetLineHeight();

    editorOpen(gElevation);

    auto centeredX = [&](const char* text) { return (kWinWidth - fontGetStringWidth(text)) / 2; };

    ScopedFont font(1);

    // Static chrome and labels drawn once; buttons are themed and registered once.
    windowDrawBorder(win);
    windowDrawText(win, "MAP EDGE SETUP", 0, centeredX("MAP EDGE SETUP"), 8, titleColor);

    windowDrawText(win, gMapHeader.name[0] != '\0' ? gMapHeader.name : "<unnamed>", 130, 12, 26, textColor);

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "Elev: %d", workingElevation);
    windowDrawText(win, tmp, 0, 168, 26, textColor);

    _win_register_text_button(win, 12, 58, -1, -1, -1, kEvAddRect, "Add Rect", 0);
    _win_register_text_button(win, 120, 58, -1, -1, -1, kEvDelRect, "Delete Rect", 0);
    _win_register_text_button(win, 12, 86, -1, -1, -1, kEvPrevRect, "<<", 0);
    _win_register_text_button(win, 180, 86, -1, -1, -1, kEvNextRect, ">>", 0);
    _win_register_text_button(win, 96, 108, -1, -1, -1, kEvSideTop, "Top", 0);
    _win_register_text_button(win, 90, 190, -1, -1, -1, kEvSideBottom, "Bottom", 0);
    _win_register_text_button(win, 12, 148, -1, -1, -1, kEvSideLeft, "Left", 0);
    _win_register_text_button(win, 174, 148, -1, -1, -1, kEvSideRight, "Right", 0);
    _win_register_text_button(win, 12, 222, -1, -1, -1, kEvCancel, "Cancel", 0);
    _win_register_text_button(win, 160, 222, -1, -1, -1, kEvSave, "Save", 0);

    // Repaints only the parts that change with editor state, then blits the window.
    auto render = [&]() {
        char buffer[64];

        snprintf(buffer, sizeof(buffer), "Num Rects: %d", static_cast<int>(currentZones().size()));
        bufferFill(buf + 40 * pitch + 10, kWinWidth - 20, lineHeight, pitch, winBgColor);
        windowDrawText(win, buffer, 0, centeredX(buffer), 40, textColor);

        snprintf(buffer, sizeof(buffer), "Rect: %d", activeRect >= 0 ? activeRect + 1 : 0);
        bufferFill(buf + 90 * pitch + 50, kWinWidth - 100, lineHeight, pitch, winBgColor);
        windowDrawText(win, buffer, 0, centeredX(buffer), 90, textColor);

        // Reference diagram: red rect, with the side under edit drawn green.
        bufferFill(buf + (kBoxTop - 1) * pitch + (kBoxLeft - 1), kBoxRight - kBoxLeft + 3, kBoxBottom - kBoxTop + 3, pitch, winBgColor);
        bufferDrawRect(buf, pitch, kBoxLeft, kBoxTop, kBoxRight, kBoxBottom, rectColor);
        switch (moveSide) {
        case EdgeSide::Top:
            bufferDrawLine(buf, pitch, kBoxLeft, kBoxTop, kBoxRight, kBoxTop, highlightColor);
            break;
        case EdgeSide::Bottom:
            bufferDrawLine(buf, pitch, kBoxLeft, kBoxBottom, kBoxRight, kBoxBottom, highlightColor);
            break;
        case EdgeSide::Left:
            bufferDrawLine(buf, pitch, kBoxLeft, kBoxTop, kBoxLeft, kBoxBottom, highlightColor);
            break;
        case EdgeSide::Right:
            bufferDrawLine(buf, pitch, kBoxRight, kBoxTop, kBoxRight, kBoxBottom, highlightColor);
            break;
        default:
            break;
        }

        windowRefresh(win);
    };

    // Repaint both the iso overlay and the dialog after a state change.
    auto refreshAll = [&]() {
        tileWindowRefresh();
        render();
    };

    refreshAll(); // initial draw of dialog + iso overlay

    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();
        int keyCode = inputGetInput();

        int mx, my;
        mouseGetPosition(&mx, &my);
        bool overDialog = mx >= winX && mx < winX + kWinWidth && my >= winY && my < winY + kWinHeight;
        bool moving = moveSide != EdgeSide::None;

        // Live-track the selected side under the cursor while over the iso view.
        if (moving && !overDialog) {
            if (editorUpdateMoveFromScreen(mx, my)) {
                tileWindowRefresh();
            }
        }

        if (keyCode == KEY_ESCAPE) {
            if (moving) {
                editorCancelMove();
                refreshAll();
            } else {
                editorClose(false);
                done = true;
            }
        } else if (keyCode == -2) {
            int buttons = mouseGetEvent();
            if (moving && (buttons & MOUSE_EVENT_LEFT_BUTTON_DOWN) && !overDialog) {
                editorCommitMove();
                refreshAll();
            } else if (moving && (buttons & MOUSE_EVENT_RIGHT_BUTTON_DOWN)) {
                editorCancelMove();
                refreshAll();
            }
        } else if (keyCode != -1) {
            bool refresh = true;
            switch (keyCode) {
            case kEvAddRect:
                editorAddRect();
                break;
            case kEvDelRect:
                editorDeleteRect();
                break;
            case kEvPrevRect:
                editorPrevRect();
                break;
            case kEvNextRect:
                editorNextRect();
                break;
            case kEvSideTop:
                editorBeginMove(EdgeSide::Top);
                break;
            case kEvSideBottom:
                editorBeginMove(EdgeSide::Bottom);
                break;
            case kEvSideLeft:
                editorBeginMove(EdgeSide::Left);
                break;
            case kEvSideRight:
                editorBeginMove(EdgeSide::Right);
                break;
            case kEvCancel:
                editorClose(false);
                done = true;
                refresh = false;
                break;
            case kEvSave:
                editorClose(true);
                done = true;
                refresh = false;
                break;
            default:
                refresh = false;
                break;
            }
            if (refresh) {
                refreshAll();
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);
    tileWindowRefresh(); // repaint the iso view without the overlay
}

} // namespace fallout
