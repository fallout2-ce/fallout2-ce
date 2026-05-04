#include "mapper/mp_scrpt.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "color.h"
#include "db.h"
#include "debug.h"
#include "geometry.h"
#include "map.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "proto_instance.h"
#include "scripts.h"
#include "settings.h"
#include "svga.h"
#include "text_object.h"
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

static int _scr_show_toggled = 0;

// 0x4C26D0
void map_scr_toggle_hexes()
{
    int markerFid = buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, 0);

    if (!_scr_show_toggled) {
        // REMOVE mode: erase all existing spatial marker objects
        for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
            Object* obj = objectFindFirstAtElevation(elev);
            while (obj != nullptr) {
                if (obj->fid == markerFid) {
                    objectDestroy(obj, nullptr);
                    obj = objectFindFirstAtElevation(elev);
                    continue;
                }
                obj = objectFindNextAtElevation();
            }
        }
    } else {
        // SHOW mode: iterate spatial scripts (SID 0x1000000..0x10002BC)
        for (int sid = 0; sid < 700; sid++) {
            Script* scr;
            if (scriptGetScript(0x1000000 | sid, &scr) != -1) {
                int builtTile = scr->sp.built_tile;
                if (builtTile == -1 || builtTile == 0) {
                    debugPrint("\nError: Spatial Script has invalid hex location...deleting!");
                    return;
                }

                Object* obj;
                if (objectCreateWithFidPid(&obj, markerFid, -1) != -1) {
                    obj->flags |= 4;
                    Rect rect;
                    _obj_toggle_flat(obj, &rect);
                    objectSetLocation(obj,
                        builtTileGetTile(builtTile),
                        builtTileGetElevation(builtTile),
                        &rect);
                }
            }
        }
    }

    _scr_show_toggled = 1 - _scr_show_toggled;
    tileWindowRefresh();
}

// =========================================================================
// Script selection dialog
// =========================================================================

// scr_find_str_index_ — case-insensitive lookup
static int scr_find_index(const char* name, int count)
{
    for (int i = 0; i < count; i++) {
        char nameBuf[32];
        scriptsGetFileName(i, nameBuf, sizeof(nameBuf));
        char* dot = strchr(nameBuf, '.');
        if (dot != nullptr) *dot = '\0';
        if (stricmp(nameBuf, name) == 0) {
            return i;
        }
    }
    return -1;
}

int scr_choose(int scriptType)
{
    const int kDialogColor = 0x10104;
    static char* kScriptTypeNames[] = { (char*)"SYSTEM", (char*)"SPATIAL", (char*)"TIMED", (char*)"ITEM", (char*)"CRITTER" };

    int count = scriptsGetListLength();
    if (count == 0) {
        return -1;
    }

    int type = scriptType;
    if (type == -1) {
        type = _win_list_select("Script Type", kScriptTypeNames, 5, nullptr, 100, 100, kDialogColor);
        if (type == -1) {
            type = 0;
        }
    }

    char path[COMPAT_MAX_PATH];
    _script_make_path(path);
    strcat(path, "scripts.lst");

    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    char** strlist = (char**)internal_malloc(sizeof(char*) * count);
    if (strlist == nullptr) {
        fileClose(stream);
        return -1;
    }

    char buf[520];
    int lineCount;
    for (lineCount = 0; lineCount < count; lineCount++) {
        if (fileReadString(buf, sizeof(buf), stream) == nullptr) {
            debugPrint("\nError! in mp_scrpt!");
            break;
        }

        char* pound = strchr(buf, '#');
        if (pound != nullptr) {
            *pound = '\0';
        }

        strlist[lineCount] = (char*)internal_malloc(520);
        if (strlist[lineCount] == nullptr) {
            fileClose(stream);
            for (int i = 0; i < lineCount; i++) {
                internal_free(strlist[i]);
            }
            internal_free(strlist);
            return -1;
        }

        strcpy(strlist[lineCount], buf);

        char* dot = strchr(strlist[lineCount], '.');
        if (dot != nullptr) {
            for (int j = 0; j < 4; j++) {
                dot[j] = ' ';
            }
        }
    }

    fileClose(stream);

    int result = -1;

    if (lineCount == count) {
        if (settings.mapper.sort_script_list) {
            qsort(strlist, lineCount, sizeof(char*), [](const void* a, const void* b) -> int {
                return strcmp(*(const char**)a, *(const char**)b);
            });
        }

        int selection = _win_list_select("Script", strlist, lineCount, nullptr, 50, 100, kDialogColor);

        if (selection >= 0) {
            char* p = strchr(strlist[selection], ' ');
            if (p != nullptr) {
                *p = '\0';
            }

            int foundIndex = scr_find_index(strlist[selection], count);
            if (foundIndex >= 0) {
                result = foundIndex | (type << 24);
            }
        } else if (selection == -1) {
            char typedName[14] = { };
            _win_get_str(typedName, 13, "Type in Script Name", 100, 50);
            strupr(typedName);

            char* intStr = strstr(typedName, ".INT");
            if (intStr != nullptr) {
                *intStr = '\0';
            }

            if (strcmp(typedName, "NO SCRIPT") == 0 || strcmp(typedName, "NONE") == 0) {
                result = -2;
            } else {
                int foundIndex = scr_find_index(typedName, count);
                if (foundIndex >= 0) {
                    result = foundIndex | (type << 24);
                }
            }
        }
    }

    for (int i = 0; i < lineCount; i++) {
        internal_free(strlist[i]);
    }
    internal_free(strlist);

    return result;
}

int map_scr_add_spatial(int tile, int elevation)
{
    int scriptId = scr_choose(1);
    if (scriptId < 0) {
        return -1;
    }

    int radius = 3;
    if (win_get_num_i(&radius, 0, 50, false, "Radius:", 100, 100) == -1) {
        return -1;
    }

    int sid;
    if (scriptAdd(&sid, SCRIPT_TYPE_SPATIAL) == -1) {
        return -1;
    }

    Script* scr;
    if (scriptGetScript(sid, &scr) == -1) {
        return -1;
    }

    int markerFid = buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, 0);
    Object* obj;
    if (objectCreateWithFidPid(&obj, markerFid, -1) != -1) {
        obj->flags |= OBJECT_NO_SAVE;
        Rect rect;
        _obj_toggle_flat(obj, &rect);
        objectSetLocation(obj, tile, elevation, &rect);
        tileWindowRefreshRect(&rect, elevation);
    }

    scr->sp.built_tile = builtTileCreate(tile, elevation);
    scr->index = scriptId & 0xFFFFFF;
    scr->sp.radius = radius;
    _scr_find_str_run_info(scriptId & 0xFFFFFF, nullptr, sid);

    return 0;
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

static void scr_label_object(Object* obj, const char* scriptName)
{
    Rect rect;
    objectGetRect(obj, &rect);

    const int kScreenWidth = rectGetWidth(&_scr_size);
    const int kScreenHeight = rectGetHeight(&_scr_size);
    if (rect.left < 0 || rect.right >= kScreenWidth || rect.bottom >= kScreenHeight) {
        return;
    }

    char nameBuf[64];
    if (scriptName[0] == '\0') {
        strcpy(nameBuf, "<ERROR>");
    } else {
        strcpy(nameBuf, scriptName);
        char* dot = strchr(nameBuf, '.');
        if (dot) *dot = '\0';
    }

    textObjectAdd(obj, nameBuf, 101, _colorTable[31744], _colorTable[0], &rect);
    tileWindowRefreshRect(&rect, gElevation);
}

static void scr_label_script(Script* scr)
{
    char nameBuf[32];
    if (scriptsGetFileName(scr->index, nameBuf, sizeof(nameBuf)) == 0) {
        scr_label_object(scr->owner, nameBuf);
    } else {
        scr_label_object(scr->owner, "");
    }
}

// 0x4C285C
void scr_debug_print_scripts()
{
    debugPrint("\n\n\t<<< SCRIPT DEBUG DUMP >>>\n");

    textObjectsReset();

    constexpr int kMaxScriptId = 32000;

    // Phase 1: Scripts WITH owners on current elevation — label on owner
    for (int type = 0; type < SCRIPT_TYPE_COUNT; type++) {
        for (int id = 0; id < kMaxScriptId; id++) {
            int sid = (type << 24) | id;
            Script* scr;
            if (scriptGetScript(sid, &scr) != -1 && scr->owner != nullptr) {
                if (scr->owner->elevation == gElevation) {
                    scr_label_script(scr);
                }
            }
        }
    }

    // Phase 2: Scripts WITHOUT owners — find marker object at script's built_tile
    const int kMarkerFid = buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, 0);
    for (int type = 0; type < SCRIPT_TYPE_COUNT; type++) {
        for (int id = 0; id < kMaxScriptId; id++) {
            int sid = (type << 24) | id;
            Script* scr;
            if (scriptGetScript(sid, &scr) != -1 && scr->owner == nullptr) {
                int builtTile = scr->sp.built_tile;
                if (builtTile == -1 || builtTile == 0) continue;

                int tile = builtTileGetTile(builtTile);
                int elevation = builtTileGetElevation(builtTile);
                if (tile == -1 || elevation != gElevation) continue;

                Object* obj = objectFindFirstAtLocation(elevation, tile);
                while (obj != nullptr) {
                    if (obj->fid == kMarkerFid) break;
                    obj = objectFindNextAtLocation();
                }

                if (obj != nullptr) {
                    char nameBuf[32];
                    scriptsGetFileName(scr->index, nameBuf, sizeof(nameBuf));
                    scr_label_object(obj, nameBuf);
                } else {
                    debugPrint("\n\n\t\t\t<ERROR>\n\n");
                }
            }
        }
    }

    debugPrint("\n");
}

} // namespace fallout
