#ifndef STAT_H
#define STAT_H

#include "db.h"
#include "obj_types.h"
#include "proto_types.h"
#include "stat_defs.h"

namespace fallout {

#define STAT_ERR_INVALID_STAT (-5)

int statsInit();
int statsReset();
int statsExit();
int statsLoad(File* stream);
int statsSave(File* stream);
void statResetUnspentApBonuses();
void statSetUnspentApBonus(int multiplier);
int statGetUnspentApBonus();
void statSetUnspentApPerkBonus(int multiplier);
int statGetUnspentApPerkBonus();
int critterGetStat(Object* critter, Stat stat);
int critterGetBaseStatWithTraitModifier(Object* critter, Stat stat);
int critterGetBaseStat(Object* critter, Stat stat);
int critterGetBonusStat(Object* critter, Stat stat);
int critterSetBaseStat(Object* critter, Stat stat, int value);
int critterIncBaseStat(Object* critter, Stat stat);
int critterDecBaseStat(Object* critter, Stat stat);
int critterSetBonusStat(Object* critter, Stat stat, int value);
void protoCritterDataResetStats(CritterProtoData* data);
void critterUpdateDerivedStats(Object* critter);
char* statGetName(Stat stat);
char* statGetDescription(Stat stat);
char* statGetValueDescription(int value);
int pcGetStat(int pcStat);
int pcSetStat(int pcStat, int value);
void pcStatsReset();
int pcGetExperienceForNextLevel();
int pcGetExperienceForLevel(int level);
char* pcStatGetName(int pcStat);
char* pcStatGetDescription(int pcStat);
int statGetFrmId(Stat stat);
int statRoll(Object* critter, Stat stat, int modifier, int* howMuch);
int pcAddExperience(int xp, int* xpGained = nullptr);
int pcAddExperienceWithOptions(int xp, bool doParty, int* xpGained = nullptr);
int pcSetExperience(int xp);

static inline bool pcStatIsValid(int pcStat)
{
    return pcStat >= 0 && pcStat < PC_STAT_COUNT;
}

} // namespace fallout

#endif /* STAT_H */
