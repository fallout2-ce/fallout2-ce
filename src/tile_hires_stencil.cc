#include "tile_hires_stencil.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "geometry.h"
#include "map_edge.h"
#include "settings.h"
#include "stdio.h"
#include "tile.h"
#include <string.h>
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

// Dimensions of the grid of squares were calculated by
// going through all hex tiles and finding the lowest and highest x and y.
// This way we can be sure that all hex tiles can be covered by squares.
static_assert(square_width * square_grid_width == 8000);
static_assert(square_height * square_grid_height == 3600);

// What can be seen on the screen in original resolution
static constexpr int screen_view_width = 640;
static constexpr int screen_view_height = 380;
static constexpr int screen_view_half_width = screen_view_width / 2;
static constexpr int screen_view_half_height = screen_view_height / 2;

// How many squares can be seen in horizontal and vertical directions
static constexpr int squares_screen_width_half = screen_view_width / 2 / square_width;
static constexpr int squares_screen_height_half = screen_view_height / 2 / square_height;
// Horizontal visibility fits hex grid perfectly
static_assert(screen_view_width % (2 * square_width) == 0);
// (480-100) does not divide by the vertical square step. The remainder is split
// into equal open strips above and below the fully covered squares.
static constexpr int edge_strip = (screen_view_height % (2 * square_height)) / 2;
static_assert(edge_strip == 10);

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

static void clean_cache()
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

    // Fully covered rows are squareY-15 .. squareY+14. The row above shows its
    // bottom strip; the row at squareY+15 shows its top strip.
    const int topPartialRow = vertical_start_full - 1;
    const int bottomPartialRow = vertical_end_full;
    int rowStart = vertical_start;
    if (vertical_start == vertical_start_full) {
        rowStart = topPartialRow;
    }

    for (int x = horizontal_start; x <= horizontal_end; x++) {
        for (int y = rowStart; y <= vertical_end; y++) {
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
    // A loaded EDG file owns clamping and clipping. Stencil stays off.
    if (!gTileBorderInitialized || gCenterTile < 0 || gCenterTile >= HEX_GRID_SIZE) {
        return;
    }
    if (visited_tiles[gElevation][gCenterTile]) {
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

    bool haveContent = false;
    Rect content {};

    while (!tiles_to_visit.empty()) {
        auto tileInfo = tiles_to_visit.back();
        tiles_to_visit.pop_back();

        // tileFromScreenXY can hand back an index outside the grid. Check it
        // before visited_tiles[] — an invalid tile must not count as scrollable.
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

        int px;
        int py;
        tileToPixelOffset(tileInfo.tile, px, py);
        Rect view;
        view.left = px + screen_view_half_width;
        view.right = px - screen_view_half_width;
        view.top = py - screen_view_half_height;
        view.bottom = py + screen_view_half_height;
        // Pixel X is inverted (left > right), same as an EDG pixelRect.
        if (!haveContent) {
            content = view;
            haveContent = true;
        } else {
            if (view.left > content.left) {
                content.left = view.left;
            }
            if (view.right < content.right) {
                content.right = view.right;
            }
            if (view.top < content.top) {
                content.top = view.top;
            }
            if (view.bottom > content.bottom) {
                content.bottom = view.bottom;
            }
        }

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

    if (haveContent) {
        mapEdgeSetSyntheticPixelRect(gElevation, content);
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
            if (cell == StencilSquare::Clear) {
                continue;
            }

            int screenX = x * square_width + screen_diff.x;
            int screenY = y * square_height + screen_diff.y;

            // Rect is inclusive. right/bottom are the last pixel of the square.
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

void tile_hires_stencil_reset_cache()
{
    clean_cache();
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

} // namespace fallout
