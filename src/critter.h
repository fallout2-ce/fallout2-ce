#ifndef CRITTER_H
#define CRITTER_H

#include "animation.h"
#include "combat_defs.h"
#include "db.h"
#include "obj_types.h"
#include "proto_types.h"

namespace fallout {

const int kGorisCombatBaseFid = 100;
const int kGorisRobeBaseFid = 99;

int critterInit();
void critterReset();
void critterExit();
int critterLoad(File* stream);
int critterSave(File* stream);
char* critterGetName(Object* obj);
void critterProtoDataCopy(CritterProtoData* dest, CritterProtoData* src);
int dudeSetName(const char* name);
void dudeResetName();
int critterGetHitPoints(Object* critter);
int critterAdjustHitPoints(Object* critter, int hp);
int critterGetPoison(Object* critter);
int critterAdjustPoison(Object* obj, int amount);
int poisonEventProcess(Object* obj, void* data);
int critterGetRadiation(Object* critter);
int critterAdjustRadiation(Object* obj, int amount);
int critterCheckRadiationEvent(Object* critter);
int radiationClearDamage(Object* obj, void* data);
void radiationProcess(Object* obj, int radiationLevel, bool direction);
int radiationEventProcess(Object* obj, void* data);
int radiationEventRead(File* stream, void** dataPtr);
int radiationEventWrite(File* stream, void* data);
DamageType critterGetDamageType(Object* critter);
int killsIncByType(KillType killType);
int killsGetByType(KillType killType);
int killsLoad(File* stream);
int killsSave(File* stream);
KillType critterGetKillType(Object* critter);
char* killTypeGetName(KillType killType);
char* killTypeGetDescription(KillType killType);
int critterHealByHours(Object* obj, int hours);
void critterKill(Object* critter, AnimationType anim, bool refreshRect);
int critterGetExp(Object* critter);
bool critterIsActive(Object* critter);
bool critterIsDead(Object* critter);
bool critterIsCrippled(Object* critter);
bool critterIsProne(Object* critter);
BodyType critterGetBodyType(Object* critter);
// Checks physical/art capability only. Callers that expose weapon usability
// decisions must still call scriptHooks_CanUseWeapon with the final result.
bool critterCanUseWeapon(Object* critter, Object* weapon, HitMode hitMode);
int critterBuildGorisFid(Object* critter, int frmId);
int gcdLoad(const char* path);
int protoCritterDataRead(File* stream, CritterProtoData* critterData);
int gcdSave(const char* path);
int protoCritterDataWrite(File* stream, CritterProtoData* critterData);
void dudeDisableState(DudeState state);
void dudeEnableState(DudeState state);
void dudeToggleState(DudeState state);
bool dudeHasState(DudeState state);
int sneakEventProcess(Object* obj, void* data);
int critterDisableSneak(Object* obj, void* data);
bool dudeIsSneaking();
int knockoutEventProcess(Object* obj, void* data);
int knockoutClear(Object* obj, void* data);
int critterSetWhoHitMe(Object* critter, Object* hitMe);
bool critterCanDudeRest();
int critterGetMovementPointCostAdjustedForCrippledLegs(Object* critter, int distance);
bool critterIsEncumbered(Object* critter);
bool critterIsFleeing(Object* critter);
bool critterFlagCheck(int pid, CritterFlags flag);
void critterFlagSet(int pid, CritterFlags flag);
void critterFlagUnset(int pid, CritterFlags flag);

} // namespace fallout

#endif /* CRITTER_H */
