#include "mapper/mp_spray.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "color.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "game_mouse.h"
#include "input.h"
#include "kb.h"
#include "map.h"
#include "map_defs.h"
#include "mapper/mp_utils.h"
#include "memory.h"
#include "mouse.h"
#include "obj_types.h"
#include "platform_compat.h"
#include "proto.h"
#include "proto_txt.h"
#include "settings.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

constexpr int kSprayCellCount = 360;
constexpr int kMaxBrushSize = 11;

// Picker button event codes (match the original mapper's text-button codes).
constexpr int kSprayPrevCode = '[';
constexpr int kSprayNextCode = ']';
constexpr int kSprayUseCode = ' ';

struct SprayToolCell {
    int fid;
    int field_4;
    int field_8;
    int field_C;
};

struct SprayTool {
    SprayToolCell cells[kSprayCellCount];
    int width;
    int length;
    int idx;
};

static_assert(sizeof(SprayTool) == 5772, "SprayTool must match the original 5772-byte layout");

static SprayTool spinfo;
static int map_brush_size = 0;

static const char* const kPatternsListSuffix = "\\patterns\\patterns.lst";

// Build the tiles patterns.lst path; returns the length of the base proto dir
// (before the suffix), so callers can splice an individual pattern filename in.
static int spray_patterns_list_path(char* path)
{
    proto_make_path(path, OBJ_TYPE_TILE << 24);
    int baseLen = static_cast<int>(strlen(path));
    strcat(path, kPatternsListSuffix);
    return baseLen;
}

// Count the pattern entries listed in tiles/patterns/patterns.lst.
// Returns -1 if the list file can't be opened.
static int get_max_spray_tool(int* outCount)
{
    *outCount = 0;

    char path[COMPAT_MAX_PATH];
    spray_patterns_list_path(path);

    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    int ch;
    while ((ch = fileReadChar(stream)) != -1) {
        if (ch == '\n') {
            (*outCount)++;
        }
    }
    (*outCount)++;

    fileClose(stream);
    return 0;
}

// Load pattern number `index` (its binary .spr) into the global spinfo.
// Returns -1 on any failure.
static int load_spray_tool(int index)
{
    if (index < 0) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    int baseLen = spray_patterns_list_path(path);

    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    char line[260];
    int i;
    for (i = 0; i <= index; i++) {
        if (fileReadString(line, sizeof(line), stream) == nullptr) {
            break;
        }
    }

    int result = -1;
    if (i - 1 == index) {
        line[strcspn(line, " \n")] = '\0';
        snprintf(path + baseLen, sizeof(path) - baseLen, "\\patterns\\%s", line);

        File* spr = fileOpen(path, "rb");
        if (spr != nullptr) {
            if (fileRead(&spinfo, sizeof(SprayTool), 1, spr) > 0) {
                result = 0;
            }
            fileClose(spr);
        }
    }

    fileClose(stream);
    return result;
}

// Write the global spinfo as a binary .spr. Spray patterns are mapper-only working
// files, so they go under the temp root (relativePath e.g. "proto\tiles\patterns\00000001").
// The temp root is mounted in the VFS, so the reads (proto_make_path + fileOpen) find them.
static void save_spray_tool(const char* relativePath)
{
    File* stream = fileOpenDirect(buildDataPath(mapperTempRoot(), relativePath), "wb");
    if (stream != nullptr) {
        fileWrite(&spinfo, sizeof(SprayTool), 1, stream);
        fileClose(stream);
    }
}

// Rebuild spinfo from a text pattern source under the mapper temp root.
// Dev-only authoring path. Returns -1 if a fid fails to resolve.
static int load_spray_tool_from_text(int index)
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), R"(%s\proto\tiles\patterns\%08d.txt)", mapperTempRoot(), index);

    FILE* stream = compat_fopen(path, "rt");
    if (stream == nullptr) {
        return 0;
    }

    int result = 0;
    int cellCount = 0;
    char line[120];
    while (compat_fgets(line, sizeof(line), stream) != nullptr) {
        line[strcspn(line, "\n")] = '\0';

        char* colon = strchr(line, ':');
        if (colon == nullptr) {
            continue;
        }
        *colon = '\0';
        char* value = colon + 1;
        while (*value == ' ') {
            value++;
        }

        if (strcmp(line, "width") == 0) {
            spinfo.width = static_cast<int>(strtol(value, nullptr, 10));
        } else if (strcmp(line, "length") == 0) {
            spinfo.length = static_cast<int>(strtol(value, nullptr, 10));
        } else if (strcmp(line, "idx") == 0) {
            spinfo.idx = index;
        } else if (strcmp(line, "fid") == 0) {
            if (cellCount >= kSprayCellCount) {
                debugPrint("\nError: too many fids rebuilding spray tool from text!");
                result = -1;
                break;
            }
            if (proto_read_text_fid(value, &spinfo.cells[cellCount].fid, OBJ_TYPE_TILE) == -1) {
                result = -1;
                break;
            }
            cellCount++;
        } else {
            debugPrint("\nError: rebuilding spray tool from text!");
        }
    }

    fclose(stream);
    return result;
}

// Render the pattern preview (isometric tiles) into the picker window.
// Returns -1 if the scratch buffer can't be allocated.
static int draw_spray_to_win(int win, SprayTool* tool)
{
    if (tool->width <= 0 || tool->length <= 0 || tool->width * tool->length > kSprayCellCount) {
        mapperShowMessage("Invalid spray tool dimensions!");
        return -1;
    }

    int pitch = 48 * tool->width;
    int bufHeight = 12 * tool->length;
    int bufPixels = pitch * bufHeight;

    unsigned char* buf = static_cast<unsigned char*>(internal_malloc(4 * bufPixels));
    if (buf == nullptr) {
        mapperShowMessage("Unable to allocate mem for buf!");
        return -1;
    }
    memset(buf, 0, bufPixels);

    int cellIndex = 0;
    for (int row = 0; row < tool->length; row++) {
        int x = pitch / 2 + (-32 + 32 * row) / 2;
        int y = 24 * row / 2;
        for (int col = 0; col < tool->width; col++) {
            CacheEntry* cache;
            Art* art = artLock(tool->cells[cellIndex].fid, &cache);
            if (art == nullptr) {
                break;
            }

            unsigned char* frameData = artGetFrameData(art, 0, 0);
            int w = artGetWidth(art, 0, 0);
            int h = artGetHeight(art, 0, 0);
            blitBufferToBufferTrans(frameData, w, h, w, buf + x + pitch * y, pitch);
            artUnlock(cache);

            cellIndex++;
            x -= 24;
            y += 6;
        }
    }

    unsigned char* winBuf = windowGetBuffer(win);
    int winWidth = windowGetWidth(win);
    int winHeight = windowGetHeight(win);
    unsigned char* dest = winBuf + 20 * winWidth + 20;

    if (winWidth - 80 <= pitch || winHeight - 80 <= bufHeight) {
        blitBufferToBufferStretch(buf, pitch, bufHeight, pitch, dest, winWidth - 60, winHeight - 60, winWidth);
    } else {
        int offsetX = (winWidth - 80 - pitch) / 2;
        int offsetY = (winHeight - 80 - bufHeight) / 2;
        bufferFill(dest, winWidth - 80, winHeight - 80, winWidth, 0);
        blitBufferToBuffer(buf, pitch, bufHeight, pitch, dest + offsetX + winWidth * offsetY, winWidth);
    }

    internal_free(buf);
    windowRefresh(win);
    return 0;
}

// Show the pattern picker; on Use, *out points to the loaded spinfo and returns 0.
// On Cancel / failure / empty list, *out is null and returns -1.
static int pick_spray_tool(SprayTool** out)
{
    *out = nullptr;

    int count = 0;
    if (get_max_spray_tool(&count) == -1 || count == 0) {
        return -1;
    }

    int win = windowCreate(60, 10, 520, 350, _colorTable[15855], WINDOW_MOVE_ON_TOP | WINDOW_MODAL);
    if (win == -1) {
        return -1;
    }

    _win_register_text_button(win, 30, 320, -1, -1, -1, kSprayPrevCode, "<<", 0);
    _win_register_text_button(win, 140, 320, -1, -1, -1, kSprayNextCode, ">>", 0);
    _win_register_text_button(win, 190, 320, -1, -1, -1, kSprayUseCode, "Use", 0);
    _win_register_text_button(win, 260, 320, -1, -1, -1, KEY_ESCAPE, "Cancel", 0);

    char numText[16];
    snprintf(numText, sizeof(numText), "%d", 0);
    windowDrawText(win, numText, 80, 100, 323, _colorTable[32747] | 0x10000);
    Rect numRect = { 100, 323, 180, 333 };

    *out = &spinfo;
    if (load_spray_tool(0) == -1 || draw_spray_to_win(win, *out) == -1) {
        windowDestroy(win);
        *out = nullptr;
        return -1;
    }

    windowDrawBorder(win);
    windowRefresh(win);

    int current = 0;
    int result = -1;
    while (true) {
        sharedFpsLimiter.mark();

        int input = inputGetInput();
        bool redraw = false;

        if (input == KEY_ESCAPE) {
            *out = nullptr;
            break;
        } else if (input == kSprayUseCode || input == KEY_RETURN) {
            result = 0;
            break;
        } else if (input == kSprayPrevCode) {
            if (--current < 0) {
                current = 0;
            } else if (load_spray_tool(current) != -1) {
                redraw = true;
            }
        } else if (input == kSprayNextCode) {
            current++;
            if (current < count - 1) {
                redraw = load_spray_tool(current) != -1;
            } else {
                current = count - 2;
            }
        }

        if (redraw) {
            snprintf(numText, sizeof(numText), "%d", current);
            windowDrawText(win, numText, 80, 100, 323, _colorTable[32747] | 0x10000);
            windowRefreshRect(win, &numRect);
            draw_spray_to_win(win, *out);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);
    return result;
}

// Paint the active pattern across the brush area centered on the clicked tile.
static void copy_spray_paint(SprayTool* tool)
{
    if (tool->width <= 0 || tool->length <= 0) {
        return;
    }

    int areaHeight = _scr_size.bottom - _scr_size.top - 100;
    if (_mouse_click_in(0, 0, _scr_size.right - _scr_size.left, areaHeight) == 0) {
        return;
    }

    int mx, my;
    mouseGetPosition(&mx, &my);
    int tile = squareTileFromScreenXY(mx, my, gElevation);
    if (tile == -1) {
        return;
    }

    int col = tile % SQUARE_GRID_WIDTH;
    int rowMin = -map_brush_size;
    int rowMax = map_brush_size;
    int colMin = -map_brush_size;
    int colMax = map_brush_size;

    // Faithful to the original: the row offset bounds are clamped using the
    // column coordinate (tile % grid-width) rather than the row. Preserved as-is.
    if (col - map_brush_size < 0) {
        rowMin = col;
    }
    if (col + rowMax > SQUARE_GRID_WIDTH) {
        rowMax = SQUARE_GRID_WIDTH - col;
    }
    if (col + colMin < 0) {
        colMin = col;
    }
    if (col + colMax > SQUARE_GRID_WIDTH) {
        colMax = SQUARE_GRID_WIDTH - col;
    }

    for (int i = rowMin; i <= rowMax; i++) {
        if (colMin > colMax) {
            continue;
        }
        int base = tile + SQUARE_GRID_WIDTH * i;
        for (int t = base + colMin; t <= base + colMax; t++) {
            int cellIndex = t % SQUARE_GRID_WIDTH % tool->width + t / SQUARE_GRID_WIDTH % tool->length * tool->width;
            if (cellIndex < 0 || cellIndex >= kSprayCellCount) {
                continue;
            }
            SprayToolCell* cell = &tool->cells[cellIndex];
            if (cell->fid != -1 && cell->field_4 != -1) {
                place_tile_at(cell->fid, t);
            }
        }
    }

    int sx, sy;
    squareTileToScreenXY(tile, &sx, &sy, gElevation);
    int left = sx - 80 * map_brush_size;
    int top = sy - 36 * map_brush_size;
    Rect rect;
    rect.left = left;
    rect.top = top;
    rect.right = left + 160 * (map_brush_size + 1);
    rect.bottom = top + 72 * (map_brush_size + 1);
    tileWindowRefreshRect(&rect, gElevation);
}

bool place_tile_at(int fid, int tileIndex)
{
    if (tileIndex == -1) {
        return false;
    }

    int art = fid & 0xFFF;
    int* squarePtr = &_square[gElevation]->field_0[tileIndex];
    int oldValue = *squarePtr;

    if (tileRoofIsVisible()) {
        if (buildFid(OBJ_TYPE_TILE, (oldValue >> 16) & 0xFFF, 0, 0, 0) == fid) {
            return false;
        }
        int rotation = ((oldValue >> 16) & 0xF000) >> 12;
        *squarePtr = ((rotation | art) << 16) | (oldValue & 0xFFFF);
    } else {
        if (buildFid(OBJ_TYPE_TILE, oldValue & 0xFFF, 0, 0, 0) == fid) {
            return false;
        }
        int rotation = (oldValue & 0xF000) >> 12;
        *squarePtr = (oldValue & 0xFFFF0000) | rotation | art;
    }

    return true;
}

void create_spray_tool()
{
    // The pattern-authoring path was disabled in the shipped mapper: the
    // original create_spray_tool just returns -1 without doing anything.
}

void copy_spray_tile()
{
    SprayTool* tool = nullptr;
    if (pick_spray_tool(&tool) == -1) {
        mapperShowTimedMsg("No active spray tool!");
        return;
    }

    while (true) {
        sharedFpsLimiter.mark();

        int input = inputGetInput();

        if (input == KEY_PLUS) {
            if (++map_brush_size > kMaxBrushSize) {
                map_brush_size = kMaxBrushSize;
            }
        } else if (input == KEY_MINUS) {
            if (--map_brush_size < 0) {
                map_brush_size = 0;
            }
        } else if (input == KEY_PAGE_UP) {
            mapSetElevation(gElevation + 1);
        } else if (input == KEY_PAGE_DOWN) {
            mapSetElevation(gElevation - 1);
        } else if (input == KEY_ESCAPE) {
            break;
        } else if (input == -1) {
            int event = mouseGetEvent();
            if ((event & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                break;
            }
            if ((event & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                copy_spray_paint(tool);
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
}

int rebuild_spray_tools()
{
    int count = 0;
    if (get_max_spray_tool(&count) == -1) {
        return -1;
    }

    debugPrint("\n\t<<< Rebuilding Patterns >>>");

    char path[COMPAT_MAX_PATH];
    for (int i = 1; i < count; i++) {
        snprintf(path, sizeof(path), "proto\\tiles\\patterns\\%08d", i);
        debugPrint("\nLoading pattern %s", path);

        if (load_spray_tool_from_text(i) == -1) {
            debugPrint("\nError: load_spray_tool_from_text failed!");
        } else {
            save_spray_tool(path);
        }
    }

    return 0;
}

} // namespace fallout
