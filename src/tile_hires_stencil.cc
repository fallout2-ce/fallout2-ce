#include "tile_hires_stencil.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "geometry.h"
#include "map.h"
#include "map_edge.h"
#include "object.h"
#include "settings.h"
#include "stdio.h"
#include "tile.h"
#include "window_manager.h"
#include <string.h>
#include <unordered_map>
#include <vector>

// #define DO_DEBUG_CHECKS 1

#ifdef DO_DEBUG_CHECKS
#include <stdlib.h>
#endif

namespace fallout {

//
// This file is responsible for drawing a overlay over tiles which
//   are not visible in original screen resolution.
//

/** This holds hex tiles which can be center tile */
static bool visited_tiles[ELEVATION_COUNT][HEX_GRID_SIZE];

static constexpr int pixels_per_horizontal_move = 32;
static constexpr int pixels_per_vertical_move = 24;

// A overlay is actually a grid of small squares, each square is 16x12 pixels
//  which is half of possible screen move by keyboard or mouse
static constexpr int squares_per_horizontal_move = 2;
static constexpr int squares_per_vertical_move = 2;
static constexpr int square_width = pixels_per_horizontal_move / squares_per_horizontal_move;
static constexpr int square_height = pixels_per_vertical_move / squares_per_vertical_move;

static_assert(pixels_per_horizontal_move % square_width == 0);
static_assert(pixels_per_vertical_move % square_height == 0);

// Dimensions of the grid of squares
static constexpr int square_grid_width = 500;
static constexpr int square_grid_height = 300;

// Solid is 0 so a cleared grid is fully black.
enum class StencilSquare : unsigned char {
    Solid = 0,
    Clear,
    OpenTop,
    OpenBottom,
    OpenBoth,
};

static_assert(static_cast<unsigned char>(StencilSquare::Solid) == 0);

// This array holds information about which squares are visible
static StencilSquare visible_squares[ELEVATION_COUNT][square_grid_width][square_grid_height];

// Dimensions of the grid of squares were calculated by
// going through all hex tiles and finding the lowest and highest x and y.
// This way we can be sure that all hex tiles can be covered by squares.
static_assert(square_width * square_grid_width == 8000);
static_assert(square_height * square_grid_height == 3600);

// What can be seen on the screen in original resolution
static constexpr int screen_view_width = 640;
static constexpr int screen_view_height = 380;

// How many squares can be seen in horizontal and vertical directions
static constexpr int squares_screen_width_half = screen_view_width / 2 / square_width;
static constexpr int squares_screen_height_half = screen_view_height / 2 / square_height;
// Horizontal visibility fits hex grid perfectly
static_assert(screen_view_width % (2 * square_width) == 0);
// In the vertical direction we have 10+10 pixels left per each direction
// which is covered by squares but theoretically could be seen in the original game
static constexpr int edge_strip = (screen_view_height % (2 * square_height)) / 2;
static_assert(edge_strip == 10);

static bool gIsTileHiresStencilEnabled = false;

// Clear beats an open strip. An open strip beats solid black.
// OpenTop together with OpenBottom becomes OpenBoth.
static void mark_square(int x, int y, StencilSquare value)
{
    if (x < 0 || y < 0 || x >= square_grid_width || y >= square_grid_height) {
        return;
    }

    StencilSquare& cell = visible_squares[gElevation][x][y];
    if (cell == StencilSquare::Clear || value == StencilSquare::Solid) {
        return;
    }
    if (value == StencilSquare::Clear || cell == StencilSquare::Solid) {
        cell = value;
        return;
    }
    if (cell != value) {
        cell = StencilSquare::OpenBoth;
    }
}

void tile_hires_stencil_reset_cache()
{
    memset(visited_tiles, 0, sizeof(visited_tiles));
    memset(visible_squares, 0, sizeof(visible_squares));
}
static void clean_cache_for_elevation(MapElevation elevation)
{
    memset(visited_tiles[elevation], 0, sizeof(visited_tiles[elevation]));
    memset(visible_squares[elevation], 0, sizeof(visible_squares[elevation]));
}

static Point get_screen_diff()
{
    constexpr int hex_tile_with_lowest_x = 199;
    constexpr int hex_tile_with_lowest_y = 0;

#ifdef DO_DEBUG_CHECKS
    {
        // Just to ensure once again that we have calculated the lowest and highest hex tiles correctly

        int minX = 0x7FFFFFFF;
        int minY = 0x7FFFFFFF;
        int maxX = 0;
        int maxY = 0;
        int minTileX = -1;
        int minTileY = -1;
        for (int t = 0; t < HEX_GRID_SIZE; t++) {
            int x;
            int y;
            tileToScreenXY(t, &x, &y);
            if (x < minX) {
                minX = x;
                minTileX = t;
            };
            if (y < minY) {
                minY = y;
                minTileY = t;
            };
            if (x > maxX) {
                maxX = x;
            }
            if (y > maxY) {
                maxY = y;
            }
        }
        if (minTileX != hex_tile_with_lowest_x || minTileY != hex_tile_with_lowest_y) {
            printf("min tileX=%i x=%i tileY=%i y=%i maxX=%i maxY=%i dx=%i dy=%i\n",
                minTileX, minX,
                minTileY, minY,
                maxX, maxY,
                maxX - minX, maxY - minY);
            exit(108);
        }
    };
#endif

    int offsetX;
    int tmp;
    int offsetY;
    tileToScreenXY(hex_tile_with_lowest_x, &offsetX, &tmp);
    tileToScreenXY(hex_tile_with_lowest_y, &tmp, &offsetY);

    Point out;
    out.x = offsetX;
    out.y = offsetY;
    return out;
};

// This enum is used to tell the marking function to not to re-mark
// squares which are already marked by previous calls from neighboring tiles
enum class MarkOnlyPart {
    FULL,
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

static void mark_screen_tiles_around_as_visible(int center_tile, const Point& screen_diff, MarkOnlyPart part)
{
    int centerTileScreenX;
    int centerTileScreenY;
    tileToScreenXY(center_tile, &centerTileScreenX, &centerTileScreenY);

    int centerTileGlobalX = centerTileScreenX - screen_diff.x;
    int centerTileGlobalY = centerTileScreenY - screen_diff.y;

#ifdef DO_DEBUG_CHECKS
    {
        if (centerTileGlobalX % square_width != 0 || centerTileGlobalY % square_height != 0) {
            printf("centerTileGlobalX=%i, centerTileGlobalY=%i\n", centerTileGlobalX, centerTileGlobalY);
            exit(101);
        }

        if (
            centerTileGlobalX - squares_screen_width_half * square_width < 0 || centerTileGlobalY - squares_screen_height_half * square_height < 0) {
            printf("centerTileGlobalX=%i, centerTileGlobalY=%i\n", centerTileGlobalX, centerTileGlobalY);
            printf("kekekek border=%i\n", gTileBorderInitialized);
            exit(102);
        }
        if (
            centerTileGlobalX + squares_screen_width_half * square_width >= square_width * square_grid_width
            || centerTileGlobalY + squares_screen_height_half * square_height >= square_height * square_grid_height) {
            printf("centerTileGlobalX=%i, centerTileGlobalY=%i\n", centerTileGlobalX, centerTileGlobalY);
            exit(103);
        }
    };
#endif

    int squareX = centerTileGlobalX / square_width;
    int squareY = centerTileGlobalY / square_height;

    int horizonal_start_full = squareX - squares_screen_width_half + 1;
    int horizonal_end_full = squareX + squares_screen_width_half;

    int horizontal_start = part != MarkOnlyPart::RIGHT
        ? horizonal_start_full
        : horizonal_end_full - squares_per_horizontal_move;
    int horizontal_end = part != MarkOnlyPart::LEFT
        ? horizonal_end_full
        : horizonal_start_full + squares_per_horizontal_move;

    int vertical_start_full = squareY - squares_screen_height_half;
    int vertical_end_full = squareY + squares_screen_height_half;

    int vertical_start = part != MarkOnlyPart::DOWN
        ? vertical_start_full
        : vertical_end_full - squares_per_vertical_move;
    int vertical_end = part != MarkOnlyPart::UP
        ? vertical_end_full
        : vertical_start_full + squares_per_vertical_move;

    // The row above the full view keeps its bottom strip, the row below keeps its top strip.
    const int topPartialRow = vertical_start_full - 1;
    const int bottomPartialRow = vertical_end_full;
    if (vertical_start == vertical_start_full) {
        vertical_start = topPartialRow;
    }

    for (int x = horizontal_start; x <= horizontal_end; x++) {
        for (int y = vertical_start; y <= vertical_end; y++) {
            StencilSquare value = StencilSquare::Clear;
            if (y == topPartialRow) {
                value = StencilSquare::OpenBottom;
            } else if (y == bottomPartialRow) {
                value = StencilSquare::OpenTop;
            }
            mark_square(x, y, value);
        }
    }
}

void tile_hires_stencil_on_center_tile_or_elevation_change()
{
    if (!gIsTileHiresStencilEnabled || mapEdgeIsLoaded()) {
        return;
    }
    // With EDG loaded, the EdgeClipping path handles blackening via ClearRect/CheckRect.
    // The stencil used as a backup when no EDG is present.
    if (!gTileBorderInitialized || gCenterTile < 0 || gCenterTile >= HEX_GRID_SIZE || visited_tiles[gElevation][gCenterTile]) {
        return;
    }

    clean_cache_for_elevation(gElevation);

    struct TileToVisit {
        int tile;
        MarkOnlyPart part;
    };

    std::vector<struct TileToVisit> tiles_to_visit {};
    tiles_to_visit.reserve(7000);

    tiles_to_visit.push_back({ gCenterTile, MarkOnlyPart::FULL });

    int visited_tiles_count = 0;

    auto screen_diff = get_screen_diff();

    while (!tiles_to_visit.empty()) {
        auto tileInfo = tiles_to_visit.back();
        tiles_to_visit.pop_back();

        if (tileInfo.tile < 0 || tileInfo.tile >= HEX_GRID_SIZE) {
            continue;
        }
        if (visited_tiles[gElevation][tileInfo.tile]) {
            continue;
        }

        if (tileInfo.tile != gCenterTile) {
            if (_obj_scroll_blocking_at(tileInfo.tile, gElevation) == 0) {
                continue;
            }
            // TODO: Maybe create new function in tile.cc and use it here
            int tile_x = HEX_GRID_WIDTH - 1 - tileInfo.tile % HEX_GRID_WIDTH;
            int tile_y = tileInfo.tile / HEX_GRID_WIDTH;
            if (tile_x <= gTileBorderMinX || tile_x >= gTileBorderMaxX || tile_y <= gTileBorderMinY || tile_y >= gTileBorderMaxY) {
                continue;
            }
        }

        visited_tiles_count++;

        visited_tiles[gElevation][tileInfo.tile] = true;
        mark_screen_tiles_around_as_visible(tileInfo.tile, screen_diff, tileInfo.part);

        int tileScreenX;
        int tileScreenY;
        if (tileToScreenXY(tileInfo.tile, &tileScreenX, &tileScreenY) == -1) {
            continue;
        }

        // tile size is 32 x 18
        //
        //  / \      ^
        // |   |     | 18
        //  \ /      v
        //
        // <-32->
        //
        // Scrolling left-right changes x by 32
        // But scrolling top-bottom changes y by 24
        //
        //
        //        / \
        //       |   |         <----- tiles on vertical change, 24 px
        //      / \ / \            |
        //     |   |   |   <------ | ----- tiles on horizontal change, 32 px
        //      \ / \ /            |
        //       |   |         <--/
        //        \ /
        //

        constexpr int tile_center_offset_x = 16;
        constexpr int tile_center_offset_y = 8;

        tiles_to_visit.push_back({ tileFromScreenXY(
                                       tileScreenX - pixels_per_horizontal_move + tile_center_offset_x,
                                       tileScreenY + tile_center_offset_y,
                                       true),
            MarkOnlyPart::LEFT });
        tiles_to_visit.push_back({ tileFromScreenXY(
                                       tileScreenX + pixels_per_horizontal_move + tile_center_offset_x,
                                       tileScreenY + tile_center_offset_y,
                                       true),
            MarkOnlyPart::RIGHT });
        tiles_to_visit.push_back({ tileFromScreenXY(
                                       tileScreenX + tile_center_offset_x,
                                       tileScreenY - pixels_per_vertical_move + tile_center_offset_y,
                                       true),
            MarkOnlyPart::UP });
        tiles_to_visit.push_back({ tileFromScreenXY(
                                       tileScreenX + tile_center_offset_x,
                                       tileScreenY + pixels_per_vertical_move + tile_center_offset_y,
                                       true),
            MarkOnlyPart::DOWN });
    }

    // debugPrint("tile_hires_stencil_on_center_tile_or_elevation_change visited_tiles_count=%i\n", visited_tiles_count);
}

void tile_hires_stencil_draw(Rect* rect, unsigned char* buffer, int windowWidth, int windowHeight)
{
    if (!gIsTileHiresStencilEnabled || mapEdgeIsLoaded()) {
        return;
    }

    int minX = rect->left;
    int minY = rect->top;
    int maxX = rect->right;
    int maxY = rect->bottom;

    auto screen_diff = get_screen_diff();
    int minXglobal = minX - screen_diff.x;
    int minYglobal = minY - screen_diff.y;
    int maxXglobal = maxX - screen_diff.x;
    int maxYglobal = maxY - screen_diff.y;

    if (minXglobal < 0) {
        // This should never happen
        minXglobal = 0;
    };
    if (minYglobal < 0) {
        // This should never happen
        minYglobal = 0;
    };
    if (maxXglobal >= square_width * square_grid_width) {
        // This can happen if screen resulution is really huge
        maxXglobal = square_width * square_grid_width - 1;
    }
    if (maxYglobal >= square_height * square_grid_height) {
        // This can happen if screen resulution is really huge
        maxYglobal = square_height * square_grid_height - 1;
    }

    if (minXglobal % square_width != 0) {
        minXglobal = minXglobal - (minXglobal % square_width);
    };
    if (minYglobal % square_height != 0) {
        minYglobal = minYglobal - (minYglobal % square_height);
    };
    maxXglobal++;
    if ((maxXglobal) % square_width != 0) {
        maxXglobal = maxXglobal - (maxXglobal % square_width) + square_width;
    };
    maxYglobal++;
    if ((maxYglobal) % square_height != 0) {
        maxYglobal = maxYglobal - (maxYglobal % square_height) + square_height;
    };

    int minSquareX = minXglobal / square_width;
    int minSquareY = minYglobal / square_height;
    int maxSquareX = maxXglobal / square_width;
    int maxSquareY = maxYglobal / square_height;
    if (minSquareX < 0) {
        minSquareX = 0;
    }
    if (minSquareY < 0) {
        minSquareY = 0;
    }
    if (maxSquareX >= square_grid_width) {
        maxSquareX = square_grid_width - 1;
    }
    if (maxSquareY >= square_grid_height) {
        maxSquareY = square_grid_height - 1;
    }
    for (int x = minSquareX; x <= maxSquareX; x++) {
        for (int y = minSquareY; y <= maxSquareY; y++) {
            StencilSquare cell = visible_squares[gElevation][x][y];
            if (cell != StencilSquare::Clear) {
                int screenX = x * square_width + screen_diff.x;
                int screenY = y * square_height + screen_diff.y;

                Rect squareRect;
                squareRect.left = screenX;
                squareRect.top = screenY;
                squareRect.right = screenX + square_width - 1;
                squareRect.bottom = screenY + square_height - 1;
                if (cell == StencilSquare::OpenTop || cell == StencilSquare::OpenBoth) {
                    squareRect.top += edge_strip;
                }
                if (cell == StencilSquare::OpenBottom || cell == StencilSquare::OpenBoth) {
                    squareRect.bottom -= edge_strip;
                }
                if (squareRect.top > squareRect.bottom) {
                    continue;
                }

                Rect intersection;
                if (rectIntersection(rect, &squareRect, &intersection) == -1) {
                    continue;
                };

                bufferFill(buffer + windowWidth * intersection.top + intersection.left,
                    intersection.right - intersection.left + 1,
                    intersection.bottom - intersection.top + 1,
                    windowWidth,
                    COLOR_FIRST);
            }
        }
    }
}

void tile_hires_stencil_init()
{
    gIsTileHiresStencilEnabled = !settings.system.executableIsMapper() && settings.ui.enable_high_resolution_stencil;
    if (!gIsTileHiresStencilEnabled) {
        return;
    }

    if (settings.ui.ignore_map_edges) {
        static bool isMessageShown = false;
        if (!isMessageShown) {
            debugPrint("Tile hires stencil is disabled because map edges are ignored.");
            isMessageShown = true;
        }
        gIsTileHiresStencilEnabled = false;
        return;
    }

    debugPrint("tile_hires_stencil_init\n");
}

void tile_hires_stencil_on_map_load()
{
    if (!gIsTileHiresStencilEnabled) {
        return;
    }
    tile_hires_stencil_reset_cache();
    tile_hires_stencil_on_center_tile_or_elevation_change();
    tileWindowRefresh();
    debugPrint("tile_hires_stencil_on_map_load\n");
}

namespace {

    constexpr int kLimitTileWidth = 32;
    constexpr int kLimitTileHeight = 24;

    struct StencilLimit {
        bool have;
        Rect pixelRect;
        Rect scrollBorderRect;
        int windowWidth;
        int windowHeight;
    };

    StencilLimit stencilLimits[ELEVATION_COUNT];
    int stencilAlignX = 0;
    int stencilAlignY = 0;
    int stencilMaxAlignX = 0;
    int stencilMaxAlignY = 0;
    Rect stencilVisibleArea = {};
    bool stencilHaveVisibleArea = false;

    void absolute_screen(int tile, int& sx, int& sy)
    {
        int x = tile % HEX_GRID_WIDTH;
        int y = tile / HEX_GRID_WIDTH;
        int column = HEX_GRID_WIDTH - 1 - x;
        sx = 48 * (column / 2) + 16 * y;
        sy = 12 * (-(column / 2)) + 12 * y;
        if (column & 1) {
            sx += 32;
        }
    }

    int tile_from_absolute_screen(int sx, int sy)
    {
        struct Key {
            int x;
            int y;
            bool operator==(const Key& other) const { return x == other.x && y == other.y; }
        };
        struct Hash {
            size_t operator()(const Key& key) const
            {
                return (static_cast<size_t>(key.x) << 16) ^ static_cast<unsigned>(key.y);
            }
        };
        static std::unordered_map<Key, int, Hash> inverse;
        static bool ready = false;
        if (!ready) {
            inverse.reserve(HEX_GRID_SIZE);
            for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
                int x;
                int y;
                absolute_screen(tile, x, y);
                inverse[{ x, y }] = tile;
            }
            ready = true;
        }
        auto found = inverse.find({ sx, sy });
        if (found == inverse.end()) {
            return -1;
        }
        return found->second;
    }

    bool iso_window_size(int& width, int& height)
    {
        Rect winRect;
        if (windowGetRect(gIsoWindow, &winRect) == -1) {
            return false;
        }
        width = rectGetWidth(&winRect);
        height = rectGetHeight(&winRect);
        return width > 0 && height > 0;
    }

    void contract_limit(StencilLimit& limit, int winWidth, int winHeight)
    {
        const int winHalfWidth = winWidth / 2;
        const int winHalfHeight = winHeight / 2;
        stencilMaxAlignX = winHalfWidth % kLimitTileWidth;
        stencilMaxAlignY = winHalfHeight % kLimitTileHeight;
        const int winHalfWidthSnapped = winHalfWidth - stencilMaxAlignX;
        const int winHalfHeightSnapped = winHalfHeight - stencilMaxAlignY;

        limit.scrollBorderRect = limit.pixelRect;

        long rectHalfWidth = (limit.scrollBorderRect.left - limit.scrollBorderRect.right) / 2;
        if (rectHalfWidth < winHalfWidthSnapped) {
            long remainder = rectHalfWidth % kLimitTileWidth;
            long snapped = rectHalfWidth - remainder;
            limit.scrollBorderRect.left -= snapped;
            limit.scrollBorderRect.right += snapped + (remainder ? kLimitTileWidth : 0);
        } else {
            limit.scrollBorderRect.left -= winHalfWidthSnapped;
            limit.scrollBorderRect.right += winHalfWidthSnapped;
        }

        long rectHalfHeight = (limit.scrollBorderRect.bottom - limit.scrollBorderRect.top) / 2;
        if (rectHalfHeight < winHalfHeightSnapped) {
            long remainder = rectHalfHeight % kLimitTileHeight;
            long snapped = rectHalfHeight - remainder;
            limit.scrollBorderRect.top += snapped + (remainder ? kLimitTileHeight : 0);
            limit.scrollBorderRect.bottom -= snapped;
        } else {
            limit.scrollBorderRect.top += winHalfHeightSnapped;
            limit.scrollBorderRect.bottom -= winHalfHeightSnapped;
        }

        if ((limit.scrollBorderRect.left < limit.scrollBorderRect.right) || (limit.scrollBorderRect.left - limit.scrollBorderRect.right) == kLimitTileWidth) {
            limit.scrollBorderRect.left = limit.scrollBorderRect.right;
        }
        if ((limit.scrollBorderRect.bottom < limit.scrollBorderRect.top) || (limit.scrollBorderRect.bottom - limit.scrollBorderRect.top) == kLimitTileHeight) {
            limit.scrollBorderRect.bottom = limit.scrollBorderRect.top;
        }

        limit.windowWidth = winWidth;
        limit.windowHeight = winHeight;
    }

    bool ensure_contracted(MapElevation elevation)
    {
        if (!elevationIsValid(elevation)) {
            return false;
        }
        StencilLimit& limit = stencilLimits[elevation];
        if (!limit.have) {
            return false;
        }
        int width;
        int height;
        if (!iso_window_size(width, height)) {
            return false;
        }
        if (limit.windowWidth != width || limit.windowHeight != height) {
            contract_limit(limit, width, height);
        }
        return true;
    }

    void update_stencil_alignment(const StencilLimit& limit, int px, int py)
    {
        stencilAlignX = 0;
        stencilAlignY = 0;
        if (px == limit.scrollBorderRect.left) {
            stencilAlignX = -stencilMaxAlignX;
        } else if (px == limit.scrollBorderRect.right) {
            stencilAlignX = stencilMaxAlignX;
        }
        if (py == limit.scrollBorderRect.top) {
            stencilAlignY = -stencilMaxAlignY;
        } else if (py == limit.scrollBorderRect.bottom) {
            stencilAlignY = stencilMaxAlignY;
        }
    }

    bool tile_outside_legacy_border(int tile)
    {
        if (!gTileBorderInitialized) {
            return false;
        }
        int tileX = HEX_GRID_WIDTH - 1 - tile % HEX_GRID_WIDTH;
        int tileY = tile / HEX_GRID_WIDTH;
        return tileX <= gTileBorderMinX || tileX >= gTileBorderMaxX || tileY <= gTileBorderMinY || tileY >= gTileBorderMaxY;
    }

    void build_limit(MapElevation elevation, int seed)
    {
        std::vector<char> seen(HEX_GRID_SIZE);
        std::vector<int> pending;
        pending.push_back(seed);

        bool haveContent = false;
        Rect content {};

        while (!pending.empty()) {
            int tile = pending.back();
            pending.pop_back();
            if (tile < 0 || tile >= HEX_GRID_SIZE || seen[tile]) {
                continue;
            }
            seen[tile] = 1;
            if (tile != seed) {
                if (_obj_scroll_blocking_at(tile, elevation) == 0) {
                    continue;
                }
                if (tile_outside_legacy_border(tile)) {
                    continue;
                }
            }

            int px;
            int py;
            tileToPixelOffset(tile, px, py);
            Rect view;
            view.left = px + screen_view_width / 2;
            view.right = px - screen_view_width / 2;
            view.top = py - screen_view_height / 2;
            view.bottom = py + screen_view_height / 2;
            if (!haveContent) {
                content = view;
                haveContent = true;
            } else {
                if (view.left > content.left) content.left = view.left;
                if (view.right < content.right) content.right = view.right;
                if (view.top < content.top) content.top = view.top;
                if (view.bottom > content.bottom) content.bottom = view.bottom;
            }

            int sx;
            int sy;
            absolute_screen(tile, sx, sy);
            const int stepX[4] = { -pixels_per_horizontal_move, pixels_per_horizontal_move, 0, 0 };
            const int stepY[4] = { 0, 0, -pixels_per_vertical_move, pixels_per_vertical_move };
            for (int direction = 0; direction < 4; direction++) {
                pending.push_back(tile_from_absolute_screen(sx + stepX[direction], sy + stepY[direction]));
            }
        }

        if (!haveContent) {
            return;
        }

        int width;
        int height;
        if (!iso_window_size(width, height)) {
            return;
        }
        StencilLimit& limit = stencilLimits[elevation];
        limit.pixelRect = content;
        limit.have = true;
        contract_limit(limit, width, height);
    }

    void prepare_limit(MapElevation elevation, int seed)
    {
        if (!gIsTileHiresStencilEnabled || mapEdgeIsLoaded() || !elevationIsValid(elevation) || !tileIsValid(seed)) {
            return;
        }
        if (stencilLimits[elevation].have) {
            ensure_contracted(elevation);
            return;
        }
        build_limit(elevation, seed);
    }

} // namespace

void tile_hires_stencil_reset_limits()
{
    for (StencilLimit& limit : stencilLimits) {
        limit = {};
    }
    stencilAlignX = 0;
    stencilAlignY = 0;
    stencilMaxAlignX = 0;
    stencilMaxAlignY = 0;
    stencilVisibleArea = {};
    stencilHaveVisibleArea = false;
    tile_hires_stencil_reset_cache();
}

bool tile_hires_stencil_limits_active()
{
    return gIsTileHiresStencilEnabled && !mapEdgeIsLoaded() && elevationIsValid(gElevation) && stencilLimits[gElevation].have;
}

int tile_hires_stencil_clamp_center(int tile, MapElevation elevation)
{
    prepare_limit(elevation, tile);
    if (!elevationIsValid(elevation) || !stencilLimits[elevation].have || !ensure_contracted(elevation)) {
        return tile;
    }

    int px;
    int py;
    tileToPixelOffset(tile, px, py);
    const StencilLimit& limit = stencilLimits[elevation];
    int cx = (px <= limit.scrollBorderRect.left)
        ? ((px >= limit.scrollBorderRect.right) ? px : limit.scrollBorderRect.right)
        : limit.scrollBorderRect.left;
    int cy = (py <= limit.scrollBorderRect.bottom)
        ? ((py >= limit.scrollBorderRect.top) ? py : limit.scrollBorderRect.top)
        : limit.scrollBorderRect.bottom;
    update_stencil_alignment(limit, cx, cy);
    pixelToTileCoord(cx, cy);
    return cx + cy * HEX_GRID_WIDTH;
}

bool tile_hires_stencil_tile_in_bounds(int tile)
{
    if (!tile_hires_stencil_limits_active() || !ensure_contracted(gElevation)) {
        return true;
    }
    int px;
    int py;
    tileToPixelOffset(tile, px, py);
    const Rect& border = stencilLimits[gElevation].scrollBorderRect;
    return px >= border.right && px <= border.left && py >= border.top && py <= border.bottom;
}

bool tile_hires_stencil_set_boundary_mods(int tile)
{
    if (!tile_hires_stencil_limits_active() || !ensure_contracted(gElevation)) {
        stencilAlignX = 0;
        stencilAlignY = 0;
        return false;
    }
    int px;
    int py;
    tileToPixelOffset(tile, px, py);
    const int oldX = stencilAlignX;
    const int oldY = stencilAlignY;
    update_stencil_alignment(stencilLimits[gElevation], px, py);
    return stencilAlignX != oldX || stencilAlignY != oldY;
}

int tile_hires_stencil_get_tile_x_alignment()
{
    if (mapEdgeIsLoaded() || !gIsTileHiresStencilEnabled) {
        return 0;
    }
    return stencilAlignX;
}

int tile_hires_stencil_get_tile_y_alignment()
{
    if (mapEdgeIsLoaded() || !gIsTileHiresStencilEnabled) {
        return 0;
    }
    return stencilAlignY;
}

bool tile_hires_stencil_compute_visible_area(MapElevation elevation, Rect* outRect)
{
    if (!gIsTileHiresStencilEnabled || mapEdgeIsLoaded() || !ensure_contracted(elevation)) {
        return false;
    }

    int px;
    int py;
    tileToPixelOffset(gCenterTile, px, py);
    px += stencilAlignX;
    py -= stencilAlignY;

    int width;
    int height;
    if (!iso_window_size(width, height)) {
        return false;
    }
    const int screenOriginX = px + width / 2 - 1;
    const int screenOriginY = py - (height / 2 - 1);
    const Rect& pixelRect = stencilLimits[elevation].pixelRect;
    outRect->left = screenOriginX - pixelRect.left;
    outRect->right = screenOriginX - pixelRect.right;
    outRect->top = pixelRect.top - screenOriginY;
    outRect->bottom = pixelRect.bottom - screenOriginY;
    stencilVisibleArea = *outRect;
    stencilHaveVisibleArea = true;
    return true;
}

bool tile_hires_stencil_is_over_clipped_area(int screenX, int screenY)
{
    if (!tile_hires_stencil_limits_active() || !stencilHaveVisibleArea) {
        return false;
    }
    if (screenX >= stencilVisibleArea.left && screenX <= stencilVisibleArea.right && screenY >= stencilVisibleArea.top && screenY < stencilVisibleArea.bottom) {
        return false;
    }
    return windowGetAtPoint(screenX, screenY) == gIsoWindow;
}

} // namespace fallout
