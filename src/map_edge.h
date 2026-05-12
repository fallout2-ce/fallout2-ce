#ifndef MAP_EDGE_H
#define MAP_EDGE_H

#include "geometry.h"

namespace fallout {

struct EdgeZone {
    // Raw tile indices from EDG file (tileRect in tile space, 0-39999).
    Rect tileRect;

    // Pixel-offset space scroll boundary (screen-size dependent).
    Rect borderRect;

    // borderRect shifted by half window size (for EdgeClipping mapVisibleArea).
    Rect rect2;

    // Center point in pixel-offset space, aligned to 32x24.
    Point center;

    // Square-grid clip rect for floor/roof rendering (v2 EDG only).
    // Valid when left >= 0.
    Rect squareRect;

    int clipData;
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

// Fills the squareRect for the given elevation.
// Only valid when mapEdgeHasSquareRect(elevation) returns true.
void mapEdgeGetSquareRect(int elevation, Rect* outRect);

// Recalculate all pixel-space fields using current screen dimensions.
// Call when resolution changes while a map is loaded.
void mapEdgeRecalc();

} // namespace fallout

#endif /* MAP_EDGE_H */
