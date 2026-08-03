#ifndef PERK_H
#define PERK_H

#include "db.h"
#include "obj_types.h"
#include "perk_defs.h"
#include "skill_defs.h"

namespace fallout {

int perksInit();
void perksReset();
void perksExit();
int perksLoad(File* stream);
int perksSave(File* stream);
int perkAdd(Object* critter, Perk perk);
int perkAddForce(Object* critter, Perk perk);
int perkRemove(Object* critter, Perk perk);
int perkGetAvailablePerks(Object* critter, Perk* perks);
int perkGetRank(Object* critter, Perk perk);
char* perkGetName(Perk perk);
char* perkGetDescription(Perk perk);
int perkGetFrmId(Perk perk);
void perkAddEffect(Object* critter, Perk perk);
void perkRemoveEffect(Object* critter, Perk perk);
int perkGetSkillModifier(Object* critter, Skill skill);

// Returns true if critter has at least one rank in specified perk.
//
// NOTE: Most perks have only 1 rank, which means dude either have perk, or
// not.
//
// On the other hand, there are several places in editor, where they made two
// consequtive calls to [perkGetRank], first to check for presence, then get
// the actual value for displaying. So a macro could exist, or this very
// function, but due to similarity to [perkGetRank] it could have been
// collapsed by compiler.
static inline bool perkHasRank(Object* critter, Perk perk)
{
    return perkGetRank(critter, perk) != 0;
}

} // namespace fallout

#endif /* PERK_H */
