#ifndef COMBAT_AI_DEFS_H
#define COMBAT_AI_DEFS_H

namespace fallout {

enum AreaAttackMode : int {
    AREA_ATTACK_MODE_INVALID = -1,
    AREA_ATTACK_MODE_ALWAYS,
    AREA_ATTACK_MODE_SOMETIMES,
    AREA_ATTACK_MODE_BE_SURE,
    AREA_ATTACK_MODE_BE_CAREFUL,
    AREA_ATTACK_MODE_BE_ABSOLUTELY_SURE,
    AREA_ATTACK_MODE_COUNT,
    AREA_ATTACK_MODE_FIRST = AREA_ATTACK_MODE_ALWAYS
};

inline bool areaAttackModeIsValid(int areaAttackMode)
{
    return areaAttackMode >= AREA_ATTACK_MODE_FIRST && areaAttackMode < AREA_ATTACK_MODE_COUNT;
}

enum RunAwayMode : int {
    RUN_AWAY_MODE_INVALID = -1,
    RUN_AWAY_MODE_NONE,
    RUN_AWAY_MODE_COWARD,
    RUN_AWAY_MODE_FINGER_HURTS,
    RUN_AWAY_MODE_BLEEDING,
    RUN_AWAY_MODE_NOT_FEELING_GOOD,
    RUN_AWAY_MODE_TOURNIQUET,
    RUN_AWAY_MODE_NEVER,
    RUN_AWAY_MODE_COUNT,
    RUN_AWAY_MODE_FIRST = RUN_AWAY_MODE_NONE
};

inline RunAwayMode operator++(RunAwayMode& e, int)
{
    RunAwayMode result = e;
    e = static_cast<RunAwayMode>(static_cast<int>(e) + 1);
    return result;
}

inline RunAwayMode operator--(RunAwayMode& e, int)
{
    RunAwayMode result = e;
    e = static_cast<RunAwayMode>(static_cast<int>(e) - 1);
    return result;
}

inline bool runAwayModeIsValid(int runAwayMode)
{
    return runAwayMode >= RUN_AWAY_MODE_FIRST && runAwayMode < RUN_AWAY_MODE_COUNT;
}

enum BestWeapon : int {
    BEST_WEAPON_INVALID = -1,
    BEST_WEAPON_NO_PREF,
    BEST_WEAPON_MELEE,
    BEST_WEAPON_MELEE_OVER_RANGED,
    BEST_WEAPON_RANGED_OVER_MELEE,
    BEST_WEAPON_RANGED,
    BEST_WEAPON_UNARMED,
    BEST_WEAPON_UNARMED_OVER_THROW,
    BEST_WEAPON_RANDOM,
    BEST_WEAPON_COUNT,
    BEST_WEAPON_FIRST = BEST_WEAPON_NO_PREF
};

inline bool bestWeaponIsValid(int bestWeapon)
{
    return bestWeapon >= BEST_WEAPON_FIRST && bestWeapon < BEST_WEAPON_COUNT;
}

enum DistanceMode : int {
    DISTANCE_INVALID = -1,
    DISTANCE_STAY_CLOSE,
    DISTANCE_CHARGE,
    DISTANCE_SNIPE,
    DISTANCE_ON_YOUR_OWN,
    DISTANCE_STAY,
    DISTANCE_COUNT,
    DISTANCE_FIRST = DISTANCE_STAY_CLOSE
};

inline bool distanceModeIsValid(int distanceMode)
{
    return distanceMode >= DISTANCE_FIRST && distanceMode < DISTANCE_COUNT;
}

enum AttackWho : int {
    ATTACK_WHO_INVALID = -1,
    ATTACK_WHO_WHOMEVER_ATTACKING_ME,
    ATTACK_WHO_STRONGEST,
    ATTACK_WHO_WEAKEST,
    ATTACK_WHO_WHOMEVER,
    ATTACK_WHO_CLOSEST,
    ATTACK_WHO_COUNT,
    ATTACK_WHO_FIRST = ATTACK_WHO_WHOMEVER_ATTACKING_ME
};

inline bool attackWhoIsValid(int attackWho)
{
    return attackWho >= ATTACK_WHO_FIRST && attackWho < ATTACK_WHO_COUNT;
}

typedef enum ChemUse {
    CHEM_USE_CLEAN,
    CHEM_USE_STIMS_WHEN_HURT_LITTLE,
    CHEM_USE_STIMS_WHEN_HURT_LOTS,
    CHEM_USE_SOMETIMES,
    CHEM_USE_ANYTIME,
    CHEM_USE_ALWAYS,
    CHEM_USE_COUNT,
} ChemUse;

typedef enum Disposition {
    DISPOSITION_NONE,
    DISPOSITION_CUSTOM,
    DISPOSITION_COWARD,
    DISPOSITION_DEFENSIVE,
    DISPOSITION_AGGRESSIVE,
    DISPOSITION_BERKSERK,
    DISPOSITION_COUNT,
} Disposition;

typedef enum HurtTooMuch {
    HURT_BLIND,
    HURT_CRIPPLED,
    HURT_CRIPPLED_LEGS,
    HURT_CRIPPLED_ARMS,
    HURT_COUNT,
} HurtTooMuch;

} // namespace fallout

#endif /* COMBAT_AI_DEFS_H */
