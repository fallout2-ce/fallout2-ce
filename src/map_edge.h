#ifndef MAP_EDGE_H
#define MAP_EDGE_H

namespace fallout {

struct EdgeZone {
    // Scroll boundary in CE tile_x/tile_y coordinates (inclusive).
    // CE tile_x = HEX_GRID_WIDTH - 1 - tile % HEX_GRID_WIDTH
    // CE tile_y = tile / HEX_GRID_WIDTH
    int minTileX;
    int maxTileX;
    int minTileY;
    int maxTileY;

    // squareRect in sfall native format (v2 only).
    // valid range: squareTile%100 in [sqRight, sqLeft], squareTile/100 in [sqTop, sqBottom]
    // All fields are -1 when not present (v1 EDG).
    int sqLeft;
    int sqTop;
    int sqRight;
    int sqBottom;

    EdgeZone* next;
};

// Load EDG file for a map. mapName is the raw map filename e.g. "ARROYO.MAP".
// Silently does nothing if no .edg file is found.
void mapEdgeInit(const char* mapName);

// Free all loaded EDG data. Safe to call when nothing is loaded.
void mapEdgeFree();

// Returns true if EDG data was successfully loaded for the current map.
bool mapEdgeIsLoaded();

// Clamps tile to the EDG boundary for the given elevation.
// Returns the clamped tile (may equal tile if already in bounds).
int mapEdgeClampTile(int tile, int elevation);

// Returns true if tile is within the EDG boundary for the given elevation.
// Used by the stencil flood-fill as a traversal gate.
bool mapEdgeTileInBounds(int tile, int elevation);

// Returns true if squareRect data (v2 EDG) is available for this elevation.
bool mapEdgeHasSquareRect(int elevation);

// Fills the squareRect fields for the given elevation.
// Only valid when mapEdgeHasSquareRect(elevation) returns true.
void mapEdgeGetSquareRect(int elevation, int* left, int* top, int* right, int* bottom);

} // namespace fallout

#endif /* MAP_EDGE_H */
