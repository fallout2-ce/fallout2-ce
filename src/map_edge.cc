#include "map_edge.h"

#include "db.h"
#include "debug.h"
#include "map_defs.h"
#include "platform_compat.h"
#include "svga.h"
#include "tile.h"

namespace fallout {

static EdgeZone* gEdgeZones[ELEVATION_COUNT];
static bool gEdgeDataLoaded = false;
static bool gEdgeVersion2 = false;
static EdgeZone* gCurrentEdgeZone = nullptr;

// Boundary alignment mods (sfall mapModWidth/Height), set by clamp/check functions.
static int gMapModWidth = 0;
static int gMapModHeight = 0;
static int gMapWidthModSize = 0;
static int gMapHeightModSize = 0;

// Convert tile index to pixel-offset coordinates.
// Equivalent to sfall ViewMap::GetTileCoordOffset.
void tileToPixelOffset(int tile, int& outX, int& outY)
{
    int x = tile % HEX_GRID_WIDTH;
    int y = (tile / HEX_GRID_WIDTH) + (x / 2);
    y &= ~1; // force even row
    x = (2 * x) + 200 - y;
    outY = 12 * y;
    outX = 16 * x;
}

// Convert pixel-offset to tile coord (in-place, like sfall GetCoordFromOffset).
void pixelToTileCoord(int& inOutX, int& inOutY)
{
    int y = inOutY / 24;
    int x = (inOutX / 32) + y - 100;
    inOutX = x;
    inOutY = (2 * y) - (x / 2);
}

// Convert pixel-offset to tile number.
static int pixelToTile(int px, int py)
{
    pixelToTileCoord(px, py);
    return px + py * HEX_GRID_WIDTH;
}

// Fill pixel-space fields of a zone from its stored tileRect corners.
// Screen-size dependent — must be re-run if resolution changes.
static void calcEdgeData(EdgeZone* zone)
{
    const int winW = screenGetWidth();
    const int winH = screenGetVisibleHeight();

    // Truncated half sizes for border expansion (matching sfall ViewMap::GetWinMapHalfSize).
    int w = (winW >> 1) - ((winW >> 1) & 31);
    int h = (winH >> 1) - ((winH >> 1) % 24);

    // Convert tileRect corners to pixel offsets → borderRect
    int px, py;

    tileToPixelOffset(zone->tileRect.left, px, py);
    zone->borderRect.left = px;

    tileToPixelOffset(zone->tileRect.top, px, py);
    zone->borderRect.top = py;

    tileToPixelOffset(zone->tileRect.right, px, py);
    zone->borderRect.right = px;

    tileToPixelOffset(zone->tileRect.bottom, px, py);
    zone->borderRect.bottom = py;

    // rect2 = borderRect shifted by (winWidth/2 - 1, winHeight/2 - 1)
    // Used by EdgeClipping to compute mapVisibleArea (screen-space clipping rect).
    const int mapWinW = (winW / 2) - 1;
    const int mapWinH = (winH / 2) - 1;

    zone->rect2.left = zone->borderRect.left - mapWinW;
    zone->rect2.right = zone->borderRect.right - mapWinW;
    zone->rect2.top = zone->borderRect.top + mapWinH;
    zone->rect2.bottom = zone->borderRect.bottom + mapWinH;

    // Expand borderRect outward by half its own size (or window half-size if very large).
    // This creates the "scroll cushion" zone that allows the camera to reach the map edge.
    {
        long rectW = (zone->borderRect.left - zone->borderRect.right) / 2;
        long _rectW = rectW;
        if (rectW & 31) {
            rectW &= ~31;
            _rectW = rectW + 32;
        }
        if (rectW < w) {
            zone->borderRect.left -= rectW;
            zone->borderRect.right += _rectW;
        } else {
            zone->borderRect.left -= w;
            zone->borderRect.right += w;
        }
    }

    {
        long rectH = (zone->borderRect.bottom - zone->borderRect.top) / 2;
        long _rectH = rectH;
        if (rectH % 24) {
            rectH -= rectH % 24;
            _rectH = rectH + 24;
        }
        if (rectH < h) {
            zone->borderRect.top += _rectH;
            zone->borderRect.bottom -= rectH;
        } else {
            zone->borderRect.top += h;
            zone->borderRect.bottom -= h;
        }
    }

    // Inverted-X guard: collapse to vertical line if left/right cross or nearly touch.
    if ((zone->borderRect.left < zone->borderRect.right) || (zone->borderRect.left - zone->borderRect.right) == 32) {
        zone->borderRect.left = zone->borderRect.right;
    }

    // Inverted-Y guard
    if ((zone->borderRect.bottom < zone->borderRect.top) || (zone->borderRect.bottom - zone->borderRect.top) == 24) {
        zone->borderRect.bottom = zone->borderRect.top;
    }

    debugPrint("EDG[%p] tileRect=(%d,%d,%d,%d) win=(%d,%d) w=%d h=%d borderRect=(%d,%d,%d,%d) rect2=(%d,%d,%d,%d)\n",
        static_cast<void*>(zone), zone->tileRect.left, zone->tileRect.top,
        zone->tileRect.right, zone->tileRect.bottom, winW, winH, w, h,
        zone->borderRect.left, zone->borderRect.top,
        zone->borderRect.right, zone->borderRect.bottom,
        zone->rect2.left, zone->rect2.top,
        zone->rect2.right, zone->rect2.bottom);

    // Compute sub-tile alignment sizes (sfall mapWidthModSize / mapHeightModSize).
    gMapWidthModSize = (winW >> 1) & 31;
    gMapHeightModSize = (winH >> 1) % 24;
}

// Multi-edge zone selection: pick the zone whose borderRect contains the pixel position.
// Matches GetCenterTile logic: advance while target is outside current zone, use last if
// none contains it.
static EdgeZone* findZoneByPixel(int px, int py, int elevation)
{
    EdgeZone* zone = gEdgeZones[elevation];
    if (zone == nullptr) {
        return nullptr;
    }

    if (zone->next != nullptr) {
        // Multi-edge: advance while target pixel is outside this zone's border.
        // rect2.left + width == borderRect.left, rect2.top - height == borderRect.top - 2
        const int width = (screenGetWidth() / 2) - 1;
        const int height = (screenGetVisibleHeight() / 2) + 1;

        EdgeZone* startZone = zone;
        while (px >= (zone->rect2.left + width) || px <= (zone->rect2.right + width)
            || py <= (zone->rect2.top - height) || py >= (zone->rect2.bottom - height)) {
            EdgeZone* next = zone->next;
            if (next == nullptr) {
                break;
            }
            zone = next;
        }
    }

    return zone;
}

// Parse EDG file, populate gEdgeZones, and compute pixel-space fields.
// EDG files use big-endian byte order (like all Fallout 2 files).
static bool parseEdgFile(File* stream)
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

        EdgeZone** tail = &gEdgeZones[elev];
        bool isFirstZone = true;

        while (true) {
            int tileRect[4];
            if (fileReadInt32List(stream, tileRect, 4) == -1) {
                return elev == ELEVATION_COUNT - 1;
            }

            auto* zone = new EdgeZone();
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

            calcEdgeData(zone);

            *tail = zone;
            tail = &zone->next;
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

void mapEdgeInit(const char* mapName)
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

    bool ok = parseEdgFile(stream);
    fileClose(stream);

    if (ok) {
        gEdgeDataLoaded = true;
        debugPrint("mapEdgeInit: loaded %s\n", edgPath);
    } else {
        debugPrint("mapEdgeInit: failed to parse %s\n", edgPath);
        mapEdgeFree();
    }
}

void mapEdgeFree()
{
    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        EdgeZone* zone = gEdgeZones[elev];
        while (zone != nullptr) {
            EdgeZone* next = zone->next;
            delete zone;
            zone = next;
        }
        gEdgeZones[elev] = nullptr;
    }
    gEdgeDataLoaded = false;
    gCurrentEdgeZone = nullptr;
    gMapModWidth = 0;
    gMapModHeight = 0;
    gMapWidthModSize = 0;
    gMapHeightModSize = 0;
}

bool mapEdgeIsLoaded()
{
    return gEdgeDataLoaded;
}

// Shared helper: set gMapModWidth/Height if the pixel is on a borderRect edge.
static void setBoundaryMods(const EdgeZone* zone, int px, int py);

int mapEdgeClampTile(int tile, int elevation)
{
    int px, py;
    tileToPixelOffset(tile, px, py);

    EdgeZone* zone = findZoneByPixel(px, py, elevation);
    if (zone == nullptr) {
        return tile;
    }

    // Set current zone for subsequent scroll blocking checks.
    gCurrentEdgeZone = zone;

    // Clamp pixel position to borderRect (matching sfall GetCenterTile).
    // Note: X is inverted (left > right), so the valid range is [right, left].
    int cx = (px <= zone->borderRect.left)
        ? ((px >= zone->borderRect.right) ? px : zone->borderRect.right)
        : zone->borderRect.left;

    int cy = (py <= zone->borderRect.bottom)
        ? ((py >= zone->borderRect.top) ? py : zone->borderRect.top)
        : zone->borderRect.bottom;

    setBoundaryMods(zone, cx, cy);

    return pixelToTile(cx, cy);
}

bool mapEdgeTileInBounds(int tile, int elevation)
{
    int px, py;
    tileToPixelOffset(tile, px, py);

    EdgeZone* zone = gCurrentEdgeZone;
    if (zone == nullptr) {
        return false;
    }

    // Note: X is inverted (left > right), so check px in [right, left].
    return px >= zone->borderRect.right && px <= zone->borderRect.left
        && py >= zone->borderRect.top && py <= zone->borderRect.bottom;
}

// Shared helper: set gMapModWidth/Height if the pixel is on a borderRect edge.
static void setBoundaryMods(const EdgeZone* zone, int px, int py)
{
    gMapModWidth = 0;
    gMapModHeight = 0;
    if (px == zone->borderRect.left) {
        gMapModWidth = -gMapWidthModSize;
    } else if (px == zone->borderRect.right) {
        gMapModWidth = gMapWidthModSize;
    }
    if (py == zone->borderRect.top) {
        gMapModHeight = -gMapHeightModSize;
    } else if (py == zone->borderRect.bottom) {
        gMapModHeight = gMapHeightModSize;
    }
}

bool mapEdgeSetBoundaryMods(int tile, int elevation)
{
    int px, py;
    tileToPixelOffset(tile, px, py);

    EdgeZone* zone = gCurrentEdgeZone;
    if (zone == nullptr) {
        return false;
    }

    const int oldModW = gMapModWidth;
    const int oldModH = gMapModHeight;
    setBoundaryMods(zone, px, py);
    return gMapModWidth != oldModW || gMapModHeight != oldModH;
}

int mapEdgeGetModWidth() { return gMapModWidth; }

int mapEdgeGetModHeight() { return gMapModHeight; }

bool mapEdgeHasSquareRect(int elevation)
{
    EdgeZone* zone = gEdgeZones[elevation];
    return gEdgeVersion2 && zone != nullptr && zone->squareRect.left >= 0;
}

void mapEdgeGetSquareRect(int elevation, Rect* outRect)
{
    EdgeZone* zone = gEdgeZones[elevation];
    *outRect = zone->squareRect;
}

void mapEdgeRecalc()
{
    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        EdgeZone* zone = gEdgeZones[elev];
        while (zone != nullptr) {
            calcEdgeData(zone);
            zone = zone->next;
        }
    }
}

bool mapEdgeComputeVisibleArea(int elevation, Rect* outRect)
{
    if (!gEdgeDataLoaded) return false;

    int px, py;
    tileToPixelOffset(gCenterTile, px, py);

    px += gMapModWidth;
    py -= gMapModHeight;

    EdgeZone* zone = findZoneByPixel(px, py, elevation);
    if (zone == nullptr) return false;

    // Convert pixel-offset space to screen space via rect_2.
    // Matches sfall EdgeClipping::rect_inside_bound_clip mapVisibleArea computation.
    outRect->left = px - zone->rect2.left;
    outRect->right = px - zone->rect2.right;
    outRect->top = zone->rect2.top - py;
    outRect->bottom = zone->rect2.bottom - py;

    debugPrint("EDG: visibleArea tile=%d px=(%d,%d) mods=(%d,%d) zone=%p rect2=(%d,%d,%d,%d) result=(%d,%d,%d,%d)\n",
        gCenterTile, px, py, gMapModWidth, gMapModHeight,
        static_cast<void*>(zone),
        zone->rect2.left, zone->rect2.top, zone->rect2.right, zone->rect2.bottom,
        outRect->left, outRect->top, outRect->right, outRect->bottom);

    return true;
}

} // namespace fallout
