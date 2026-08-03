#ifndef QUEUE_H
#define QUEUE_H

#include "db.h"
#include "obj_types.h"
#include "perk_defs.h"
#include "stat_defs.h"

namespace fallout {

enum EventType : int {
    EVENT_TYPE_DRUG = 0,
    EVENT_TYPE_KNOCKOUT = 1,
    EVENT_TYPE_WITHDRAWAL = 2,
    EVENT_TYPE_SCRIPT = 3,
    EVENT_TYPE_GAME_TIME = 4,
    EVENT_TYPE_POISON = 5,
    EVENT_TYPE_RADIATION = 6,
    EVENT_TYPE_FLARE = 7,
    EVENT_TYPE_EXPLOSION = 8,
    EVENT_TYPE_ITEM_TRICKLE = 9,
    EVENT_TYPE_SNEAK = 10,
    EVENT_TYPE_EXPLOSION_FAILURE = 11,
    EVENT_TYPE_MAP_UPDATE_EVENT = 12,
    EVENT_TYPE_GSOUND_SFX_EVENT = 13,
    EVENT_TYPE_COUNT,
    EVENT_TYPE_FIRST = EVENT_TYPE_DRUG
};

inline EventType operator++(EventType& e, int)
{
    EventType result = e;
    e = static_cast<EventType>(static_cast<int>(e) + 1);
    return result;
}

typedef struct DrugEffectEvent {
    int drugPid;
    Stat stats[3];
    int modifiers[3];
} DrugEffectEvent;

typedef struct WithdrawalEvent {
    int active; // 0 == end withdrawal, 1 == start withdrawal
    int pid;
    Perk perk;
} WithdrawalEvent;

typedef struct ScriptEvent {
    int sid;
    int fixedParam;
} ScriptEvent;

typedef struct RadiationEvent {
    int radiationLevel;
    int isHealing;
} RadiationEvent;

typedef struct AmbientSoundEffectEvent {
    int ambientSoundEffectIndex;
} AmbientSoundEffectEvent;

typedef int QueueEventHandler(Object* owner, void* data);
typedef void QueueEventDataFreeProc(void* data);
typedef int QueueEventDataReadProc(File* stream, void** dataPtr);
typedef int QueueEventDataWriteProc(File* stream, void* data);

void queueInit();
int queueExit();
int queueLoad(File* stream);
int queueSave(File* stream);
int queueAddEvent(int delay, Object* owner, void* data, EventType eventType);
int queueRemoveEvents(Object* owner);
int queueRemoveEventsByType(Object* owner, EventType eventType);
bool queueHasEvent(Object* owner, EventType eventType);
int queueProcessEvents();
void queueClear();
void queueClearByEventType(EventType eventType, QueueEventHandler* fn);
unsigned int queueGetNextEventTime();
void _queue_leaving_map();
bool queueIsEmpty();
void* queueFindFirstEvent(Object* owner, EventType eventType);
void* queueFindNextEvent(Object* owner, EventType eventType);

} // namespace fallout

#endif /* QUEUE_H */
