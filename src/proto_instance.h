#ifndef PROTOTYPE_INSTANCES_H
#define PROTOTYPE_INSTANCES_H

#include "obj_types.h"

namespace fallout {

enum UseItemResultCode {
    USE_ITEM_RESULT_ERROR = -1,
    USE_ITEM_RESULT_OK = 0,
    USE_ITEM_RESULT_REMOVE = 1,
    USE_ITEM_RESULT_DROP = 2,
};

int objectGetSid(Object* object, int* sidPtr);
int objectSetScriptFromProto(Object* object, int* sidPtr);
int objectSetScript(Object* obj, int scriptType, int scriptIndex);
int objectLookAt(Object* critter, Object* target);
int objectLookAtFunc(Object* critter, Object* target, void (*fn)(const char* string));
int objectExamine(Object* critter, Object* target);
int objectExamineFunc(Object* critter, Object* target, void (*fn)(const char* string));
int objectPickup(Object* critter, Object* item);
int objectDrop(Object* invenObj, Object* itemObj);
int objectDestroy(Object* obj);
UseItemResultCode objectUseItemInternal(Object* critter, Object* item);
UseItemResultCode objectUseItem(Object* userObj, Object* item);
UseItemResultCode objectUseItemOnInternal(Object* critter, Object* targetObj, Object* item);
UseItemResultCode objectUseItemOn(Object* user, Object* targetObj, Object* item);
int checkSceneryUseActionPointCost(Object* obj, Object* _);
int objectUse(Object* user, Object* targetObj);
int objectUseDoor(Object* user, Object* doorObj, bool animateOnly = false);
int objectUseContainer(Object* critter, Object* item);
int objectUseSkillOn(Object* source, Object* target, int skill);
bool objectIsLocked(Object* obj);
int objectLock(Object* obj);
int objectUnlock(Object* obj);
int objectIsOpen(Object* obj);
int objectOpen(Object* obj);
int objectClose(Object* obj);
int objectJamLock(Object* obj);
int objectUnjamLock(Object* obj);
int objectUnjamAll();
int objectAttemptPlacement(Object* obj, int tile, int elevation, int radius);
int objectAttemptPlacementPartyMember(Object* obj, int tile, int elevation);

} // namespace fallout

#endif /* PROTOTYPE_INSTANCES_H */
