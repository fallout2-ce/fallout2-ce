#ifndef SKILL_DEFS_H
#define SKILL_DEFS_H

namespace fallout {

// max number of tagged skills
#define NUM_TAGGED_SKILLS 4

#define DEFAULT_TAGGED_SKILLS 3

// Available skills.
enum Skill : int {
    SKILL_INVALID = -1,
    SKILL_SMALL_GUNS,
    SKILL_BIG_GUNS,
    SKILL_ENERGY_WEAPONS,
    SKILL_UNARMED,
    SKILL_MELEE_WEAPONS,
    SKILL_THROWING,
    SKILL_FIRST_AID,
    SKILL_DOCTOR,
    SKILL_SNEAK,
    SKILL_LOCKPICK,
    SKILL_STEAL,
    SKILL_TRAPS,
    SKILL_SCIENCE,
    SKILL_REPAIR,
    SKILL_SPEECH,
    SKILL_BARTER,
    SKILL_GAMBLING,
    SKILL_OUTDOORSMAN,
    SKILL_COUNT,
    SKILL_FIRST = SKILL_SMALL_GUNS,
};

inline bool skillIsValid(int skill)
{
    return skill >= SKILL_FIRST && skill < SKILL_COUNT;
}

inline Skill operator++(Skill& e, int)
{
    Skill result = e;
    e = static_cast<Skill>(static_cast<int>(e) + 1);
    return result;
}

} // namespace fallout

#endif /* SKILL_DEFS_H */
