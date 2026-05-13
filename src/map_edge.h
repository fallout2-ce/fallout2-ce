#ifndef MAP_EDGE_H
#define MAP_EDGE_H

#include "geometry.h"

namespace fallout {

struct EdgeZone {
    // Raw tile indices from EDG file (tileRect in tile space, 0-39999).
    Rect tileRect;

    // Pixel-offset space scroll boundary (screen-size dependent).
    // X axis is inverted: left > right (smaller tile index → larger pixel X).
    // Y axis is normal: bottom > top.
    Rect borderRect;

    // borderRect shifted by half window size (for EdgeClipping mapVisibleArea).
    Rect rect2;

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

// Convert tile index to pixel-offset coordinates.
// Equivalent to sfall ViewMap::GetTileCoordOffset.
void tileToPixelOffset(int tile, int& outX, int& outY);

// Convert pixel-offset to tile coord (in-place, like sfall GetCoordFromOffset).
void pixelToTileCoord(int& inOutX, int& inOutY);

// Fills the squareRect for the given elevation.
// Only valid when mapEdgeHasSquareRect(elevation) returns true.
void mapEdgeGetSquareRect(int elevation, Rect* outRect);

// Recalculate all pixel-space fields using current screen dimensions.
// Call when resolution changes while a map is loaded.
void mapEdgeRecalc();

// Pixel-offset adjustments for sub-tile boundary alignment (sfall mapModWidth/Height).
// Computed by mapEdgeClampTile or mapEdgeSetBoundaryMods, read by tileSetCenter
// to adjust _tile_offx / _tile_offy so the camera stops exactly at the borderRect edge.
int mapEdgeGetModWidth();
int mapEdgeGetModHeight();

// For the blocking (flags == 0) scroll path: sets boundary mod values if the given
// tile's pixel position is exactly on a borderRect edge.
// Returns true if the tile is on a boundary edge (mods changed) — caller should set
// TILE_SET_CENTER_REFRESH_WINDOW for full redraw, matching sfall CheckBorder result==1.
bool mapEdgeSetBoundaryMods(int tile, int elevation);

// Computes the screen-space visible area rectangle by converting the current center
// tile's pixel offset (adjusted by mapModWidth/Height) through the edge zone's rect_2.
// Equivalent to sfall EdgeClipping's mapVisibleArea. Returns false if no EDG loaded.
bool mapEdgeComputeVisibleArea(int elevation, Rect* outRect);

} // namespace fallout

#endif /* MAP_EDGE_H */
