
#include "radiation.h"
#include "critter.h"
#include "display_monitor.h"
#include "game.h"
#include "interface.h"
#include "item.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "proto.h"
#include "proto_types.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "stat.h"

namespace fallout {

// Something with radiation.
//
// 0x56D7CC
static int oldRadLevel;

// Modifiers to endurance for performing radiation damage check.
//
// 0x518340
const int gRadiationEnduranceModifiers[RADIATION_LEVEL_COUNT] = {
    2,
    0,
    -2,
    -4,
    -6,
    -8,
};

// List of stats affected by radiation.
//
// The values of this list specify stats that can be affected by radiation.
// The amount of penalty to every stat (identified by index) is stored
// separately in [gRadiationEffectPenalties] per radiation level.
//
// The order of stats is important - primary stats must be at the top. See
// [RADIATION_EFFECT_PRIMARY_STAT_COUNT] for more info.
//
// 0x518358
const int gRadiationEffectStats[RADIATION_EFFECT_COUNT] = {
    STAT_STRENGTH,
    STAT_PERCEPTION,
    STAT_ENDURANCE,
    STAT_CHARISMA,
    STAT_INTELLIGENCE,
    STAT_AGILITY,
    STAT_CURRENT_HIT_POINTS,
    STAT_HEALING_RATE,
};

// List of stat modifiers caused by radiation at different radiation levels.
//
// 0x518378
const int gRadiationEffectPenalties[RADIATION_LEVEL_COUNT][RADIATION_EFFECT_COUNT] = {
    // clang-format off
    {   0,   0,   0,   0,   0,   0,   0,   0 },
    {  -1,   0,   0,   0,   0,   0,   0,   0 },
    {  -1,   0,   0,   0,   0,  -1,   0,  -3 },
    {  -2,   0,  -1,   0,   0,  -2,  -5,  -5 },
    {  -4,  -3,  -3,  -3,  -1,  -5, -15, -10 },
    {  -6,  -5,  -5,  -5,  -3,  -6, -20, -10 },
    // clang-format on
};

// 0x42D38C
int critterGetRadiation(Object* obj)
{
    return PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER ? obj->data.critter.radiation : 0;
}
// 0x42D3A4
int critterAdjustRadiation(Object* obj, int amount)
{
    MessageListItem messageListItem;

    if (obj != gDude) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    if (amount > 0) {
        amount -= critterGetStat(obj, STAT_RADIATION_RESISTANCE) * amount / 100;
    }

    if (amount > 0) {
        proto->critter.data.flags |= CRITTER_RADIATED;
    }

    if (amount > 0) {
        Object* geigerCounter = nullptr;

        Object* item1 = critterGetItem1(gDude);
        if (item1 != nullptr) {
            if (item1->pid == PROTO_ID_GEIGER_COUNTER_I || item1->pid == PROTO_ID_GEIGER_COUNTER_II) {
                geigerCounter = item1;
            }
        }

        Object* item2 = critterGetItem2(gDude);
        if (item2 != nullptr) {
            if (item2->pid == PROTO_ID_GEIGER_COUNTER_I || item2->pid == PROTO_ID_GEIGER_COUNTER_II) {
                geigerCounter = item2;
            }
        }

        if (geigerCounter != nullptr) {
            if (miscItemIsOn(geigerCounter)) {
                if (amount > 5) {
                    // The geiger counter is clicking wildly.
                    messageListItem.num = 1009;
                } else {
                    // The geiger counter is clicking.
                    messageListItem.num = 1008;
                }

                if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }
            }
        }
    }

    if (amount >= 10) {
        // You have received a large dose of radiation.
        messageListItem.num = 1007;

        if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }

    obj->data.critter.radiation += amount;
    if (obj->data.critter.radiation < 0) {
        obj->data.critter.radiation = 0;
    }

    if (obj == gDude) {
        indicatorBarRefresh();
    }

    return 0;
}

// 0x42D618
int _get_rad_damage_level(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    oldRadLevel = radiationEvent->radiationLevel;

    return 0;
}

// 0x42D4F4
// note: original ASM always returned 0
int critterCheckRadiationEvent(Object* obj)
{
    if (obj != gDude) {
        return 0;
    }

    Proto* proto;
    protoGetProto(obj->pid, &proto);
    if ((proto->critter.data.flags & CRITTER_RADIATED) == 0) {
        return 0;
    }

    oldRadLevel = 0;

    queueClearByEventType(EVENT_TYPE_RADIATION, _get_rad_damage_level);

    // NOTE: Uninline
    int radiation = critterGetRadiation(obj);

    int radiationLevel;
    if (radiation > 999)
        radiationLevel = RADIATION_LEVEL_FATAL;
    else if (radiation > 599)
        radiationLevel = RADIATION_LEVEL_DEADLY;
    else if (radiation > 399)
        radiationLevel = RADIATION_LEVEL_CRITICAL;
    else if (radiation > 199)
        radiationLevel = RADIATION_LEVEL_ADVANCED;
    else if (radiation > 99)
        radiationLevel = RADIATION_LEVEL_MINOR;
    else
        radiationLevel = RADIATION_LEVEL_NONE;

    if (statRoll(obj, STAT_ENDURANCE, gRadiationEnduranceModifiers[radiationLevel], nullptr) <= ROLL_FAILURE) {
        radiationLevel++;
    }

    if (radiationLevel > oldRadLevel) {
        // Create timer event for applying radiation damage.
        RadiationEvent* radiationEvent = (RadiationEvent*)internal_malloc(sizeof(*radiationEvent));
        if (radiationEvent == nullptr) {
            return 0;
        }

        radiationEvent->radiationLevel = radiationLevel;
        radiationEvent->isHealing = 0;
        queueAddEvent(GAME_TIME_TICKS_PER_HOUR * randomBetween(4, 18), obj, radiationEvent, EVENT_TYPE_RADIATION);
    }

    proto->critter.data.flags &= ~CRITTER_RADIATED;

    return 0;
}

// 0x42D624
int radiationClearDamage(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    if (radiationEvent->isHealing) {
        radiationProcess(obj, radiationEvent->radiationLevel, true);
    }

    return 1;
}

// Applies radiation.
//
// 0x42D63C
void radiationProcess(Object* obj, int radiationLevel, bool isHealing)
{
    MessageListItem messageListItem;

    if (radiationLevel == RADIATION_LEVEL_NONE) {
        return;
    }

    int radiationLevelIndex = radiationLevel - 1;
    int modifier = isHealing ? -1 : 1;

    if (obj == gDude) {
        // Radiation level message, higher is worse.
        messageListItem.num = 1000 + radiationLevelIndex;

        // SFALL: Fix radiation message when removing radiation effects.
        if (isHealing) {
            // You feel better.
            messageListItem.num = 3003;
        }

        if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
    }

    for (int effect = 0; effect < RADIATION_EFFECT_COUNT; effect++) {
        int value = critterGetBonusStat(obj, gRadiationEffectStats[effect]);
        value += modifier * gRadiationEffectPenalties[radiationLevelIndex][effect];
        critterSetBonusStat(obj, gRadiationEffectStats[effect], value);
    }

    // SFALL: Prevent death when removing radiation effects.
    if (!isHealing) {
        if ((obj->data.critter.combat.results & DAM_DEAD) == 0) {
            // Loop thru effects affecting primary stats. If any of the primary stat
            // dropped below minimal value, kill it.
            for (int effect = 0; effect < RADIATION_EFFECT_PRIMARY_STAT_COUNT; effect++) {
                int base = critterGetBaseStatWithTraitModifier(obj, gRadiationEffectStats[effect]);
                int bonus = critterGetBonusStat(obj, gRadiationEffectStats[effect]);
                if (base + bonus < PRIMARY_STAT_MIN) {
                    critterKill(obj, -1, 1);
                    break;
                }
            }
        }
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) != 0) {
        if (obj == gDude) {
            // You have died from radiation sickness.
            messageListItem.num = 1006;
            if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
                // SFALL: Display a pop-up message box about death from radiation.
                gameShowDeathDialog(messageListItem.text);
            }
        }
    }
}

// 0x42D740
int radiationEventProcess(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;
    if (!radiationEvent->isHealing) {
        // Schedule healing stats event in 7 days.
        RadiationEvent* newRadiationEvent = (RadiationEvent*)internal_malloc(sizeof(*newRadiationEvent));
        if (newRadiationEvent != nullptr) {
            queueClearByEventType(EVENT_TYPE_RADIATION, radiationClearDamage);
            newRadiationEvent->radiationLevel = radiationEvent->radiationLevel;
            newRadiationEvent->isHealing = 1;
            queueAddEvent(GAME_TIME_TICKS_PER_DAY * 7, obj, newRadiationEvent, EVENT_TYPE_RADIATION);
        }
    }

    radiationProcess(obj, radiationEvent->radiationLevel, radiationEvent->isHealing);

    return 1;
}

// 0x42D7A0
int radiationEventRead(File* stream, void** dataPtr)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)internal_malloc(sizeof(*radiationEvent));
    if (radiationEvent == nullptr) {
        return -1;
    }

    if (fileReadInt32(stream, &(radiationEvent->radiationLevel)) == -1) goto err;
    if (fileReadInt32(stream, &(radiationEvent->isHealing)) == -1) goto err;

    *dataPtr = radiationEvent;
    return 0;

err:

    internal_free(radiationEvent);
    return -1;
}

// 0x42D7FC
int radiationEventWrite(File* stream, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    if (fileWriteInt32(stream, radiationEvent->radiationLevel) == -1) return -1;
    if (fileWriteInt32(stream, radiationEvent->isHealing) == -1) return -1;

    return 0;
}

} // namespace fallout
