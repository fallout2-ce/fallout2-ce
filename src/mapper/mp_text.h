#ifndef FALLOUT_MAPPER_MP_TEXT_H_
#define FALLOUT_MAPPER_MP_TEXT_H_

#include "db.h"

namespace fallout {

int proto_build_all_texts();
// mode==0 → rebuild from text; mode==1 → generate text versions from binary maps.
void load_all_maps_text(int mode = 0);
void map_save_text();
void obj_load_text(File* stream);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MP_TEXT_H_ */
