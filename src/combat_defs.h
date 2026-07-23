#ifndef COMBAT_DEFS_H
#define COMBAT_DEFS_H

#include "obj_types.h"

#define EXPLOSION_TARGET_COUNT (6)

#define CRTICIAL_EFFECT_COUNT (6)

#define WEAPON_CRITICAL_FAILURE_TYPE_COUNT (7)
#define WEAPON_CRITICAL_FAILURE_EFFECT_COUNT (5)

namespace fallout {

enum CombatState : unsigned int {
    COMBAT_STATE_OUT_COMBAT = 0x00,
    COMBAT_STATE_IN_COMBAT = 0x01,
    COMBAT_STATE_PLAYER_TURN = 0x02,
    COMBAT_STATE_EXIT_REQUESTED = 0x08,
};

inline fallout::CombatState& operator&=(fallout::CombatState& lhs, unsigned int rhs)
{
    lhs = static_cast<fallout::CombatState>(static_cast<unsigned int>(lhs) & rhs);
    return lhs;
}

inline constexpr CombatState operator~(CombatState rhs)
{
    return static_cast<CombatState>(~static_cast<unsigned int>(rhs));
}

inline CombatState& operator&=(CombatState& lhs, CombatState rhs)
{
    lhs = static_cast<CombatState>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
    return lhs;
}

inline CombatState& operator|=(CombatState& lhs, CombatState rhs)
{
    lhs = static_cast<CombatState>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
    return lhs;
}

enum HitMode : int {
    HIT_MODE_INVALID = -1,
    HIT_MODE_LEFT_WEAPON_PRIMARY = 0,
    HIT_MODE_LEFT_WEAPON_SECONDARY = 1,
    HIT_MODE_RIGHT_WEAPON_PRIMARY = 2,
    HIT_MODE_RIGHT_WEAPON_SECONDARY = 3,
    HIT_MODE_PUNCH = 4,
    HIT_MODE_KICK = 5,
    HIT_MODE_LEFT_WEAPON_RELOAD = 6,
    HIT_MODE_RIGHT_WEAPON_RELOAD = 7,

    // Punch Level 2
    HIT_MODE_STRONG_PUNCH = 8,

    // Punch Level 3
    HIT_MODE_HAMMER_PUNCH = 9,

    // Punch Level 4 aka 'Lightning Punch'
    HIT_MODE_HAYMAKER = 10,

    // Punch Level 5 aka 'Chop Punch'
    HIT_MODE_JAB = 11,

    // Punch Level 6 aka 'Dragon Punch'
    HIT_MODE_PALM_STRIKE = 12,

    // Punch Level 7 aka 'Force Punch'
    HIT_MODE_PIERCING_STRIKE = 13,

    // Kick Level 2
    HIT_MODE_STRONG_KICK = 14,

    // Kick Level 3
    HIT_MODE_SNAP_KICK = 15,

    // Kick Level 4 aka 'Roundhouse Kick'
    HIT_MODE_POWER_KICK = 16,

    // Kick Level 5
    HIT_MODE_HIP_KICK = 17,

    // Kick Level 6 aka 'Jump Kick'
    HIT_MODE_HOOK_KICK = 18,

    // Kick Level 7 aka 'Death Blossom Kick'
    HIT_MODE_PIERCING_KICK = 19,
    HIT_MODE_COUNT,
    FIRST_ADVANCED_PUNCH_HIT_MODE = HIT_MODE_STRONG_PUNCH,
    LAST_ADVANCED_PUNCH_HIT_MODE = HIT_MODE_PIERCING_STRIKE,
    FIRST_ADVANCED_KICK_HIT_MODE = HIT_MODE_STRONG_KICK,
    LAST_ADVANCED_KICK_HIT_MODE = HIT_MODE_PIERCING_KICK,
    FIRST_ADVANCED_UNARMED_HIT_MODE = FIRST_ADVANCED_PUNCH_HIT_MODE,
    LAST_ADVANCED_UNARMED_HIT_MODE = LAST_ADVANCED_KICK_HIT_MODE,
};

inline static bool hitModeIsValid(int hitMode)
{
    return hitMode >= HIT_MODE_LEFT_WEAPON_PRIMARY && hitMode < HIT_MODE_COUNT;
}

inline HitMode operator++(HitMode& e, int)
{
    HitMode result = e;
    e = static_cast<HitMode>(static_cast<int>(e) + 1);
    return result;
}

enum HitLocation : int {
    HIT_LOCATION_HEAD = 0,
    HIT_LOCATION_LEFT_ARM = 1,
    HIT_LOCATION_RIGHT_ARM = 2,
    HIT_LOCATION_TORSO = 3,
    HIT_LOCATION_RIGHT_LEG = 4,
    HIT_LOCATION_LEFT_LEG = 5,
    HIT_LOCATION_EYES = 6,
    HIT_LOCATION_GROIN = 7,
    HIT_LOCATION_UNCALLED = 8,
    HIT_LOCATION_COUNT = 9,
    HIT_LOCATION_SPECIFIC_COUNT = HIT_LOCATION_COUNT - 1,
};

inline static bool hitLocationIsValid(int hitLocation)
{
    return hitLocation >= HIT_LOCATION_HEAD && hitLocation < HIT_LOCATION_COUNT;
}

inline HitLocation operator++(HitLocation& e, int)
{
    HitLocation result = e;
    e = static_cast<HitLocation>(static_cast<int>(e) + 1);
    return result;
}

typedef struct CombatStartData {
    Object* attacker;
    Object* defender;
    int actionPointsBonus;
    int accuracyBonus;
    int damageBonus;
    int minDamage;
    int maxDamage;
    int overrideAttackResults;
    int attackerResults;
    int targetResults;
} CombatStartData;

typedef struct Attack {
    Object* attacker;
    HitMode hitMode;
    Object* weapon;
    HitLocation attackHitLocation; // UNUSED?
    int attackerDamage;
    int attackerFlags;
    int ammoQuantity;
    int criticalMessageId;
    Object* defender;
    int tile;
    HitLocation defenderHitLocation;
    int defenderDamage;
    int defenderFlags;
    int defenderKnockback;
    Object* intendedTarget; // mainTarget
    int extrasLength;
    Object* extras[EXPLOSION_TARGET_COUNT];
    int extrasHitLocation[EXPLOSION_TARGET_COUNT];
    int extrasDamage[EXPLOSION_TARGET_COUNT];
    int extrasFlags[EXPLOSION_TARGET_COUNT];
    int extrasKnockback[EXPLOSION_TARGET_COUNT];
} Attack;

enum CriticalHitDataMember : int {
    CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER = 0,
    CRIT_DATA_MEMBER_FLAGS = 1,
    CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT = 2,
    CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT_MODIFIER = 3,
    CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS = 4,
    CRIT_DATA_MEMBER_MESSAGE_ID = 5,
    CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID = 6,
    CRIT_DATA_MEMBER_COUNT = 7,
};

inline static bool criticalHitDataMemberIsValid(int criticalHitDataMember)
{
    return criticalHitDataMember >= CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER && criticalHitDataMember < CRIT_DATA_MEMBER_COUNT;
}

inline CriticalHitDataMember operator++(CriticalHitDataMember& e, int)
{
    CriticalHitDataMember result = e;
    e = static_cast<CriticalHitDataMember>(static_cast<int>(e) + 1);
    return result;
}

// Provides metadata about critical hit effect.
typedef union CriticalHitDescription {
    struct {
        int damageMultiplier;

        // Damage flags that will be applied to defender.
        int flags;

        // Stat to check to upgrade this critical hit to massive critical hit or
        // -1 if there is no massive critical hit.
        int massiveCriticalStat;

        // Bonus/penalty to massive critical stat.
        int massiveCriticalStatModifier;

        // Additional damage flags if this critical hit become massive critical.
        int massiveCriticalFlags;

        int messageId;
        int massiveCriticalMessageId;
    };

    // SFALL: Allow indexed access to the data above.
    int values[CRIT_DATA_MEMBER_COUNT];
} CriticalHitDescription;

enum CombatBadShot : int {
    COMBAT_BAD_SHOT_OK = 0,
    COMBAT_BAD_SHOT_NO_AMMO = 1,
    COMBAT_BAD_SHOT_OUT_OF_RANGE = 2,
    COMBAT_BAD_SHOT_NOT_ENOUGH_AP = 3,
    COMBAT_BAD_SHOT_ALREADY_DEAD = 4,
    COMBAT_BAD_SHOT_AIM_BLOCKED = 5,
    COMBAT_BAD_SHOT_ARM_CRIPPLED = 6,
    COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED = 7
};

} // namespace fallout

#endif /* COMBAT_DEFS_H */
