#ifndef TRAIT_H
#define TRAIT_H

#include "db.h"
#include "obj_types.h"
#include "skill_defs.h"
#include "stat_defs.h"
#include "trait_defs.h"

namespace fallout {

int traitsInit();
void traitsReset();
void traitsExit();
int traitsLoad(File* stream);
int traitsSave(File* stream);
void traitsSetSelected(Trait trait1, Trait trait2);
void traitsGetSelected(Trait* trait1, Trait* trait2);
char* traitGetName(Trait trait);
char* traitGetDescription(Trait trait);
SkillDexFrameId traitGetFrmId(Trait trait);
bool traitIsSelected(Trait trait);
int traitGetStatModifier(Stat stat);
int traitGetSkillModifier(Skill skill);

} // namespace fallout

#endif /* TRAIT_H */
