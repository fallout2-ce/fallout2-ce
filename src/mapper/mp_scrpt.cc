#include "mapper/mp_scrpt.h"

#include "art.h"
#include "object.h"
#include "scripts.h"
#include "tile.h"

namespace fallout {

// 0x49B170
int map_scr_remove_spatial(int tile, int elevation)
{
    Script* scr;
    Object* obj;
    Rect rect;

    scr = scriptGetFirstSpatialScript(elevation);
    while (scr != NULL) {
        if (builtTileGetTile(scr->sp.built_tile) == tile) {
            scriptRemove(scr->sid);

            scr = scriptGetFirstSpatialScript(elevation);
            continue;
        }

        scr = scriptGetNextSpatialScript();
    }

    obj = objectFindFirstAtElevation(elevation);
    while (obj != NULL) {
        if (obj->tile == tile && buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, 0) == obj->fid) {
            objectDestroy(obj, &rect);
            tileWindowRefreshRect(&rect, elevation);

            obj = objectFindFirstAtElevation(elevation);
            continue;
        }

        obj = objectFindNextAtElevation();
    }

    return 0;
}

// 0x49B214
int map_scr_remove_all_spatials()
{
    int elevation;
    Script* scr;
    Object* obj;
    int sid;

    for (elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        scr = scriptGetFirstSpatialScript(elevation);
        while (scr != NULL) {
            scriptRemove(scr->sid);

            scr = scriptGetFirstSpatialScript(elevation);
        }

        obj = objectFindFirstAtElevation(elevation);
        while (obj != NULL) {
            if (buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, 0) == obj->fid) {
                objectDestroy(obj, NULL);

                obj = objectFindFirstAtElevation(elevation);
                continue;
            }

            obj = objectFindNextAtElevation();
        }
    }

    tileWindowRefresh();

    for (sid = 0; sid < 15000; sid++) {
        if (scriptGetScript(sid, &scr) != -1) {
            if (scr->owner != NULL) {
                if (scr->owner->pid == 0x500000C) {
                    scr->owner->sid = -1;
                    scriptRemove(sid);
                }
            }
        }
    }

    return 0;
}

// =========================================================================
// P2 stubs — to be fully implemented from mapper2.asm
// =========================================================================

void map_scr_add_spatial()
{
    // TODO: add a spatial script trigger at cursor position
}

void map_set_script()
{
    // TODO: set the map-level script
}

void map_show_script()
{
    // TODO: display info about the current map script
}

void scr_list_str()
{
    // TODO: list all scripts via debug print
}

} // namespace fallout
