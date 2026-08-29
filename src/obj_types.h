#ifndef OBJ_TYPES_H
#define OBJ_TYPES_H

#include "worldmap_defs.h"

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

enum ObjectFrameId : int {
    OBJECT_FRAME_ID_INVALID = -1,
    OBJECT_FRAME_ID_FIRST = 0,
    OBJECT_FRAME_ID_LAST = 4095
};

inline constexpr ObjectFrameId operator+(ObjectFrameId lhs, int rhs) {
    return static_cast<ObjectFrameId>(static_cast<int>(lhs) + rhs);
}

inline constexpr ObjectFrameId operator-(ObjectFrameId lhs, int rhs) {
    return static_cast<ObjectFrameId>(static_cast<int>(lhs) - rhs);
}

inline ObjectFrameId operator++(ObjectFrameId& e, int)
{
    ObjectFrameId result = e;
    e = e + 1;
    return result;
}

inline bool objectFrameIdIsValid(int frmId)
{
    return frmId >= OBJECT_FRAME_ID_FIRST && frmId <= OBJECT_FRAME_ID_LAST;
}

inline ObjectFrameId objectFrameIdFromFid(int fid)
{
    return static_cast<ObjectFrameId>(fid & 0xFFF);
}

inline ObjectFrameId objectFrameIdFromPid(int pid)
{
    return static_cast<ObjectFrameId>(pid & 0xFFFFFF);
}

enum InterfaceFrameId : int {
    INTF_FRM_ID_0 = 0, // blank.frm - used be mset000.frm for top of bouncing mouse cursor
    INTF_FRM_ID_1 = 1, // msef000.frm - hex mouse cursor
    INTF_FRM_ID_2 = 2, // egg
    INTF_FRM_ID_3 = 3, // exit grid marker
    INTF_FRM_ID_6 = 6, // skilldex button normal
    INTF_FRM_ID_7 = 7, // skilldex button pressed
    INTF_FRM_ID_8 = 8, // lilredup.frm - little red button up
    INTF_FRM_ID_9 = 9, // lilreddn.frm - little red button down
    INTF_FRM_ID_10 = 10, // map button pressed
    INTF_FRM_ID_13 = 13, // map button normal
    INTF_FRM_ID_16 = 16, // display monitor background
    INTF_FRM_ID_17 = 17, // options button pressed
    INTF_FRM_ID_18 = 18, // options button normal
    INTF_FRM_ID_31 = 31, // item button pressed
    INTF_FRM_ID_32 = 32, // item button normal
    INTF_FRM_ID_46 = 46, // inventory button pressed
    INTF_FRM_ID_47 = 47, // inventory button normal
    INTF_FRM_ID_56 = 56, // character button pressed
    INTF_FRM_ID_57 = 57, // character button normal
    INTF_FRM_ID_58 = 58, // pipboy button pressed
    INTF_FRM_ID_59 = 59, // pipboy button normal
    INTF_FRM_ID_73 = 73, // item button disabled
    INTF_FRM_ID_82 = 82, // numbers.frm - numbers for the hit points and fatigue counters
    INTF_FRM_ID_83 = 83, // green light
    INTF_FRM_ID_84 = 84, // yellow light
    INTF_FRM_ID_85 = 85, // red light
    INTF_FRM_ID_86 = 86, // perk dialog background
    INTF_FRM_ID_102 = 102, // dialog background
    INTF_FRM_ID_104 = 104, // interface bar end button
    INTF_FRM_ID_105 = 105, // end button normal
    INTF_FRM_ID_106 = 106, // end button pressed
    INTF_FRM_ID_107 = 107, // end combat button normal
    INTF_FRM_ID_108 = 108, // end combat button pressed
    INTF_FRM_ID_109 = 109, // endltgrn.frm - green lights around end turn/combat window
    INTF_FRM_ID_110 = 110, // endltred.frm - red lights around end turn/combat window
    INTF_FRM_ID_113 = 113, // inventory background
    INTF_FRM_ID_115 = 115, // hilight1.frm - dialogue upper hilight
    INTF_FRM_ID_116 = 116, // hilight2.frm - dialogue lower hilight
    INTF_FRM_ID_118 = 118, // called shot background
    INTF_FRM_ID_122 = 122, // previous button normal
    INTF_FRM_ID_123 = 123, // previous button pressed
    INTF_FRM_ID_124 = 124, // next button normal
    INTF_FRM_ID_125 = 125, // next button pressed
    INTF_FRM_ID_126 = 126, // indicator box
    INTF_FRM_ID_129 = 129, // months.frm - month strings for pip boy
    INTF_FRM_ID_136 = 136, // worldmap background
    INTF_FRM_ID_138 = 138, // wmaploc.frm - world map location marker
    INTF_FRM_ID_139 = 139, // wmaptarg.frm - world map move target maker #1
    INTF_FRM_ID_140 = 140, // main menu background image
    INTF_FRM_ID_168 = 168, // hotspot1.frm - town map selector shape #1
    INTF_FRM_ID_170 = 170, // BIGNUM.frm
    INTF_FRM_ID_174 = 174, // character selector background
    INTF_FRM_ID_209 = 209, // done box
    INTF_FRM_ID_223 = 223, // hotspot2.frm - town map selector shape #2
    INTF_FRM_ID_282 = 282, // actpick.frm - action pick
    INTF_FRM_ID_283 = 283, // actmenu.frm - action menu
    INTF_FRM_ID_284 = 284, // acttohit.frm - action to hit
    INTF_FRM_ID_285 = 285, // mirrored arrow
    INTF_FRM_ID_288 = 288, // bullseye aiming
    INTF_FRM_ID_289 = 289, // action points
    INTF_FRM_ID_290 = 290, // movement points numbers
    INTF_FRM_ID_291 = 291, // reload action menu
    INTF_FRM_ID_292 = 292, // use action menu
    INTF_FRM_ID_294 = 294, // use on action menu
    INTF_FRM_ID_297 = 297, // help background
    INTF_FRM_ID_299 = 299, // main menu button normal
    INTF_FRM_ID_300 = 300, // main menu button pressed
    INTF_FRM_ID_306 = 306, // timer overlay
    INTF_FRM_ID_307 = 307,
    INTF_FRM_ID_308 = 308,
    INTF_FRM_ID_309 = 309, // DEATH.FRM
    INTF_FRM_ID_327 = 327, // endgame ending panning screen
    INTF_FRM_ID_363 = 363, // wmscreen - worldmap overlay screen
    INTF_FRM_ID_364 = 364, // wmtabs.frm - worldmap town tabs underlay
    INTF_FRM_ID_365 = 365, // wmdial.frm - worldmap night/day dial
    INTF_FRM_ID_366 = 366, // wmglobe.frm - worldmap globe stamp overlay
    INTF_FRM_ID_367 = 367, // wmtbedge.frm - worldmap town tabs edging overlay
    INTF_FRM_ID_390 = 390, // control.frm - party member control interface
    INTF_FRM_ID_391 = 391, // custom.frm - party member control interface
    INTF_FRM_ID_419 = 419, // game dialog background
};

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
    Map destinationMap; // obj_pudg.pudstairs.destMap
    int destinationBuiltTile; // obj_pudg.pudstairs.destBuiltTile
} StairsSceneryData;

typedef struct ElevatorSceneryData {
    int type;
    int level;
} ElevatorSceneryData;

typedef struct LadderSceneryData {
    Map destinationMap;
    int destinationBuiltTile;
} LadderSceneryData;

typedef union SceneryObjectData {
    DoorSceneryData door;
    StairsSceneryData stairs;
    ElevatorSceneryData elevator;
    LadderSceneryData ladder;
} SceneryObjectData;

typedef struct MiscObjectData {
    Map map;
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
