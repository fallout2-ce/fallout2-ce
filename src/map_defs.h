#ifndef MAPDEFS_H
#define MAPDEFS_H

namespace fallout {

enum MapHeaderFlags : int {
    MAP_HEADER_NONE = 0x00,
    MAP_HEADER_SAVED = 0x01,
    MAP_HEADER_ELEVATION_0 = 0x02,
    MAP_HEADER_ELEVATION_1 = 0x04,
    MAP_HEADER_ELEVATION_2 = 0x08
};

constexpr inline MapHeaderFlags operator~(MapHeaderFlags rhs)
{
    return static_cast<MapHeaderFlags>(~static_cast<int>(rhs));
}

inline MapHeaderFlags& operator&=(MapHeaderFlags& lhs, MapHeaderFlags rhs)
{
    return lhs = static_cast<MapHeaderFlags>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline MapHeaderFlags& operator|=(MapHeaderFlags& lhs, MapHeaderFlags rhs)
{
    return lhs = static_cast<MapHeaderFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

#define ELEVATION_COUNT (3)

#define SQUARE_GRID_WIDTH (100)
#define SQUARE_GRID_HEIGHT (100)
#define SQUARE_GRID_SIZE (SQUARE_GRID_WIDTH * SQUARE_GRID_HEIGHT)

#define HEX_GRID_WIDTH (200)
#define HEX_GRID_HEIGHT (200)
#define HEX_GRID_SIZE (HEX_GRID_WIDTH * HEX_GRID_HEIGHT)

static inline bool elevationIsValid(int elevation)
{
    return elevation >= 0 && elevation < ELEVATION_COUNT;
}

static inline bool squareGridTileIsValid(int tile)
{
    return tile >= 0 && tile < SQUARE_GRID_SIZE;
}

static inline bool hexGridTileIsValid(int tile)
{
    return tile >= 0 && tile < HEX_GRID_SIZE;
}

} // namespace fallout

#endif /* MAPDEFS_H */
