#ifndef OBJ_TYPES_H
#define OBJ_TYPES_H

namespace fallout {

// Rotation
enum Rotation : int {
    ROTATION_INVALID = -1,
    ROTATION_NE, // 0
    ROTATION_E, // 1
    ROTATION_SE, // 2
    ROTATION_SW, // 3
    ROTATION_W, // 4
    ROTATION_NW, // 5
    ROTATION_COUNT,
    ROTATION_FIRST = ROTATION_NE,
    ROTATION_LAST = ROTATION_NW,
};

inline Rotation operator+(Rotation lhs, int rhs)
{
    return static_cast<Rotation>(static_cast<int>(lhs) + rhs);
}

inline Rotation operator-(Rotation lhs, int rhs)
{
    return static_cast<Rotation>(static_cast<int>(lhs) - rhs);
}

inline Rotation operator%(Rotation lhs, int rhs)
{
    return static_cast<Rotation>(static_cast<int>(lhs) % rhs);
}

inline Rotation operator++(Rotation& e, int)
{
    Rotation result = e;
    e = e + 1;
    return result;
}

inline Rotation& operator--(Rotation& e)
{
    e = e - 1;
    return e;
}

inline bool rotationIsValid(int rotation)
{
    return rotation >= ROTATION_FIRST && rotation < ROTATION_COUNT;
}

inline Rotation rotationFromFid(int fid)
{
    int rotation = (fid & 0x70000000) >> 28;
    return static_cast<Rotation>(rotation);
}

enum ObjectType : int {
    OBJ_TYPE_INVALID = -1,
    OBJ_TYPE_ITEM,
    OBJ_TYPE_CRITTER,
    OBJ_TYPE_SCENERY,
    OBJ_TYPE_WALL,
    OBJ_TYPE_TILE,
    OBJ_TYPE_MISC,
    OBJ_TYPE_INTERFACE,
    OBJ_TYPE_INVENTORY,
    OBJ_TYPE_HEAD,
    OBJ_TYPE_BACKGROUND,
    OBJ_TYPE_SKILLDEX,
    OBJ_TYPE_COUNT,
    OBJ_TYPE_PROTO_COUNT = OBJ_TYPE_INTERFACE,
    OBJ_TYPE_FIRST = OBJ_TYPE_ITEM
};

inline ObjectType operator++(ObjectType& e, int)
{
    ObjectType result = e;
    e = static_cast<ObjectType>(static_cast<int>(e) + 1);
    return result;
}

inline bool objectTypeIsValid(int type)
{
    return type >= OBJ_TYPE_FIRST && type < OBJ_TYPE_COUNT;
}

inline ObjectType objectTypeFromFid(int fid)
{
    int objectType = (fid & 0xF000000) >> 24;
    return static_cast<ObjectType>(objectType);
}

inline ObjectType objectTypeFromPid(int pid)
{
    int objectType = pid >> 24;
    return static_cast<ObjectType>(objectType);
}

#define SID_TYPE(value) (value) >> 24

enum OutlineType : int {
    OUTLINE_TYPE_NONE = 0x00,
    OUTLINE_TYPE_HOSTILE = 0x01,
    OUTLINE_TYPE_SAME_TEAM = 0x02,
    OUTLINE_TYPE_BODY = 0x04,
    OUTLINE_TYPE_FRIENDLY = 0x08,
    OUTLINE_TYPE_ITEM = 0x10,
    OUTLINE_TYPE_BLOCKED = 0x20,
    OUTLINE_TYPE_MAX = 0xFFFFFF
};

#define OUTLINE_PALETTED 0x40000000
#define OUTLINE_DISABLED 0x80000000

constexpr inline OutlineType operator&(OutlineType lhs, OutlineType rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr inline OutlineType operator&(OutlineType lhs, unsigned int rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) & rhs);
}

constexpr inline OutlineType operator&(OutlineType lhs, int rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) & rhs);
}

constexpr inline OutlineType operator|(OutlineType lhs, int rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) | rhs);
}

constexpr inline OutlineType operator|(OutlineType lhs, unsigned int rhs)
{
    return static_cast<OutlineType>(static_cast<int>(lhs) | rhs);
}

inline OutlineType& operator&=(OutlineType& lhs, unsigned int rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline OutlineType& operator|=(OutlineType& lhs, unsigned int rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

enum ObjectFlags : unsigned int {
    OBJECT_NONE = 0x00,
    OBJECT_HIDDEN = 0x01,
    OBJECT_0X02 = 0x02,

    // Specifies that the object should not be saved to the savegame file.
    //
    // This flag is used in these situations:
    //  - To prevent saving of system objects like dude (which has separate
    // saving routine), egg, mouse cursors, etc.
    //  - To prevent saving of temporary objects (projectiles, explosion
    // effects, etc.).
    //  - To prevent saving of objects which cannot be removed for some reason,
    // like objects trying to delete themselves from scripting engine (used
    // together with `OBJECT_HIDDEN` to prevent affecting game world).
    OBJECT_NO_SAVE = 0x04,
    OBJECT_FLAT = 0x08,
    OBJECT_NO_BLOCK = 0x10,
    OBJECT_LIGHTING = 0x20,

    // Specifies that the object should not be removed (freed) from the game
    // world for whatever reason.
    //
    // This flag is used to prevent freeing of system objects like dude, egg,
    // mouse cursors, etc.
    OBJECT_NO_REMOVE = 0x400,
    OBJECT_MULTIHEX = 0x800,
    OBJECT_NO_HIGHLIGHT = 0x1000,
    OBJECT_QUEUED = 0x2000, // set if there was/is any event for the object
    OBJECT_TRANS_RED = 0x4000,
    OBJECT_TRANS_NONE = 0x8000,
    OBJECT_TRANS_WALL = 0x10000,
    OBJECT_TRANS_GLASS = 0x20000,
    OBJECT_TRANS_STEAM = 0x40000,
    OBJECT_TRANS_ENERGY = 0x80000,
    OBJECT_IN_LEFT_HAND = 0x1000000,
    OBJECT_IN_RIGHT_HAND = 0x2000000,
    OBJECT_WORN = 0x4000000,
    OBJECT_WALL_TRANS_END = 0x10000000,
    OBJECT_LIGHT_THRU = 0x20000000,
    OBJECT_SEEN = 0x40000000,
    OBJECT_SHOOT_THRU = 0x80000000,

    OBJECT_IN_ANY_HAND = OBJECT_IN_LEFT_HAND | OBJECT_IN_RIGHT_HAND,
    OBJECT_EQUIPPED = OBJECT_IN_ANY_HAND | OBJECT_WORN,
    OBJECT_FLAG_0xFC000 = OBJECT_TRANS_ENERGY | OBJECT_TRANS_STEAM | OBJECT_TRANS_GLASS | OBJECT_TRANS_WALL | OBJECT_TRANS_NONE | OBJECT_TRANS_RED,
    OBJECT_OPEN_DOOR = OBJECT_SHOOT_THRU | OBJECT_LIGHT_THRU | OBJECT_NO_BLOCK,
};

constexpr inline ObjectFlags operator&(ObjectFlags lhs, ObjectFlags rhs)
{
    return static_cast<ObjectFlags>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline ObjectFlags operator|(ObjectFlags lhs, ObjectFlags rhs)
{
    return static_cast<ObjectFlags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

constexpr inline ObjectFlags operator~(ObjectFlags rhs)
{
    return static_cast<ObjectFlags>(~static_cast<unsigned int>(rhs));
}

constexpr inline ObjectFlags operator^(ObjectFlags lhs, ObjectFlags rhs)
{
    return static_cast<ObjectFlags>(static_cast<unsigned int>(lhs) ^ static_cast<unsigned int>(rhs));
}

inline ObjectFlags& operator&=(ObjectFlags& lhs, ObjectFlags rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline ObjectFlags& operator|=(ObjectFlags& lhs, ObjectFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline ObjectFlags& operator^=(ObjectFlags& lhs, ObjectFlags rhs)
{
    lhs = lhs ^ rhs;
    return lhs;
}

enum DudeState : int {
    DUDE_STATE_SNEAKING = 0,
    DUDE_STATE_POISONED = 1,
    DUDE_STATE_RADIATED = 2,
    DUDE_STATE_LEVEL_UP_AVAILABLE = 3,
    DUDE_STATE_ADDICTED = 4,
    DUDE_STATE_COUNT = 5,
    DUDE_STATE_FIRST = DUDE_STATE_SNEAKING
};

inline bool dudeStateIsValid(int state)
{
    return state >= DUDE_STATE_FIRST && state < DUDE_STATE_COUNT;
}

enum CritterFlags : int {
    CRITTER_NONE = 0x00,
    // CRITTER_DUDE_XXX are valid only for PC
    CRITTER_DUDE_SNEAKING = 0x01,
    CRITTER_DUDE_RADIATED = 0x02,
    CRITTER_DUDE_LEVEL_UP_AVAILABLE = 0x08,
    CRITTER_DUDE_ADDICTED = 0x10,
    CRITTER_BARTER = 0x02,
    CRITTER_NO_STEAL = 0x20,
    CRITTER_NO_DROP = 0x40,
    CRITTER_NO_LIMBS = 0x80,
    CRITTER_NO_AGE = 0x100,
    CRITTER_NO_HEAL = 0x200,
    CRITTER_INVULNERABLE = 0x400,
    CRITTER_FLAT = 0x800,
    CRITTER_SPECIAL_DEATH = 0x1000,
    CRITTER_LONG_LIMBS = 0x2000,
    CRITTER_NO_KNOCKBACK = 0x4000,
};

constexpr inline CritterFlags operator&(CritterFlags lhs, CritterFlags rhs)
{
    return static_cast<CritterFlags>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr inline CritterFlags operator|(CritterFlags lhs, CritterFlags rhs)
{
    return static_cast<CritterFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr inline CritterFlags operator~(CritterFlags rhs)
{
    return static_cast<CritterFlags>(~static_cast<int>(rhs));
}

inline CritterFlags& operator&=(CritterFlags& lhs, CritterFlags rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline CritterFlags& operator|=(CritterFlags& lhs, CritterFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

// These two values are the same but stored in different fields.
#define CONTAINER_FLAG_JAMMED 0x04000000
#define DOOR_FLAG_JAMMGED 0x04000000

#define CONTAINER_FLAG_LOCKED 0x02000000
#define DOOR_FLAG_LOCKED 0x02000000

enum CritterManeuver : int {
    CRITTER_MANEUVER_NONE = 0,
    CRITTER_MANEUVER_ENGAGING = 0x01,
    CRITTER_MANEUVER_DISENGAGING = 0x02,
    CRITTER_MANUEVER_FLEEING = 0x04,
};

constexpr inline CritterManeuver operator&(CritterManeuver lhs, CritterManeuver rhs)
{
    return static_cast<CritterManeuver>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr inline CritterManeuver operator|(CritterManeuver lhs, CritterManeuver rhs)
{
    return static_cast<CritterManeuver>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr inline CritterManeuver operator~(CritterManeuver rhs)
{
    return static_cast<CritterManeuver>(~static_cast<int>(rhs));
}

inline CritterManeuver& operator&=(CritterManeuver& lhs, CritterManeuver rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline CritterManeuver& operator|=(CritterManeuver& lhs, CritterManeuver rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

enum Dam : int {
    DAM_NONE = 0x00,
    DAM_KNOCKED_OUT = 0x01,
    DAM_KNOCKED_DOWN = 0x02,
    DAM_CRIP_LEG_LEFT = 0x04,
    DAM_CRIP_LEG_RIGHT = 0x08,
    DAM_CRIP_ARM_LEFT = 0x10,
    DAM_CRIP_ARM_RIGHT = 0x20,
    DAM_BLIND = 0x40,
    DAM_DEAD = 0x80,
    DAM_HIT = 0x100,
    DAM_CRITICAL = 0x200,
    DAM_ON_FIRE = 0x400,
    DAM_BYPASS = 0x800,
    DAM_EXPLODE = 0x1000,
    DAM_DESTROY = 0x2000,
    DAM_DROP = 0x4000,
    DAM_LOSE_TURN = 0x8000,
    DAM_HIT_SELF = 0x10000,
    DAM_LOSE_AMMO = 0x20000,
    DAM_DUD = 0x40000,
    DAM_HURT_SELF = 0x80000,
    DAM_RANDOM_HIT = 0x100000,
    DAM_CRIP_RANDOM = 0x200000,
    DAM_BACKWASH = 0x400000,
    DAM_PERFORM_REVERSE = 0x800000,
    DAM_CRIP_LEG_ANY = DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT,
    DAM_CRIP_ARM_ANY = DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT,
    DAM_CRIP = DAM_CRIP_LEG_ANY | DAM_CRIP_ARM_ANY | DAM_BLIND,
};

constexpr inline Dam operator&(Dam lhs, Dam rhs)
{
    return static_cast<Dam>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr inline Dam operator|(Dam lhs, Dam rhs)
{
    return static_cast<Dam>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr inline Dam operator~(Dam rhs)
{
    return static_cast<Dam>(~static_cast<int>(rhs));
}

inline Dam& operator&=(Dam& lhs, Dam rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline Dam& operator|=(Dam& lhs, Dam rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

#define OBJ_LOCKED 0x02000000
#define OBJ_JAMMED 0x04000000

typedef struct Object Object;

typedef struct InventoryItem {
    Object* item;
    int quantity;
} InventoryItem;

// Represents inventory of the object.
typedef struct Inventory {
    int length;
    int capacity;
    InventoryItem* items;
} Inventory;

typedef struct WeaponObjectData {
    int ammoQuantity; // obj_pudg.pudweapon.cur_ammo_quantity
    int ammoTypePid; // obj_pudg.pudweapon.cur_ammo_type_pid
} WeaponObjectData;

typedef struct AmmoItemData {
    int quantity; // obj_pudg.pudammo.cur_ammo_quantity
} AmmoItemData;

typedef struct MiscItemData {
    int charges; // obj_pudg.pudmisc_item.curr_charges
} MiscItemData;

typedef struct KeyItemData {
    int keyCode; // obj_pudg.pudkey_item.cur_key_code
} KeyItemData;

typedef union ItemObjectData {
    WeaponObjectData weapon;
    AmmoItemData ammo;
    MiscItemData misc;
    KeyItemData key;
} ItemObjectData;

typedef struct CritterCombatData {
    CritterManeuver maneuver; // obj_pud.combat_data.maneuver
    int ap; // obj_pud.combat_data.curr_mp
    Dam results; // obj_pud.combat_data.results
    int damageLastTurn; // obj_pud.combat_data.damage_last_turn
    int aiPacket; // obj_pud.combat_data.ai_packet
    int team; // obj_pud.combat_data.team_num
    union {
        Object* whoHitMe; // obj_pud.combat_data.who_hit_me
        int whoHitMeCid;
    };
} CritterCombatData;

typedef struct CritterObjectData {
    int reaction; // obj_pud.reaction_to_pc (unused)
    CritterCombatData combat; // obj_pud.combat_data
    int hp; // obj_pud.curr_hp
    int radiation; // obj_pud.curr_rad
    int poison; // obj_pud.curr_poison
} CritterObjectData;

typedef struct DoorSceneryData {
    int openFlags; // obj_pudg.pudportal.cur_open_flags
} DoorSceneryData;

typedef struct StairsSceneryData {
    int destinationMap; // obj_pudg.pudstairs.destMap
    int destinationBuiltTile; // obj_pudg.pudstairs.destBuiltTile
} StairsSceneryData;

typedef struct ElevatorSceneryData {
    int type;
    int level;
} ElevatorSceneryData;

typedef struct LadderSceneryData {
    int destinationMap;
    int destinationBuiltTile;
} LadderSceneryData;

typedef union SceneryObjectData {
    DoorSceneryData door;
    StairsSceneryData stairs;
    ElevatorSceneryData elevator;
    LadderSceneryData ladder;
} SceneryObjectData;

typedef struct MiscObjectData {
    int map;
    int tile;
    int elevation;
    Rotation rotation;
} MiscObjectData;

// TODO: use C-style inheritance for different ObjectData variants instead of unions within unions.
typedef struct ObjectData {
    Inventory inventory;
    union {
        CritterObjectData critter;
        struct {
            int flags;
            union {
                ItemObjectData item;
                SceneryObjectData scenery;
                MiscObjectData misc;
            };
        };
    };
} ObjectData;

typedef struct Object {
    int id; // obj_id
    int tile; // obj_tile_num
    int x; // obj_x
    int y; // obj_y
    int sx; // obj_sx
    int sy; // obj_sy
    int frame; // obj_cur_frm
    Rotation rotation; // obj_cur_rot
    int fid; // obj_fid
    ObjectFlags flags; // obj_flags
    int elevation; // obj_elev
    ObjectData data;
    int pid; // obj_pid
    int cid; // obj_cid
    int lightDistance; // obj_light_distance
    int lightIntensity; // obj_light_intensity
    OutlineType outline; // obj_outline
    int sid; // obj_sid
    Object* owner;
    int scriptIndex; // TODO: remove
} Object;

typedef struct ObjectListNode {
    Object* obj;
    struct ObjectListNode* next;
} ObjectListNode;

#define BUILT_TILE_TILE_MASK 0x3FFFFFF
#define BUILT_TILE_ELEVATION_MASK 0xE0000000
#define BUILT_TILE_ELEVATION_SHIFT 29
#define BUILT_TILE_ROTATION_MASK 0x1C000000
#define BUILT_TILE_ROTATION_SHIFT 26

static inline int builtTileGetTile(int builtTile)
{
    return builtTile & BUILT_TILE_TILE_MASK;
}

static inline int builtTileGetElevation(int builtTile)
{
    return (builtTile & BUILT_TILE_ELEVATION_MASK) >> BUILT_TILE_ELEVATION_SHIFT;
}

static inline Rotation builtTileGetRotation(int builtTile)
{
    return static_cast<Rotation>((builtTile & BUILT_TILE_ROTATION_MASK) >> BUILT_TILE_ROTATION_SHIFT);
}

static inline int builtTileCreate(int tile, int elevation)
{
    return tile | ((elevation << BUILT_TILE_ELEVATION_SHIFT) & BUILT_TILE_ELEVATION_MASK);
}

} // namespace fallout

#endif /* OBJ_TYPES_H */
