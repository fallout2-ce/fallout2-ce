#ifndef STAT_DEFS
#define STAT_DEFS

namespace fallout {

// The minimum value of SPECIAL stat.
#define PRIMARY_STAT_MIN (1)

// The maximum value of SPECIAL stat.
#define PRIMARY_STAT_MAX (10)

// The number of values of SPECIAL stat.
//
// Every stat value has it's own human readable description. This value is used
// as number of these descriptions.
#define PRIMARY_STAT_RANGE ((PRIMARY_STAT_MAX) - (PRIMARY_STAT_MIN) + 1)

// The maximum number of PC level.
#define PC_LEVEL_MAX 99

// Available stats.
enum Stat : int {
    STAT_INVALID = -1,
    STAT_STRENGTH,
    STAT_PERCEPTION,
    STAT_ENDURANCE,
    STAT_CHARISMA,
    STAT_INTELLIGENCE,
    STAT_AGILITY,
    STAT_LUCK,
    STAT_MAXIMUM_HIT_POINTS,
    STAT_MAXIMUM_ACTION_POINTS,
    STAT_ARMOR_CLASS,
    STAT_UNARMED_DAMAGE,
    STAT_MELEE_DAMAGE,
    STAT_CARRY_WEIGHT,
    STAT_SEQUENCE,
    STAT_HEALING_RATE,
    STAT_CRITICAL_CHANCE,
    STAT_BETTER_CRITICALS,
    STAT_DAMAGE_THRESHOLD,
    STAT_DAMAGE_THRESHOLD_LASER,
    STAT_DAMAGE_THRESHOLD_FIRE,
    STAT_DAMAGE_THRESHOLD_PLASMA,
    STAT_DAMAGE_THRESHOLD_ELECTRICAL,
    STAT_DAMAGE_THRESHOLD_EMP,
    STAT_DAMAGE_THRESHOLD_EXPLOSION,
    STAT_DAMAGE_RESISTANCE,
    STAT_DAMAGE_RESISTANCE_LASER,
    STAT_DAMAGE_RESISTANCE_FIRE,
    STAT_DAMAGE_RESISTANCE_PLASMA,
    STAT_DAMAGE_RESISTANCE_ELECTRICAL,
    STAT_DAMAGE_RESISTANCE_EMP,
    STAT_DAMAGE_RESISTANCE_EXPLOSION,
    STAT_RADIATION_RESISTANCE,
    STAT_POISON_RESISTANCE,
    STAT_AGE,
    STAT_GENDER,
    STAT_CURRENT_HIT_POINTS,
    STAT_CURRENT_POISON_LEVEL,
    STAT_CURRENT_RADIATION_LEVEL,
    STAT_COUNT,
    STAT_FIRST = STAT_STRENGTH,

    // Number of primary stats.
    PRIMARY_STAT_COUNT = 7,

    // Number of SPECIAL stats (primary + secondary).
    SPECIAL_STAT_COUNT = 33,

    // Number of saveable stats (i.e. excluding CURRENT pseudostats).
    SAVEABLE_STAT_COUNT = 35,
};

inline bool statIsValid(int stat)
{
    return stat >= STAT_FIRST && stat < STAT_COUNT;
}

inline Stat operator++(Stat& e, int)
{
    Stat result = e;
    e = static_cast<Stat>(static_cast<int>(e) + 1);
    return result;
}

// Special stats that are only relevant to player character.
enum PcStat : int {
    PC_STAT_UNSPENT_SKILL_POINTS,
    PC_STAT_LEVEL,
    PC_STAT_EXPERIENCE,
    PC_STAT_REPUTATION,
    PC_STAT_KARMA,
    PC_STAT_COUNT,
    PC_STAT_FIRST = PC_STAT_UNSPENT_SKILL_POINTS
};

inline bool pcStatIsValid(int pcStat)
{
    return pcStat >= PC_STAT_FIRST && pcStat < PC_STAT_COUNT;
}

inline PcStat operator++(PcStat& e, int)
{
    PcStat result = e;
    e = static_cast<PcStat>(static_cast<int>(e) + 1);
    return result;
}

} // namespace fallout

#endif /* STAT_DEFS */
