#include "map_edge.h"

#include "db.h"
#include "debug.h"
#include "geometry.h"
#include "map.h"
#include "map_defs.h"
#include "platform_compat.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"

#include <cassert>
#include <memory>

namespace fallout {

// Tile size in pixels
constexpr int kTileWidth = 32;
constexpr int kTileHeight = 24;

static std::unique_ptr<EdgeZone> gEdgeZones[ELEVATION_COUNT];
static bool gEdgeDataLoaded = false;
static bool gEdgeVersion2 = false;
static EdgeZone* gCurrentEdgeZone = nullptr;

// Boundary alignment mods (sfall mapModWidth/Height), set by clamp/check functions.
static int currentTileXAlignment = 0;
static int currentTileYAlignment = 0;
static int maxTileXAlignment = 0;
static int maxTileYAlignment = 0;

static Rect gMapVisibleArea;

// Convert tile index to pixel-offset coordinates.
// Equivalent to sfall ViewMap::GetTileCoordOffset.
void tileToPixelOffset(int tile, int& outX, int& outY)
{
    int x = tile % HEX_GRID_WIDTH;
    int y = (tile / HEX_GRID_WIDTH) + (x / 2);
    y &= ~1; // force even row
    x = (2 * x) + 200 - y;
    outY = kTileHeight / 2 * y;
    outX = kTileWidth / 2 * x;
}

// Convert pixel-offset to tile coord (in-place, like sfall GetCoordFromOffset).
void pixelToTileCoord(int& inOutX, int& inOutY)
{
    int y = inOutY / kTileHeight;
    int x = (inOutX / kTileWidth) + y - 100;
    inOutX = x;
    inOutY = (2 * y) - (x / 2);
}

// Convert pixel-offset to tile number.
static int pixelToTile(int px, int py)
{
    pixelToTileCoord(px, py);
    return px + py * HEX_GRID_WIDTH;
}

static Size getIsoWindowSize()
{
    Rect winRect;
    assert(windowGetRect(gIsoWindow, &winRect) != -1);
    return {rectGetWidth(&winRect), rectGetHeight(&winRect)};
}

// Fill pixel-space fields of a zone from its stored tileRect corners.
// Screen-size dependent — must be re-run if resolution changes.
static void calcEdgeData(EdgeZone* zone)
{
    const auto [winWidth, winHeight] = getIsoWindowSize();
    const int winHalfWidth = winWidth / 2;
    const int winHalfHeight = winHeight / 2;

    // Compute sub-tile alignment sizes (sfall mapWidthModSize / mapHeightModSize).
    maxTileXAlignment = winHalfWidth % kTileWidth;
    maxTileYAlignment = winHalfHeight % kTileHeight;

    // Truncated half sizes for border contraction (matching sfall ViewMap::GetWinMapHalfSize).
    const int winHalfWidthSnapped = winHalfWidth - maxTileXAlignment;
    const int winHalfHeightSnapped = winHalfHeight - maxTileYAlignment;

    // Convert tileRect corners to pixel offsets → scrollBorderRect
    int px, py;

    tileToPixelOffset(zone->tileRect.left, px, py);
    zone->scrollBorderRect.left = px;

    tileToPixelOffset(zone->tileRect.top, px, py);
    zone->scrollBorderRect.top = py;

    tileToPixelOffset(zone->tileRect.right, px, py);
    zone->scrollBorderRect.right = px;

    tileToPixelOffset(zone->tileRect.bottom, px, py);
    zone->scrollBorderRect.bottom = py;

    // rect2 = tileRect in pixel coordinates shifted by to the right-bottom by (winHalfW - 1, winHalfH - 1)
    // Used by EdgeClipping to compute mapVisibleArea (screen-space clipping rect).
    zone->rect2.left = zone->scrollBorderRect.left - winHalfWidth + 1;
    zone->rect2.right = zone->scrollBorderRect.right - winHalfWidth + 1;
    zone->rect2.top = zone->scrollBorderRect.top + winHalfHeight - 1;
    zone->rect2.bottom = zone->scrollBorderRect.bottom + winHalfHeight - 1;

    // Contract scrollBorderRect inward by window half-size (or half its own size snapped to hex grid, if it's smaller than the window).
    // Narrows the scrollable area, with guards below collapsing to a point if it crosses.
    {
        long rectHalfWidth = (zone->scrollBorderRect.left - zone->scrollBorderRect.right) / 2;
        if (rectHalfWidth < winHalfWidthSnapped) {
            long remainder = rectHalfWidth % kTileWidth;
            long rectHalfWidthSnapped = rectHalfWidth - remainder;
            zone->scrollBorderRect.left -= rectHalfWidthSnapped;
            zone->scrollBorderRect.right += rectHalfWidthSnapped + (remainder ? kTileWidth : 0);
        } else {
            zone->scrollBorderRect.left -= winHalfWidthSnapped;
            zone->scrollBorderRect.right += winHalfWidthSnapped;
        }
    }

    {
        long rectHalfHeight = (zone->scrollBorderRect.bottom - zone->scrollBorderRect.top) / 2;
        if (rectHalfHeight < winHalfHeightSnapped) {
            long remainder = rectHalfHeight % kTileHeight;
            long rectHalfHeightSnapped = rectHalfHeight - remainder;
            zone->scrollBorderRect.top += rectHalfHeightSnapped + (remainder ? kTileHeight : 0);
            zone->scrollBorderRect.bottom -= rectHalfHeightSnapped;
        } else {
            zone->scrollBorderRect.top += winHalfHeightSnapped;
            zone->scrollBorderRect.bottom -= winHalfHeightSnapped;
        }
    }

    // Inverted-X guard: collapse to vertical line if left/right cross or nearly touch.
    if ((zone->scrollBorderRect.left < zone->scrollBorderRect.right) || (zone->scrollBorderRect.left - zone->scrollBorderRect.right) == kTileWidth) {
        zone->scrollBorderRect.left = zone->scrollBorderRect.right;
    }

    // Inverted-Y guard
    if ((zone->scrollBorderRect.bottom < zone->scrollBorderRect.top) || (zone->scrollBorderRect.bottom - zone->scrollBorderRect.top) == kTileHeight) {
        zone->scrollBorderRect.bottom = zone->scrollBorderRect.top;
    }

    debugPrint("EDG[%p] tileRect=(%d,%d,%d,%d) win=(%d,%d) half size snap=(%d,%d) scrollBorderRect=(%d,%d,%d,%d) rect2=(%d,%d,%d,%d)\n",
        static_cast<void*>(zone), zone->tileRect.left, zone->tileRect.top,
        zone->tileRect.right, zone->tileRect.bottom, winWidth, winHeight, winHalfWidthSnapped, winHalfHeightSnapped,
        zone->scrollBorderRect.left, zone->scrollBorderRect.top,
        zone->scrollBorderRect.right, zone->scrollBorderRect.bottom,
        zone->rect2.left, zone->rect2.top,
        zone->rect2.right, zone->rect2.bottom);
}

// Multi-edge zone selection: pick the zone whose scrollBorderRect contains the pixel position.
// Matches GetCenterTile logic: advance while target is outside current zone, use last if
// none contains it.
static EdgeZone* findZoneByPixel(int px, int py, int elevation)
{
    EdgeZone* zone = gEdgeZones[elevation].get();
    if (zone == nullptr) {
        return nullptr;
    }

    if (zone->next != nullptr) {
        const auto [winWidth, winHeight] = getIsoWindowSize();
        // Multi-edge: advance while target pixel is outside this zone's border.
        // rect2.left + width == scrollBorderRect.left, rect2.top - height == scrollBorderRect.top - 2
        const int width = winWidth - 1;
        const int height = winHeight + 1;

        while (px >= (zone->rect2.left + width) || px <= (zone->rect2.right + width)
            || py <= (zone->rect2.top - height) || py >= (zone->rect2.bottom - height)) {
            EdgeZone* next = zone->next.get();
            if (next == nullptr) {
                break;
            }
            zone = next;
        }
    }

    return zone;
}

// Load EDG file, populate gEdgeZones, and compute pixel-space fields.
// EDG files use big-endian byte order (like all Fallout 2 files).
static bool mapEdgeLoadFromStream(File* stream)
{
    int magic;
    if (fileReadInt32(stream, &magic) == -1 || magic != 'EDGE') return false;

    int version;
    if (fileReadInt32(stream, &version) == -1 || (version != 1 && version != 2)) return false;

    int reserved;
    if (fileReadInt32(stream, &reserved) == -1 || reserved != 0) return false;

    gEdgeVersion2 = (version == 2);

    int levelIndicator = 0;

    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        int sqLeft = 99, sqTop = 0, sqRight = 0, sqBottom = 99;
        int sqClipData = 0;

        if (gEdgeVersion2) {
            int sqRect[4];
            if (fileReadInt32List(stream, sqRect, 4) == -1
                || fileReadInt32(stream, &sqClipData) == -1) {
                return false;
            }
            sqLeft = sqRect[0];
            sqTop = sqRect[1];
            sqRight = sqRect[2];
            sqBottom = sqRect[3];
        }

        if (levelIndicator != elev) {
            continue; // no tileRect data for this elevation
        }

        auto tail = &gEdgeZones[elev];
        bool isFirstZone = true;

        while (true) {
            int tileRect[4];
            if (fileReadInt32List(stream, tileRect, 4) == -1) {
                return elev == ELEVATION_COUNT - 1;
            }

            auto zone = std::make_unique<EdgeZone>();
            // File stores RECT order: [0]=left, [1]=top, [2]=right, [3]=bottom.
            zone->tileRect.left = tileRect[0];
            zone->tileRect.top = tileRect[1];
            zone->tileRect.right = tileRect[2];
            zone->tileRect.bottom = tileRect[3];

            zone->squareRect.left = sqLeft;
            zone->squareRect.top = sqTop;
            zone->squareRect.right = sqRight;
            zone->squareRect.bottom = sqBottom;
            zone->clipData = isFirstZone ? sqClipData : 0;
            zone->next = nullptr;

            calcEdgeData(zone.get());

            *tail = std::move(zone);
            tail = &(*tail)->next;
            isFirstZone = false;

            if (fileReadInt32(stream, &levelIndicator) == -1) {
                return elev == ELEVATION_COUNT - 1;
            }

            if (levelIndicator != elev) {
                break; // next elevation begins
            }
        }
    }

    return true;
}

void mapEdgeLoad(const char* mapName)
{
    mapEdgeFree();

    char fname[COMPAT_MAX_FNAME];
    compat_splitpath(mapName, nullptr, nullptr, fname, nullptr);

    char edgPath[COMPAT_MAX_PATH];
    snprintf(edgPath, sizeof(edgPath), "MAPS\\%s.EDG", fname);

    File* stream = fileOpen(edgPath, "rb");
    if (stream == nullptr) {
        return;
    }

    bool ok = mapEdgeLoadFromStream(stream);
    fileClose(stream);

    if (ok) {
        gEdgeDataLoaded = true;
        debugPrint("mapEdgeLoad: loaded %s\n", edgPath);
    } else {
        debugPrint("mapEdgeLoad: failed to parse %s\n", edgPath);
        mapEdgeFree();
    }
}

void mapEdgeFree()
{
    for (auto& gEdgeZone : gEdgeZones) {
        gEdgeZone.reset();
    }
    gEdgeDataLoaded = false;
    gCurrentEdgeZone = nullptr;
    currentTileXAlignment = 0;
    currentTileYAlignment = 0;
    maxTileXAlignment = 0;
    maxTileYAlignment = 0;
    gMapVisibleArea = {};
}

bool mapEdgeIsLoaded()
{
    return gEdgeDataLoaded;
}

bool mapEdgeZoneIsSelected()
{
    return gCurrentEdgeZone != nullptr;
}

// Shared helper: set currentTileXAlignment/Height if the pixel is on a scrollBorderRect edge.
static void updateTileAlignment(const EdgeZone* zone, int px, int py);

int mapEdgeSelectZoneAndClamp(int tile, int elevation)
{
    int px, py;
    tileToPixelOffset(tile, px, py);

    // Set current zone for subsequent scroll blocking checks.
    EdgeZone* zone = gCurrentEdgeZone = findZoneByPixel(px, py, elevation);
    if (zone == nullptr) {
        return tile;
    }

    // Clamp pixel position to scrollBorderRect (matching sfall GetCenterTile).
    // Note: X is inverted (left > right), so the valid range is [right, left].
    int cx = (px <= zone->scrollBorderRect.left)
        ? ((px >= zone->scrollBorderRect.right) ? px : zone->scrollBorderRect.right)
        : zone->scrollBorderRect.left;

    int cy = (py <= zone->scrollBorderRect.bottom)
        ? ((py >= zone->scrollBorderRect.top) ? py : zone->scrollBorderRect.top)
        : zone->scrollBorderRect.bottom;

    updateTileAlignment(zone, cx, cy);

    return pixelToTile(cx, cy);
}

bool mapEdgeTileInBounds(int tile)
{
    int px, py;
    tileToPixelOffset(tile, px, py);

    EdgeZone* zone = gCurrentEdgeZone;
    if (zone == nullptr) {
        return false;
    }

    // Note: X is inverted (left > right), so check px in [right, left].
    return px >= zone->scrollBorderRect.right && px <= zone->scrollBorderRect.left
        && py >= zone->scrollBorderRect.top && py <= zone->scrollBorderRect.bottom;
}

// Shared helper: set currentTileXAlignment if the pixel is on a scrollBorderRect edge.
static void updateTileAlignment(const EdgeZone* zone, int px, int py)
{
    currentTileXAlignment = 0;
    currentTileYAlignment = 0;
    if (px == zone->scrollBorderRect.left) {
        currentTileXAlignment = -maxTileXAlignment;
    } else if (px == zone->scrollBorderRect.right) {
        currentTileXAlignment = maxTileXAlignment;
    }
    if (py == zone->scrollBorderRect.top) {
        currentTileYAlignment = -maxTileYAlignment;
    } else if (py == zone->scrollBorderRect.bottom) {
        currentTileYAlignment = maxTileYAlignment;
    }
}

bool mapEdgeSetBoundaryMods(int tile)
{
    int px, py;
    tileToPixelOffset(tile, px, py);

    EdgeZone* zone = gCurrentEdgeZone;
    if (zone == nullptr) {
        currentTileXAlignment = 0;
        currentTileYAlignment = 0;
        return false;
    }

    const int oldModW = currentTileXAlignment;
    const int oldModH = currentTileYAlignment;
    updateTileAlignment(zone, px, py);
    return currentTileXAlignment != oldModW || currentTileYAlignment != oldModH;
}

int mapEdgeGetTileXAlignment() { return currentTileXAlignment; }

int mapEdgeGetTileYAlignment() { return currentTileYAlignment; }

bool mapEdgeHasSquareRect(int elevation)
{
    const auto& zone = gEdgeZones[elevation];
    return gEdgeVersion2 && zone != nullptr && zone->squareRect.left >= 0;
}

void mapEdgeGetSquareRect(int elevation, Rect* outRect)
{
    const auto& zone = gEdgeZones[elevation];
    *outRect = zone->squareRect;
}

void mapEdgeRecalc()
{
    for (auto& gEdgeZone : gEdgeZones) {
        const auto* zone = &gEdgeZone;
        while (zone != nullptr) {
            calcEdgeData(zone->get());
            zone = &zone->get()->next;
        }
    }
}

bool mapEdgeComputeVisibleArea(int elevation, Rect* outRect)
{
    if (!gEdgeDataLoaded) return false;

    int px, py;
    tileToPixelOffset(gCenterTile, px, py);

    px += currentTileXAlignment;
    py -= currentTileYAlignment;

    EdgeZone* zone = findZoneByPixel(px, py, elevation);
    if (zone == nullptr) return false;

    // Convert pixel-offset space to screen space via rect2.
    // Matches sfall EdgeClipping::rect_inside_bound_clip mapVisibleArea computation.
    outRect->left = px - zone->rect2.left;
    outRect->right = px - zone->rect2.right;
    outRect->top = zone->rect2.top - py;
    outRect->bottom = zone->rect2.bottom - py;

    gMapVisibleArea = *outRect;

    return true;
}

bool mapEdgeIsOverClippedArea(int screenX, int screenY)
{
    if (!gEdgeDataLoaded) return false;

    if (screenX >= gMapVisibleArea.left && screenX <= gMapVisibleArea.right && screenY >= gMapVisibleArea.top && screenY < gMapVisibleArea.bottom) return false;

    return windowGetAtPoint(screenX, screenY) == gIsoWindow;
}

} // namespace fallout
