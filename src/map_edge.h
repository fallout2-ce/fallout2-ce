#ifndef MAP_EDGE_H
#define MAP_EDGE_H

#include <memory>

#include "geometry.h"

namespace fallout {

struct EdgeZone {
    // Raw tile indices from EDG file (tileRect in tile space, 0-39999).
    Rect tileRect;

    // Pixel-space rect from tileRect corner conversion (before contraction).
    Rect pixelRect;

    // Pixel-space boundary for center-tile scroll blocking (screen-size dependent).
    // Derived from pixelRect by contracting inward by window half-size and snapping to hex grid.
    // X axis is inverted: left > right (smaller tile index → larger pixel X).
    // Y axis is normal: bottom > top.
    Rect scrollBorderRect;

    // Square-grid clip rect for floor/roof rendering (v2 EDG only).
    // Valid when left >= 0.
    // TODO: implement
    Rect squareRect;

    // Bit-mask specifying for each of 4 sides from the squareRect whether to draw black squares on top of non-flat objects.
    int clipData;

    std::unique_ptr<EdgeZone> next;
};

// Load EDG file for a map. mapName is the raw map filename e.g. "ARROYO.MAP".
// Silently does nothing if no .edg file is found.
void mapEdgeLoad(const char* mapName);

// Free all loaded EDG data. Safe to call when nothing is loaded.
void mapEdgeFree();

// Returns true if EDG data was successfully loaded for the current map.
bool mapEdgeIsLoaded();

// Returns true if a zone was selected on last tileSetCenter call.
bool mapEdgeZoneIsSelected();

// Clamps tile to the EDG boundary for the given elevation.
// Returns the clamped tile (may equal tile if already in bounds).
int mapEdgeSelectZoneAndClamp(int tile, int elevation);

// Returns true if tile is within the EDG boundary for the given elevation.
// Used by the stencil flood-fill as a traversal gate.
bool mapEdgeTileInBounds(int tile);

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
// Used when camera is exactly at one of the scroll border edges.
int mapEdgeGetTileXAlignment();
int mapEdgeGetTileYAlignment();

// For the blocking (flags == 0) scroll path: sets boundary mod values if the given
// tile's pixel position is exactly on a scrollBorderRect edge.
// Returns true if the tile is on a boundary edge (mods changed) — caller should set
// TILE_SET_CENTER_REFRESH_WINDOW for full redraw, matching sfall CheckBorder result==1.
bool mapEdgeSetBoundaryMods(int tile);

// Computes the screen-space visible area rectangle.
// Equivalent to sfall EdgeClipping's mapVisibleArea. Returns false if no EDG loaded.
bool mapEdgeComputeVisibleArea(int elevation, Rect* outRect);

// Returns true if the screen coordinate is over the map window but outside the EDG-visible area.
bool mapEdgeIsOverClippedArea(int screenX, int screenY);

} // namespace fallout

#endif /* MAP_EDGE_H */
