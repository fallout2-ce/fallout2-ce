#include "mapper/mp_text.h"

#include <string.h>

#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "db.h"
#include "debug.h"
#include "input.h"
#include "kb.h"
#include "map.h"
#include "map_defs.h"
#include "mapper/mapper.h"
#include "mapper/mp_proto.h"
#include "obj_types.h"
#include "object.h"
#include "proto.h"
#include "scripts.h"
#include "settings.h"
#include "text_font.h"
#include "window_manager_private.h"
#include "xfile.h"

namespace fallout {

// Forward declarations for save functions.
static int obj_save_obj_text(File* stream, Object* obj);
// Functions to build path strings for PID/FID (maps to original pid_to_string/fid_to_string)
static const char* pid_to_string(int pid);
static const char* fid_to_string(int fid);

static const char* pid_to_string(int pid)
{
    static char pidStr[COMPAT_MAX_PATH];

    if (_proto_list_str(pid, pidStr) == -1) {
        return "";
    }

    char* dot = strchr(pidStr, '.');
    if (dot != nullptr) {
        *dot = '\0';
    }

    return pidStr;
}

static const char* fid_to_string(int fid)
{
    static char fidStr[COMPAT_MAX_PATH];

    if (art_list_str(fid, fidStr) == -1) {
        return "";
    }

    char* dot = strchr(fidStr, '.');
    if (dot != nullptr) {
        *dot = '\0';
    }

    return fidStr;
}

static int scr_save_text(File* stream)
{
    if (stream == nullptr) {
        return -1;
    }

    for (int scriptType = 0; scriptType < SCRIPT_TYPE_COUNT; scriptType++) {
        ScriptList* scriptList = &gScriptLists[scriptType];

        int scriptCount = scriptList->length * SCRIPT_LIST_EXTENT_SIZE;
        if (scriptList->tail != nullptr) {
            scriptCount += scriptList->tail->length - SCRIPT_LIST_EXTENT_SIZE;
        }

        xfilePrintFormatted(stream, "scr_num: %d\n", scriptCount);

        if (scriptList->head == nullptr) {
            continue;
        }

        ScriptListExtent* scriptExtent = scriptList->head;
        while (scriptExtent != nullptr) {
            xfilePrintFormatted(stream, "\n[[SCRIPT]]\n\n");
            for (int index = 0; index < scriptExtent->length; index++) {
                Script* script = &scriptExtent->scripts[index];

                xfilePrintFormatted(stream, "scr_id: %u\n", script->sid);
                xfilePrintFormatted(stream, "scr_next: %u\n", script->field_4);
                xfilePrintFormatted(stream, "scr_flags: %d\n", script->flags);
                xfilePrintFormatted(stream, "scr_script_idx: %d\n", script->index);
                xfilePrintFormatted(stream, "scr_oid: %u\n", script->ownerId);
                xfilePrintFormatted(stream, "scr_local_var_offset: %u\n", script->localVarsOffset);
                xfilePrintFormatted(stream, "scr_num_local_vars: %u\n\n", script->localVarsCount);

                if (scriptType == SCRIPT_TYPE_SPATIAL) {
                    xfilePrintFormatted(stream, "scr_udata.sp.built_tile: %d\n\n", script->sp.built_tile);
                    xfilePrintFormatted(stream, "scr_udata.sp.radius: %d\n\n", script->sp.radius);
                } else if (scriptType == SCRIPT_TYPE_TIMED) {
                    xfilePrintFormatted(stream, "scr_udata.tm.time: %d\n\n", script->tm.time);
                }
            }
            scriptExtent = scriptExtent->next;
        }
    }

    return 0;
}

static int obj_save_text(File* stream)
{
    if (stream == nullptr) {
        return -1;
    }

    xfilePrintFormatted(stream, "[[OBJECTS BEGIN]]\n\n");

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
            for (ObjectListNode* objectListNode = gObjectListHeadByTile[tile]; objectListNode != nullptr; objectListNode = objectListNode->next) {
                Object* obj = objectListNode->obj;
                if (obj->elevation != elevation) {
                    continue;
                }
                if ((obj->flags & OBJECT_NO_SAVE) == 0) {
                    obj_save_obj_text(stream, obj);
                }
            }
        }
    }

    xfilePrintFormatted(stream, "[[OBJECTS END]]\n");
    return 0;
}

static int obj_save_obj_text(File* stream, Object* obj)
{
    if (stream == nullptr || obj == nullptr) {
        return -1;
    }

    xfilePrintFormatted(stream, "[OBJECT BEGIN]\n");
    xfilePrintFormatted(stream, "obj_id: %d\n", obj->id);
    xfilePrintFormatted(stream, "obj_tile_num: %d\n", obj->tile);
    xfilePrintFormatted(stream, "obj_x: %d\n", obj->x);
    xfilePrintFormatted(stream, "obj_y: %d\n", obj->y);
    xfilePrintFormatted(stream, "obj_sx: %d\n", obj->sx);
    xfilePrintFormatted(stream, "obj_sy: %d\n", obj->sy);
    xfilePrintFormatted(stream, "obj_cur_frm: %u\n", obj->frame);
    xfilePrintFormatted(stream, "obj_cur_rot: %u\n", obj->rotation);
    xfilePrintFormatted(stream, "obj_pid: %u %s\n", obj->pid, pid_to_string(obj->pid));
    xfilePrintFormatted(stream, "obj_fid: %u %s\n", obj->fid, fid_to_string(obj->fid));
    xfilePrintFormatted(stream, "obj_flags: %u\n", obj->flags);
    xfilePrintFormatted(stream, "obj_elev: %d\n", obj->elevation);
    xfilePrintFormatted(stream, "obj_cid: %u\n", obj->cid);
    xfilePrintFormatted(stream, "obj_light_distance: %d\n", obj->lightDistance);
    xfilePrintFormatted(stream, "obj_light_intensity: %d\n", obj->lightIntensity);
    xfilePrintFormatted(stream, "obj_outline: %d\n", obj->outline);
    xfilePrintFormatted(stream, "obj_sid: %u\n", obj->sid);
    xfilePrintFormatted(stream, "obj_pud.inv_size: %d\n", obj->data.inventory.length);
    xfilePrintFormatted(stream, "obj_pud.inv_max: %d\n", obj->data.inventory.capacity);

    int pidType = PID_TYPE(obj->pid);

    if (pidType == OBJ_TYPE_CRITTER) {
        xfilePrintFormatted(stream, "obj_pud.reaction_to_pc: %d\n", obj->data.critter.reaction);
        xfilePrintFormatted(stream, "obj_pud.curr_hp: %d\n", obj->data.critter.hp);
        xfilePrintFormatted(stream, "obj_pud.curr_rad: %d\n", obj->data.critter.radiation);
        xfilePrintFormatted(stream, "obj_pud.curr_poison: %d\n", obj->data.critter.poison);
        xfilePrintFormatted(stream, "obj_pud.combat_data.curr_mp: %d\n", obj->data.critter.combat.ap);
        xfilePrintFormatted(stream, "obj_pud.combat_data.damage_last_turn: %d\n", obj->data.critter.combat.damageLastTurn);
        xfilePrintFormatted(stream, "obj_pud.combat_data.results: %d\n", obj->data.critter.combat.results);
        xfilePrintFormatted(stream, "obj_pud.combat_data.ai_packet: %d\n", obj->data.critter.combat.aiPacket);
        xfilePrintFormatted(stream, "obj_pud.combat_data.team_num: %d\n", obj->data.critter.combat.team);
        if (obj->data.critter.combat.whoHitMe != nullptr) {
            xfilePrintFormatted(stream, "obj_pud.combat_data.who_hit_me: %d\n", obj->data.critter.combat.whoHitMeCid);
        } else {
            xfilePrintFormatted(stream, "obj_pud.combat_data.who_hit_me: -1\n");
        }
    } else {
        xfilePrintFormatted(stream, "obj_pudg.updated_flags: %d\n", obj->data.flags);

        Proto* proto;
        if (protoGetProto(obj->pid, &proto) == -1) {
            return -1;
        }

        // Get subtype from proto type field (same offset for ItemProto and SceneryProto)
        int protoSubtype = static_cast<int>(proto->item.type);

        switch (pidType) {
        case OBJ_TYPE_ITEM:
            switch (protoSubtype) {
            case ITEM_TYPE_WEAPON:
                xfilePrintFormatted(stream, "obj_pudg.pudweapon.cur_ammo_quantity: %d\n", obj->data.item.weapon.ammoQuantity);
                xfilePrintFormatted(stream, "obj_pudg.pudweapon.cur_ammo_type_pid: %d\n", obj->data.item.weapon.ammoTypePid);
                break;
            case ITEM_TYPE_AMMO:
                xfilePrintFormatted(stream, "obj_pudg.pudammo.cur_ammo_quantity: %d\n", obj->data.item.ammo.quantity);
                break;
            case ITEM_TYPE_MISC:
                xfilePrintFormatted(stream, "obj_pudg.pudmisc_item.curr_charges: %d\n", obj->data.item.misc.charges);
                break;
            case ITEM_TYPE_KEY:
                xfilePrintFormatted(stream, "obj_pudg.pudkey_item.cur_key_code: %d\n", obj->data.item.key.keyCode);
                break;
            }
            break;
       case OBJ_TYPE_SCENERY:
            switch (protoSubtype) {
            case SCENERY_TYPE_DOOR:
                xfilePrintFormatted(stream, "obj_pudg.pudportal.cur_open_flags: %d\n", obj->data.scenery.door.openFlags);
                break;
            case SCENERY_TYPE_STAIRS:
                xfilePrintFormatted(stream, "obj_pudg.pudstairs.destMap: %d\n", obj->data.scenery.stairs.destinationMap);
                xfilePrintFormatted(stream, "obj_pudg.pudstairs.destBuiltTile: %d\n", obj->data.scenery.stairs.destinationBuiltTile);
                break;
            case SCENERY_TYPE_ELEVATOR:
                xfilePrintFormatted(stream, "obj_pudg.pudelevator.elevType: %d\n", obj->data.scenery.elevator.type);
                xfilePrintFormatted(stream, "obj_pudg.pudelevator.elevLevel: %d\n", obj->data.scenery.elevator.level);
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }

    if (obj->data.inventory.length == 0) {
        xfilePrintFormatted(stream, "[OBJECT END]\n\n");
        return 0;
    }

    xfilePrintFormatted(stream, "obj_pud.[BEGIN INVEN ITEMS]:\n");

    if (obj->data.inventory.length <= 0) {
        xfilePrintFormatted(stream, "obj_pud.[END INVEN ITEMS]:\n");
        xfilePrintFormatted(stream, "[OBJECT END]\n\n");
        return 0;
    }

    for (int i = 0; i < obj->data.inventory.length; i++) {
        InventoryItem* item = &obj->data.inventory.items[i];
        xfilePrintFormatted(stream, "obj_pud.inven_item: %d\n", i);
        xfilePrintFormatted(stream, "obj_pud.inv.count: %d\n", item->quantity);
        if (obj_save_obj_text(stream, item->item) == -1) {
            return -1;
        }
    }

    xfilePrintFormatted(stream, "obj_pud.[END INVEN ITEMS]:\n");
    xfilePrintFormatted(stream, "[OBJECT END]\n\n");

    return 0;
}

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

void load_all_maps_text(int mode)
{
    // TODO: load all maps from text format. mode==0 → rebuild from text;
    // mode==1 → generate text versions from binary maps.
    (void)mode;
    mapperShowTimedMsg("Loading maps from text not implemented yet!");
}

// map_save_text
void map_save_text()
{
    if (gMapHeader.name[0] == '\0') {
        return;
    }

    char fileName[80];

    // Copy map name and replace extension with ".txt".
    strcpy(fileName, gMapHeader.name);

    char* dot = strchr(fileName, '.');
    if (dot != nullptr) {
        strcpy(dot, ".txt");
    } else {
        strcat(fileName, ".txt");
    }

    scriptsDisable();
    scriptRemove(gMapSid);
    gMapSid = -1;

    // Build path: MAPS\<mapname>.txt
    const char* mapFilePath = mapBuildPath(fileName);

    File* stream = fileOpen(mapFilePath, "wt");
    if (stream == nullptr) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Unable to open %s--TEXT to write!", gMapHeader.name);
        _win_msg(msg, 80, 80, 0);
        scriptsEnable();
        return;
    }

    // Scan elevations to determine which are empty (all floor tiles).
    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int tile;
        for (tile = 0; tile < SQUARE_GRID_SIZE; tile++) {
            int fid = buildFid(OBJ_TYPE_TILE, _square[elevation]->field_0[tile] & 0xFFF, 0, 0, 0);
            if (fid != buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
                break;
            }

            fid = buildFid(OBJ_TYPE_TILE, (_square[elevation]->field_0[tile] >> 16) & 0xFFF, 0, 0, 0);
            if (fid != buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
                break;
            }
        }

        if (tile == SQUARE_GRID_SIZE) {
            Object* object = objectFindFirstAtElevation(elevation);
            if (object != nullptr) {
                gMapHeader.flags &= ~_map_data_elev_flags[elevation];
            } else {
                gMapHeader.flags |= _map_data_elev_flags[elevation];
            }
        } else {
            gMapHeader.flags &= ~_map_data_elev_flags[elevation];
        }
    }

    gMapHeader.localVariablesCount = gMapLocalVarsLength;
    gMapHeader.darkness = 1;

    // Write map header data in text format.
    xfilePrintFormatted(stream, "\n>>>>>>>>>>: MAP_DATA <<<<<<<<<<\n\n");
    xfilePrintFormatted(stream, "map_ver: %d\n", gMapHeader.version);
    xfilePrintFormatted(stream, "map_name: %s\n", gMapHeader.name);
    xfilePrintFormatted(stream, "map_ent_tile: %d\n", gMapHeader.enteringTile);
    xfilePrintFormatted(stream, "map_ent_elev: %d\n", gMapHeader.enteringElevation);
    xfilePrintFormatted(stream, "map_ent_rot: %d\n", gMapHeader.enteringRotation);
    xfilePrintFormatted(stream, "map_num_loc_vars: %d\n", gMapHeader.localVariablesCount);
    xfilePrintFormatted(stream, "map_script_idx: %d\n", gMapHeader.scriptIndex);
    xfilePrintFormatted(stream, "map_flags: %d\n", gMapHeader.flags);
    xfilePrintFormatted(stream, "map_darkness: %d\n", gMapHeader.darkness);
    xfilePrintFormatted(stream, "map_num_glob_vars: %d\n", gMapHeader.globalVariablesCount);
    xfilePrintFormatted(stream, "map_number: %d\n", gMapHeader.index);

    if (gMapHeader.globalVariablesCount > 0) {
        for (int i = 0; i < gMapHeader.globalVariablesCount; i++) {
            xfilePrintFormatted(stream, "map_glob_var: %d %d\n", i, gMapGlobalVars[i]);
        }
    }

    if (gMapHeader.localVariablesCount > 0) {
        for (int i = 0; i < gMapHeader.localVariablesCount; i++) {
            xfilePrintFormatted(stream, "map_loc_var: %d %d\n", i, gMapLocalVars[i]);
        }
    }

    xfileWriteString("\n", stream);
    xfilePrintFormatted(stream, "\n>>>>>>>>>>: MAP_SQUARES <<<<<<<<<<\n\n");

    for (int elev = 0; elev < ELEVATION_COUNT; elev++) {
        if ((gMapHeader.flags & _map_data_elev_flags[elev]) == 0) {
            xfilePrintFormatted(stream, "\nsquare_elev: %d\n\n", elev);
            for (int tileData : _square[elev]->field_0) {
                xfilePrintFormatted(stream, "sq: %u ", tileData);

                // Floor tile art file path.
                int floorFid = buildFid(OBJ_TYPE_TILE, tileData & 0xFFF, 0, 0, 0);
                char* floorPath = artBuildFilePath(floorFid);
                if (floorPath != nullptr) {
                    char* dot = strrchr(floorPath, '.');
                    if (dot != nullptr) {
                        *dot = '\0';
                    }
                    char* lastSlash = strrchr(floorPath, '\\');
                    if (lastSlash != nullptr) {
                        xfilePrintFormatted(stream, "%s ", lastSlash + 1);
                    } else {
                        xfilePrintFormatted(stream, "%s ", floorPath);
                    }
                } else {
                    xfilePrintFormatted(stream, " ");
                }

                // Roof tile art file path.
                int roofFid = buildFid(OBJ_TYPE_TILE, (tileData >> 16) & 0xFFF, 0, 0, 0);
                char* roofPath = artBuildFilePath(roofFid);
                if (roofPath != nullptr) {
                    char* dot = strrchr(roofPath, '.');
                    if (dot != nullptr) {
                        *dot = '\0';
                    }
                    char* lastSlash = strrchr(roofPath, '\\');
                    if (lastSlash != nullptr) {
                        xfilePrintFormatted(stream, "%s\n", lastSlash + 1);
                    } else {
                        xfilePrintFormatted(stream, "%s\n", roofPath);
                    }
                } else {
                    xfilePrintFormatted(stream, "\n");
                }
            }
            xfilePrintFormatted(stream, "\n\n");
        }
    }

    // Save scripts in text format.
    xfilePrintFormatted(stream, "\n>>>>>>>>>>: SCRIPTS <<<<<<<<<<\n\n");
    xfilePrintFormatted(stream, "\nSCRS:\n");
    int scrResult = scr_save_text(stream);
    if (scrResult == -1) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Error saving scripts in %s--TEXT", gMapHeader.name);
        _win_msg(msg, 80, 80, 0);
    }

    // Save objects in text format.
    xfilePrintFormatted(stream, "\n>>>>>>>>>>: OBJECTS <<<<<<<<<<\n\n");
    int objResult = obj_save_text(stream);
    if (objResult == -1) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Error saving objects in %s--TEXT", gMapHeader.name);
        _win_msg(msg, 80, 80, 0);
    }

    fileClose(stream);

    char msg[80];
    snprintf(msg, sizeof(msg), "%s--TEXT saved.", gMapHeader.name);
    win_timed_msg(msg, _colorTable[31744] | FONT_SHADOW);
    debugPrint("\n%s--TEXT saved.", gMapHeader.name);

    scriptsEnable();
}

} // namespace fallout
