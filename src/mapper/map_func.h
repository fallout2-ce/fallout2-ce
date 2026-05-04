#ifndef FALLOUT_MAPPER_MAP_FUNC_H_
#define FALLOUT_MAPPER_MAP_FUNC_H_

#include "geometry.h"

namespace fallout {

void setup_map_dirs();
void copy_proto_lists();
void place_entrance_hex();
void pick_region(Rect* rect);
void sort_rect(Rect* a, Rect* b);
void draw_rect(Rect* rect, unsigned char color);
void erase_rect(Rect* rect);
int toolbar_proto(int type, int id);
bool map_toggle_block_obj_viewing_on();

void map_load_dialog();
void map_save_dialog();
void map_save_as();
void map_info_dialog();
void create_spray_tool();
void copy_spray_tile();
void map_toggle_block_obj_viewing();
void map_clear_elevation();
void mapper_shift_map();
void mapper_shift_map_elev();
void mapper_copy_map_elev();
void mapper_flush_cache();
int pickHex();
int pickToolbar(int topY);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MAP_FUNC_H_ */
