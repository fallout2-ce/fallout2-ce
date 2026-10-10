#ifndef FALLOUT_MAPPER_MP_SCRPTR_H_
#define FALLOUT_MAPPER_MP_SCRPTR_H_

#include "scripts.h"

namespace fallout {

int map_scr_remove_spatial(int tile, int elevation);
int map_scr_remove_all_spatials();

int map_scr_add_spatial(int tile, int elevation);
void map_scr_toggle_hexes();
void map_set_script(int scriptIndex);
void map_show_script();
void scr_debug_print_scripts();
int scr_choose(ScriptType scriptType);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MP_SCRPTR_H_ */
