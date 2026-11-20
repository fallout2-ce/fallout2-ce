#ifndef MAPPER_MAP_EDGE_SETUP_H
#define MAPPER_MAP_EDGE_SETUP_H

namespace fallout {

// Registers the iso-view overlay hook with the tile renderer. Call once at mapper init.
void mapEdgeSetupInit();

// Opens the Map Edge Setup dialog to edit the current map's edge rects.
// Edits the in-memory edge data; the disk write happens on the next map save.
void mapEdgeSetupDialog();

// Opens the Map Angled Edge Setup dialog to edit the current elevation's squareRect/clipSides.
// Edits the in-memory edge data; the disk write happens on the next map save.
void mapEdgeSquareSetupDialog();

// Toggles the persistent edge-rects overlay shown over the iso view while editing.
void mapEdgeSetupToggleOverlay();

} // namespace fallout

#endif /* MAPPER_MAP_EDGE_SETUP_H */
