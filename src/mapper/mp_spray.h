#ifndef FALLOUT_MAPPER_MP_SPRAY_H_
#define FALLOUT_MAPPER_MP_SPRAY_H_

namespace fallout {

// Authoring path was disabled in the original mapper (the function just fails).
void create_spray_tool();

// Pick a tile pattern and mouse-paint it onto the square tile grid.
void copy_spray_tile();

// Rebuild all binary pattern (.spr) files from their text sources under the
// mapper temp root. Returns -1 on failure. Dev-only: requires the text sources.
int rebuild_spray_tools();

// Write a single floor/roof tile at an explicit square tile index (no refresh).
// Returns true if the tile data actually changed.
bool place_tile_at(int fid, int tileIndex);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MP_SPRAY_H_ */
