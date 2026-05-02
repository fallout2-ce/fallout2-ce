#include "map_edge.h"

#include <algorithm>

#include "db.h"
#include "debug.h"
#include "map_defs.h"
#include "platform_compat.h"

namespace fallout {

static EdgeZone* gEdgeZones[ELEVATION_COUNT];
static bool gEdgeDataLoaded = false;

// Find the appropriate zone for a tile, or return the last zone as fallback
// (matches sfall behavior: use last zone when tile is outside all zones).
static EdgeZone* findZone(int tile, int elevation)
{
    int tile_x = HEX_GRID_WIDTH - 1 - tile % HEX_GRID_WIDTH;
    int tile_y = tile / HEX_GRID_WIDTH;

    EdgeZone* zone = gEdgeZones[elevation];
    EdgeZone* last = zone;
    while (zone != nullptr) {
        last = zone;
        if (tile_x >= zone->minTileX && tile_x <= zone->maxTileX
            && tile_y >= zone->minTileY && tile_y <= zone->maxTileY) {
            return zone;
        }
        zone = zone->next;
    }
    return last;
}

// Parse the EDG file and populate gEdgeZones. Returns true on success.
// EDG files use big-endian byte order (same as other Fallout 2 files).
static bool parseEdgFile(File* stream)
{
    int magic;
    if (fileReadInt32(stream, &magic) == -1 || magic != 'EDGE') return false;

    int version;
    if (fileReadInt32(stream, &version) == -1 || (version != 1 && version != 2)) return false;

    int reserved;
    if (fileReadInt32(stream, &reserved) == -1 || reserved != 0) return false;

    int levelIndicator = 0;

    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        int sqLeft = -1, sqTop = -1, sqRight = -1, sqBottom = -1;
        if (version == 2) {
            int sqRect[4], clipData;
            if (fileReadInt32List(stream, sqRect, 4) == -1
                || fileReadInt32(stream, &clipData) == -1) {
                return false;
            }
            sqLeft = sqRect[0];
            sqTop = sqRect[1];
            sqRight = sqRect[2];
            sqBottom = sqRect[3];
            // clipData skipped: CE's black fill is equivalent to sfall's blank-tile render
        }

        if (levelIndicator != elev) {
            // No tileRect data for this elevation in the file
            continue;
        }

        EdgeZone** tail = &gEdgeZones[elev];
        bool isFirstZone = true;

        while (true) {
            int tileRect[4];
            if (fileReadInt32List(stream, tileRect, 4) == -1) {
                // EOF during read
                return elev == ELEVATION_COUNT - 1;
            }

            EdgeZone* zone = new EdgeZone();
            // tileRect[0]=left, [1]=right: left tile has smaller tile_x than right in CE coords
            zone->minTileX = HEX_GRID_WIDTH - 1 - (tileRect[0] % HEX_GRID_WIDTH);
            zone->maxTileX = HEX_GRID_WIDTH - 1 - (tileRect[1] % HEX_GRID_WIDTH);
            zone->minTileY = tileRect[2] / HEX_GRID_WIDTH;
            zone->maxTileY = tileRect[3] / HEX_GRID_WIDTH;
            // squareRect is per-elevation (stored in first zone only)
            zone->sqLeft = isFirstZone ? sqLeft : -1;
            zone->sqTop = isFirstZone ? sqTop : -1;
            zone->sqRight = isFirstZone ? sqRight : -1;
            zone->sqBottom = isFirstZone ? sqBottom : -1;
            zone->next = nullptr;

            *tail = zone;
            tail = &zone->next;
            isFirstZone = false;

            if (fileReadInt32(stream, &levelIndicator) == -1) {
                // EOF after tileRect is valid for the last elevation
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

    // Build "MAPS\<basename>.EDG" from e.g. "ARROYO.MAP"
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
}

bool mapEdgeIsLoaded()
{
    return gEdgeDataLoaded;
}

int mapEdgeClampTile(int tile, int elevation)
{
    EdgeZone* zone = findZone(tile, elevation);
    if (zone == nullptr) {
        return tile;
    }

    int tile_x = HEX_GRID_WIDTH - 1 - tile % HEX_GRID_WIDTH;
    int tile_y = tile / HEX_GRID_WIDTH;

    int cx = std::clamp(tile_x, zone->minTileX, zone->maxTileX);
    int cy = std::clamp(tile_y, zone->minTileY, zone->maxTileY);

    return HEX_GRID_WIDTH * cy + (HEX_GRID_WIDTH - 1 - cx);
}

bool mapEdgeTileInBounds(int tile, int elevation)
{
    EdgeZone* zone = findZone(tile, elevation);
    if (zone == nullptr) {
        return false;
    }

    int tile_x = HEX_GRID_WIDTH - 1 - tile % HEX_GRID_WIDTH;
    int tile_y = tile / HEX_GRID_WIDTH;

    return tile_x >= zone->minTileX && tile_x <= zone->maxTileX
        && tile_y >= zone->minTileY && tile_y <= zone->maxTileY;
}

bool mapEdgeHasSquareRect(int elevation)
{
    EdgeZone* zone = gEdgeZones[elevation];
    return zone != nullptr && zone->sqLeft >= 0;
}

void mapEdgeGetSquareRect(int elevation, int* left, int* top, int* right, int* bottom)
{
    EdgeZone* zone = gEdgeZones[elevation];
    *left = zone->sqLeft;
    *top = zone->sqTop;
    *right = zone->sqRight;
    *bottom = zone->sqBottom;
}

} // namespace fallout
