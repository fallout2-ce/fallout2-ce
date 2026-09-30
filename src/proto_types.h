#ifndef PROTO_TYPES_H
#define PROTO_TYPES_H

#include <cassert>
#include <type_traits>

#include "art_defs.h"
#include "obj_types.h"
#include "perk_defs.h"
#include "skill_defs.h"
#include "stat_defs.h"
#include "worldmap.h"

namespace fallout {

// Number of prototypes in prototype extent.
#define PROTO_LIST_EXTENT_SIZE 16

// Max number of prototypes of one type to be stored in prototype cache lists.
// Once this value is reached the top most proto extent is removed from the
// cache list.
//
// See:
// - [protoPtr]
// - [protoRemoveSomeList]
#define PROTO_LIST_MAX_ENTRIES 512

enum Gender : int {
    GENDER_MALE,
    GENDER_FEMALE,
    GENDER_COUNT,
};

enum ItemType : int {
    ITEM_TYPE_INVALID = -1,
    ITEM_TYPE_ARMOR,
    ITEM_TYPE_CONTAINER,
    ITEM_TYPE_DRUG,
    ITEM_TYPE_WEAPON,
    ITEM_TYPE_AMMO,
    ITEM_TYPE_MISC,
    ITEM_TYPE_KEY,
    ITEM_TYPE_COUNT,
    ITEM_TYPE_FIRST = ITEM_TYPE_ARMOR,
};

inline ItemType operator++(ItemType& e, int)
{
    ItemType result = e;
    e = static_cast<ItemType>(static_cast<int>(e) + 1);
    return result;
}

enum SceneryType : int {
    SCENERY_TYPE_DOOR,
    SCENERY_TYPE_STAIRS,
    SCENERY_TYPE_ELEVATOR,
    SCENERY_TYPE_LADDER_UP,
    SCENERY_TYPE_LADDER_DOWN,
    SCENERY_TYPE_GENERIC,
    SCENERY_TYPE_COUNT,
    SCENERY_TYPE_FIRST = SCENERY_TYPE_DOOR,
};

inline SceneryType operator++(SceneryType& e, int)
{
    SceneryType result = e;
    e = static_cast<SceneryType>(static_cast<int>(e) + 1);
    return result;
}

enum MaterialType : int {
    MATERIAL_TYPE_INVALID = -1,
    MATERIAL_TYPE_GLASS,
    MATERIAL_TYPE_METAL,
    MATERIAL_TYPE_PLASTIC,
    MATERIAL_TYPE_WOOD,
    MATERIAL_TYPE_DIRT,
    MATERIAL_TYPE_STONE,
    MATERIAL_TYPE_CEMENT,
    MATERIAL_TYPE_LEATHER,
    MATERIAL_TYPE_COUNT,
    MATERIAL_TYPE_FIRST = MATERIAL_TYPE_GLASS
};

inline MaterialType operator++(MaterialType& e, int)
{
    MaterialType result = e;
    e = static_cast<MaterialType>(static_cast<int>(e) + 1);
    return result;
}

enum DamageType : int {
    DAMAGE_TYPE_NORMAL,
    DAMAGE_TYPE_LASER,
    DAMAGE_TYPE_FIRE,
    DAMAGE_TYPE_PLASMA,
    DAMAGE_TYPE_ELECTRICAL,
    DAMAGE_TYPE_EMP,
    DAMAGE_TYPE_EXPLOSION,
    DAMAGE_TYPE_COUNT,
    DAMAGE_TYPE_FIRST = DAMAGE_TYPE_NORMAL,
};

inline Stat operator+(Stat lhs, DamageType rhs)
{
    return static_cast<Stat>(static_cast<int>(lhs) + static_cast<int>(rhs));
}

inline DamageType operator++(DamageType& e, int)
{
    DamageType result = e;
    e = static_cast<DamageType>(static_cast<int>(e) + 1);
    return result;
}

inline bool damageTypeIsValid(int damageType)
{
    return damageType >= DAMAGE_TYPE_FIRST && damageType < DAMAGE_TYPE_COUNT;
}

enum CaliberType : int {
    CALIBER_TYPE_NONE,
    CALIBER_TYPE_ROCKET,
    CALIBER_TYPE_FLAMETHROWER_FUEL,
    CALIBER_TYPE_C_ENERGY_CELL,
    CALIBER_TYPE_D_ENERGY_CELL,
    CALIBER_TYPE_223,
    CALIBER_TYPE_5_MM,
    CALIBER_TYPE_40_CAL,
    CALIBER_TYPE_10_MM,
    CALIBER_TYPE_44_CAL,
    CALIBER_TYPE_14_MM,
    CALIBER_TYPE_12_GAUGE,
    CALIBER_TYPE_9_MM,
    CALIBER_TYPE_BB,
    CALIBER_TYPE_45_CAL,
    CALIBER_TYPE_2_MM,
    CALIBER_TYPE_4_7_MM_CASELESS,
    CALIBER_TYPE_NH_NEEDLER,
    CALIBER_TYPE_7_62,
    CALIBER_TYPE_COUNT,
    CALIBER_TYPE_FIRST = CALIBER_TYPE_NONE
};

inline CaliberType operator++(CaliberType& e, int)
{
    CaliberType result = e;
    e = static_cast<CaliberType>(static_cast<int>(e) + 1);
    return result;
}

enum RaceType : int {
    RACE_TYPE_CAUCASIAN,
    RACE_TYPE_AFRICAN,
    RACE_TYPE_COUNT,
    RACE_TYPE_FIRST = RACE_TYPE_CAUCASIAN,
};

inline RaceType operator++(RaceType& e, int)
{
    RaceType result = e;
    e = static_cast<RaceType>(static_cast<int>(e) + 1);
    return result;
}

enum BodyType : int {
    BODY_TYPE_BIPED,
    BODY_TYPE_QUADRUPED,
    BODY_TYPE_ROBOTIC,
    BODY_TYPE_COUNT,
    BODY_TYPE_FIRST = BODY_TYPE_BIPED,
};

inline BodyType operator++(BodyType& e, int)
{
    BodyType result = e;
    e = static_cast<BodyType>(static_cast<int>(e) + 1);
    return result;
}

enum KillType : int {
    KILL_TYPE_INVALID = -1,
    KILL_TYPE_MAN,
    KILL_TYPE_WOMAN,
    KILL_TYPE_CHILD,
    KILL_TYPE_SUPER_MUTANT,
    KILL_TYPE_GHOUL,
    KILL_TYPE_BRAHMIN,
    KILL_TYPE_RADSCORPION,
    KILL_TYPE_RAT,
    KILL_TYPE_FLOATER,
    KILL_TYPE_CENTAUR,
    KILL_TYPE_ROBOT,
    KILL_TYPE_DOG,
    KILL_TYPE_MANTIS,
    KILL_TYPE_DEATH_CLAW,
    KILL_TYPE_PLANT,
    KILL_TYPE_GECKO,
    KILL_TYPE_ALIEN,
    KILL_TYPE_GIANT_ANT,
    KILL_TYPE_BIG_BAD_BOSS,
    KILL_TYPE_DEFAULT_COUNT = 19,

    // Sfall has the option to treat kill type numbers as shorts, thus doubling
    // number of kill types it can deal with without breaking backwards
    // compatibility.
    KILL_TYPE_OVERRIDE_COUNT = KILL_TYPE_DEFAULT_COUNT * 2,
    KILL_TYPE_PLAYER = KILL_TYPE_OVERRIDE_COUNT,
    KILL_TYPE_FIRST = KILL_TYPE_MAN,
};

inline KillType operator++(KillType& e, int)
{
    KillType result = e;
    e = static_cast<KillType>(static_cast<int>(e) + 1);
    return result;
}

inline bool killTypeIsValid(int killType)
{
    return killType >= KILL_TYPE_FIRST && killType < KILL_TYPE_DEFAULT_COUNT;
}

inline bool killTypeOverrideIsValid(int killType)
{
    // killTypeOverrideIsValid evaluates player's kill type as valid even though KILL_TYPE_PLAYER equals KILL_TYPE_OVERRIDE_COUNT
    return killType >= KILL_TYPE_FIRST && killType <= KILL_TYPE_OVERRIDE_COUNT;
}

enum class ItemProtoTypeId : int {
    PowerArmor = 3,
    SmallEnergyCell = 38,
    MicroFusionCell = 39,
    Stimpak = 40,
    Money = 41,
    FirstAidKit = 47,
    Radaway = 48,
    Dynamite = 51,
    GeigerCounter = 52,
    Mentats = 53,
    StealthBoy = 54,
    MotionSensor = 59,
    BigBookOfScience = 73,
    DeansElectronics = 76,
    Flare = 79,
    FirstAidBook = 80,
    PlasticExplosives = 85,
    ScoutHandBook = 86,
    Buffout = 87,
    DoctorsBag = 91,
    GunsAndBullets = 102,
    NukaCola = 106,
    Psycho = 110,
    Beer = 124,
    Booze = 125,
    SuperStimpak = 144,
    MolotovCocktail = 159,
    LitFlare = 205,
    ArmedDynamite = 206,
    ActivatedGeigerCounter = 207,
    ArmedPlasticExplosives = 209,
    ActivatedStealthBoy = 210,
    HardenedPowerArmor = 232,
    Jet = 259,
    JetAntidote = 260,
    HealingPowder = 273,
    DeckOfTragicCards = 304,
    CatsPawIssue5 = 331,
    AdvancedPowerArmor = 348,
    AdvancedPowerArmorMkII = 349,
    Shiv = 383,
    SolarScorcher = 390,
    SuperCattleProd = 399,
    MegaPowerFist = 407,
    FieldMedicsFirstAidKit = 408,
    ParamedicsBag = 409,
    RamirezBoxClosed = 431,
    MirroredShades = 433,
    RaidersMap = 444,
    CarTrunk = 455,
    JesseContainer = 467,
    PipBoyLingualEnhancer = 499,
    PipBoyMedicalEnhancer = 516,
    SurveyMap = 523,
};

enum class CritterProtoTypeId : int {
    Dude = 0,
    GunGuardFemale = 47,
    Cyberdog = 136,
    Goris = 152,
    Marcus = 161,
    EnclavePatrolMale = 250,
    Dogmeat = 342,
    PariahDog = 384,
    AutoCannon = 480
};

enum class SceneryProtoTypeId : int {
    ExitGridAutomapMarker = 49,
    BrotherhoodDoor = 153,
    BlockingHexAutomap = 344,
    ElevatorDoor = 421,
    ElevatorDoorAlternate = 470,
    ForceFieldNorthSouth = 491,
    FirstRadioactiveGoo = 985,
    LastRadioactiveGoo = 988,
    Car = 1009,
    ElevatorStub = 1293
};

enum class MiscProtoTypeId : int {
    Blood = 4,
    Id0x0C = 12,
    FirstExitGrid = 16,
    LastExitGrid = 23
};

enum class WallProtoTypeId : int {};
enum class TileProtoTypeId : int {};

enum ProtoFlags : unsigned int {
    PROTO_FLAG_NONE = 0x00,
    PROTO_FLAG_FLAT = 0x08,
    PROTO_FLAG_NO_BLOCK = 0x10,
    PROTO_FLAG_MULTIHEX = 0x800,
    PROTO_FLAG_NO_HIGHLIGHT = 0x1000,
    PROTO_FLAG_TRANS_RED = 0x4000,
    PROTO_FLAG_TRANS_NONE = 0x8000,
    PROTO_FLAG_TRANS_WALL = 0x10000,
    PROTO_FLAG_TRANS_GLASS = 0x20000,
    PROTO_FLAG_TRANS_STEAM = 0x40000,
    PROTO_FLAG_TRANS_ENERGY = 0x80000,
    PROTO_FLAG_WALL_TRANS_END = 0x10000000,
    PROTO_FLAG_LIGHT_THRU = 0x20000000,
    PROTO_FLAG_SHOOT_THRU = 0x80000000,
};

constexpr inline ProtoFlags operator&(ProtoFlags lhs, ProtoFlags rhs)
{
    return static_cast<ProtoFlags>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline ProtoFlags operator|(ProtoFlags lhs, ProtoFlags rhs)
{
    return static_cast<ProtoFlags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

enum ProtoExtendedFlags : unsigned int {
    PROTO_EXT_FLAG_NONE = 0x0000,

    // NOTE: `extendedFlags` packs non-boolean weapon data into the low
    // nibbles (`0x0F` and `0xF0`) for attack mode metadata.

    PROTO_EXT_FLAG_BIG_GUN = 0x0100,
    PROTO_EXT_FLAG_IS_TWO_HANDED = 0x0200,
    // PROTO_EXT_FLAG_ENERGY = 0x0400, // Sfall
    PROTO_EXT_FLAG_CAN_USE = 0x0800,
    PROTO_EXT_FLAG_CAN_USE_ON = 0x1000,
    PROTO_EXT_FLAG_LOOK = 0x2000, // never checked, AFAICT
    PROTO_EXT_FLAG_CAN_TALK_TO = 0x4000,
    PROTO_EXT_FLAG_CAN_PICK_UP = 0x8000,

    // This flag is used on weapons to indicate that's an natural (integral)
    // part of it's owner, for example Claw, or Robot's Rocket Launcher. Items
    // with this flag on do count toward total weight and cannot be dropped.
    // Also used with scenery.
    PROTO_EXT_FLAG_HIDDEN = 0x08000000,

    // Scenery using this flag plays the ground-level magic hands animation.
    PROTO_EXT_FLAG_MAGIC_HANDS_GROUND = 0x0001,

    // Wall/scenery corner orientation flags, used for lighting and visibility checks
    PROTO_EXT_FLAG_NORTH_CORNER = 0x10000000,
    PROTO_EXT_FLAG_SOUTH_CORNER = 0x20000000,
    PROTO_EXT_FLAG_EAST_CORNER = 0x40000000,
    PROTO_EXT_FLAG_WEST_CORNER = 0x80000000,
};

constexpr inline ProtoExtendedFlags operator|(ProtoExtendedFlags lhs, ProtoExtendedFlags rhs)
{
    return static_cast<ProtoExtendedFlags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

inline ProtoExtendedFlags& operator|=(ProtoExtendedFlags& lhs, ProtoExtendedFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline ProtoExtendedFlags& operator^=(ProtoExtendedFlags& lhs, ProtoExtendedFlags rhs)
{
    lhs = static_cast<ProtoExtendedFlags>(static_cast<unsigned int>(lhs) ^ static_cast<unsigned int>(rhs));
    return lhs;
}

typedef struct {
    int armorClass; // d.ac
    int damageResistance[7]; // d.dam_resist
    int damageThreshold[7]; // d.dam_thresh
    Perk perk; // d.perk
    int maleFid; // d.male_fid
    int femaleFid; // d.female_fid
} ProtoItemArmorData;

typedef struct {
    int maxSize; // d.max_size
    int openFlags; // d.open_flags
} ProtoItemContainerData;

typedef struct {
    Stat stat[3]; // d.stat
    int amount[3]; // d.amount
    int duration1; // d.duration1
    int amount1[3]; // d.amount1
    int duration2; // d.duration2
    int amount2[3]; // d.amount2
    int addictionChance; // d.addiction_chance
    Perk withdrawalEffect; // d.withdrawal_effect
    int withdrawalOnset; // d.withdrawal_onset
} ProtoItemDrugData;

typedef struct {
    WeaponAnimation animationCode; // d.animation_code
    int minDamage; // d.min_damage
    int maxDamage; // d.max_damage
    DamageType damageType; // d.dt
    int maxRange1; // d.max_range1
    int maxRange2; // d.max_range2
    int projectilePid; // d.proj_pid
    int minStrength; // d.min_st
    int actionPointCost1; // d.mp_cost1
    int actionPointCost2; // d.mp_cost2
    int criticalFailureType; // d.crit_fail_table
    Perk perk; // d.perk
    int rounds; // d.rounds
    CaliberType caliber; // d.caliber
    int ammoTypePid; // d.ammo_type_pid
    int ammoCapacity; // d.max_ammo
    unsigned char soundCode; // d.sound_id
} ProtoItemWeaponData;

typedef struct {
    CaliberType caliber; // d.caliber
    int quantity; // d.quantity
    int armorClassModifier; // d.ac_adjust
    int damageResistanceModifier; // d.dr_adjust
    int damageMultiplier; // d.dam_mult
    int damageDivisor; // d.dam_div
} ProtoItemAmmoData;

typedef struct {
    int powerTypePid; // d.power_type_pid
    int powerType; // d.power_type
    int charges; // d.charges
} ProtoItemMiscData;

typedef struct {
    int keyCode; // d.key_code
} ProtoItemKeyData;

typedef struct ItemProtoData {
    union {
        struct {
            int field_0;
            int field_4;
            int field_8; // max charges
            int field_C;
            int field_10;
            int field_14;
            int field_18;
        } unknown;
        ProtoItemArmorData armor;
        ProtoItemContainerData container;
        ProtoItemDrugData drug;
        ProtoItemWeaponData weapon;
        ProtoItemAmmoData ammo;
        ProtoItemMiscData misc;
        ProtoItemKeyData key;
    };
} ItemProtoData;

typedef struct ItemProto {
    int pid; // pid
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    ProtoFlags flags; // flags
    ProtoExtendedFlags extendedFlags; // flags_ext
    int sid; // sid
    ItemType type; // type
    ItemProtoData data; // d
    MaterialType material; // material
    int size; // size
    int weight; // weight
    int cost; // cost
    int inventoryFid; // inv_fid
    unsigned char soundId;
} ItemProto;

typedef struct CritterProtoData {
    CritterFlags flags; // d.flags
    int baseStats[SAVEABLE_STAT_COUNT]; // d.stat_base
    int bonusStats[SAVEABLE_STAT_COUNT]; // d.stat_bonus
    int skills[SKILL_COUNT]; // d.stat_points
    BodyType bodyType; // d.body
    int experience;
    KillType killType;
    // Looks like this is the "native" damage type when critter is unarmed.
    DamageType damageType;
} CritterProtoData;

typedef struct CritterProto {
    int pid; // pid
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    ProtoFlags flags; // flags
    ProtoExtendedFlags extendedFlags; // flags_ext
    int sid; // sid
    CritterProtoData data; // d
    int headFid; // head_fid
    int aiPacket; // ai_packet
    int team; // team_num
} CritterProto;

typedef struct {
    int openFlags; // d.open_flags
    int keyCode; // d.key_code
} SceneryProtoDoorData;

typedef struct {
    int destinationBuiltTile; // d.lower_tile
    Map destinationMap; // d.upper_tile
} SceneryProtoStairsData;

typedef struct {
    int type;
    int level;
} SceneryProtoElevatorData;

typedef struct {
    int destinationBuiltTile; // destination built tile
} SceneryProtoLadderData;

typedef struct {
    int genericFlags;
} SceneryProtoGenericData;

typedef struct SceneryProtoData {
    union {
        SceneryProtoDoorData door;
        SceneryProtoStairsData stairs;
        SceneryProtoElevatorData elevator;
        SceneryProtoLadderData ladder;
        SceneryProtoGenericData generic;
    };
} SceneryProtoData;

typedef struct SceneryProto {
    int pid; // id
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    ProtoFlags flags; // flags
    ProtoExtendedFlags extendedFlags; // flags_ext
    int sid; // sid
    SceneryType type; // type
    SceneryProtoData data;
    MaterialType material; // material
    int field_30; //
    unsigned char soundId;
} SceneryProto;

typedef struct WallProto {
    int pid; // id
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    ProtoFlags flags; // flags
    ProtoExtendedFlags extendedFlags; // flags_ext
    int sid; // sid
    MaterialType material; // material
} WallProto;

typedef struct TileProto {
    int pid; // id
    int messageId; // message_num
    int fid; // fid
    ProtoFlags flags; // flags
    ProtoExtendedFlags extendedFlags; // flags_ext
    int sid; // sid
    MaterialType material; // material
} TileProto;

typedef struct MiscProto {
    int pid; // id
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    ProtoFlags flags; // flags
    ProtoExtendedFlags extendedFlags; // flags_ext
} MiscProto;

typedef union Proto {
    struct {
        int pid; // pid
        int messageId; // message_num
        int fid; // fid

        // TODO: Move to NonTile props?
        int lightDistance;
        int lightIntensity;
        ProtoFlags flags;
        ProtoExtendedFlags extendedFlags;
        int sid;
    };
    ItemProto item;
    CritterProto critter;
    SceneryProto scenery;
    WallProto wall;
    TileProto tile;
    MiscProto misc;
} Proto;

typedef struct ProtoListExtent {
    Proto* proto[PROTO_LIST_EXTENT_SIZE];
    // Number of protos in the extent
    int length;
    struct ProtoListExtent* next;
} ProtoListExtent;

typedef struct ProtoList {
    ProtoListExtent* head;
    ProtoListExtent* tail;
    // Number of extents in the list.
    int length;
    // Number of lines in proto/{type}/{type}.lst.
    int max_entries_num;
} ProtoList;

constexpr inline int protoIdFromPid(int pid)
{
    return pid & 0xFFFFFF;
}

template <typename T>
struct MapProtoTypeIdToObjectType;

template <>
struct MapProtoTypeIdToObjectType<ItemProtoTypeId> {
    static constexpr ObjectType value = OBJ_TYPE_ITEM;
};

template <>
struct MapProtoTypeIdToObjectType<CritterProtoTypeId> {
    static constexpr ObjectType value = OBJ_TYPE_CRITTER;
};

template <>
struct MapProtoTypeIdToObjectType<SceneryProtoTypeId> {
    static constexpr ObjectType value = OBJ_TYPE_SCENERY;
};

template <>
struct MapProtoTypeIdToObjectType<WallProtoTypeId> {
    static constexpr ObjectType value = OBJ_TYPE_WALL;
};

template <>
struct MapProtoTypeIdToObjectType<TileProtoTypeId> {
    static constexpr ObjectType value = OBJ_TYPE_TILE;
};

template <>
struct MapProtoTypeIdToObjectType<MiscProtoTypeId> {
    static constexpr ObjectType value = OBJ_TYPE_MISC;
};

class ProtoId {
public:
    static constexpr int kEmptyPid = -1;
    static constexpr int kInvalidProtoId = -1;
    static constexpr int kMinProtoId = 0;

    constexpr ProtoId()
        : ProtoId(OBJ_TYPE_INVALID, kEmptyPid, kInvalidProtoId)
    {
    }

    constexpr explicit ProtoId(int pid)
        : ProtoId(pid == kEmptyPid ? OBJ_TYPE_INVALID : objectTypeFromPid(pid), pid, pid)
    {
    }

    constexpr ProtoId(const Proto* proto)
        : ProtoId(proto == nullptr ? kEmptyPid : proto->pid)
    {
    }

    constexpr ProtoId(const Object* object)
        : ProtoId(object == nullptr ? kEmptyPid : object->pid)
    {
    }

    template <typename TProtoTypeId,
        typename = std::void_t<
            decltype(MapProtoTypeIdToObjectType<TProtoTypeId>::value)>>
    constexpr ProtoId(TProtoTypeId protoId)
        : ProtoId(MapProtoTypeIdToObjectType<TProtoTypeId>::value, buildPid(MapProtoTypeIdToObjectType<TProtoTypeId>::value, static_cast<int>(protoId)), static_cast<int>(protoId))
    {
    }

    constexpr bool hasObjectType() const { return objectTypeIsValid(_objectType); }
    constexpr bool hasPid() const { return _pid > kEmptyPid; }

    constexpr ObjectType objectType() const { return hasObjectType() ? _objectType : OBJ_TYPE_INVALID; }
    constexpr int pid() const { return _pid; }
    constexpr int protoId() const { return _protoId; }

    template <typename TProtoTypeId,
        typename = std::void_t<
            decltype(MapProtoTypeIdToObjectType<TProtoTypeId>::value)>>
    constexpr TProtoTypeId protoId() const
    {
        if (hasPid()) {
            assert(_objectType == MapProtoTypeIdToObjectType<TProtoTypeId>::value && "ProtoId::protoId<TProtoTypeId>() object type doesn't match the TProtoTypeId type!");
            if (_objectType == MapProtoTypeIdToObjectType<TProtoTypeId>::value) {
                return static_cast<TProtoTypeId>(_protoId);
            }
        }

        return static_cast<TProtoTypeId>(kInvalidProtoId);
    }

    constexpr bool operator==(const ProtoId& other) const
    {
        return _pid == other._pid && _objectType == other._objectType;
    }

    constexpr bool operator!=(const ProtoId& other) const
    {
        return _pid != other._pid || _objectType != other._objectType;
    }

    template <typename TProtoTypeId,
        typename = std::void_t<
            decltype(MapProtoTypeIdToObjectType<TProtoTypeId>::value)>>
    constexpr bool operator==(TProtoTypeId protoId) const
    {
        return _pid == buildPid(MapProtoTypeIdToObjectType<TProtoTypeId>::value, static_cast<int>(protoId));
    }

    template <typename TProtoTypeId,
        typename = std::void_t<
            decltype(MapProtoTypeIdToObjectType<TProtoTypeId>::value)>>
    constexpr bool operator!=(TProtoTypeId protoId) const
    {
        return _pid != buildPid(MapProtoTypeIdToObjectType<TProtoTypeId>::value, static_cast<int>(protoId));
    }

protected:
    static constexpr int kProtoIdMask = 0x00FFFFFF;
    static constexpr int kObjectTypeMask = 0x0F000000;

    static constexpr int kObjectTypeMaskPosition = 24;

    constexpr ProtoId(ObjectType objectType, int pid, int protoId)
        : _objectType(objectType)
        , _pid(pid)
        , _protoId(protoId < kMinProtoId ? kInvalidProtoId : protoIdFromPid(protoId))
    {
    }

private:
    ObjectType _objectType;
    int _pid;
    int _protoId;

    /* PID Structure:
        4  bits unused
        4  bits for object type
        24 bits for proto id
    */
    static constexpr int buildPid(ObjectType objectType, int protoId)
    {
        if (!objectTypeIsValid(objectType) || protoId < kMinProtoId) {
            return kEmptyPid;
        }

        return ((objectType << kObjectTypeMaskPosition) & kObjectTypeMask) | (protoId & kProtoIdMask);
    }
};

} // namespace fallout

#endif /* PROTO_TYPES_H */
