#ifndef FALLOUT_MAPPER_MP_SCRPTR_H_
#define FALLOUT_MAPPER_MP_SCRPTR_H_

#include "map_defs.h"

namespace fallout {

int map_scr_remove_spatial(int tile, MapElevation elevation);
int map_scr_remove_all_spatials();

int map_scr_add_spatial(int tile, MapElevation elevation);
void map_scr_toggle_hexes();
void map_set_script(int scriptIndex);
void map_show_script();
void scr_debug_print_scripts();
int scr_choose(int scriptType);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MP_SCRPTR_H_ */
