#include "queue.h"

#include "actions.h"
#include "critter.h"
#include "display_monitor.h"
#include "game.h"
#include "game_sound.h"
#include "item.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "scripts.h"

namespace fallout {

typedef struct QueueListNode {
    unsigned int time;
    int type;
    Object* owner;
    void* data;
    struct QueueListNode* next;
} QueueListNode;

typedef struct EventTypeDescription {
    QueueEventHandler* handlerProc;
    QueueEventDataFreeProc* freeProc;
    QueueEventDataReadProc* readProc;
    QueueEventDataWriteProc* writeProc;
    bool clearOnMapExit; // unused
    QueueEventHandler* mapExitHandlerProc; // unused
} EventTypeDescription;

static int flareEventProcess(Object* obj, void* data);
static int explosionEventProcess(Object* obj, void* data);
static int explosionExit(Object* obj, void* data);
static int explosionProcess(Object* obj, bool animate);
static int explosionFailureEventProcess(Object* obj, void* data);

// Last queue list node found during [queueFindFirstEvent] and
// [queueFindNextEvent] calls.
//
// 0x51C690
static QueueListNode* gLastFoundQueueListNode = nullptr;

// 0x6648C0
static QueueListNode* gQueueListHead;

// 0x51C540
static EventTypeDescription gEventTypeDescriptions[EVENT_TYPE_COUNT] = {
    // EVENT_TYPE_DRUG
    { drugEffectEventProcess, internal_free, drugEffectEventRead, drugEffectEventWrite, true, drugItemClear },
    // EVENT_TYPE_KNOCKOUT
    { knockoutEventProcess, nullptr, nullptr, nullptr, true, knockoutClear },
    // EVENT_TYPE_WITHDRAWAL
    { withdrawalEventProcess, internal_free, withdrawalEventRead, withdrawalEventWrite, true, withdrawalClear },
    // EVENT_TYPE_SCRIPT
    { scriptEventProcess, internal_free, scriptEventRead, scriptEventWrite, true, nullptr },
    // EVENT_TYPE_GAME_TIME
    { gameTimeEventProcess, nullptr, nullptr, nullptr, true, nullptr },
    // EVENT_TYPE_POISON
    { poisonEventProcess, nullptr, nullptr, nullptr, false, nullptr },
    // EVENT_TYPE_RADIATION
    { radiationEventProcess, internal_free, radiationEventRead, radiationEventWrite, false, nullptr },
    // EVENT_TYPE_FLARE
    { flareEventProcess, nullptr, nullptr, nullptr, true, flareEventProcess },
    // EVENT_TYPE_EXPLOSION
    { explosionEventProcess, nullptr, nullptr, nullptr, true, explosionExit },
    // EVENT_TYPE_ITEM_TRICKLE
    { miscItemTrickleEventProcess, nullptr, nullptr, nullptr, true, miscItemTurnOffFromQueue },
    // EVENT_TYPE_SNEAK
    { sneakEventProcess, nullptr, nullptr, nullptr, true, critterDisableSneak },
    // EVENT_TYPE_EXPLOSION_FAILURE
    { explosionFailureEventProcess, nullptr, nullptr, nullptr, true, explosionExit },
    // EVENT_TYPE_MAP_UPDATE_EVENT
    { mapUpdateEventProcess, nullptr, nullptr, nullptr, true, nullptr },
    // EVENT_TYPE_GSOUND_SFX_EVENT
    { ambientSoundEffectEventProcess, internal_free, nullptr, nullptr, true, nullptr },
};

// 0x4A2320 queue_init_
void queueInit()
{
    gQueueListHead = nullptr;
}

// 0x4A2330 queue_reset_
int queueExit()
{
    queueClear();
    return 0;
}

// 0x4A2338 queue_load_
int queueLoad(File* stream)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) {
        return -1;
    }

    QueueListNode* oldListHead = gQueueListHead;
    gQueueListHead = nullptr;

    QueueListNode** nextPtr = &gQueueListHead;

    int rc = 0;
    for (int index = 0; index < count; index += 1) {
        QueueListNode* queueListNode = (QueueListNode*)internal_malloc(sizeof(*queueListNode));
        if (queueListNode == nullptr) {
            rc = -1;
            break;
        }

        if (fileReadUInt32(stream, &(queueListNode->time)) == -1) {
            internal_free(queueListNode);
            rc = -1;
            break;
        }

        if (fileReadInt32(stream, &(queueListNode->type)) == -1) {
            internal_free(queueListNode);
            rc = -1;
            break;
        }

        int objectId;
        if (fileReadInt32(stream, &objectId) == -1) {
            internal_free(queueListNode);
            rc = -1;
            break;
        }

        Object* obj;
        if (objectId == -2) {
            obj = nullptr;
        } else {
            obj = objectFindFirst();
            while (obj != nullptr) {
                obj = inventoryFindById(obj, objectId);
                if (obj != nullptr) {
                    break;
                }
                obj = objectFindNext();
            }
        }

        queueListNode->owner = obj;

        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[queueListNode->type]);
        if (eventTypeDescription->readProc != nullptr) {
            if (eventTypeDescription->readProc(stream, &(queueListNode->data)) == -1) {
                internal_free(queueListNode);
                rc = -1;
                break;
            }
        } else {
            queueListNode->data = nullptr;
        }

        queueListNode->next = nullptr;

        *nextPtr = queueListNode;
        nextPtr = &(queueListNode->next);
    }

    if (rc == -1) {
        while (gQueueListHead != nullptr) {
            QueueListNode* next = gQueueListHead->next;

            EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[gQueueListHead->type]);
            if (eventTypeDescription->freeProc != nullptr) {
                eventTypeDescription->freeProc(gQueueListHead->data);
            }

            internal_free(gQueueListHead);

            gQueueListHead = next;
        }
    }

    while (oldListHead != nullptr) {
        QueueListNode** queueListNodePtr = &gQueueListHead;
        while (*queueListNodePtr != nullptr) {
            if ((*queueListNodePtr)->time > oldListHead->time) {
                break;
            }

            queueListNodePtr = &((*queueListNodePtr)->next);
        }

        QueueListNode* next = oldListHead->next;
        oldListHead->next = *queueListNodePtr;
        *queueListNodePtr = oldListHead;
        oldListHead = next;
    }

    return rc;
}

// 0x4A24E0 queue_save_
int queueSave(File* stream)
{
    QueueListNode* queueListNode;

    int count = 0;

    queueListNode = gQueueListHead;
    while (queueListNode != nullptr) {
        count += 1;
        queueListNode = queueListNode->next;
    }

    if (fileWriteInt32(stream, count) == -1) {
        return -1;
    }

    queueListNode = gQueueListHead;
    while (queueListNode != nullptr) {
        Object* object = queueListNode->owner;
        int objectId = object != nullptr ? object->id : -2;

        if (fileWriteUInt32(stream, queueListNode->time) == -1) {
            return -1;
        }

        if (fileWriteInt32(stream, queueListNode->type) == -1) {
            return -1;
        }

        if (fileWriteInt32(stream, objectId) == -1) {
            return -1;
        }

        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[queueListNode->type]);
        if (eventTypeDescription->writeProc != nullptr) {
            if (eventTypeDescription->writeProc(stream, queueListNode->data) == -1) {
                return -1;
            }
        }

        queueListNode = queueListNode->next;
    }

    return 0;
}

// 0x4A258C queue_add_
int queueAddEvent(int delay, Object* obj, void* data, int eventType)
{
    QueueListNode* newQueueListNode = (QueueListNode*)internal_malloc(sizeof(QueueListNode));
    if (newQueueListNode == nullptr) {
        return -1;
    }

    newQueueListNode->time = gameTimeGetTime() + delay;
    newQueueListNode->type = eventType;
    newQueueListNode->owner = obj;
    newQueueListNode->data = data;

    if (obj != nullptr) {
        obj->flags |= OBJECT_QUEUED;
    }

    QueueListNode** queueListNodePtr = &gQueueListHead;

    while (*queueListNodePtr != nullptr) {
        if (newQueueListNode->time < (*queueListNodePtr)->time) {
            break;
        }

        queueListNodePtr = &((*queueListNodePtr)->next);
    }

    newQueueListNode->next = *queueListNodePtr;
    *queueListNodePtr = newQueueListNode;

    return 0;
}

// 0x4A25F4 queue_remove_
int queueRemoveEvents(Object* owner)
{
    QueueListNode* queueListNode = gQueueListHead;
    QueueListNode** queueListNodePtr = &gQueueListHead;

    while (queueListNode) {
        if (queueListNode->owner == owner) {
            QueueListNode* temp = queueListNode;

            queueListNode = queueListNode->next;
            *queueListNodePtr = queueListNode;

            EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[temp->type]);
            if (eventTypeDescription->freeProc != nullptr) {
                eventTypeDescription->freeProc(temp->data);
            }

            internal_free(temp);
        } else {
            queueListNodePtr = &(queueListNode->next);
            queueListNode = queueListNode->next;
        }
    }

    return 0;
}

// 0x4A264C queue_remove_this_
int queueRemoveEventsByType(Object* owner, int eventType)
{
    QueueListNode* queueListNode = gQueueListHead;
    QueueListNode** queueListNodePtr = &gQueueListHead;

    while (queueListNode) {
        if (queueListNode->owner == owner && queueListNode->type == eventType) {
            QueueListNode* temp = queueListNode;

            queueListNode = queueListNode->next;
            *queueListNodePtr = queueListNode;

            EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[temp->type]);
            if (eventTypeDescription->freeProc != nullptr) {
                eventTypeDescription->freeProc(temp->data);
            }

            internal_free(temp);
        } else {
            queueListNodePtr = &(queueListNode->next);
            queueListNode = queueListNode->next;
        }
    }

    return 0;
}

// Returns true if there is at least one event of given type scheduled.
//
// 0x4A26A8 queue_find_
bool queueHasEvent(Object* owner, int eventType)
{
    QueueListNode* queueListEvent = gQueueListHead;
    while (queueListEvent != nullptr) {
        if (owner == queueListEvent->owner && eventType == queueListEvent->type) {
            return true;
        }

        queueListEvent = queueListEvent->next;
    }

    return false;
}

// 0x4A26D0 queue_process_
int queueProcessEvents()
{
    unsigned int time = gameTimeGetTime();
    // TODO: this is 0 or 1, but in some cases -1. Probably needs to be bool.
    int stopProcess = 0;

    while (gQueueListHead != nullptr) {
        QueueListNode* queueListNode = gQueueListHead;
        if (time < queueListNode->time || stopProcess != 0) {
            break;
        }

        gQueueListHead = queueListNode->next;

        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[queueListNode->type]);
        stopProcess = eventTypeDescription->handlerProc(queueListNode->owner, queueListNode->data);

        if (eventTypeDescription->freeProc != nullptr) {
            eventTypeDescription->freeProc(queueListNode->data);
        }

        internal_free(queueListNode);
    }

    return stopProcess;
}

// 0x4A2748 queue_clear_
void queueClear()
{
    QueueListNode* queueListNode = gQueueListHead;
    while (queueListNode != nullptr) {
        QueueListNode* next = queueListNode->next;

        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[queueListNode->type]);
        if (eventTypeDescription->freeProc != nullptr) {
            eventTypeDescription->freeProc(queueListNode->data);
        }

        internal_free(queueListNode);

        queueListNode = next;
    }

    gQueueListHead = nullptr;
}

// 0x4A2790 queue_clear_type_
void queueClearByEventType(int eventType, QueueEventHandler* fn)
{
    QueueListNode** ptr = &gQueueListHead;
    QueueListNode* curr = *ptr;

    while (curr != nullptr) {
        if (eventType == curr->type) {
            QueueListNode* tmp = curr;

            *ptr = curr->next;
            curr = *ptr;

            if (fn != nullptr && fn(tmp->owner, tmp->data) != 1) {
                *ptr = tmp;
                ptr = &(tmp->next);
            } else {
                EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[tmp->type]);
                if (eventTypeDescription->freeProc != nullptr) {
                    eventTypeDescription->freeProc(tmp->data);
                }

                internal_free(tmp);

                // SFALL: Re-read next event since `fn` handler can change it.
                // This fixes crash when leaving the map while waiting for
                // someone to die of a super stimpak overdose.
                curr = *ptr;
            }
        } else {
            ptr = &(curr->next);
            curr = *ptr;
        }
    }
}

// 0x4A2808 queue_next_time_
unsigned int queueGetNextEventTime()
{
    if (gQueueListHead == nullptr) {
        return 0;
    }

    return gQueueListHead->time;
}

// 0x4A281C queue_destroy_
static int flareEventProcess(Object* obj, void* data)
{
    objectDestroy(obj);
    return 1;
}

// 0x4A2828 queue_explode_
static int explosionEventProcess(Object* obj, void* data)
{
    return explosionProcess(obj, true);
}

// 0x4A2830 queue_explode_exit_
static int explosionExit(Object* obj, void* data)
{
    return explosionProcess(obj, false);
}

// 0x4A2834
static int explosionProcess(Object* explosive, bool animate)
{
    int tile;
    int elevation;

    Object* owner = objectGetOwner(explosive);
    if (owner) {
        tile = owner->tile;
        elevation = owner->elevation;
    } else {
        tile = explosive->tile;
        elevation = explosive->elevation;
    }

    int maxDamage;
    int minDamage;

    // SFALL
    explosiveGetDamage(explosive->pid, &minDamage, &maxDamage);

    // FIXME: I guess this is a little bit wrong, dude can never be null, I
    // guess it needs to check if owner is dude.
    if (gDude != nullptr) {
        if (perkHasRank(gDude, PERK_DEMOLITION_EXPERT)) {
            maxDamage += 10;
            minDamage += 10;
        }
    }

    if (actionExplode(tile, elevation, minDamage, maxDamage, gDude, animate) == -2) {
        queueAddEvent(50, explosive, nullptr, EVENT_TYPE_EXPLOSION);
    } else {
        objectDestroy(explosive);
    }

    return 1;
}

// 0x4A28E4 queue_premature_
static int explosionFailureEventProcess(Object* obj, void* data)
{
    MessageListItem msg;

    // Due to your inept handling, the explosive detonates prematurely.
    msg.num = 4000;
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        displayMonitorAddMessage(msg.text);
    }

    return explosionProcess(obj, true);
}

// 0x4A2920 queue_leaving_map_
void _queue_leaving_map()
{
    for (int eventType = 0; eventType < EVENT_TYPE_COUNT; eventType++) {
        EventTypeDescription* eventTypeDescription = &(gEventTypeDescriptions[eventType]);
        if (eventTypeDescription->clearOnMapExit) {
            queueClearByEventType(eventType, eventTypeDescription->mapExitHandlerProc);
        }
    }
}

// 0x4A294C queue_is_empty_
bool queueIsEmpty()
{
    return gQueueListHead == nullptr;
}

// 0x4A295C queue_find_first_
void* queueFindFirstEvent(Object* owner, int eventType)
{
    QueueListNode* queueListNode = gQueueListHead;
    while (queueListNode != nullptr) {
        if (owner == queueListNode->owner && eventType == queueListNode->type) {
            gLastFoundQueueListNode = queueListNode;
            return queueListNode->data;
        }
        queueListNode = queueListNode->next;
    }

    gLastFoundQueueListNode = nullptr;
    return nullptr;
}

// 0x4A2994 queue_find_next_
void* queueFindNextEvent(Object* owner, int eventType)
{
    if (gLastFoundQueueListNode != nullptr) {
        QueueListNode* queueListNode = gLastFoundQueueListNode->next;
        while (queueListNode != nullptr) {
            if (owner == queueListNode->owner && eventType == queueListNode->type) {
                gLastFoundQueueListNode = queueListNode;
                return queueListNode->data;
            }
            queueListNode = queueListNode->next;
        }
    }

    gLastFoundQueueListNode = nullptr;

    return nullptr;
}

} // namespace fallout
