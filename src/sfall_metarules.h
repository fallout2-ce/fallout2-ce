#ifndef FALLOUT_SFALL_METARULES_H_
#define FALLOUT_SFALL_METARULES_H_

#include "interpreter.h"

namespace fallout {

void sfall_metarule(Program* program, int args);

void sprintf_lite(Program* program, int args, const char* infoOpcodeName);

// New static function declarations for metarules
static void mf_set_terrain_name(Program* program, int args);
static void mf_get_terrain_name(Program* program, int args);
static void mf_set_town_title(Program* program, int args);

} // namespace fallout

#endif /* FALLOUT_SFALL_METARULES_H_ */
