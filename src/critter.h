#ifndef CRITTER_H
#define CRITTER_H

#include "db.h"
#include "obj_types.h"
#include "proto_types.h"

namespace fallout {

typedef enum DudeState {
    DUDE_STATE_SNEAKING = 0,
    DUDE_STATE_LEVEL_UP_AVAILABLE = 3,
    DUDE_STATE_ADDICTED = 4,
} DudeState;

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
int critterGetDamageType(Object* critter);
int killsIncByType(int killType);
int killsGetByType(int killType);
int killsLoad(File* stream);
int killsSave(File* stream);
int critterGetKillType(Object* critter);
char* killTypeGetName(int killType);
char* killTypeGetDescription(int killType);
int critterHealByHours(Object* obj, int hours);
void critterKill(Object* critter, int anim, bool refreshRect);
int critterGetExp(Object* critter);
bool critterIsActive(Object* critter);
bool critterIsDead(Object* critter);
bool critterIsCrippled(Object* critter);
bool critterIsProne(Object* critter);
int critterGetBodyType(Object* critter);
int gcdLoad(const char* path);
int protoCritterDataRead(File* stream, CritterProtoData* critterData);
int gcdSave(const char* path);
int protoCritterDataWrite(File* stream, CritterProtoData* critterData);
void dudeDisableState(int state);
void dudeEnableState(int state);
void dudeToggleState(int state);
bool dudeHasState(int state);
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
bool critterFlagCheck(int pid, int flag);
void critterFlagSet(int pid, int flag);
void critterFlagUnset(int pid, int flag);

} // namespace fallout

#endif /* CRITTER_H */
