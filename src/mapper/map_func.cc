#include "mapper/map_func.h"

#include "actions.h"
#include "art.h"
#include "color.h"
#include "db.h"
#include "debug.h"
#include "game.h"
#include "game_mouse.h"
#include "graph_lib.h"
#include "input.h"
#include "kb.h"
#include "map.h"
#include "mapper/mapper.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "proto.h"
#include "proto_instance.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

// 0x5595CC
static bool block_obj_view_on = false;

static int fidShowList[9] = { 0 };
static int blockedFidCache[9] = { 0 };

constexpr int kBlockViewArtType[9] = { 2, 2, 2, 2, 3, 3, 5, 5, 2 };
constexpr const char* kBlockViewListName[9] = {
    "block2", "block3", "block4", "block5",
    "block2", "block3", "scrblk", "trnblk", "blkexit"
};
constexpr int kBlockingProtoList[9] = {
    0x02000043, 0x02000080, 0x0200008D, 0x02000158,
    0x0300026D, 0x0300026E, 0x0500000C, 0x05000005,
    0x02000031
};

// 0x4825B0
void setup_map_dirs()
{
    // TODO: Incomplete.
}

// 0x4826B4
void copy_proto_lists()
{
    // TODO: Incomplete.
}

// 0x482708
void place_entrance_hex()
{
    int x;
    int y;
    int tile;

    while (inputGetInput() != -2) {
    }

    if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
        if (_mouse_click_in(0, 0, _scr_size.right - _scr_size.left, _scr_size.bottom - _scr_size.top - 100)) {
            mouseGetPosition(&x, &y);

            tile = tileFromScreenXY(x, y);
            if (tile != -1) {
                if (tileSetCenter(tile, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) == 0) {
                    mapSetEnteringLocation(gElevation, tile, rotation);
                } else {
                    win_timed_msg("ERROR: Entrance out of range!", _colorTable[31744]);
                }
            }
        }
    }
}

// 0x4841C4
void pick_region(Rect* rect)
{
    Rect temp;
    int x;
    int y;

    gameMouseSetCursor(MOUSE_CURSOR_PLUS);
    gameMouseObjectsHide();

    while (1) {
        if (inputGetInput() == -2
            && (mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
            break;
        }
    }

    get_input_position(&x, &y);
    temp.left = x;
    temp.top = y;
    temp.right = x;
    temp.bottom = y;

    while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
        inputGetInput();

        get_input_position(&x, &y);

        if (x != temp.right || y != temp.bottom) {
            erase_rect(rect);
            sort_rect(rect, &temp);
            draw_rect(rect, _colorTable[32747]);
        }
    }

    erase_rect(rect);
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    gameMouseObjectsShow();
}

// 0x484294
void sort_rect(Rect* a, Rect* b)
{
    if (b->right > b->left) {
        a->left = b->left;
        a->right = b->right;
    } else {
        a->left = b->right;
        a->right = b->left;
    }

    if (b->bottom > b->top) {
        a->top = b->top;
        a->bottom = b->bottom;
    } else {
        a->top = b->bottom;
        a->bottom = b->top;
    }
}

// 0x4842D4
void draw_rect(Rect* rect, unsigned char color)
{
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    int max_dimension;

    if (height < width) {
        max_dimension = width;
    } else {
        max_dimension = height;
    }

    unsigned char* buffer = (unsigned char*)internal_malloc(max_dimension);
    if (buffer != NULL) {
        memset(buffer, color, max_dimension);
        _scr_blit(buffer, width, 1, 0, 0, width, 1, rect->left, rect->top);
        _scr_blit(buffer, 1, height, 0, 0, 1, height, rect->left, rect->top);
        _scr_blit(buffer, width, 1, 0, 0, width, 1, rect->left, rect->bottom);
        _scr_blit(buffer, 1, height, 0, 0, 1, height, rect->right, rect->top);
        internal_free(buffer);
    }
}

// 0x4843A0
void erase_rect(Rect* rect)
{
    Rect r = *rect;

    r.bottom = rect->top;
    windowRefreshAll(&r);

    r.bottom = rect->bottom;
    r.left = rect->right;
    windowRefreshAll(&r);

    r.left = rect->left;
    r.top = rect->bottom;
    windowRefreshAll(&r);

    r.top = rect->top;
    r.right = rect->left;
    windowRefreshAll(&r);
}

// 0x484400
int toolbar_proto(int type, int id)
{
    if (id < proto_max_id(type)) {
        return (type << 24) | id;
    } else {
        return -1;
    }
}

// 0x485D44
bool map_toggle_block_obj_viewing_on()
{
    return block_obj_view_on;
}

void map_toggle_block_obj_viewing(int mode)
{
    if (mode == 0 && !block_obj_view_on) return;
    if (mode == 1 && block_obj_view_on) return;

    if (!block_obj_view_on && fidShowList[0] == 0) {
        for (int i = 0; i < 9; i++) {
            int fid = artListIndex(kBlockViewArtType[i], kBlockViewListName[i]);
            if (fid == -1) {
                debugPrint("\nError: art_list_index failed in toggle_obj_view");
            } else {
                fidShowList[i] = buildFid(kBlockViewArtType[i], fid, 0, 0, 0);
            }
        }
    }

    for (Object* obj = objectFindFirstAtElevation(gElevation); obj != nullptr; obj = objectFindNextAtElevation()) {
        if (obj == gDude || obj == gGameMouseBouncingCursor || (obj->flags & OBJECT_NO_SAVE)) continue;

        for (int index = 0; index < 9; index++) {
            if (kBlockingProtoList[index] != obj->pid) continue;

            if (block_obj_view_on) {
                if (blockedFidCache[index] != 0) {
                    obj->fid = blockedFidCache[index];
                }
            } else {
                if (blockedFidCache[index] == 0) {
                    Proto* proto;
                    if (protoGetProto(obj->pid, &proto) == 0) {
                        blockedFidCache[index] = proto->fid;
                    }
                }
                if (fidShowList[index] != 0) {
                    obj->fid = fidShowList[index];
                }
            }
        }
    }

    block_obj_view_on = !block_obj_view_on;
    tileWindowRefresh();
}

// =========================================================================
// P2 stubs — to be fully implemented from mapper2.asm
// =========================================================================

void map_load_dialog()
{
    char** fileList;
    int count = fileNameListInit("maps\\*.map", &fileList);
    if (count == -1) {
        win_timed_msg("No maps found!", _colorTable[31744]);
        return;
    }

    int index = _win_list_select_at("Select a map to load:", fileList, count, nullptr, 80, 80, 0x10104, 0);
    if (index != -1) {
        mapLoadByName(fileList[index]);
    }

    fileNameListFree(&fileList, count);
}

void map_save_dialog()
{
    if (gMapHeader.name[0] == '\0') {
        map_save_as();
        return;
    }

    if (win_yes_no("Save map?", 80, 80, 0x10104)) {
        _map_save();
    }
}

void map_save_as()
{
    char newName[16] = { 0 };
    int rc = _win_get_str(newName, 8, "Save file (no extension):", 80, 80);
    if (rc == -1) return;

    compat_strupr(newName);
    if (strstr(newName, ".MAP") == nullptr) {
        strcat(newName, ".MAP");
    }

    strncpy(gMapHeader.name, newName, sizeof(gMapHeader.name) - 1);
    gMapHeader.name[sizeof(gMapHeader.name) - 1] = '\0';

    _map_save();
}

void map_info_dialog()
{
    char info[256];
    snprintf(info, sizeof(info), "Map: %s\nElevation: %d\nIndex: %d\nFlags: 0x%x\nScript: %d",
        gMapHeader.name, gElevation, gMapHeader.index, gMapHeader.flags, gMapHeader.scriptIndex);
    _win_msg(info, 80, 80, 0x10104);
}

void map_clear_elevation()
{
    Object* obj = objectFindFirstAtElevation(gElevation);
    while (obj != nullptr) {
        if (obj != gDude) {
            objectDestroy(obj);
            tileWindowRefresh();
            obj = objectFindFirstAtElevation(gElevation);
            continue;
        }
        obj = objectFindNextAtElevation();
    }
}

void create_spray_tool()
{
    // TODO: create spray paint pattern tool
}

void copy_spray_tile()
{
    // TODO: copy spray tool tile pattern
}

void mapper_shift_map()
{
    // TODO: shift all map objects by an offset (needs tile coordinate system knowledge)
    win_timed_msg("Shift Map not yet implemented", _colorTable[31744]);
}

void mapper_shift_map_elev()
{
    // TODO: shift objects on one elevation by an offset
    win_timed_msg("Shift Map Elev not yet implemented", _colorTable[31744]);
}

void mapper_copy_map_elev()
{
    // TODO: copy objects from current elevation to another via user prompt
    win_timed_msg("Copy Map Elev not yet implemented", _colorTable[31744]);
}

void mapper_flush_cache()
{
    // TODO: flush all caches (art, sound, etc.)
}

// pick_hex
int pickHex()
{
    constexpr int kIsoMarginTop = 16;

    int elevation = gElevation;

    while (true) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        if (keyCode == -2) {
            int buttons = mouseGetEvent();
            if (buttons & MOUSE_EVENT_LEFT_BUTTON_DOWN) {
                int isoRight = windowGetWidth(gIsoWindow) - 1;
                int isoBottom = windowGetHeight(gIsoWindow) - 1;
                if (_mouse_click_in(0, kIsoMarginTop, isoRight, isoBottom)) {
                    int x, y;
                    mouseGetPosition(&x, &y);
                    int tile = tileFromScreenXY(x, y);
                    if (tile != -1) {
                        return tile;
                    }
                }
            }
        }

        if (keyCode == KEY_CTRL_ARROW_RIGHT || keyCode == KEY_CTRL_ARROW_LEFT) {
            rotation = (keyCode == KEY_CTRL_ARROW_RIGHT)
                ? (rotation + 1) % ROTATION_COUNT
                : (rotation + ROTATION_COUNT - 1) % ROTATION_COUNT;
            Rect rect;
            objectSetRotation(gGameMouseBouncingCursor, rotation, &rect);
            tileWindowRefreshRect(&rect, elevation);
        }

        if (keyCode == KEY_PAGE_UP || keyCode == KEY_PAGE_DOWN) {
            int newElevation = elevation;
            if (keyCode == KEY_PAGE_UP) {
                if (elevation < ELEVATION_COUNT - 1) {
                    newElevation = elevation + 1;
                }
            } else {
                if (elevation > 0) {
                    newElevation = elevation - 1;
                }
            }
            if (newElevation != elevation) {
                elevation = newElevation;
                mapSetElevation(elevation);
                tileWindowRefresh();
                // Refresh elevation number on toolbar
                int pitch = rectGetWidth(&_scr_size);
                blitBufferToBuffer(e_num[elevation], 19, 26, 19, tool_buf + 62 * pitch + 30, pitch);
                Rect numRect = { 30, 62, 50, 88 };
                windowRefreshRect(tool_win, &numRect);
            }
        }

        if (keyCode == KEY_ESCAPE) {
            return -1;
        }

        if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            return -1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

//
int pickToolbar(int topY)
{
    // TODO: show toolbar type picker dialog at given Y position, return selected type index
    (void)topY;
    return -1;
}

// place_object_
void placeObject(int pid, int fid)
{
    int x, y;
    mouseGetPosition(&x, &y);
    int tile = tileFromScreenXY(x, y);
    if (tile == -1) {
        return;
    }

    Object* obj;
    if (objectCreateWithFidPid(&obj, fid, pid) == -1) {
        return;
    }

    Rect rect;
    objectSetLocation(obj, tile, gElevation, &rect);
    tileWindowRefreshRect(&rect, gElevation);
}

// place_tile_
void placeTile(int pid, int fid)
{
    int x, y;
    mouseGetPosition(&x, &y);
    int squareTile = squareTileFromScreenXY(x, y, gElevation);
    if (squareTile == -1) {
        return;
    }

    int newArt = fid & 0xFFF;
    int* squarePtr = &_square[gElevation]->field_0[squareTile];
    int oldValue = *squarePtr;

    if (tileRoofIsVisible()) {
        int oldRoofFid = buildFid(OBJ_TYPE_TILE, (oldValue >> 16) & 0xFFF, 0, 0, 0);
        if (oldRoofFid == fid) {
            return;
        }

        int oldRoofWord = oldValue >> 16;
        int roofRotation = (oldRoofWord & 0xF000) >> 12;
        int newRoofWord = roofRotation | newArt;
        *squarePtr = (newRoofWord << 16) | (oldValue & 0xFFFF);

        int sx, sy;
        squareTileToRoofScreenXY(squareTile, &sx, &sy, gElevation);
        Rect rect = { sx, sy, sx + 80, sy + 36 };
        tileWindowRefreshRect(&rect, gElevation);
    } else {
        int oldFloorFid = buildFid(OBJ_TYPE_TILE, oldValue & 0xFFF, 0, 0, 0);
        if (oldFloorFid == fid) {
            return;
        }

        int oldFloorWord = oldValue & 0xFFFF;
        int floorRotation = (oldFloorWord & 0xF000) >> 12;
        int newFloorWord = floorRotation | newArt;
        *squarePtr = (oldValue & 0xFFFF0000) | newFloorWord;

        int sx, sy;
        squareTileToScreenXY(squareTile, &sx, &sy, gElevation);
        Rect rect = { sx, sy, sx + 80, sy + 36 };
        tileWindowRefreshRect(&rect, gElevation);
    }
}

void copyObject()
{
    // TODO: copy the selected object (shareFpsLimiter.screen_obj) to internal clipboard
}

void copyTile()
{
    // TODO: copy the selected tile to internal clipboard
}

void eraseObject()
{
    // TODO: destroy the object under the mouse cursor (gGameMouseBouncingCursor->tile)
}

} // namespace fallout
