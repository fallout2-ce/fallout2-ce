#include "mapper/map_func.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "color.h"
#include "db.h"
#include "debug.h"
#include "game.h"
#include "game_mouse.h"
#include "input.h"
#include "kb.h"
#include "map.h"
#include "mapper/mapper.h"
#include "mapper/mp_utils.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "proto.h"
#include "proto_instance.h"
#include "scripts.h"
#include "settings.h"
#include "svga.h"
#include "text_font.h"
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
                    win_timed_msg("ERROR: Entrance out of range!", COLOR_LIGHT_YELLOW | DRAW_TEXT_FLAG_SHADOWED);
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

    while (true) {
        sharedFpsLimiter.mark();
        bool got = inputGetInput() == -2
            && (mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0;
        renderPresent();
        sharedFpsLimiter.throttle();
        if (got) break;
    }

    get_input_position(&x, &y);
    temp.left = x;
    temp.top = y;
    temp.right = x;
    temp.bottom = y;

    while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
        sharedFpsLimiter.mark();
        inputGetInput();

        get_input_position(&x, &y);

        if (x != temp.right || y != temp.bottom) {
            erase_rect(rect);
            sort_rect(rect, &temp);
            draw_rect(rect, COLOR_LIGHT_YELLOW);
        }
        renderPresent();
        sharedFpsLimiter.throttle();
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

void map_load_dialog()
{
    char** fileList;
    int count = fileNameListInit("maps\\*.map", &fileList);
    if (count == -1) {
        win_timed_msg("No maps found!", COLOR_LIGHT_YELLOW | DRAW_TEXT_FLAG_SHADOWED);
        return;
    }

    int index = _win_list_select_at("Select a map to load:", fileList, count, nullptr, 80, 80, 0x10104, 0);
    if (index != -1) {
        mapLoadByName(fileList[index]);
    }

    fileNameListFree(&fileList, count);
}

// map_save_dialog
void map_save_dialog()
{
    char newName[16] = {};
    if (_win_get_str(newName, 8, "Save file (no extension):", 80, 80) != 0) return;
    strcat(newName, ".map");
    strncpy(gMapHeader.name, newName, sizeof(gMapHeader.name) - 1);
    gMapHeader.name[sizeof(gMapHeader.name) - 1] = '\0';
    _map_save();
}

// map_save_as
int map_save_as(const char* name)
{
    strncpy(gMapHeader.name, name, sizeof(gMapHeader.name) - 1);
    gMapHeader.name[sizeof(gMapHeader.name) - 1] = '\0';
    return _map_save();
}

// map_get_name
void map_get_name(char* buf)
{
    strncpy(buf, gMapHeader.name, sizeof(gMapHeader.name));
    buf[sizeof(gMapHeader.name) - 1] = '\0';
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

static void showNotImplementedSprayToolMsg()
{
    mapperShowTimedMsg("Spray-tile painting not implemented in CE.");
}

// copy_spray_tile
// TODO: porting this requires the spray-tool subsystem (pick_spray_tool / load_spray_tool /
// draw_spray_to_win / get_max_spray_tool / `spinfo` global / `map_brush_size` global / a sizable
// SprayTool data struct + .spr file I/O). Pending user direction before pulling that in.
void copy_spray_tile()
{
    showNotImplementedSprayToolMsg();
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
// pick_toolbar
int pickToolbar(int topY)
{
    constexpr int kTypeCount = 6;
    char names[kTypeCount][20];
    char* items[kTypeCount];

    for (int i = 0; i < kTypeCount; i++) {
        snprintf(names[i], sizeof(names[i]), " %s", artGetObjectTypeName(i));
        // Original mapper highlights the type's first letter as a hotkey by uppercasing it.
        names[i][1] = toupper(names[i][1]);
        items[i] = names[i];
    }

    return _win_pull_down(items, kTypeCount, 0, topY, COLOR_LIGHT_YELLOW | DRAW_TEXT_FLAG_SHADOWED);
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

// mpMouseCheckScrolling
//
// If the mouse cursor is at the iso window's left/right/top/bottom edge, scroll the map by one
// hex step in the corresponding direction. The (dx, dy) values fed to mapScroll mirror the
// edge-mask switch in the original (left=-1, right=+1; top/bottom alone don't pan; corner cases
// blend the two).
static void mpMouseCheckScrolling(int x, int y)
{
    int left = _scr_size.left;
    int top = _scr_size.top;
    int right = _scr_size.right;
    int bottom = _scr_size.bottom;

    int mask = 0;
    if (x <= left) mask |= 1;
    if (x >= right) mask |= 2;
    if (y <= top) mask |= 4;
    if (y >= bottom) mask |= 8;

    int dx = 0;
    int dy = 0;
    switch (mask) {
    case 1:
        dx = -1;
        break;
    case 2:
        dx = 1;
        break;
    case 5:
        dx = -1;
        break; // top-left
    case 6:
        dx = 1;
        break; // top-right
    case 9:
        dx = -1;
        dy = 1;
        break; // bottom-left
    case 10:
        dx = 1;
        break; // bottom-right
    default:
        return;
    }
    if (dx != 0 || dy != 0) {
        mapScroll(dx, dy);
    }
}

// Copy buffer for copy_object_to_tile_pobj / copyObject. mpCopyEntry mirrors the original's
// interleaved {fid, pid, dx, dy} record; mpCopyList2 holds the source object pointer.
struct CopyEntry {
    int fid;
    int pid;
    int dx;
    int dy;
};
constexpr int kMaxCopyEntries = 400;
static CopyEntry mpCopyList[kMaxCopyEntries];
static Object* mpCopyList2[kMaxCopyEntries];
static int mpCopyCount = 0;

// Scaffold shared by copyObject's and copyTile's placement loops. `place(ix, iy)` is invoked
// on each in-bounds left-click with the anchor screen coordinates (already corrected to the
// hex tile center for object placement; raw click position for tile placement). The helper
// owns the gmouse disable/enable bracket, the FPS limiter pacing, edge-scroll handling, and
// the keyboard scroll / Home / Esc handling. Right-click and Esc end the loop.
template <typename PlaceFn>
static void mp_run_placement_loop(PlaceFn place)
{
    _gmouse_disable(0);

    while (true) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        int mx, my;
        mouseGetPosition(&mx, &my);
        mpMouseCheckScrolling(mx, my);

        bool exit = false;

        if (keyCode == -2) {
            int mouseEvent = mouseGetEvent();
            if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                int ix, iy;
                get_input_position(&ix, &iy);
                if (_mouse_click_in(0, 0,
                        _scr_size.right - _scr_size.left,
                        _scr_size.bottom - _scr_size.top - 100)) {
                    place(ix, iy);
                    tileWindowRefresh();
                }
            } else if ((mouseEvent & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                exit = true;
            }
        }

        if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            exit = true;
        }

        switch (keyCode) {
        case KEY_HOME: // Home
            if (gDude != nullptr) {
                if (gElevation != gDude->elevation) {
                    mapSetElevation(gDude->elevation);
                }
                tileSetCenter(gDude->tile, TILE_SET_CENTER_REFRESH_WINDOW);
            }
            break;
        case KEY_ARROW_UP:
        case KEY_ARROW_DOWN:
            mapScroll(0, 0);
            break;
        case KEY_ARROW_LEFT:
            mapScroll(-1, 0);
            break;
        case KEY_ARROW_RIGHT:
            mapScroll(1, 0);
            break;
        case KEY_ESCAPE:
            exit = true;
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();

        if (exit) break;
    }

    _gmouse_enable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
}

// copy_object_to_tile_pobj
//
// Place a single source object at `dstTile` on the current elevation. The placement is only
// attempted when one of the original "permissive" gates is true: art-only mode is enabled, the
// destination has no blocker, srcObj is missing or is a tile (different placement model), the
// proto fetch fails, or the proto carries the "always allow" flag (0x10). When none of these
// hold, an existing blocker prevents placement and the function returns without modifying
// anything (matching the original mapper).
//
// When the gate passes, look for a duplicate (same tile + same fid). If one exists, the
// function does NOT place a second copy; for stackable items (PID 41 in F2) it bumps the
// existing instance's charge count instead. Otherwise it deep-copies srcObj, places the copy,
// rewires script ownership, sets rotation for critter/misc art, and optionally refreshes the
// affected screen rect.
static void copy_object_to_tile_pobj(int srcFid, int dstTile, Object* srcObj, bool refresh)
{
    gGameMouseBouncingCursor->flags |= OBJECT_HIDDEN;
    Object* existing = _obj_blocking_at(nullptr, dstTile, gElevation);
    gGameMouseBouncingCursor->flags &= ~OBJECT_HIDDEN;

    bool useArtNotProtos = settings.mapper.use_art_not_protos;

    Proto* proto = nullptr;
    bool gatePassed = useArtNotProtos
        || existing == nullptr
        || srcObj == nullptr
        || PID_TYPE(srcObj->pid) == OBJ_TYPE_TILE
        || protoGetProto(srcObj->pid, &proto) == -1
        || (proto != nullptr && (proto->flags & 0x10) != 0);

    if (!gatePassed) {
        return;
    }

    // Look for an existing duplicate (same tile + same fid) before placing.
    for (Object* obj = objectFindFirstAtElevation(gElevation);
         obj != nullptr;
         obj = objectFindNextAtElevation()) {
        if (obj->tile == dstTile
            && obj->fid == srcFid
            && obj != gGameMouseBouncingCursor
            && obj != gGameMouseHexCursor) {
            existing = obj;
            break;
        }
    }

    if (existing != nullptr) {
        // Stackable item (PID 41 in F2 == bottle caps) auto-merges by bumping count.
        if (existing->pid == 41) {
            existing->data.item.misc.charges++;
        }
        return;
    }

    if (srcObj == nullptr) {
        return;
    }

    // Choose the FID to write — block-view mode swaps proto-blocking FIDs with their highlighted
    // counterparts (kBlockingProtoList → fidShowList).
    int newFid = srcFid;
    if (block_obj_view_on) {
        for (int i = 0; i < 9; i++) {
            if (kBlockingProtoList[i] == srcObj->pid) {
                newFid = fidShowList[i];
                break;
            }
        }
    }

    Object* copy = nullptr;
    if (_obj_copy(&copy, srcObj) == -1) {
        debugPrint("ERROR: Error copying object in copy_object_to_tile_pobj!\n");
        return;
    }

    Rect rect;
    objectSetLocation(copy, dstTile, gElevation, &rect);

    if (!useArtNotProtos) {
        int sid = -1;
        objectGetSid(copy, &sid);
        if (sid != -1) {
            Script* script = nullptr;
            if (scriptGetScript(sid, &script) != -1) {
                if (SID_TYPE(sid) == 1 /* spatial */) {
                    script->sp.built_tile = (gElevation << 29) | dstTile;
                }
                script->owner = copy;
            }
        }
    }

    int newType = FID_TYPE(newFid);
    if (newType == OBJ_TYPE_CRITTER || newType == OBJ_TYPE_MISC) {
        objectSetRotation(copy, rotation, nullptr);
    }

    if (refresh) {
        tileWindowRefreshRect(&rect, copy->elevation);
    }
}

// copy_object
//
// Pick a rectangular region; gather every object inside it (optionally filtered by type) into a
// per-mouse-position offset table; then enter "place" mode where each left-click drops a copy of
// the captured set at the new mouse position. Right-click / Esc exits.
void copyObject(int filterType)
{
    Rect region;
    pick_region(&region);

    // Resolve the centering tile for the region.
    int centerTile = tileFromScreenXY(region.left, region.top);
    if (centerTile != -1) {
        tileToScreenXY(centerTile, &region.left, &region.top);
    }

    mpCopyCount = 0;

    int elevation = gElevation;
    // Walk the region cell-by-cell and add matching objects. Outer loop skips by 12 vertically
    // and 32 horizontally to match the hex stride used by the original (one unit per visual hex).
    for (int sy = region.top + 8; sy < region.bottom; sy += 12) {
        for (int sx = region.left + 16; sx < region.right; sx += 32) {
            int tile = tileFromScreenXY(sx, sy);
            if (tile == -1) continue;

            for (Object* obj = objectFindFirstAtElevation(elevation);
                 obj != nullptr;
                 obj = objectFindNextAtElevation()) {
                if (obj->tile != tile
                    || obj == gDude
                    || obj == gGameMouseBouncingCursor
                    || obj == gGameMouseHexCursor
                    || (obj->flags & OBJECT_HIDDEN) != 0
                    || filterType != -1 && FID_TYPE(obj->fid) != filterType) continue;

                if (mpCopyCount >= kMaxCopyEntries) {
                    _win_msg("Too many objects in region!", 80, 80, COLOR_RED);
                    return;
                }

                mpCopyList[mpCopyCount].fid = obj->fid;
                mpCopyList[mpCopyCount].pid = obj->pid;
                mpCopyList[mpCopyCount].dx = sx - region.left;
                mpCopyList[mpCopyCount].dy = sy - region.top;
                mpCopyList2[mpCopyCount] = obj;
                mpCopyCount++;
            }
        }
    }

    if (mpCopyCount == 0) {
        _win_msg("Nothing selected.", 80, 80, COLOR_RED);
        return;
    }

    mp_run_placement_loop([](int ix, int iy) {
        // Snap to the picked hex tile so per-object offsets stamp from a consistent anchor.
        int anchorTile = tileFromScreenXY(ix, iy);
        if (anchorTile != -1) {
            tileToScreenXY(anchorTile, &ix, &iy);
        }
        for (int i = 0; i < mpCopyCount; i++) {
            int dstTile = tileFromScreenXY(ix + mpCopyList[i].dx, iy + mpCopyList[i].dy);
            if (dstTile != -1) {
                copy_object_to_tile_pobj(mpCopyList[i].fid, dstTile, mpCopyList2[i], false);
            }
        }
    });
}

// squares_in_rect
//
// Fills `outTiles` with up to `cap` square-tile indices that lie fully inside `screenRect` on
// elevation `elevation`. Returns the number of tiles written. Hex-grid screen-coords aren't
// monotonic with square-grid coords (the iso projection is angled), so the bounding box is
// computed by taking min/max of all four screen-rect corners.
static int squares_in_rect(const Rect* screenRect, int elevation, int* outTiles, int cap)
{
    if (cap <= 0) return 0;

    int xs[4], ys[4];
    squareTileScreenToCoord(screenRect->left, screenRect->top, elevation, &xs[0], &ys[0]);
    squareTileScreenToCoord(screenRect->right, screenRect->top, elevation, &xs[1], &ys[1]);
    squareTileScreenToCoord(screenRect->left, screenRect->bottom, elevation, &xs[2], &ys[2]);
    squareTileScreenToCoord(screenRect->right, screenRect->bottom, elevation, &xs[3], &ys[3]);

    int sx0 = xs[0], sx1 = xs[0];
    int sy0 = ys[0], sy1 = ys[0];
    for (int i = 1; i < 4; i++) {
        if (xs[i] < sx0) sx0 = xs[i];
        if (xs[i] > sx1) sx1 = xs[i];
        if (ys[i] < sy0) sy0 = ys[i];
        if (ys[i] > sy1) sy1 = ys[i];
    }

    if (sx0 < 0) sx0 = 0;
    if (sx1 >= SQUARE_GRID_WIDTH) sx1 = SQUARE_GRID_WIDTH - 1;
    if (sy0 < 0) sy0 = 0;
    if (sy1 >= SQUARE_GRID_HEIGHT) sy1 = SQUARE_GRID_HEIGHT - 1;

    int count = 0;
    for (int sy = sy0; sy <= sy1; sy++) {
        for (int sx = sx0; sx <= sx1; sx++) {
            int tileIndex = sy * SQUARE_GRID_WIDTH + sx;
            int tx, ty;
            squareTileToScreenXY(tileIndex, &tx, &ty, elevation);

            Rect tileRect = { tx, ty, tx + 79, ty + 35 };
            Rect inter;
            if (rectIntersection(&tileRect, screenRect, &inter) == 0
                && inter.left == tileRect.left
                && inter.top == tileRect.top
                && inter.right == tileRect.right
                && inter.bottom == tileRect.bottom) {
                outTiles[count++] = tileIndex;
                if (count == cap) return count;
            }
        }
    }
    return count;
}

// copy_tile
//
// Pick a rectangular screen region, capture every fully-contained square tile's floor-art FID,
// then enter placement mode where each left-click stamps the captured pattern at the cursor
// position. Right-click / Esc exits.
void copyTile()
{
    constexpr int kMaxTiles = 60;

    Rect region;
    pick_region(&region);

    int srcTiles[kMaxTiles];
    int srcCount = squares_in_rect(&region, gElevation, srcTiles, kMaxTiles);
    if (srcCount == 0) {
        win_timed_msg("No tiles in area.", COLOR_RED | DRAW_TEXT_FLAG_SHADOWED);
        return;
    }
    if (srcCount == kMaxTiles) {
        win_timed_msg("Too many tiles in region!", COLOR_RED | DRAW_TEXT_FLAG_SHADOWED);
        return;
    }

    int srcFid[kMaxTiles];
    int srcDx[kMaxTiles];
    int srcDy[kMaxTiles];
    for (int i = 0; i < srcCount; i++) {
        int floorArt = _square[gElevation]->field_0[srcTiles[i]] & 0xFFF;
        srcFid[i] = buildFid(OBJ_TYPE_TILE, floorArt, 0, 0, 0);

        int sx, sy;
        squareTileToScreenXY(srcTiles[i], &sx, &sy, gElevation);
        srcDx[i] = sx - region.left;
        srcDy[i] = sy - region.top;
    }

    int blankFid = buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0);

    mp_run_placement_loop([&](int ix, int iy) {
        for (int i = 0; i < srcCount; i++) {
            if (srcFid[i] == blankFid) continue;
            int dstSx = ix + srcDx[i];
            int dstSy = iy + srcDy[i] + 12;
            int dstSquare = squareTileFromScreenXY(dstSx, dstSy, gElevation);
            if (dstSquare != -1) {
                int* word = &_square[gElevation]->field_0[dstSquare];
                int rotBits = (*word & 0xF000) >> 12;
                int newFloor = (srcFid[i] & 0xFFF) | rotBits;
                *word = (*word & 0xFFFF0000) | (newFloor & 0xFFFF);
            }
        }
    });
}

// erase_object
void eraseObject()
{
    gameMouseSetCursor(MOUSE_CURSOR_DESTROY);
    gameMouseObjectsHide();

    while (true) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        bool exit = false;

        if (keyCode == -2) {
            int mouseEvent = mouseGetEvent();
            if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if (!_mouse_click_in(0, 0,
                        _scr_size.right - _scr_size.left,
                        _scr_size.bottom - _scr_size.top - 100)) {
                    exit = true;
                } else {
                    // Find topmost (last-rendered) object at gElevation under the mouse.
                    Object* hit = nullptr;
                    for (Object* obj = objectFindFirstAtElevation(gElevation);
                         obj != nullptr;
                         obj = objectFindNextAtElevation()) {
                        if (obj == gDude) continue;
                        if (PID_TYPE(obj->pid) == OBJ_TYPE_INTERFACE) continue;
                        if ((obj->flags & OBJECT_HIDDEN) != 0) continue;
                        if (obj == gGameMouseBouncingCursor) continue;
                        if (obj == gGameMouseHexCursor) continue;
                        // Original mapper also tested `obj->flags & 0x40` here; that flag is
                        // unused in CE.

                        Rect rect;
                        objectGetRect(obj, &rect);
                        if (_mouse_click_in(rect.left, rect.top, rect.right, rect.bottom)) {
                            hit = obj;
                        }
                    }

                    if (hit != nullptr) {
                        // Don't destroy exit-grid markers (interface art, id=3).
                        int exitGridFid = buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, 0);
                        if (hit->fid != exitGridFid) {
                            Rect rect;
                            int elev = hit->elevation;
                            reg_anim_clear(hit);
                            objectDestroy(hit, &rect);
                            tileWindowRefreshRect(&rect, elev);
                        }
                    }
                }
            } else if ((mouseEvent & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                exit = true;
            }
        }

        if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            exit = true;
        }

        renderPresent();
        sharedFpsLimiter.throttle();

        if (exit) break;
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    gameMouseObjectsShow();
}

// mapper_flush_cache
void mapper_flush_cache()
{
    artCacheFlush();
}

// create_spray_tool
// Original implementation just returns -1 (the spray-tool authoring path was disabled in the
// shipped mapper). We keep the same disabled behavior.
void create_spray_tool()
{
}

// File-scope constants used by mapper_shift_map_once and its helpers (file-scope so the
// inner lambdas don't need to capture them — MSVC won't accept implicit capture of locals
// with `[]` even if they're constexpr).
static constexpr int kShiftMapWidth = 200;
static constexpr int kShiftMapHeight = 200;
static constexpr int kShiftMapTiles = kShiftMapWidth * kShiftMapHeight; // 40000

// mapper_shift_map_once
//
// Translates every shiftable object on the current elevation by (dx*2, dy*1) tile-units (the 2x
// scaling on dx mirrors the original — F2's hex grid alternates rows so a "one-column" user
// shift moves through two underlying hex columns). Objects shifted off the map are destroyed;
// floor/roof tile data shifts in lock-step. Spatial scripts are translated using the same
// (dx*2, dy*200) formula and clamped to [1, 39998].
static void mapper_shift_map_once(int dx, int dy)
{
    constexpr int kSqWidth = SQUARE_GRID_WIDTH; // 100
    constexpr int kSqHeight = SQUARE_GRID_HEIGHT; // 100

    // Snapshot the object list and original tile of each shiftable object before mutating
    // anything (objectSetLocation / objectDestroy can renumber the linked list).
    struct Snap {
        Object* obj;
        int tile;
    };
    std::vector<Snap> snap;
    for (Object* obj = objectFindFirstAtElevation(gElevation);
         obj != nullptr;
         obj = objectFindNextAtElevation()) {
        if (obj->tile < 0 || obj->tile >= kShiftMapTiles) continue;
        if (obj == gDude) continue;
        if (obj == gGameMouseBouncingCursor) continue;
        if (obj == gGameMouseHexCursor) continue;
        snap.push_back({ obj, obj->tile });
    }

    int* sq = _square[gElevation]->field_0;

    // Apply the per-object shift / drop rule. `keepRow`/`keepCol` decide whether an old tile
    // survives; `newTile` maps a surviving tile to its destination.
    auto applyShift = [&](auto isInDropRegion, auto newTile) {
        for (auto& s : snap) {
            if (isInDropRegion(s.tile)) {
                objectDestroy(s.obj, nullptr);
            } else {
                int dst = newTile(s.tile);
                if (dst < 0 || dst >= kShiftMapTiles) {
                    objectDestroy(s.obj, nullptr);
                } else if (dst != s.tile) {
                    objectSetLocation(s.obj, dst, gElevation, nullptr);
                }
            }
        }
    };

    if (dx == 1) {
        // dx=1: Drop the leftmost two columns; tiles in cols 2..199 move to cols 0..197.
        applyShift(
            [](int t) { return (t % kShiftMapWidth) < 2; },
            [](int t) { return t - 2; });
        // Square grid shifts by 1 column (it's half-resolution vs. the hex grid).
        for (int row = 0; row < kSqHeight; row++) {
            for (int col = 0; col < kSqWidth - 1; col++) {
                sq[row * kSqWidth + col] = sq[row * kSqWidth + col + 1];
            }
        }
    } else if (dx == -1) {
        applyShift(
            [](int t) { return (t % kShiftMapWidth) >= kShiftMapWidth - 2; },
            [](int t) { return t + 2; });
        for (int row = 0; row < kSqHeight; row++) {
            for (int col = kSqWidth - 1; col > 0; col--) {
                sq[row * kSqWidth + col] = sq[row * kSqWidth + col - 1];
            }
        }
    } else if (dy == 1) {
        applyShift(
            [](int t) { return (t / kShiftMapWidth) >= kShiftMapHeight - 2; },
            [](int t) { return t + 2 * kShiftMapWidth; });
        for (int row = kSqHeight - 1; row > 0; row--) {
            for (int col = 0; col < kSqWidth; col++) {
                sq[row * kSqWidth + col] = sq[(row - 1) * kSqWidth + col];
            }
        }
    } else if (dy == -1) {
        applyShift(
            [](int t) { return (t / kShiftMapWidth) < 2; },
            [](int t) { return t - 2 * kShiftMapWidth; });
        for (int row = 0; row < kSqHeight - 1; row++) {
            for (int col = 0; col < kSqWidth; col++) {
                sq[row * kSqWidth + col] = sq[(row + 1) * kSqWidth + col];
            }
        }
    }

    // Translate every spatial script the same way.
    Script* script = scriptGetFirstSpatialScript(gElevation);
    while (script != nullptr) {
        int oldTile = script->sp.built_tile & 0x3FFFFFF;
        int newTile = oldTile + 400 * dy - 2 * dx;
        if (newTile >= 1 && newTile < kShiftMapTiles - 1) {
            int rotBits = (script->sp.built_tile >> 26) & 0x7;
            script->sp.built_tile = newTile | ((rotBits << 26) & 0x1C000000) | (gElevation << 29);
        }
        script = scriptGetNextSpatialScript();
    }

    tileWindowRefresh();
}

// mapper_shift_map
void mapper_shift_map()
{
    if (!win_yes_no("Are you sure you want to shift this map?", 180, 120, 0x10104)) {
        return;
    }

    while (true) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        bool done = false;

        if (keyCode == -2) {
            if ((mouseGetEvent() & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                done = true;
            }
        } else {
            switch (keyCode) {
            case KEY_ESCAPE:
                done = true;
                break;
            case KEY_ARROW_UP:
                mapper_shift_map_once(0, -1);
                break;
            case KEY_ARROW_LEFT:
                mapper_shift_map_once(-1, 0);
                break;
            case KEY_ARROW_RIGHT:
                mapper_shift_map_once(1, 0);
                break;
            case KEY_ARROW_DOWN:
                mapper_shift_map_once(0, 1);
                break;
            }
        }

        if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            done = true;
        }

        renderPresent();
        sharedFpsLimiter.throttle();

        if (done) return;
    }
}

// mapper_shift_map_elev
void mapper_shift_map_elev()
{
    if (!win_yes_no("Are you sure you want to shift this elevation?", 180, 120, 0x10104)) {
        return;
    }

    int destElev = gElevation + 1;
    if (win_get_num_i(&destElev, 1, 3, false, "Destination elevation:", 100, 100) == -1) {
        return;
    }

    destElev -= 1;
    if (destElev == gElevation) {
        return;
    }

    // Wipe the destination elevation: destroy all objects there except the dude and mouse cursors.
    Object* obj = objectFindFirstAtElevation(destElev);
    while (obj != nullptr) {
        while (obj != nullptr
            && (obj == gGameMouseBouncingCursor
                || obj == gGameMouseHexCursor
                || obj == gDude
                || (obj->flags & OBJECT_NO_REMOVE) != 0)) {
            obj = objectFindNextAtElevation();
        }
        if (obj == nullptr) break;
        objectDestroy(obj, nullptr);
        obj = objectFindFirstAtElevation(destElev);
    }

    // Move every surviving object from the source elevation to the destination.
    obj = objectFindFirstAtElevation(gElevation);
    while (obj != nullptr) {
        while (obj != nullptr
            && (obj == gGameMouseBouncingCursor
                || obj == gGameMouseHexCursor
                || obj == gDude
                || (obj->flags & OBJECT_NO_REMOVE) != 0)) {
            obj = objectFindNextAtElevation();
        }
        if (obj == nullptr) break;
        int tile = obj->tile;
        objectSetLocation(obj, tile, destElev, nullptr);
        obj = objectFindFirstAtElevation(gElevation);
    }

    // Copy the tile (floor + roof) data to the destination elevation, then strip floor + roof
    // tiles in the source elevation back to "blank" art (id=1).
    memcpy(_square[destElev]->field_0, _square[gElevation]->field_0, 40000);

    int blankFid = buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0);

    // Match the original mapper's tile word format (preserved here even though the rotation
    // bits end up overlapping the low nibble of the art id — same convention as placeTile).
    int* src = _square[gElevation]->field_0;
    for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
        int v = src[i];
        int floorRot = (v & 0xF000) >> 12;
        int roofRot = ((v >> 16) & 0xF000) >> 12;
        int newRoofWord = (blankFid & 0xFFF) | roofRot;
        int newFloorWord = (blankFid & 0xFFF) | floorRot;
        src[i] = (newRoofWord << 16) | (newFloorWord & 0xFFFF);
    }

    // Move all spatial scripts from source elevation to destination, plus any exit-grid objects
    // co-located with each script.
    Script* script = scriptGetFirstSpatialScript(gElevation);
    while (script != nullptr) {
        int builtTile = script->sp.built_tile;
        int tile = builtTile & 0x3FFFFFF;
        int elevBits = (builtTile >> 26) & 0x7;
        int newBuiltTile = tile | (destElev << 29) | ((elevBits << 26) & 0x1C000000);

        int exitGridFid = buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, 0);
        Object* exitGrid = objectFindFirstAtLocation(gElevation, tile);
        while (exitGrid != nullptr) {
            if (exitGrid->fid == exitGridFid) {
                objectSetLocation(exitGrid, tile, destElev, nullptr);
                break;
            }
            exitGrid = objectFindNextAtLocation();
        }

        script->sp.built_tile = newBuiltTile;
        script = scriptGetNextSpatialScript();
    }

    mapSetElevation(destElev);
    tileWindowRefresh();
}

// mapper_copy_map_elev
void mapper_copy_map_elev()
{
    if (!win_yes_no("Are you sure you want to copy this elevation?", 180, 120, 0x10104)) {
        return;
    }

    int destElev = gElevation + 1;
    if (win_get_num_i(&destElev, 1, 3, false, "Destination elevation:", 100, 100) == -1) {
        return;
    }

    destElev -= 1;
    if (destElev == gElevation) {
        return;
    }

    // Wipe the destination elevation first.
    Object* obj = objectFindFirstAtElevation(destElev);
    while (obj != nullptr) {
        while (obj != nullptr
            && (obj == gGameMouseBouncingCursor
                || obj == gGameMouseHexCursor
                || obj == gDude
                || (obj->flags & OBJECT_NO_REMOVE) != 0)) {
            obj = objectFindNextAtElevation();
        }
        if (obj == nullptr) break;
        objectDestroy(obj, nullptr);
        obj = objectFindFirstAtElevation(destElev);
    }

    // For each non-skipped, non-PID(-1) object on the source elevation, deep-copy via _obj_copy
    // and place the copy at the same tile on the destination elevation. We track the "previous"
    // object so iteration can resume after each copy.
    Object* prev = nullptr;
    obj = objectFindFirstAtElevation(gElevation);
    while (obj != nullptr) {
        if (prev != nullptr) {
            // Re-walk to where we left off.
            while (obj != nullptr && obj != prev) {
                obj = objectFindNextAtElevation();
            }
            if (obj == nullptr) {
                debugPrint("ERROR: Error in map copy elev!\n");
                return;
            }
            obj = objectFindNextAtElevation();
        }

        while (obj != nullptr
            && (obj == gGameMouseBouncingCursor
                || obj == gGameMouseHexCursor
                || obj == gDude
                || (obj->flags & OBJECT_NO_REMOVE) != 0
                || obj->pid == -1)) {
            obj = objectFindNextAtElevation();
        }
        if (obj == nullptr) break;

        if (obj->sid != -1) {
            debugPrint("WARNING: Map copy elev: object %d has script %d\n", obj->pid, obj->sid);
        }

        Object* copy = nullptr;
        if (_obj_copy(&copy, obj) == -1) {
            debugPrint("ERROR: Error in map copy elev (copy failed)\n");
            return;
        }

        prev = obj;
        objectSetLocation(copy, obj->tile, destElev, nullptr);
        obj = objectFindFirstAtElevation(gElevation);
    }

    memcpy(_square[destElev]->field_0, _square[gElevation]->field_0, 40000);
    mapSetElevation(destElev);
    tileWindowRefresh();
}

} // namespace fallout
