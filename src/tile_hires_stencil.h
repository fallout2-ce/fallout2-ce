#ifndef TILE_HIRES_STENCIL_H
#define TILE_HIRES_STENCIL_H

#include "geometry.h"
#include "map_defs.h"

namespace fallout {

void tile_hires_stencil_init();

void tile_hires_stencil_reset_cache();

void tile_hires_stencil_reset_limits();

void tile_hires_stencil_on_map_load();

void tile_hires_stencil_on_center_tile_or_elevation_change();

void tile_hires_stencil_draw(Rect* rect, unsigned char* buffer, int windowWidth, int windowHeight);

bool tile_hires_stencil_limits_active();

int tile_hires_stencil_clamp_center(int tile, MapElevation elevation);

bool tile_hires_stencil_tile_in_bounds(int tile);

bool tile_hires_stencil_set_boundary_mods(int tile);

int tile_hires_stencil_get_tile_x_alignment();

int tile_hires_stencil_get_tile_y_alignment();

bool tile_hires_stencil_compute_visible_area(MapElevation elevation, Rect* outRect);

bool tile_hires_stencil_is_over_clipped_area(int screenX, int screenY);

} // namespace fallout

#endif