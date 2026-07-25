#include "mapper/mp_text.h"

#include "color.h"
#include "input.h"
#include "kb.h"
#include "mapper/mp_proto.h"
#include "mapper/mp_utils.h"
#include "obj_types.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

// 0x49DAC4
int proto_build_all_texts()
{
    for (int type = OBJ_TYPE_ITEM; type <= OBJ_TYPE_MISC; type++) {
        if (inputGetInput() == KEY_ESCAPE) {
            win_timed_msg("BUILD all Protos has been aborted!", COLOR_LIGHT_YELLOW | DRAW_TEXT_FLAG_SHADOWED);
            return 0;
        }
        if (proto_build_all_type(type) == -1) {
            win_timed_msg("Build Text Error: Type Failed Build!", COLOR_LIGHT_YELLOW | DRAW_TEXT_FLAG_SHADOWED);
            return -1;
        }
    }
    return 0;
}

void load_all_maps_text(int mode)
{
    // TODO: load all maps from text format. mode==0 → rebuild from text;
    // mode==1 → generate text versions from binary maps.
    (void)mode;
    mapperShowTimedMsg("Loading maps from text not implemented yet!");
}

void map_save_text()
{
    // TODO:
    mapperShowTimedMsg("Saving map text not implemented yet!");
}

} // namespace fallout
