#include "mapper/map_edge_setup.h"

#include <algorithm>
#include <functional>
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
constexpr int kClipAllColor = 29386; // square side clips all objects
constexpr int kClipLowColor = 25120; // square side clips low objects only

// A side of the active rect that can be moved.
enum class EdgeSide {
    None,
    Left,
    Top,
    Right,
    Bottom,
};

// Which rect the open dialog edits: per-zone tileRect or the per-elevation squareRect.
enum class EditMode {
    Tiles,
    Squares,
};

static bool active = false; // dialog open → draw the overlay
static bool showAllRects = false; // persistent overlay toggle (Settings menu)
static EditMode editMode = EditMode::Tiles;
static int workingElevation = 0;
static int activeRect = -1;
static EdgeSide moveSide = EdgeSide::None;
static int moveOrigValue = 0; // side value to restore on Cancel move

// Square-grid basis vectors in screen space (one step along column / row axis).
constexpr int kSquareColDx = -48;
constexpr int kSquareColDy = 12;
constexpr int kSquareRowDx = 32;
constexpr int kSquareRowDy = 24;

// squareTileToScreenXY returns the tile's bounding-box top-left; the cell parallelogram's
// top corner sits this far to the right of it.
constexpr int kSquareOriginDx = -kSquareColDx;

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

    Rect result {};
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

static void editorOpen(int elevation, EditMode mode)
{
    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        backup[elev] = mapEdgeGetElevationData(elev);
    }

    editMode = mode;
    workingElevation = elevation;
    active = true;
    moveSide = EdgeSide::None;
    activeRect = currentZones().empty() ? -1 : 0;
}

static void editorClose(bool save)
{
    if (save) {
        if (editMode == EditMode::Squares) {
            mapEdgeUpgradeToVersion2();
        }
        mapEdgeRecalc();
    } else {
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

// The rect being edited: the active zone's tileRect, or the elevation's squareRect.
static Rect* activeTargetRect()
{
    if (editMode == EditMode::Squares) {
        return &mapEdgeGetElevationData(workingElevation).squareRect;
    }
    std::vector<EdgeZone>& zones = currentZones();
    if (activeRect < 0 || activeRect >= static_cast<int>(zones.size())) {
        return nullptr;
    }
    return &zones[activeRect].tileRect;
}

// Returns a writable pointer to the active rect's side value, or nullptr.
static int* activeSideValue(EdgeSide side)
{
    Rect* target = activeTargetRect();
    if (target == nullptr) {
        return nullptr;
    }
    Rect& r = *target;
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

// Mutable clip flag for a square side at the working elevation.
static bool* squareClipSideFlag(EdgeSide side)
{
    EdgeZone::ClipSides& clip = mapEdgeGetElevationData(workingElevation).clipSides;
    switch (side) {
    case EdgeSide::Left:
        return &clip.left;
    case EdgeSide::Top:
        return &clip.top;
    case EdgeSide::Right:
        return &clip.right;
    case EdgeSide::Bottom:
        return &clip.bottom;
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

    int newValue;
    if (editMode == EditMode::Squares) {
        int square = squareTileFromScreenXY(screenX, screenY, workingElevation);
        if (square < 0) {
            return false;
        }
        bool horizontal = moveSide == EdgeSide::Left || moveSide == EdgeSide::Right;
        newValue = horizontal ? square % SQUARE_GRID_WIDTH : square / SQUARE_GRID_WIDTH;
    } else {
        newValue = tileFromScreenXY(screenX, screenY, true);
        if (!hexGridTileIsValid(newValue)) {
            return false;
        }
    }

    if (newValue == *value) {
        return false;
    }

    *value = newValue;
    return true;
}

static void editorToggleClip(EdgeSide side)
{
    bool* flag = squareClipSideFlag(side);
    if (flag != nullptr) {
        *flag = !*flag;
    }
}

static void editorResetSquare()
{
    EdgeElevationData& data = mapEdgeGetElevationData(workingElevation);
    data.squareRect = { SQUARE_GRID_WIDTH - 1, 0, 0, SQUARE_GRID_HEIGHT - 1 };
    data.clipSides = {};
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

// Clips an arbitrary line to clip (Liang-Barsky) and draws it; bufferDrawLine does not clip.
static void drawClippedLine(unsigned char* buffer, int pitch, const Rect* clip, int x0, int y0, int x1, int y1, int color)
{
    float t0 = 0.0f;
    float t1 = 1.0f;
    int dx = x1 - x0;
    int dy = y1 - y0;
    const int p[4] = { -dx, dx, -dy, dy };
    const int q[4] = { x0 - clip->left, clip->right - x0, y0 - clip->top, clip->bottom - y0 };

    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) {
            if (q[i] < 0) return; // parallel and outside
            continue;
        }
        float t = static_cast<float>(q[i]) / p[i];
        if (p[i] < 0) {
            if (t > t1) return;
            if (t > t0) t0 = t;
        } else {
            if (t < t0) return;
            if (t < t1) t1 = t;
        }
    }

    // Clamp against float-truncation landing a pixel outside the clip rect (OOB write).
    auto clampX = [&](int x) { return std::min(std::max(x, clip->left), clip->right); };
    auto clampY = [&](int y) { return std::min(std::max(y, clip->top), clip->bottom); };

    bufferDrawLine(buffer, pitch,
        clampX(x0 + static_cast<int>(t0 * dx)), clampY(y0 + static_cast<int>(t0 * dy)),
        clampX(x0 + static_cast<int>(t1 * dx)), clampY(y0 + static_cast<int>(t1 * dy)), color);
}

// Screen positions of the four outline vertices enclosing the visible squares of sr.
// Indices: 0 top, 1 left, 2 bottom, 3 right (each offset to a cell corner by the basis).
static void squareRectScreenCorners(const Rect& sr, int elevation, int outX[4], int outY[4])
{
    auto corner = [&](int col, int row, int dx, int dy, int index) {
        int x;
        int y;
        squareTileToScreenXY(col + SQUARE_GRID_WIDTH * row, &x, &y, elevation);
        outX[index] = x + kSquareOriginDx + dx;
        outY[index] = y + dy;
    };

    corner(sr.right, sr.top, 0, 0, 0);
    corner(sr.left, sr.top, kSquareColDx, kSquareColDy, 1);
    corner(sr.left, sr.bottom, kSquareColDx + kSquareRowDx, kSquareColDy + kSquareRowDy, 2);
    corner(sr.right, sr.bottom, kSquareRowDx, kSquareRowDy, 3);
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

// Display color of a square side: green while moving, else its clip-mode color.
static int squareSideColor(EdgeSide side)
{
    if (side == moveSide) {
        return _colorTable[kMoveSideColor];
    }
    bool* flag = squareClipSideFlag(side);
    return _colorTable[(flag != nullptr && *flag) ? kClipAllColor : kClipLowColor];
}

// Outlines the squareRect over the iso view, one diagonal edge per side.
static void drawSquareOverlay(unsigned char* buffer, int pitch, int elevation, const Rect* clip)
{
    int cx[4];
    int cy[4];
    squareRectScreenCorners(mapEdgeGetElevationData(elevation).squareRect, elevation, cx, cy);

    const EdgeSide sides[4] = { EdgeSide::Top, EdgeSide::Left, EdgeSide::Bottom, EdgeSide::Right };
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        drawClippedLine(buffer, pitch, clip, cx[i], cy[i], cx[j], cy[j], squareSideColor(sides[i]));
    }
}

// Registered as the tile renderer's overlay hook. While a dialog is open, only the active
// rect is drawn (plus the moving side highlighted). Otherwise the persistent toggle outlines
// every edge rect. Never drawn in play mode.
static void renderOverlay(unsigned char* buffer, int pitch, int elevation, const Rect* clip)
{
    if (!mapEdgeIsMapperMode()) {
        return;
    }

    const int red = _colorTable[kActiveRectColor];

    if (!active) {
        if (showAllRects) {
            for (const EdgeZone& zone : mapEdgeGetElevationData(elevation).zones) {
                drawScreenRectOutline(buffer, pitch, clip, tileRectToScreenRect(zone.tileRect), red);
            }
            if (mapEdgeHasSquareRect(elevation)) {
                drawSquareOverlay(buffer, pitch, elevation, clip);
            }
        }
        return;
    }

    if (elevation != workingElevation) {
        return;
    }

    if (editMode == EditMode::Squares) {
        drawSquareOverlay(buffer, pitch, elevation, clip);
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

// Dialog button event codes (kept clear of real key/mouse codes).
constexpr int kEvSideTop = 0x3205;
constexpr int kEvSideBottom = 0x3206;
constexpr int kEvSideLeft = 0x3207;
constexpr int kEvSideRight = 0x3208;
constexpr int kEvCancel = 0x3209;
constexpr int kEvSave = 0x320A;
constexpr int kEvAddRect = 0x3201;
constexpr int kEvDelRect = 0x3202;
constexpr int kEvPrevRect = 0x3203;
constexpr int kEvNextRect = 0x3204;
constexpr int kEvClipTop = 0x3211;
constexpr int kEvClipBottom = 0x3212;
constexpr int kEvClipLeft = 0x3213;
constexpr int kEvClipRight = 0x3214;
constexpr int kEvDefaultReset = 0x3215;

// Shared modal loop. ESC, mouse, side-move and Cancel/Save are common; dispatch handles
// dialog-specific button codes and reports whether the change needs a refresh.
static void runEditLoop(int win, int winX, int winY, int winW, int winH,
    const std::function<void()>& render, const std::function<bool(int)>& dispatch)
{
    auto refreshAll = [&]() {
        tileWindowRefresh();
        render();
    };

    refreshAll();

    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();
        int keyCode = inputGetInput();

        int mx;
        int my;
        mouseGetPosition(&mx, &my);
        bool overDialog = mx >= winX && mx < winX + winW && my >= winY && my < winY + winH;
        bool moving = moveSide != EdgeSide::None;

        if (moving && !overDialog && editorUpdateMoveFromScreen(mx, my)) {
            tileWindowRefresh();
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
                refresh = dispatch(keyCode);
                break;
            }
            if (refresh) {
                refreshAll();
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
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

// Shared dialog palette indices.
constexpr int kWinBgColor = 3082; // dark blue
constexpr int kTextColor = 16344; // lighter gray
constexpr int kTitleColor = 32767; // white

constexpr int kDialogWidth = 230;
constexpr int kDialogHeight = 250;

// Created window plus the handles both dialogs need for rendering.
struct SetupWindow {
    int win;
    int x;
    int y;
    unsigned char* buf;
    int pitch;
};

// Creates the centered setup window and draws its border, title, map name and elevation.
// Requires a font to be active. Returns win == -1 on failure.
static SetupWindow openSetupWindow(const char* title, int elevation)
{
    int x = (rectGetWidth(&_scr_size) - kDialogWidth) / 2;
    int y = (rectGetHeight(&_scr_size) - kDialogHeight) / 2;

    int win = windowCreate(x, y, kDialogWidth, kDialogHeight, _colorTable[kWinBgColor], WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return { -1, 0, 0, nullptr, 0 };
    }

    windowDrawBorder(win);
    windowDrawText(win, title, 0, (kDialogWidth - fontGetStringWidth(title)) / 2, 8, _colorTable[kTitleColor]);
    windowDrawText(win, gMapHeader.name[0] != '\0' ? gMapHeader.name : "<unnamed>", 130, 13, 30, _colorTable[kTextColor]);

    char elev[32];
    snprintf(elev, sizeof(elev), "Elev: %d", elevation);
    windowDrawText(win, elev, 0, 157, 30, _colorTable[kTextColor]);

    return { win, x, y, windowGetBuffer(win), windowGetWidth(win) };
}

void mapEdgeSetupDialog()
{
    // Reference diagram box; sides are highlighted green while being moved.
    constexpr int kBoxLeft = 60;
    constexpr int kBoxTop = 128;
    constexpr int kBoxRight = 170;
    constexpr int kBoxBottom = 186;

    const int winBgColor = _colorTable[kWinBgColor];
    const int textColor = _colorTable[kTextColor];
    const int rectColor = _colorTable[kActiveRectColor]; // red
    const int highlightColor = _colorTable[kMoveSideColor]; // green

    ScopedFont font(1);

    editorOpen(gElevation, EditMode::Tiles);

    SetupWindow w = openSetupWindow("MAP EDGE SETUP", workingElevation);
    if (w.win == -1) {
        return;
    }

    const int win = w.win;
    unsigned char* buf = w.buf;
    const int pitch = w.pitch;
    const int lineHeight = fontGetLineHeight();
    auto centeredX = [&](const char* text) { return (kDialogWidth - fontGetStringWidth(text)) / 2; };

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
        bufferFill(buf + 40 * pitch + 10, kDialogWidth - 20, lineHeight, pitch, winBgColor);
        windowDrawText(win, buffer, 0, centeredX(buffer), 40, textColor);

        snprintf(buffer, sizeof(buffer), "Rect: %d", activeRect >= 0 ? activeRect + 1 : 0);
        bufferFill(buf + 90 * pitch + 50, kDialogWidth - 100, lineHeight, pitch, winBgColor);
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

    runEditLoop(win, w.x, w.y, kDialogWidth, kDialogHeight, render, [](int keyCode) {
        switch (keyCode) {
        case kEvAddRect:
            editorAddRect();
            return true;
        case kEvDelRect:
            editorDeleteRect();
            return true;
        case kEvPrevRect:
            editorPrevRect();
            return true;
        case kEvNextRect:
            editorNextRect();
            return true;
        default:
            return false;
        }
    });

    windowDestroy(win);
    tileWindowRefresh(); // repaint the iso view without the overlay
}

void mapEdgeSquareSetupDialog()
{
    // Fixed diagram parallelogram, skewed like the iso square grid.
    // Vertices: 0 top, 1 left, 2 bottom, 3 right. Edge i..i+1 maps to a rect side.
    constexpr int kDiagX[4] = { 136, 10, 99, 227 };
    constexpr int kDiagY[4] = { 107, 141, 207, 174 };
    const EdgeSide kDiagSides[4] = { EdgeSide::Top, EdgeSide::Left, EdgeSide::Bottom, EdgeSide::Right };

    const int winBgColor = _colorTable[kWinBgColor];
    const int textColor = _colorTable[kTextColor];

    ScopedFont font(1);

    editorOpen(gElevation, EditMode::Squares);

    SetupWindow w = openSetupWindow("MAP ANGLED EDGE SETUP", workingElevation);
    if (w.win == -1) {
        return;
    }

    const int win = w.win;
    unsigned char* buf = w.buf;
    const int pitch = w.pitch;
    const int lineHeight = fontGetLineHeight();
    auto centeredX = [&](const char* text) { return (kDialogWidth - fontGetStringWidth(text)) / 2; };

    windowDrawText(win, "Clip Objects (clip)", 0, 12, 50, textColor);
    windowDrawText(win, "All Objects", 0, 12, 62, _colorTable[kClipAllColor]);
    windowDrawText(win, "Low Objects", 0, 12, 74, _colorTable[kClipLowColor]);

    _win_register_text_button(win, 113, 64, -1, -1, -1, kEvDefaultReset, "Default Reset", 0);

    _win_register_text_button(win, 51, 114, -1, -1, -1, kEvSideTop, "mv", 0);
    _win_register_text_button(win, 88, 137, -1, -1, -1, kEvClipTop, "clp", 0);
    _win_register_text_button(win, 43, 167, -1, -1, -1, kEvSideLeft, "mv", 0);
    _win_register_text_button(win, 78, 156, -1, -1, -1, kEvClipLeft, "clp", 0);
    _win_register_text_button(win, 166, 131, -1, -1, -1, kEvSideRight, "mv", 0);
    _win_register_text_button(win, 125, 143, -1, -1, -1, kEvClipRight, "clp", 0);
    _win_register_text_button(win, 138, 184, -1, -1, -1, kEvSideBottom, "mv", 0);
    _win_register_text_button(win, 114, 161, -1, -1, -1, kEvClipBottom, "clp", 0);

    _win_register_text_button(win, 30, 224, -1, -1, -1, kEvCancel, "Cancel", 0);
    _win_register_text_button(win, 143, 224, -1, -1, -1, kEvSave, "Save", 0);

    auto render = [&]() {
        // Skewed diagram: fixed edges colored by clip mode (green while moving). Repainted
        // on refresh; same pixels overwrite the previous colors, so no clear is needed.
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            bufferDrawLine(buf, pitch, kDiagX[i], kDiagY[i], kDiagX[j], kDiagY[j], squareSideColor(kDiagSides[i]));
        }

        const Rect& sr = mapEdgeGetElevationData(workingElevation).squareRect;
        char status[64];
        snprintf(status, sizeof(status), "col %d-%d  row %d-%d", sr.right, sr.left, sr.top, sr.bottom);
        bufferFill(buf + 91 * pitch + 10, kDialogWidth - 20, lineHeight, pitch, winBgColor);
        windowDrawText(win, status, 0, centeredX(status), 91, textColor);

        windowRefresh(win);
    };

    runEditLoop(win, w.x, w.y, kDialogWidth, kDialogHeight, render, [](int keyCode) {
        switch (keyCode) {
        case kEvClipTop:
            editorToggleClip(EdgeSide::Top);
            return true;
        case kEvClipBottom:
            editorToggleClip(EdgeSide::Bottom);
            return true;
        case kEvClipLeft:
            editorToggleClip(EdgeSide::Left);
            return true;
        case kEvClipRight:
            editorToggleClip(EdgeSide::Right);
            return true;
        case kEvDefaultReset:
            editorResetSquare();
            return true;
        default:
            return false;
        }
    });

    windowDestroy(win);
    tileWindowRefresh(); // repaint the iso view without the overlay
}

} // namespace fallout
