#ifndef PERK_H
#define PERK_H

#include "art_defs.h"
#include "db.h"
#include "obj_types.h"
#include "perk_defs.h"
#include "skill_defs.h"

namespace fallout {

enum class PerkProperty {
    FrmId,
    MaxRank,
    MinLevel,
    Stat,
    StatModifier,
    Param1,
    Value1,
    ParamMode,
    Param2,
    Value2,
    Strength,
    Perception,
    Endurance,
    Charisma,
    Intelligence,
    Agility,
    Luck,
};

int perksInit();
void perksReset();
void perksExit();
int perksLoad(File* stream);
int perksSave(File* stream);
int perkAdd(Object* critter, Perk perk);
int perkAddAtLevel(Object* critter, Perk perk, int level);
int perkAddForce(Object* critter, Perk perk);
int perkRemove(Object* critter, Perk perk);
int perkGetAvailablePerks(Object* critter, int level, Perk* perks);
int perkGetRank(Object* critter, Perk perk);
char* perkGetName(Perk perk);
int perkGetMaxRank(Perk perk);
char* perkGetDescription(Perk perk);
SkillDexFrameId perkGetFrmId(Perk perk);
bool perkSetProperty(Perk perk, PerkProperty property, int value);
bool perkSetName(Perk perk, const char* value);
bool perkSetDescription(Perk perk, const char* value);
void perkAddEffect(Object* critter, Perk perk);
void perkRemoveEffect(Object* critter, Perk perk);
int perkGetSkillModifier(Object* critter, Skill skill);
int perkGetNightVisionBonus();
int perkGetMasterTraderBonus();
int perkGetEducatedBonus();
int perkGetHealerMinBonus();
int perkGetHealerMaxBonus();
int perkGetLifegiverBonus();
int perkGetWeaponLongRangeBonus();
int perkGetWeaponScopeRangeBonus();
int perkGetWeaponScopeRangePenalty();
int perkGetWeaponAccurateBonus();
int perkGetVaultCityInoculationsPoisonBonus();
int perkGetVaultCityInoculationsRadBonus();
int perkGetCautiousNatureBonus();
int perkGetComprehensionBonus();
int perkGetDemolitionExpertBonus();
int perkGetLivingAnatomyBonus();
int perkGetPyromaniacBonus();
int perkGetStonewallPercent();
int perkGetWeaponHandlingBonus();

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
