#ifndef SKILL_H
#define SKILL_H

#include "db.h"
#include "obj_types.h"
#include "proto_types.h"
#include "skill_defs.h"

namespace fallout {

extern int _gIsSteal;
extern int _gStealCount;
extern int _gStealSize;

enum class SkillStealResult {
    Caught = 0,
    Success = 1,
    Fail = 2,
};

int skillsInit();
void skillsReset();
void skillsExit();
int skillsLoad(File* stream);
int skillsSave(File* stream);
void protoCritterDataResetSkills(CritterProtoData* data);
void skillsSetTagged(Skill* skills, int count);
void skillsGetTagged(Skill* skills, int count);
int skillsAddTagged(Skill skill);
int skillsRemoveTagged(Skill skill);
bool skillIsTagged(Skill skill);
int skillGetValue(Object* critter, Skill skill);
void skillSetMaximum(int maximum);
int skillGetDefaultValue(Skill skill);
int skillAdd(Object* critter, Skill skill);
int skillAddForce(Object* critter, Skill skill);
int skillsGetCost(int skillValue);
int skillSub(Object* critter, Skill skill);
int skillSubForce(Object* critter, Skill skill);
int skillRoll(Object* critter, Skill skill, int modifier, int* howMuch);
char* skillGetName(Skill skill);
char* skillGetDescription(Skill skill);
char* skillGetAttributes(Skill skill);
int skillGetFrmId(Skill skill);
int skillUse(Object* obj, Object* target, Skill skill, int skillBonus);
SkillStealResult skillsPerformStealing(Object* thief, Object* target, Object* item, int quantity, bool isPlanting, int* xpOverride);
int skillGetGameDifficultyModifier(Skill skill);
int skillUpdateLastUse(Skill skill);
int skillsUsageSave(File* stream);
int skillsUsageLoad(File* stream);
char* skillsGetGenericResponse(Object* critter, bool isDude);

} // namespace fallout

#endif /* SKILL_H */
