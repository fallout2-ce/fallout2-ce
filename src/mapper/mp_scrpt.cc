#include "mapper/mp_scrpt.h"

#include "art.h"
#include "color.h"
#include "debug.h"
#include "map.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "proto_instance.h"
#include "scripts.h"
#include "tile.h"
#include "window_manager.h"
#include "window_manager_private.h"

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
    int x, y;
    mouseGetPosition(&x, &y);
    int tile = tileFromScreenXY(x, y);
    if (tile == -1) return;

    // Place spatial script marker object
    Object* obj;
    if (objectCreateWithFidPid(&obj, buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, 0), -1) != -1) {
        objectSetLocation(obj, tile, gElevation, nullptr);
    }
    // TODO: register the spatial script itself
}

void map_set_script()
{
    char info[256];
    snprintf(info, sizeof(info), "Map SID: %d", gMapSid);
    _win_msg(info, 80, 80, 0x10104);
    // TODO: full script selection dialog
}

void map_show_script()
{
    char info[256];
    snprintf(info, sizeof(info), "Map script SID: %d\nMap name: %s",
        gMapSid, gMapHeader.name);
    _win_msg(info, 80, 80, 0x10104);
}

void scr_list_str()
{
    // TODO: list all scripts via debug print or dialog
    debugPrint("\n--- Script List ---\n");
    debugPrint("Map SID: %d\n", gMapSid);
    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        Script* scr = scriptGetFirstSpatialScript(elev);
        int count = 0;
        while (scr != nullptr) {
            count++;
            scr = scriptGetNextSpatialScript();
        }
        if (count > 0) {
            debugPrint("Elev %d: %d spatial scripts\n", elev, count);
        }
    }
}

} // namespace fallout
