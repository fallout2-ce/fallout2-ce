#include "mapper/mp_text.h"

#include "color.h"
#include "input.h"
#include "kb.h"
#include "mapper/mp_proto.h"
#include "obj_types.h"
#include "window_manager_private.h"

namespace fallout {

// 0x49DAC4
int proto_build_all_texts()
{
    for (int type = OBJ_TYPE_ITEM; type <= OBJ_TYPE_MISC; type++) {
        if (inputGetInput() == KEY_ESCAPE) {
            win_timed_msg("BUILD all Protos has been aborted!", _colorTable[32747] | 0x10000);
            return 0;
        }
        if (proto_build_all_type(type) == -1) {
            win_timed_msg("Build Text Error: Type Failed Build!", _colorTable[32747] | 0x10000);
            return -1;
        }
    }
    return 0;
}

void load_all_maps_text()
{
    // TODO: load all maps from text format
}

} // namespace fallout
