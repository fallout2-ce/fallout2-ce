#ifndef FALLOUT_MAPPER_MP_SCRPTR_H_
#define FALLOUT_MAPPER_MP_SCRPTR_H_

namespace fallout {

int map_scr_remove_spatial(int tile, int elevation);
int map_scr_remove_all_spatials();

void map_scr_add_spatial();
void map_scr_toggle_hexes();
void map_set_script();
void map_show_script();
void scr_list_str();

} // namespace fallout

#endif /* FALLOUT_MAPPER_MP_SCRPTR_H_ */
