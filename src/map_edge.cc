#include "map_edge.h"

#include "db.h"
#include "debug.h"
#include "geometry.h"
#include "map.h"
#include "map_defs.h"
#include "platform_compat.h"
#include "settings.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"

#include <cassert>

namespace fallout {

// Tile size in pixels
constexpr int kTileWidth = 32;
constexpr int kTileHeight = 24;

static EdgeElevationData edgeData[ELEVATION_COUNT];
static bool edgeDataLoaded = false;
static bool mapperMode = false;
static bool edgeVersion2 = false;
static EdgeZone* currentEdgeZone = nullptr;

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
    x = (2 * x) + HEX_GRID_WIDTH - y;
    outY = kTileHeight / 2 * y;
    outX = kTileWidth / 2 * x;
}

// Convert pixel-offset to tile coord (in-place, like sfall GetCoordFromOffset).
void pixelToTileCoord(int& inOutX, int& inOutY)
{
    int y = inOutY / kTileHeight;
    int x = (inOutX / kTileWidth) + y - (HEX_GRID_WIDTH / 2);
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
    int rc = windowGetRect(gIsoWindow, &winRect);
    assert(rc != -1);
    return { rectGetWidth(&winRect), rectGetHeight(&winRect) };
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

    // Convert tileRect corners to pixel offsets.
    // pixelRect = raw pixel-space rect before any contraction.
    int px, py;

    tileToPixelOffset(zone->tileRect.left, px, py);
    zone->pixelRect.left = zone->scrollBorderRect.left = px;

    tileToPixelOffset(zone->tileRect.top, px, py);
    zone->pixelRect.top = zone->scrollBorderRect.top = py;

    tileToPixelOffset(zone->tileRect.right, px, py);
    zone->pixelRect.right = zone->scrollBorderRect.right = px;

    tileToPixelOffset(zone->tileRect.bottom, px, py);
    zone->pixelRect.bottom = zone->scrollBorderRect.bottom = py;

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

    debugPrint("EDG[%p] tileRect=(%d,%d,%d,%d) win=(%d,%d) half size snap=(%d,%d) pixelRect=(%d,%d,%d,%d) scrollBorderRect=(%d,%d,%d,%d)\n",
        static_cast<void*>(zone), zone->tileRect.left, zone->tileRect.top,
        zone->tileRect.right, zone->tileRect.bottom, winWidth, winHeight, winHalfWidthSnapped, winHalfHeightSnapped,
        zone->pixelRect.left, zone->pixelRect.top,
        zone->pixelRect.right, zone->pixelRect.bottom,
        zone->scrollBorderRect.left, zone->scrollBorderRect.top,
        zone->scrollBorderRect.right, zone->scrollBorderRect.bottom);
}

// Multi-edge zone selection: pick the zone whose scrollBorderRect contains the pixel position.
// Matches GetCenterTile logic: advance while target is outside current zone, use last if
// none contains it.
static EdgeZone* findZoneByPixel(int px, int py, int elevation)
{
    std::vector<EdgeZone>& zones = edgeData[elevation].zones;
    if (zones.empty()) {
        return nullptr;
    }

    // Multi-edge: advance while target is outside current zone, stopping at the last.
    // width/height in original are half-window values (winHalfWidth-1, winHalfHeight+1),
    // so window size cancels out of the condition — only pixelRect ± small constants remain.
    constexpr int kZoneMarginY = 2;

    size_t index = 0;
    while (index + 1 < zones.size()) {
        const EdgeZone& zone = zones[index];
        // Point is inside current zone.
        if (px < zone.pixelRect.left && px > zone.pixelRect.right
            && py > zone.pixelRect.top - kZoneMarginY && py < zone.pixelRect.bottom - kZoneMarginY) {
            break;
        }
        index++;
    }

    return &zones[index];
}

// Build the "<name>.EDG" name.
static void buildEdgeFileName(const char* mapName, char* outPath, size_t outSize)
{
    char fname[COMPAT_MAX_FNAME];
    compat_splitpath(mapName, nullptr, nullptr, fname, nullptr);
    snprintf(outPath, outSize, "%s.EDG", fname);
}

// Unpacks the EDG clip-sides bitfield (per-elevation in v2).
static EdgeZone::ClipSides unpackClipSides(int raw)
{
    EdgeZone::ClipSides clip;
    clip.bottom = (raw & 1) != 0;
    clip.right = ((raw >> 8) & 1) != 0;
    clip.top = ((raw >> 16) & 1) != 0;
    clip.left = ((raw >> 24) & 1) != 0;
    return clip;
}

// Packs clip-sides into the EDG bitfield (inverse of unpackClipSides).
static int packClipSides(const EdgeZone::ClipSides& clip)
{
    int value = 0;
    if (clip.bottom) value |= 1;
    if (clip.right) value |= 1 << 8;
    if (clip.top) value |= 1 << 16;
    if (clip.left) value |= 1 << 24;
    return value;
}

// Parse an EDG stream into edgeData / edgeVersion2 (big-endian byte order), computing
// the runtime pixel-space fields for each zone.
static bool mapEdgeLoadFromStream(File* stream)
{
    int magic;
    if (fileReadInt32(stream, &magic) == -1 || magic != 'EDGE') return false;

    int version;
    if (fileReadInt32(stream, &version) == -1 || (version != 1 && version != 2)) return false;

    int reserved;
    if (fileReadInt32(stream, &reserved) == -1 || reserved != 0) return false;

    edgeVersion2 = (version == 2);

    int levelIndicator = 0;

    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        EdgeElevationData& data = edgeData[elev];
        data.squareRect = { SQUARE_GRID_WIDTH - 1, 0, 0, SQUARE_GRID_HEIGHT - 1 };
        data.clipSides = {};

        if (edgeVersion2) {
            int sqRect[4];
            int sqClipData;
            if (fileReadInt32List(stream, sqRect, 4) == -1 || fileReadInt32(stream, &sqClipData) == -1) {
                return false;
            }
            data.squareRect = { sqRect[0], sqRect[1], sqRect[2], sqRect[3] };
            data.clipSides = unpackClipSides(sqClipData);
        }

        if (levelIndicator != elev) {
            continue; // no tileRect data for this elevation
        }

        while (true) {
            int tileRect[4];
            if (fileReadInt32List(stream, tileRect, 4) == -1) {
                return elev == ELEVATION_COUNT - 1;
            }

            // File stores RECT order: [0]=left, [1]=top, [2]=right, [3]=bottom.
            EdgeZone zone {};
            zone.tileRect = { tileRect[0], tileRect[1], tileRect[2], tileRect[3] };
            calcEdgeData(&zone);
            data.zones.push_back(zone);

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

    char edgName[COMPAT_MAX_PATH];
    buildEdgeFileName(mapName, edgName, sizeof(edgName));
    const char* edgPath = mapBuildPath(edgName);

    File* stream = fileOpen(edgPath, "rb");
    if (stream == nullptr) {
        return;
    }

    bool ok = mapEdgeLoadFromStream(stream);
    fileClose(stream);

    if (ok) {
        edgeDataLoaded = true;
        debugPrint("mapEdgeLoad: loaded %s\n", edgPath);
    } else {
        debugPrint("mapEdgeLoad: failed to parse %s\n", edgPath);
        mapEdgeFree();
    }
}

// Index of the next elevation (>= from) that has zones, or ELEVATION_COUNT if none.
// Used as the level indicator that advances the loader past empty elevations.
static int nextElevationWithZones(int from)
{
    for (int elev = from; elev < ELEVATION_COUNT; elev++) {
        if (!edgeData[elev].zones.empty()) {
            return elev;
        }
    }
    return ELEVATION_COUNT;
}

// Writes edgeData to an EDG stream, mirroring mapEdgeLoadFromStream's byte layout.
static bool writeEdgStream(File* stream)
{
    if (fileWriteInt32(stream, 'EDGE') == -1) return false;
    if (fileWriteInt32(stream, edgeVersion2 ? 2 : 1) == -1) return false;
    if (fileWriteInt32(stream, 0) == -1) return false; // reserved

    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        const EdgeElevationData& data = edgeData[elev];

        if (edgeVersion2) {
            int sqRect[4] = { data.squareRect.left, data.squareRect.top, data.squareRect.right, data.squareRect.bottom };
            if (fileWriteInt32List(stream, sqRect, 4) == -1) return false;
            if (fileWriteInt32(stream, packClipSides(data.clipSides)) == -1) return false;
        }

        const int zoneCount = static_cast<int>(data.zones.size());
        for (int i = 0; i < zoneCount; i++) {
            const Rect& r = data.zones[i].tileRect;
            int tileRect[4] = { r.left, r.top, r.right, r.bottom };
            if (fileWriteInt32List(stream, tileRect, 4) == -1) return false;

            // Level indicator: same elevation while more zones follow, otherwise the
            // index of the next elevation that has zones (so the loader advances to it).
            int levelIndicator = (i + 1 < zoneCount) ? elev : nextElevationWithZones(elev + 1);
            if (fileWriteInt32(stream, levelIndicator) == -1) return false;
        }
    }

    return true;
}

void mapEdgeSave(const char* mapName)
{
    int totalZones = 0;
    for (const EdgeElevationData& data : edgeData) {
        totalZones += static_cast<int>(data.zones.size());
    }
    if (totalZones == 0) {
        return; // nothing to write
    }

    char edgName[COMPAT_MAX_PATH];
    buildEdgeFileName(mapName, edgName, sizeof(edgName));
    const char* edgPath = mapBuildSavePath(edgName);

    File* stream = fileOpen(edgPath, "wb");
    if (stream == nullptr) {
        debugPrint("mapEdgeSave: unable to open %s for writing\n", edgPath);
        return;
    }

    bool ok = writeEdgStream(stream);
    fileClose(stream);
    debugPrint("mapEdgeSave: %s %s\n", ok ? "wrote" : "error writing", edgPath);
}

EdgeElevationData& mapEdgeGetElevationData(int elevation)
{
    return edgeData[elevation];
}

void mapEdgeFree()
{
    for (auto& data : edgeData) {
        data = EdgeElevationData {};
    }
    edgeDataLoaded = false;
    currentEdgeZone = nullptr;
    currentTileXAlignment = 0;
    currentTileYAlignment = 0;
    maxTileXAlignment = 0;
    maxTileYAlignment = 0;
    gMapVisibleArea = {};
}

bool mapEdgeIsLoaded()
{
    return edgeDataLoaded;
}

void mapEdgeSetMapperMode(bool enabled)
{
    mapperMode = enabled;
}

bool mapEdgeIsMapperMode()
{
    return mapperMode;
}

bool mapEdgeIsEnabled()
{
    // Enforced when data is loaded, we're not editing in the mapper, and the user enabled it.
    return edgeDataLoaded && !mapperMode && settings.ui.edg_support && !settings.ui.ignore_map_edges;
}

void mapEdgeUpgradeToVersion2()
{
    edgeVersion2 = true;
}

bool mapEdgeZoneIsSelected()
{
    return currentEdgeZone != nullptr;
}

// Shared helper: set currentTileXAlignment/Height if the pixel is on a scrollBorderRect edge.
static void updateTileAlignment(const EdgeZone* zone, int px, int py);

int mapEdgeSelectZoneAndClamp(int tile, int elevation)
{
    int px, py;
    tileToPixelOffset(tile, px, py);

    // Set current zone for subsequent scroll blocking checks.
    EdgeZone* zone = currentEdgeZone = findZoneByPixel(px, py, elevation);
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

    EdgeZone* zone = currentEdgeZone;
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

    EdgeZone* zone = currentEdgeZone;
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
    const EdgeElevationData& data = edgeData[elevation];
    return edgeVersion2 && !data.zones.empty() && data.squareRect.left >= 0;
}

void mapEdgeGetSquareRect(int elevation, Rect* outRect)
{
    *outRect = edgeData[elevation].squareRect;
}

EdgeZone::ClipSides mapEdgeGetClipSides(int elevation)
{
    return edgeData[elevation].clipSides;
}

void mapEdgeRecalc()
{
    for (auto& data : edgeData) {
        for (auto& zone : data.zones) {
            calcEdgeData(&zone);
        }
    }
}

bool mapEdgeComputeVisibleArea(int elevation, Rect* outRect)
{
    if (!mapEdgeIsEnabled()) return false;

    int px, py;
    tileToPixelOffset(gCenterTile, px, py);

    px += currentTileXAlignment;
    py -= currentTileYAlignment;

    EdgeZone* zone = findZoneByPixel(px, py, elevation);
    if (zone == nullptr) return false;

    // Convert pixel-offset space to screen space via screen origin.
    // screenOrigin = pixel-space coord of the screen's top-left corner.
    // X is inverted in pixel-space: screenX = screenOriginX - pixelX.
    // Matches sfall EdgeClipping::rect_inside_bound_clip mapVisibleArea computation.
    const auto [winWidth, winHeight] = getIsoWindowSize();
    const int screenOriginX = px + winWidth / 2 - 1;
    const int screenOriginY = py - (winHeight / 2 - 1);

    outRect->left = screenOriginX - zone->pixelRect.left;
    outRect->right = screenOriginX - zone->pixelRect.right;
    outRect->top = zone->pixelRect.top - screenOriginY;
    outRect->bottom = zone->pixelRect.bottom - screenOriginY;

    gMapVisibleArea = *outRect;

    return true;
}

bool mapEdgeIsOverClippedArea(int screenX, int screenY)
{
    if (!mapEdgeIsEnabled()) return false;

    if (screenX >= gMapVisibleArea.left && screenX <= gMapVisibleArea.right && screenY >= gMapVisibleArea.top && screenY < gMapVisibleArea.bottom) return false;

    return windowGetAtPoint(screenX, screenY) == gIsoWindow;
}

} // namespace fallout
