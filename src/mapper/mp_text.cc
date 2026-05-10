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
#include "scripts.h"
#include "settings.h"
#include "text_font.h"
#include "window_manager_private.h"
#include "xfile.h"

namespace fallout {

// Stub implementations for text-format save functions.
// These will be fully implemented in future rounds.
static int scrSaveText(File* stream)
{
    xfilePrintFormatted(stream, "/* Scripts not implemented in text format yet */\n");
    return 0;
}

static int objSaveText(File* stream)
{
    xfilePrintFormatted(stream, "/* Objects not implemented in text format yet */\n");
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
            for (int tile = 0; tile < SQUARE_GRID_SIZE; tile++) {
                int tileData = _square[elev]->field_0[tile];

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
    int scrResult = scrSaveText(stream);
    if (scrResult == -1) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Error saving scripts in %s--TEXT", gMapHeader.name);
        _win_msg(msg, 80, 80, 0);
    }

    // Save objects in text format.
    xfilePrintFormatted(stream, "\n>>>>>>>>>>: OBJECTS <<<<<<<<<<\n\n");
    int objResult = objSaveText(stream);
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
