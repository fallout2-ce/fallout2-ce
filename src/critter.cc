#include "critter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "animation.h"
#include "art.h"
#include "character_editor.h"
#include "combat.h"
#include "combat_defs.h"
#include "debug.h"
#include "display_monitor.h"
#include "endgame.h"
#include "game.h"
#include "geometry.h"
#include "interface.h"
#include "item.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "obj_types.h"
#include "object.h"
#include "party_member.h"
#include "platform_compat.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "reaction.h"
#include "scripts.h"
#include "sfall_object_name.h"
#include "sfall_script_hooks.h"
#include "skill.h"
#include "stat.h"
#include "tile.h"
#include "trait.h"
#include "worldmap.h"

namespace fallout {

// Maximum length of dude's name length.
#define DUDE_NAME_MAX_LENGTH (32)

// The number of effects caused by radiation.
//
// A radiation effect is an identifier and does not have it's own name. It's
// stat is specified in [gRadiationEffectStats], and it's amount is specified
// in [gRadiationEffectPenalties] for every [RadiationLevel].
#define RADIATION_EFFECT_COUNT 8

// Radiation levels.
//
// The names of levels are taken from Fallout 3, comments from Fallout 2.
typedef enum RadiationLevel {
    // Very nauseous.
    RADIATION_LEVEL_NONE,

    // Slightly fatigued.
    RADIATION_LEVEL_MINOR,

    // Vomiting does not stop.
    RADIATION_LEVEL_ADVANCED,

    // Hair is falling out.
    RADIATION_LEVEL_CRITICAL,

    // Skin is falling off.
    RADIATION_LEVEL_DEADLY,

    // Intense agony.
    RADIATION_LEVEL_FATAL,

    // The number of radiation levels.
    RADIATION_LEVEL_COUNT,
} RadiationLevel;

static int _get_rad_damage_level(Object* obj, void* data);
static int critter_kill_count_clear();
static int _critterClearObjDrugs(Object* obj, void* data);

// 0x50141C aCorpse
static char _aCorpse[] = "corpse";

// 0x501494
static char empty[] = "";

// 0x51833C name_critter
static char* _name_critter = _aCorpse;

// Modifiers to endurance for performing radiation damage check.
//
// 0x518340 bonus
static const int gRadiationEnduranceModifiers[RADIATION_LEVEL_COUNT] = {
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
// 0x518358 rad_stat
static const Stat gRadiationEffectStats[RADIATION_EFFECT_COUNT] = {
    STAT_STRENGTH,
    STAT_PERCEPTION,
    STAT_ENDURANCE,
    STAT_CHARISMA,
    STAT_INTELLIGENCE,
    STAT_AGILITY,
    STAT_CURRENT_HIT_POINTS,
    STAT_HEALING_RATE,
};

// Denotes how many primary stats at the top of [gRadiationEffectStats] array.
// These stats are used to determine if critter is alive after applying
// radiation effects.
#define RADIATION_EFFECT_PRIMARY_STAT_COUNT 6

// List of stat modifiers caused by radiation at different radiation levels.
//
// 0x518378 rad_bonus
static const int gRadiationEffectPenalties[RADIATION_LEVEL_COUNT][RADIATION_EFFECT_COUNT] = {
    // clang-format off
    {   0,   0,   0,   0,   0,   0,   0,   0 },
    {  -1,   0,   0,   0,   0,   0,   0,   0 },
    {  -1,   0,   0,   0,   0,  -1,   0,  -3 },
    {  -2,   0,  -1,   0,   0,  -2,  -5,  -5 },
    {  -4,  -3,  -3,  -3,  -1,  -5, -15, -10 },
    {  -6,  -5,  -5,  -5,  -3,  -6, -20, -10 },
    // clang-format on
};

// 0x518438 critterClearObj
static Object* _critterClearObj = nullptr;

// scrname.msg
//
// 0x56D754 critter_scrmsg_file
static MessageList gCritterMessageList;

// 0x56D75C pc_name
static char gDudeName[DUDE_NAME_MAX_LENGTH];

// 0x56D77C sneak_working
static int _sneak_working;

// 0x56D780 pc_kill_counts
static int gKillsByType[KILL_TYPE_DEFAULT_COUNT];

// Something with radiation.
//
// 0x56D7CC old_rad_level
static int oldRadLevel;

// scrname_init
// 0x42CF50 critter_init
int critterInit()
{
    dudeResetName();

    // NOTE: Uninline;
    critter_kill_count_clear();

    if (!messageListInit(&gCritterMessageList)) {
        debugPrint("\nError: Initing critter name message file!");
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%sscrname.msg", asc_5186C8);

    if (!messageListLoad(&gCritterMessageList, path)) {
        debugPrint("\nError: Loading critter name message file!");
        return -1;
    }

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_SCRNAME, &gCritterMessageList);

    return 0;
}

// 0x42CFE4 critter_reset
void critterReset()
{
    dudeResetName();

    // NOTE: Uninline;
    critter_kill_count_clear();
}

// 0x42D004 critter_exit
void critterExit()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_SCRNAME, nullptr);
    messageListFree(&gCritterMessageList);
}

// 0x42D01C critter_load
int critterLoad(File* stream)
{
    if (fileReadInt32(stream, &_sneak_working) == -1) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    return protoCritterDataRead(stream, &(proto->critter.data));
}

// 0x42D058 critter_save
int critterSave(File* stream)
{
    if (fileWriteInt32(stream, _sneak_working) == -1) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    return protoCritterDataWrite(stream, &(proto->critter.data));
}

// 0x42D094 critter_copy
void critterProtoDataCopy(CritterProtoData* dest, CritterProtoData* src)
{
    memcpy(dest, src, sizeof(CritterProtoData));
}

// 0x42D0A8 critter_name
char* critterGetName(Object* obj)
{
    if (obj == gDude) {
        return gDudeName;
    }

    char* overrideName = sfallObjectNameGet(obj);
    if (overrideName != nullptr) {
        _name_critter = overrideName;
        return overrideName;
    }

    if (obj->scriptIndex == -1) {
        if (obj->sid != -1) {
            Script* script;
            if (scriptGetScript(obj->sid, &script) != -1) {
                obj->scriptIndex = script->index;
            }
        }
    }

    char* name = nullptr;
    if (obj->scriptIndex != -1) {
        MessageListItem messageListItem;
        messageListItem.num = 101 + obj->scriptIndex;
        if (messageListGetItem(&gCritterMessageList, &messageListItem)) {
            name = messageListItem.text;
        }
    }

    if (name == nullptr || *name == '\0') {
        name = protoGetName(obj->pid);
    }

    _name_critter = name;

    return name;
}

// 0x42D138 critter_pc_set_name
int dudeSetName(const char* name)
{
    if (strlen(name) <= DUDE_NAME_MAX_LENGTH) {
        strncpy(gDudeName, name, DUDE_NAME_MAX_LENGTH);
        return 0;
    }

    return -1;
}

// 0x42D170 critter_pc_reset_name
void dudeResetName()
{
    strncpy(gDudeName, "None", DUDE_NAME_MAX_LENGTH);
}

// 0x42D18C critter_get_hits
int critterGetHitPoints(Object* critter)
{
    return objectTypeFromPid(critter->pid) == OBJ_TYPE_CRITTER ? critter->data.critter.hp : 0;
}

// 0x42D1A4 critter_adjust_hits
int critterAdjustHitPoints(Object* critter, int hp)
{
    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    int maximumHp = critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS);
    int newHp = critter->data.critter.hp + hp;

    critter->data.critter.hp = newHp;
    if (maximumHp >= newHp) {
        if (newHp <= 0 && (critter->data.critter.combat.results & DAM_DEAD) == DAM_NONE) {
            critterKill(critter, ANIM_INVALID, true);
        }
    } else {
        critter->data.critter.hp = maximumHp;
    }

    return 0;
}

// 0x42D1F8 critter_get_poison
int critterGetPoison(Object* critter)
{
    return objectTypeFromPid(critter->pid) == OBJ_TYPE_CRITTER ? critter->data.critter.poison : 0;
}

// Adjust critter's current poison by specified amount.
//
// For unknown reason this function only works on dude.
//
// The [amount] can either be positive (adds poison) or negative (removes
// poison).
//
// 0x42D210 critter_adjust_poison
int critterAdjustPoison(Object* critter, int amount)
{
    MessageListItem messageListItem;

    if (critter != gDude) {
        return -1;
    }

    if (amount > 0) {
        // Take poison resistance into account.
        amount -= amount * critterGetStat(critter, STAT_POISON_RESISTANCE) / 100;
    } else {
        if (gDude->data.critter.poison <= 0) {
            // Critter is not poisoned and we're want to decrease it even
            // further, which makes no sense.
            return 0;
        }
    }

    int newPoison = critter->data.critter.poison + amount;
    if (newPoison > 0) {
        critter->data.critter.poison = newPoison;

        queueClearByEventType(EVENT_TYPE_POISON, nullptr);
        queueAddEvent(10 * (505 - 5 * newPoison), gDude, nullptr, EVENT_TYPE_POISON);

        // You have been poisoned!
        messageListItem.num = 3000;
        if (amount < 0) {
            // You feel a little better.
            messageListItem.num = 3002;
        }
    } else {
        critter->data.critter.poison = 0;

        // You feel better.
        messageListItem.num = 3003;
    }

    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        displayMonitorAddMessage(messageListItem.text);
    }

    if (critter == gDude) {
        indicatorBarRefresh();
    }

    return 0;
}

// 0x42D318 critter_check_poison
int poisonEventProcess(Object* obj, void* data)
{
    if (obj != gDude) {
        return 0;
    }

    critterAdjustPoison(obj, -2);
    critterAdjustHitPoints(obj, -1);

    interfaceRenderHitPoints(false);

    MessageListItem messageListItem;
    // You take damage from poison.
    messageListItem.num = 3001;
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        displayMonitorAddMessage(messageListItem.text);
    }

    // NOTE: Uninline.
    int hitPoints = critterGetHitPoints(obj);
    if (hitPoints > 5) {
        return 0;
    }

    return 1;
}

// 0x42D38C critter_get_rads
int critterGetRadiation(Object* obj)
{
    return objectTypeFromPid(obj->pid) == OBJ_TYPE_CRITTER ? obj->data.critter.radiation : 0;
}

// 0x42D3A4 critter_adjust_rads
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
        proto->critter.data.flags |= CRITTER_DUDE_RADIATED;
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

// 0x42D4F4 critter_check_rads
// note: original ASM always returned 0
int critterCheckRadiationEvent(Object* obj)
{
    if (obj != gDude) {
        return 0;
    }

    Proto* proto;
    protoGetProto(obj->pid, &proto);
    if ((proto->critter.data.flags & CRITTER_DUDE_RADIATED) == CRITTER_NONE) {
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

    proto->critter.data.flags &= ~CRITTER_DUDE_RADIATED;

    return 0;
}

// 0x42D618 get_rad_damage_level
static int _get_rad_damage_level(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    oldRadLevel = radiationEvent->radiationLevel;

    return 0;
}

// 0x42D624 clear_rad_damage
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
// 0x42D63C process_rads
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
        if ((obj->data.critter.combat.results & DAM_DEAD) == DAM_NONE) {
            // Loop thru effects affecting primary stats. If any of the primary stat
            // dropped below minimal value, kill it.
            for (int effect = 0; effect < RADIATION_EFFECT_PRIMARY_STAT_COUNT; effect++) {
                int base = critterGetBaseStatWithTraitModifier(obj, gRadiationEffectStats[effect]);
                int bonus = critterGetBonusStat(obj, gRadiationEffectStats[effect]);
                if (base + bonus < PRIMARY_STAT_MIN) {
                    critterKill(obj, ANIM_INVALID, 1);
                    break;
                }
            }
        }
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) != DAM_NONE) {
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

// 0x42D740 critter_process_rads
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

// 0x42D7A0 critter_load_rads
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

// 0x42D7FC critter_save_rads
int radiationEventWrite(File* stream, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    if (fileWriteInt32(stream, radiationEvent->radiationLevel) == -1) return -1;
    if (fileWriteInt32(stream, radiationEvent->isHealing) == -1) return -1;

    return 0;
}

// 0x42D82C critter_get_base_damage_type
DamageType critterGetDamageType(Object* obj)
{
    if (objectTypeFromPid(obj->pid) != OBJ_TYPE_CRITTER) {
        return DAMAGE_TYPE_NORMAL;
    }

    Proto* proto;
    if (protoGetProto(obj->pid, &proto) == -1) {
        return DAMAGE_TYPE_NORMAL;
    }

    return proto->critter.data.damageType;
}

// NOTE: Inlined.
//
// 0x42D860 critter_kill_count_clear
static int critter_kill_count_clear()
{
    memset(gKillsByType, 0, sizeof(gKillsByType));
    return 0;
}

// 0x42D878 critter_kill_count_inc
int killsIncByType(KillType killType)
{
    if (killTypeIsValid(killType)) {
        gKillsByType[killType]++;
        return 0;
    }

    return -1;
}

// 0x42D8A8 critter_kill_count
int killsGetByType(KillType killType)
{
    if (killTypeIsValid(killType)) {
        return gKillsByType[killType];
    }

    return 0;
}

// 0x42D8C0 critter_kill_count_load
int killsLoad(File* stream)
{
    if (fileReadInt32List(stream, gKillsByType, KILL_TYPE_DEFAULT_COUNT) == -1) {
        fileClose(stream);
        return -1;
    }

    return 0;
}

// 0x42D8F0 critter_kill_count_save
int killsSave(File* stream)
{
    if (fileWriteInt32List(stream, gKillsByType, KILL_TYPE_DEFAULT_COUNT) == -1) {
        fileClose(stream);
        return -1;
    }

    return 0;
}

// 0x42D920 critter_kill_count_type
KillType critterGetKillType(Object* obj)
{
    if (obj == gDude) {
        Gender gender = static_cast<Gender>(critterGetStat(obj, STAT_GENDER));
        if (gender == GENDER_FEMALE) {
            return KILL_TYPE_WOMAN;
        }
        return KILL_TYPE_MAN;
    }

    if (objectTypeFromPid(obj->pid) != OBJ_TYPE_CRITTER) {
        return KILL_TYPE_INVALID;
    }

    Proto* proto;
    protoGetProto(obj->pid, &proto);

    return proto->critter.data.killType;
}

// 0x42D974 critter_kill_name
char* killTypeGetName(KillType killType)
{
    if (killTypeIsValid(killType)) {
        MessageListItem messageListItem;
        return getmsg(&gProtoMessageList, &messageListItem, 1450 + killType);
    }

    return empty;
}

// 0x42D9B4 critter_kill_info
char* killTypeGetDescription(KillType killType)
{
    if (killTypeIsValid(killType)) {
        MessageListItem messageListItem;
        return getmsg(&gProtoMessageList, &messageListItem, 1469 + killType);
    }

    return empty;
}

// 0x42D9F4 critter_heal_hours
// heals critters based on the number of elapsed hours
int critterHealByHours(Object* critter, int hours)
{
    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    if (critter->data.critter.hp < critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS)) {
        critterAdjustHitPoints(critter, 14 * (hours / 3));
    }

    critter->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;

    return 0;
}

// 0x42DA54 critterClearObjDrugs
static int _critterClearObjDrugs(Object* obj, void* data)
{
    return obj == _critterClearObj;
}

// 0x42DA64 critter_kill
void critterKill(Object* critter, AnimationType anim, bool refreshRect)
{
    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        return;
    }

    int elevation = critter->elevation;

    critter->data.critter.hp = 0;
    critter->data.critter.combat.results |= DAM_DEAD;

    partyMemberRemove(critter);
    scriptHooks_OnDeath(critter);

    // NOTE: Original code uses goto to jump out from nested conditions below.
    bool shouldChangeFid = false;
    FrmId frmId;
    if (critterIsProne(critter)) {
        AnimationType current = animationTypeFromFid(critter->fid);
        if (current == ANIM_FALL_BACK || current == ANIM_FALL_FRONT) {
            bool back = false;
            if (current == ANIM_FALL_BACK) {
                back = true;
            } else {
                frmId = FrmId(critter, ANIM_FALL_FRONT_SF, weaponAnimationFromFid(critter->fid), critter->rotation + 1);
                if (!frmId.exist()) {
                    back = true;
                }
            }

            if (back) {
                frmId = FrmId(critter, ANIM_FALL_BACK_SF, weaponAnimationFromFid(critter->fid), critter->rotation + 1);
            }

            shouldChangeFid = true;
        }
    } else {
        if (anim < 0) {
            anim = LAST_SF_DEATH_ANIM;
        }

        if (anim > LAST_SF_DEATH_ANIM) {
            debugPrint("\nError: Critter Kill: death_frame out of range!");
            anim = LAST_SF_DEATH_ANIM;
        }

        frmId = FrmId(critter, anim, weaponAnimationFromFid(critter->fid), critter->rotation + 1);
        int violenceFixedFid = frmId.fid();
        _obj_fix_violence_settings(&violenceFixedFid);
        frmId = FrmId(violenceFixedFid);
        if (!frmId.exist()) {
            debugPrint("\nError: Critter Kill: Can't match fid!");

            frmId = FrmId(critter, ANIM_FALL_BACK_BLOOD_SF, weaponAnimationFromFid(critter->fid), critter->rotation + 1);
            violenceFixedFid = frmId.fid();
            _obj_fix_violence_settings(&violenceFixedFid);
            frmId = FrmId(violenceFixedFid);
        }

        shouldChangeFid = true;
    }

    Rect updatedRect;
    Rect tempRect;

    if (shouldChangeFid) {
        objectSetFrame(critter, 0, &updatedRect);

        objectSetFid(critter, frmId.fid(), &tempRect);
        rectUnion(&updatedRect, &tempRect, &updatedRect);
    }

    if (!critterFlagCheck(critter->pid, CRITTER_FLAT)) {
        critter->flags |= OBJECT_NO_BLOCK;
        if ((critter->flags & OBJECT_FLAT) == OBJECT_NONE) {
            _obj_toggle_flat(critter, &tempRect);
        }
    }

    // NOTE: using uninitialized updatedRect/tempRect if fid was not set.

    rectUnion(&updatedRect, &tempRect, &updatedRect);

    _obj_turn_off_light(critter, &tempRect);
    rectUnion(&updatedRect, &tempRect, &updatedRect);

    if (critter->sid != -1) {
        scriptRemove(critter->sid);
        critter->sid = -1;
    }

    _critterClearObj = critter;
    queueClearByEventType(EVENT_TYPE_DRUG, _critterClearObjDrugs);

    itemDestroyAllHidden(critter);

    if (refreshRect) {
        tileWindowRefreshRect(&updatedRect, elevation);
    }

    if (critter == gDude) {
        endgameSetupDeathEnding(ENDGAME_DEATH_ENDING_REASON_DEATH);
        _game_user_wants_to_quit = GAME_QUIT_REQUEST_MAIN_MENU;
    }
}

// Returns experience for killing [critter].
//
// 0x42DCB8 critter_kill_exps
int critterGetExp(Object* critter)
{
    Proto* proto;
    protoGetProto(critter->pid, &proto);
    return proto->critter.data.experience;
}

// 0x42DCDC critter_is_active
bool critterIsActive(Object* critter)
{
    if (critter == nullptr) {
        return false;
    }

    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if ((critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD)) != 0) {
        return false;
    }

    if ((critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) != 0) {
        return false;
    }

    return (critter->data.critter.combat.results & DAM_DEAD) == DAM_NONE;
}

// 0x42DD18 critter_is_dead
bool critterIsDead(Object* critter)
{
    if (critter == nullptr) {
        return false;
    }

    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (critterGetStat(critter, STAT_CURRENT_HIT_POINTS) <= 0) {
        return true;
    }

    if ((critter->data.critter.combat.results & DAM_DEAD) != DAM_NONE) {
        return true;
    }

    return false;
}

// 0x42DD58 critter_is_crippled
bool critterIsCrippled(Object* critter)
{
    if (critter == nullptr) {
        return false;
    }

    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    return (critter->data.critter.combat.results & DAM_CRIP) != DAM_NONE;
}

// 0x42DD80 critter_is_prone
bool critterIsProne(Object* critter)
{
    if (critter == nullptr) {
        return false;
    }

    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    AnimationType anim = animationTypeFromFid(critter->fid);

    return (critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0
        || (anim >= FIRST_KNOCKDOWN_AND_DEATH_ANIM && anim <= LAST_KNOCKDOWN_AND_DEATH_ANIM)
        || (anim >= FIRST_SF_DEATH_ANIM && anim <= LAST_SF_DEATH_ANIM);
}

// critter_body_type
// 0x42DDC4 critter_body_type
BodyType critterGetBodyType(Object* critter)
{
    if (critter == nullptr) {
        debugPrint("\nError: critter_body_type: pobj was NULL!");
        return BODY_TYPE_BIPED;
    }

    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        return BODY_TYPE_BIPED;
    }

    Proto* proto;
    protoGetProto(critter->pid, &proto);
    return proto->critter.data.bodyType;
}

bool critterCanUseWeapon(Object* critter, Object* weapon, HitMode hitMode)
{
    if (critter == nullptr || weapon == nullptr || itemGetType(weapon) != ITEM_TYPE_WEAPON) {
        return false;
    }

    int damageFlags = critter->data.critter.combat.results;
    if ((damageFlags & DAM_CRIP_ARM_ANY) == DAM_CRIP_ARM_ANY) {
        // both limbs are crippled
        return false;
    }

    if ((damageFlags & DAM_CRIP_ARM_ANY) != DAM_NONE && weaponIsTwoHanded(weapon)) {
        return false;
    }

    // verify art exists
    Rotation rotation = critter->rotation + 1;
    WeaponAnimation animationCode = weaponGetAnimationCode(weapon);
    AnimationType weaponAnimationCode = weaponGetAnimationForHitMode(weapon, hitMode);
    const FrmId frmId = FrmId(critter, weaponAnimationCode, animationCode, rotation);
    return frmId.exist();
}

CritterFrmId critterBuildGorisFrmId(Object* critter, CritterFrameId frameId)
{
    if (critter == nullptr || frameId == CritterFrameId::Invalid) {
        return CritterFrameId::Invalid;
    }

    assert(critter->pid == PROTO_ID_GORIS);

    // Goris needs the live critter FID preserved exactly as-is except for the
    // base FRM id swap between robe and claw body art.
    return CritterFrmId(frameId, animationTypeFromFid(critter->fid), weaponAnimationFromFid(critter->fid), rotationFromFid(critter->fid));
}

// 0x42DE58 pc_load_data
int gcdLoad(const char* path)
{
    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    if (protoCritterDataRead(stream, &(proto->critter.data)) == -1) {
        fileClose(stream);
        return -1;
    }

    fileRead(gDudeName, DUDE_NAME_MAX_LENGTH, 1, stream);

    if (skillsLoad(stream) == -1) {
        fileClose(stream);
        return -1;
    }

    if (traitsLoad(stream) == -1) {
        fileClose(stream);
        return -1;
    }

    if (fileReadInt32(stream, &gCharacterEditorRemainingCharacterPoints) == -1) {
        fileClose(stream);
        return -1;
    }

    proto->critter.data.baseStats[STAT_DAMAGE_RESISTANCE_EMP] = 100;
    proto->critter.data.bodyType = BODY_TYPE_BIPED;
    proto->critter.data.experience = 0;
    proto->critter.data.killType = KILL_TYPE_MAN;

    fileClose(stream);
    return 0;
}

// 0x42DF70 critter_read_data
int protoCritterDataRead(File* stream, CritterProtoData* critterData)
{
    if (fileReadInt32Enum<CritterFlags>(stream, &(critterData->flags)) == -1) return -1;
    if (fileReadInt32List(stream, critterData->baseStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (fileReadInt32List(stream, critterData->bonusStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (fileReadInt32List(stream, critterData->skills, SKILL_COUNT) == -1) return -1;
    if (fileReadInt32Enum<BodyType>(stream, &(critterData->bodyType)) == -1) return -1;
    if (fileReadInt32(stream, &(critterData->experience)) == -1) return -1;
    if (fileReadInt32Enum<KillType>(stream, &(critterData->killType)) == -1) return -1;

    // NOTE: For unknown reason damage type is not present in two protos: Sentry
    // Bot and Weak Brahmin. These two protos are 412 bytes, not 416.
    //
    // Given that only Floating Eye Bot, Floater, and Nasty Floater have
    // natural damage type other than normal, I think addition of natural
    // damage type as a feature was a last minute design decision. Most protos
    // were updated, but not all. Another suggestion is that some team member
    // used outdated toolset to build those two protos (mapper or whatever
    // they used to create protos in the first place).
    //
    // Regardless of the reason, damage type is considered optional by original
    // code as seen at 0x42E01B.
    if (fileReadInt32Enum<DamageType>(stream, &(critterData->damageType)) == -1) {
        critterData->damageType = DAMAGE_TYPE_NORMAL;
    }

    return 0;
}

// 0x42E08C pc_save_data
int gcdSave(const char* path)
{
    File* stream = fileOpen(path, "wb");
    if (stream == nullptr) {
        return -1;
    }

    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    if (protoCritterDataWrite(stream, &(proto->critter.data)) == -1) {
        fileClose(stream);
        return -1;
    }

    fileWrite(gDudeName, DUDE_NAME_MAX_LENGTH, 1, stream);

    if (skillsSave(stream) == -1) {
        fileClose(stream);
        return -1;
    }

    if (traitsSave(stream) == -1) {
        fileClose(stream);
        return -1;
    }

    if (fileWriteInt32(stream, gCharacterEditorRemainingCharacterPoints) == -1) {
        fileClose(stream);
        return -1;
    }

    fileClose(stream);
    return 0;
}

// 0x42E174 critter_write_data
int protoCritterDataWrite(File* stream, CritterProtoData* critterData)
{
    if (fileWriteInt32Enum<CritterFlags>(stream, critterData->flags) == -1) return -1;
    if (fileWriteInt32List(stream, critterData->baseStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (fileWriteInt32List(stream, critterData->bonusStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (fileWriteInt32List(stream, critterData->skills, SKILL_COUNT) == -1) return -1;
    if (fileWriteInt32Enum<BodyType>(stream, critterData->bodyType) == -1) return -1;
    if (fileWriteInt32(stream, critterData->experience) == -1) return -1;
    if (fileWriteInt32Enum<KillType>(stream, critterData->killType) == -1) return -1;
    if (fileWriteInt32Enum<DamageType>(stream, critterData->damageType) == -1) return -1;

    return 0;
}

// 0x42E220 pc_flag_off
void dudeDisableState(DudeState state)
{
    CritterFlags flags;
    switch (state) {
    case DUDE_STATE_SNEAKING:
        flags = CRITTER_DUDE_SNEAKING;
        break;
    case DUDE_STATE_LEVEL_UP_AVAILABLE:
        flags = CRITTER_DUDE_LEVEL_UP_AVAILABLE;
        break;
    case DUDE_STATE_ADDICTED:
        flags = CRITTER_DUDE_ADDICTED;
        break;
    default:
        return;
    }

    Proto* proto;
    if (protoGetProto(gDude->pid, &proto) == -1) {
        return;
    }

    proto->critter.data.flags &= ~flags;

    if (state == DUDE_STATE_SNEAKING) {
        queueRemoveEventsByType(gDude, EVENT_TYPE_SNEAK);
    }

    indicatorBarRefresh();
}

// 0x42E26C pc_flag_on
void dudeEnableState(DudeState state)
{
    CritterFlags flags;
    switch (state) {
    case DUDE_STATE_SNEAKING:
        flags = CRITTER_DUDE_SNEAKING;
        break;
    case DUDE_STATE_LEVEL_UP_AVAILABLE:
        flags = CRITTER_DUDE_LEVEL_UP_AVAILABLE;
        break;
    case DUDE_STATE_ADDICTED:
        flags = CRITTER_DUDE_ADDICTED;
        break;
    default:
        return;
    }

    Proto* proto;
    if (protoGetProto(gDude->pid, &proto) == -1) {
        return;
    }

    proto->critter.data.flags |= flags;

    if (state == DUDE_STATE_SNEAKING) {
        sneakEventProcess(nullptr, nullptr);
    }

    indicatorBarRefresh();
}

// 0x42E2B0 pc_flag_toggle
void dudeToggleState(DudeState state)
{
    if (!dudeStateIsValid(state)) {
        return;
    }

    // NOTE: Uninline.
    if (dudeHasState(state)) {
        dudeDisableState(state);
    } else {
        dudeEnableState(state);
    }
}

// 0x42E2F8 is_pc_flag
bool dudeHasState(DudeState state)
{
    if (!dudeStateIsValid(state)) {
        return false;
    }

    CritterFlags flags;
    switch (state) {
    case DUDE_STATE_POISONED:
        return critterGetPoison(gDude) > POISON_INDICATOR_THRESHOLD;
    case DUDE_STATE_RADIATED:
        return critterGetRadiation(gDude) > RADATION_INDICATOR_THRESHOLD;
    case DUDE_STATE_SNEAKING:
        flags = CRITTER_DUDE_SNEAKING;
        break;
    case DUDE_STATE_LEVEL_UP_AVAILABLE:
        flags = CRITTER_DUDE_LEVEL_UP_AVAILABLE;
        break;
    case DUDE_STATE_ADDICTED:
        flags = CRITTER_DUDE_ADDICTED;
        break;
    default:
        return false;
    }

    Proto* proto;
    if (protoGetProto(gDude->pid, &proto) == -1) {
        return false;
    }

    return (proto->critter.data.flags & flags) != CRITTER_NONE;
}

// 0x42E32C critter_sneak_check
int sneakEventProcess(Object* obj, void* data)
{
    int time;

    int sneak = skillGetValue(gDude, SKILL_SNEAK);
    if (skillRoll(gDude, SKILL_SNEAK, 0, nullptr) < ROLL_SUCCESS) {
        time = 600;
        _sneak_working = false;

        if (sneak > 250)
            time = 100;
        else if (sneak > 200)
            time = 120;
        else if (sneak > 170)
            time = 150;
        else if (sneak > 135)
            time = 200;
        else if (sneak > 100)
            time = 300;
        else if (sneak > 80)
            time = 400;
    } else {
        time = 600;
        _sneak_working = true;
    }

    queueAddEvent(time, gDude, nullptr, EVENT_TYPE_SNEAK);

    return 0;
}

// 0x42E3E4 critter_sneak_clear
int critterDisableSneak(Object* obj, void* data)
{
    dudeDisableState(DUDE_STATE_SNEAKING);
    return 1;
}

// Returns true if dude is really sneaking.
//
// 0x42E3F4 is_pc_sneak_working
bool dudeIsSneaking()
{
    // NOTE: Uninline.
    if (dudeHasState(DUDE_STATE_SNEAKING)) {
        return _sneak_working;
    }

    return false;
}

// 0x42E424 critter_wake_up
int knockoutEventProcess(Object* obj, void* data)
{
    if ((obj->data.critter.combat.results & DAM_DEAD) != DAM_NONE) {
        return 0;
    }

    obj->data.critter.combat.results &= ~(DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN);
    obj->data.critter.combat.results |= DAM_KNOCKED_DOWN;

    if (isInCombat()) {
        obj->data.critter.combat.maneuver |= CRITTER_MANEUVER_ENGAGING;
    } else {
        _dude_standup(obj);
    }

    return 0;
}

// 0x42E460 critter_wake_clear
int knockoutClear(Object* obj, void* data)
{
    if (objectTypeFromPid(obj->pid) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) != DAM_NONE) {
        return 0;
    }

    obj->data.critter.combat.results &= ~(DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN);

    FrmId fid = FrmId(obj, ANIM_STAND, weaponAnimationFromFid(obj->fid), obj->rotation + 1);
    objectSetFid(obj, fid.fid(), nullptr);

    return 0;
}

// 0x42E4C0 critter_set_who_hit_me
// note: this is sometimes called with (attacker, defender) and sometimes with (defender, attacker).
// `hitMe` may be nullptr
int critterSetWhoHitMe(Object* critter, Object* hitMe)
{
    if (critter == nullptr) {
        return -1;
    }

    if (hitMe != nullptr && objectTypeFromFid(hitMe->fid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    if (objectTypeFromPid(critter->pid) == OBJ_TYPE_CRITTER) {
        if (hitMe == nullptr || critter->data.critter.combat.team != hitMe->data.critter.combat.team || (statRoll(critter, STAT_INTELLIGENCE, -1, nullptr) < 2 && (!objectIsPartyMember(critter) || !objectIsPartyMember(hitMe)))) {
            critter->data.critter.combat.whoHitMe = hitMe;
            if (hitMe == gDude) {
                reactionSetValue(critter, -3);
            }
        }
    }

    return 0;
}

// 0x42E564 critter_can_obj_dude_rest
bool critterCanDudeRest()
{
    if (wmRestModeIsDisabled()) {
        return false;
    }

    bool mapDisallowsRest = false;
    if (!wmMapCanRestHere(gElevation)) {
        mapDisallowsRest = true;
    }

    if (mapDisallowsRest && wmRestModeIsStrict()) {
        return false;
    }

    bool result = true;

    Object** critterList;
    int critterListLength = objectListCreate(-1, gElevation, OBJ_TYPE_CRITTER, &critterList);

    // TODO: Check conditions in this loop.
    for (int index = 0; index < critterListLength; index++) {
        Object* critter = critterList[index];
        if ((critter->data.critter.combat.results & DAM_DEAD) != DAM_NONE) {
            continue;
        }

        if (critter == gDude) {
            continue;
        }

        if (critter->data.critter.combat.whoHitMe != gDude) {
            if (!mapDisallowsRest || critter->data.critter.combat.team == gDude->data.critter.combat.team) {
                continue;
            }
        }

        result = false;
        break;
    }

    if (critterListLength != 0) {
        objectListFree(critterList);
    }

    return result;
}

// 0x42E62C critter_compute_ap_from_distance
int critterGetMovementPointCostAdjustedForCrippledLegs(Object* critter, int distance)
{
    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    int flags = critter->data.critter.combat.results;
    int actionPoints = 0;
    if ((flags & DAM_CRIP_LEG_LEFT) != DAM_NONE && (flags & DAM_CRIP_LEG_RIGHT) != DAM_NONE) {
        actionPoints = 8 * distance;
    } else if ((flags & DAM_CRIP_LEG_ANY) != DAM_NONE) {
        actionPoints = 4 * distance;
    } else {
        actionPoints = distance;
    }

    return scriptHooks_MoveCost(critter, distance, actionPoints);
}

// 0x42E66C critterIsOverloaded
bool critterIsEncumbered(Object* critter)
{
    int maxWeight = critterGetStat(critter, STAT_CARRY_WEIGHT);
    int currentWeight = objectGetInventoryWeight(critter);
    return maxWeight < currentWeight;
}

// 0x42E690 critter_is_fleeing
bool critterIsFleeing(Object* critter)
{
    return critter != nullptr
        ? (critter->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != CRITTER_MANEUVER_NONE
        : false;
}

// Checks proto critter flag.
//
// 0x42E6AC critter_flag_check
bool critterFlagCheck(int pid, CritterFlags flag)
{
    if (pid == -1) {
        return false;
    }

    if (objectTypeFromPid(pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return false;
    }

    return (proto->critter.data.flags & flag) != CRITTER_NONE;
}

// 0x42E6F0 critter_flag_set
void critterFlagSet(int pid, CritterFlags flag)
{
    if (pid == -1) {
        return;
    }

    if (objectTypeFromPid(pid) != OBJ_TYPE_CRITTER) {
        return;
    }

    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return;
    }

    proto->critter.data.flags |= flag;
}

// 0x42E71C critter_flag_unset
void critterFlagUnset(int pid, CritterFlags flag)
{
    if (pid == -1) {
        return;
    }

    if (objectTypeFromPid(pid) != OBJ_TYPE_CRITTER) {
        return;
    }

    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return;
    }

    proto->critter.data.flags &= ~flag;
}

} // namespace fallout
