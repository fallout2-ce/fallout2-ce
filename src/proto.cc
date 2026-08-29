#include "proto.h"

#include <stdio.h>
#include <string.h>

#include "art.h"
#include "character_editor.h"
#include "combat.h"
#include "config.h"
#include "critter.h"
#include "debug.h"
#include "dialog.h"
#include "game.h"
#include "game_movie.h"
#include "interface.h"
#include "map.h"
#include "memory.h"
#include "object.h"
#include "perk.h"
#include "settings.h"
#include "skill.h"
#include "stat.h"
#include "trait.h"

namespace fallout {

static int objectCritterCombatDataRead(CritterCombatData* data, File* stream);
static int objectCritterCombatDataWrite(CritterCombatData* data, File* stream);
static int _proto_update_gen(Object* obj);
static int _proto_header_load();
static int protoItemDataRead(ItemProtoData* item_data, ItemType type, File* stream);
static int protoSceneryDataRead(SceneryProtoData* scenery_data, SceneryType type, File* stream);
static int protoRead(Proto* buf, File* stream);
static int protoItemDataWrite(ItemProtoData* item_data, ItemType type, File* stream);
static int protoSceneryDataWrite(SceneryProtoData* scenery_data, SceneryType type, File* stream);
static int protoWrite(Proto* buf, File* stream);
static int _proto_load_pid(int pid, Proto** out_proto);
static int _proto_find_free_subnode(ObjectType type, Proto** out_ptr);
static void _proto_remove_some_list(ObjectType type);
static void _proto_remove_list(ObjectType type);
static int _proto_new_id(ObjectType type);

// 0x50CF3C aProto_0
static char _aProto_0[] = "proto\\";

// 0x50D1B0 aDrugStatSpecia
static char _aDrugStatSpecia[] = "Drug Stat (Special)";

// 0x50D1C4 aNone_1
static char _aNone_1[] = "None";

// 0x51C18C cd_path_base
char _cd_path_base[COMPAT_MAX_PATH];

// 0x51C290 protoLists
static ProtoList _protoLists[OBJ_TYPE_COUNT] = {
    { nullptr, nullptr, 0, 1 },
    { nullptr, nullptr, 0, 1 },
    { nullptr, nullptr, 0, 1 },
    { nullptr, nullptr, 0, 1 },
    { nullptr, nullptr, 0, 1 },
    { nullptr, nullptr, 0, 1 },
    { nullptr, nullptr, 0, 1 },
    { nullptr, nullptr, 0, 0 },
    { nullptr, nullptr, 0, 0 },
    { nullptr, nullptr, 0, 0 },
    { nullptr, nullptr, 0, 0 },
};

// 0x51C340 proto_sizes
static const size_t _proto_sizes[OBJ_TYPE_COUNT] = {
    sizeof(ItemProto), // 0x84
    sizeof(CritterProto), // 0x1A0
    sizeof(SceneryProto), // 0x38
    sizeof(WallProto), // 0x24
    sizeof(TileProto), // 0x1C
    sizeof(MiscProto), // 0x1C
    0,
    0,
    0,
    0,
    0,
};

// 0x51C36C protos_been_initialized
static int _protos_been_initialized = 0;

// obj_dude_proto
// 0x51C370 pc_proto
static CritterProto gDudeProto = {
    0x1000000,
    -1,
    0x1000001,
    0,
    0,
    PROTO_FLAG_LIGHT_THRU,
    PROTO_EXT_FLAG_NONE,
    -1,
    CRITTER_NONE,
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 18, 0, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 23, 0 },
    { 0 },
    { SKILL_SMALL_GUNS },
    BODY_TYPE_BIPED,
    0,
    KILL_TYPE_MAN,
    DAMAGE_TYPE_NORMAL,
    -1,
    0,
    0,
};

// 0x51C534 proto_path_base
static char* _proto_path_base = _aProto_0;

// 0x51C538 init_true
static int _init_true = 0;

// 0x51C53C retval
static int _retval = 0;

// 0x66452C mp_perk_code_None
static char* _mp_perk_code_None;

// 0x664530 mp_perk_code_strs
static char* _mp_perk_code_strs[PERK_COUNT];

// 0x66470C mp_critter_stats_list
static char* _mp_critter_stats_list;

// 0x664710 critter_stats_list_None
static char* _critter_stats_list_None;

// 0x664714 critter_stats_list_strs
static char* _critter_stats_list_strs[STAT_COUNT];

// Message list by object type
// 0 - pro_item.msg
// 1 - pro_crit.msg
// 2 - pro_scen.msg
// 3 - pro_wall.msg
// 4 - pro_tile.msg
// 5 - pro_misc.msg
//
// 0x6647AC proto_msg_files
static MessageList _proto_msg_files[OBJ_TYPE_PROTO_COUNT];

// 0x6647DC race_type_strs
static char* gRaceTypeNames[RACE_TYPE_COUNT];

// 0x6647E4 scenery_pro_type
static char* gSceneryTypeNames[SCENERY_TYPE_COUNT];

// proto.msg
//
// 0x6647FC proto_main_msg_file
MessageList gProtoMessageList;

// 0x664804 item_pro_material
static char* gMaterialTypeNames[MATERIAL_TYPE_COUNT];

// "<None>" from proto.msg
//
// 0x664824 proto_none_str
char* _proto_none_str;

// 0x664828 body_type_strs
static char* gBodyTypeNames[BODY_TYPE_COUNT];

// 0x664834 item_pro_type
char* gItemTypeNames[ITEM_TYPE_COUNT];

// 0x66484C
static char* gDamageTypeNames[DAMAGE_TYPE_COUNT];

// 0x66486C cal_type_strs
static char* gCaliberTypeNames[CALIBER_TYPE_COUNT];

// Perk names.
//
// 0x6648B8 perk_code_strs
static char** _perk_code_strs;

// Stat names.
//
// 0x6648BC critter_stats_list
static char** _critter_stats_list;

// 0x49E270 proto_make_path
void proto_make_path(char* path, int pid)
{
    strcpy(path, _cd_path_base);
    strcat(path, _proto_path_base);
    if (pid != -1) {
        strcat(path, artGetObjectTypeName(objectTypeFromPid(pid)));
    }
}

// Append proto file name to proto_path from proto.lst.
//
// 0x49E758 proto_list_str
int _proto_list_str(int pid, char* proto_path)
{
    if (pid == -1) {
        return -1;
    }

    if (proto_path == nullptr) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    proto_make_path(path, pid);
    strcat(path, "\\");
    strcat(path, artGetObjectTypeName(objectTypeFromPid(pid)));
    strcat(path, ".lst");

    File* stream = fileOpen(path, "rt");

    int i = 1;
    char string[256];
    while (fileReadString(string, sizeof(string), stream)) {
        if (i == objectFrameIdFromPid(pid)) {
            break;
        }

        i++;
    }

    fileClose(stream);

    if (i != objectFrameIdFromPid(pid)) {
        return -1;
    }

    char* pch = strchr(string, ' ');
    if (pch != nullptr) {
        *pch = '\0';
    }

    pch = strpbrk(string, "\r\n");
    if (pch != nullptr) {
        *pch = '\0';
    }

    strcpy(proto_path, string);

    return 0;
}

// 0x49E984 proto_size
size_t proto_size(ObjectType type)
{
    return objectTypeIsValid(type) ? _proto_sizes[type] : 0;
}

// 0x49E99C proto_action_can_use
bool _proto_action_can_use(int pid)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return false;
    }

    if ((proto->item.extendedFlags & PROTO_EXT_FLAG_CAN_USE) != PROTO_EXT_FLAG_NONE) {
        return true;
    }

    if (objectTypeFromPid(pid) == OBJ_TYPE_ITEM && proto->item.type == ITEM_TYPE_CONTAINER) {
        return true;
    }

    return false;
}

// 0x49E9DC proto_action_can_use_on
bool _proto_action_can_use_on(int pid)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return false;
    }

    if ((proto->item.extendedFlags & PROTO_EXT_FLAG_CAN_USE_ON) != PROTO_EXT_FLAG_NONE) {
        return true;
    }

    if (objectTypeFromPid(pid) == OBJ_TYPE_ITEM && proto->item.type == ITEM_TYPE_DRUG) {
        return true;
    }

    return false;
}

// 0x49EA24 proto_action_can_talk_to
bool _proto_action_can_talk_to(int pid)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return false;
    }

    if (objectTypeFromPid(pid) == OBJ_TYPE_CRITTER) {
        return true;
    }

    if ((proto->critter.extendedFlags & PROTO_EXT_FLAG_CAN_TALK_TO) != PROTO_EXT_FLAG_NONE) {
        return true;
    }

    return false;
}

// Likely returns true if item with given pid can be picked up.
//
// 0x49EA5C proto_action_can_pickup
int _proto_action_can_pickup(int pid)
{
    if (objectTypeFromPid(pid) != OBJ_TYPE_ITEM) {
        return false;
    }

    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return false;
    }

    if (proto->item.type == ITEM_TYPE_CONTAINER) {
        return (proto->item.extendedFlags & PROTO_EXT_FLAG_CAN_PICK_UP) != PROTO_EXT_FLAG_NONE;
    }

    return true;
}

// 0x49EAA4 proto_get_msg_info
char* protoGetMessage(int pid, int message)
{
    char* messageText = _proto_none_str;

    Proto* proto;
    if (protoGetProto(pid, &proto) != -1) {
        if (proto->messageId != -1) {
            MessageList* messageList = &(_proto_msg_files[objectTypeFromPid(pid)]);

            MessageListItem messageListItem;
            messageListItem.num = proto->messageId + message;
            if (messageListGetItem(messageList, &messageListItem)) {
                messageText = messageListItem.text;
            }
        }
    }

    return messageText;
}

// 0x49EAFC proto_name
char* protoGetName(int pid)
{
    if (pid == 0x1000000) {
        return critterGetName(gDude);
    }

    return protoGetMessage(pid, PROTOTYPE_MESSAGE_NAME);
}

// 0x49EB1C proto_description
char* protoGetDescription(int pid)
{
    return protoGetMessage(pid, PROTOTYPE_MESSAGE_DESCRIPTION);
}

// 0x49EB2C proto_item_init
int proto_item_init(Proto* proto, int pid)
{
    ItemFrameId protoNum = itemFrameIdFromPid(pid);

    proto->item.pid = -1;
    proto->item.messageId = 100 * protoNum;
    proto->item.fid = FrmId(protoNum - 1);
    if (!artExists(proto->item.fid)) {
        proto->item.fid = FrmId(ITEM_FRM_ID_FIRST);
    }
    proto->item.lightDistance = 0;
    proto->item.lightIntensity = 0;
    proto->item.flags = PROTO_FLAG_FLAT | PROTO_FLAG_LIGHT_THRU | PROTO_FLAG_SHOOT_THRU;
    proto->item.extendedFlags = PROTO_EXT_FLAG_LOOK | PROTO_EXT_FLAG_CAN_PICK_UP;
    proto->item.sid = -1;
    proto->item.type = ITEM_TYPE_MISC;
    proto_item_subdata_init(proto, proto->item.type);
    proto->item.material = MATERIAL_TYPE_METAL;
    proto->item.size = 1;
    proto->item.weight = 10;
    proto->item.cost = 0;
    proto->item.inventoryFid = -1;
    proto->item.soundId = '0';

    return 0;
}

// 0x49EBFC proto_item_subdata_init
int proto_item_subdata_init(Proto* proto, ItemType type)
{
    switch (type) {
    case ITEM_TYPE_ARMOR:
        proto->item.data.armor.armorClass = 0;

        for (DamageType damageType = DAMAGE_TYPE_FIRST; damageType < DAMAGE_TYPE_COUNT; damageType++) {
            proto->item.data.armor.damageResistance[damageType] = 0;
            proto->item.data.armor.damageThreshold[damageType] = 0;
        }

        proto->item.data.armor.perk = PERK_INVALID;
        proto->item.data.armor.maleFid = CRITTER_FRM_ID_INVALID;
        proto->item.data.armor.femaleFid = CRITTER_FRM_ID_INVALID;
        break;
    case ITEM_TYPE_CONTAINER:
        proto->item.data.container.openFlags = 0;
        proto->item.data.container.maxSize = 250;
        proto->item.extendedFlags |= PROTO_EXT_FLAG_CAN_USE;
        break;
    case ITEM_TYPE_DRUG:
        proto->item.data.drug.stat[0] = STAT_STRENGTH;
        proto->item.data.drug.stat[1] = STAT_INVALID;
        proto->item.data.drug.stat[2] = STAT_INVALID;
        proto->item.data.drug.amount[0] = 0;
        proto->item.data.drug.amount[1] = 0;
        proto->item.data.drug.amount[2] = 0;
        proto->item.data.drug.duration1 = 0;
        proto->item.data.drug.amount1[0] = 0;
        proto->item.data.drug.amount1[1] = 0;
        proto->item.data.drug.amount1[2] = 0;
        proto->item.data.drug.duration2 = 0;
        proto->item.data.drug.amount2[0] = 0;
        proto->item.data.drug.amount2[1] = 0;
        proto->item.data.drug.amount2[2] = 0;
        proto->item.data.drug.addictionChance = 0;
        proto->item.data.drug.withdrawalEffect = PERK_FIRST;
        proto->item.data.drug.withdrawalOnset = 0;
        proto->item.extendedFlags |= PROTO_EXT_FLAG_CAN_USE_ON;
        break;
    case ITEM_TYPE_WEAPON:
        proto->item.data.weapon.animationCode = WEAPON_ANIMATION_NONE;
        proto->item.data.weapon.minDamage = 0;
        proto->item.data.weapon.maxDamage = 0;
        proto->item.data.weapon.damageType = DAMAGE_TYPE_NORMAL;
        proto->item.data.weapon.maxRange1 = 0;
        proto->item.data.weapon.maxRange2 = 0;
        proto->item.data.weapon.projectilePid = -1;
        proto->item.data.weapon.minStrength = 0;
        proto->item.data.weapon.actionPointCost1 = 0;
        proto->item.data.weapon.actionPointCost2 = 0;
        proto->item.data.weapon.criticalFailureType = 0;
        proto->item.data.weapon.perk = PERK_INVALID;
        proto->item.data.weapon.rounds = 0;
        proto->item.data.weapon.caliber = CALIBER_TYPE_NONE;
        proto->item.data.weapon.ammoTypePid = -1;
        proto->item.data.weapon.ammoCapacity = 0;
        proto->item.data.weapon.soundCode = 0;
        break;
    case ITEM_TYPE_AMMO:
        proto->item.data.ammo.caliber = CALIBER_TYPE_NONE;
        proto->item.data.ammo.quantity = 20;
        proto->item.data.ammo.armorClassModifier = 0;
        proto->item.data.ammo.damageResistanceModifier = 0;
        proto->item.data.ammo.damageMultiplier = 1;
        proto->item.data.ammo.damageDivisor = 1;
        break;
    case ITEM_TYPE_MISC:
        proto->item.data.misc.powerTypePid = -1;
        proto->item.data.misc.powerType = 20;
        break;
    case ITEM_TYPE_KEY:
        proto->item.data.key.keyCode = -1;
        proto->item.extendedFlags |= PROTO_EXT_FLAG_CAN_USE_ON;
        break;
    default:
        break;
    }

    return 0;
}

// 0x49EDB4 proto_critter_init
int proto_critter_init(Proto* proto, int pid)
{
    if (!_protos_been_initialized) {
        return -1;
    }

    CritterFrameId num = critterFrameIdFromPid(pid);

    proto->pid = -1;
    proto->messageId = 100 * num;
    proto->fid = FrmId(num - 1, ANIM_STAND, WEAPON_ANIMATION_NONE, ROTATION_NE);
    proto->critter.lightDistance = 0;
    proto->critter.lightIntensity = 0;
    proto->critter.flags = PROTO_FLAG_LIGHT_THRU;
    proto->critter.extendedFlags = PROTO_EXT_FLAG_LOOK | PROTO_EXT_FLAG_CAN_TALK_TO;
    proto->critter.sid = -1;
    proto->critter.data.flags = CRITTER_NONE;
    proto->critter.data.bodyType = BODY_TYPE_BIPED;
    proto->critter.headFid = -1;
    proto->critter.aiPacket = 1;
    proto->critter.team = 0;
    if (!artExists(proto->fid)) {
        proto->fid = FrmId(CRITTER_FRM_ID_FIRST, ANIM_STAND, WEAPON_ANIMATION_NONE, ROTATION_NE);
    }

    CritterProtoData* data = &(proto->critter.data);
    data->experience = 60;
    data->killType = KILL_TYPE_MAN;
    data->damageType = DAMAGE_TYPE_NORMAL;
    protoCritterDataResetStats(data);
    protoCritterDataResetSkills(data);

    return 0;
}

// 0x49EEA4 clear_pupdate_data
void objectDataReset(Object* obj)
{
    // NOTE: Original code is slightly different. It uses loop to zero object
    // data byte by byte.
    memset(&(obj->data), 0, sizeof(obj->data));
}

// 0x49EEB8 proto_read_CombatData
static int objectCritterCombatDataRead(CritterCombatData* data, File* stream)
{
    if (fileReadInt32(stream, &(data->damageLastTurn)) == -1) return -1;
    if (fileReadInt32Enum<CritterManeuver>(stream, &(data->maneuver)) == -1) return -1;
    if (fileReadInt32(stream, &(data->ap)) == -1) return -1;
    if (fileReadInt32Enum<Dam>(stream, &(data->results)) == -1) return -1;
    if (fileReadInt32(stream, &(data->aiPacket)) == -1) return -1;
    if (fileReadInt32(stream, &(data->team)) == -1) return -1;
    if (fileReadInt32(stream, &(data->whoHitMeCid)) == -1) return -1;

    return 0;
}

// 0x49EF40 proto_write_CombatData
static int objectCritterCombatDataWrite(CritterCombatData* data, File* stream)
{
    if (fileWriteInt32(stream, data->damageLastTurn) == -1) return -1;
    if (fileWriteInt32Enum<CritterManeuver>(stream, data->maneuver) == -1) return -1;
    if (fileWriteInt32(stream, data->ap) == -1) return -1;
    if (fileWriteInt32Enum<Dam>(stream, data->results) == -1) return -1;
    if (fileWriteInt32(stream, data->aiPacket) == -1) return -1;
    if (fileWriteInt32(stream, data->team) == -1) return -1;
    if (fileWriteInt32(stream, data->whoHitMeCid) == -1) return -1;

    return 0;
}

// 0x49F004 proto_read_protoUpdateData
int objectDataRead(Object* obj, File* stream)
{
    Proto* proto;
    int temp;

    Inventory* inventory = &(obj->data.inventory);
    if (fileReadInt32(stream, &(inventory->length)) == -1) return -1;
    if (fileReadInt32(stream, &(inventory->capacity)) == -1) return -1;
    // CE: Original code reads inventory items pointer which is meaningless.
    if (fileReadInt32(stream, &temp) == -1) return -1;

    if (objectTypeFromPid(obj->pid) == OBJ_TYPE_CRITTER) {
        if (fileReadInt32(stream, &(obj->data.critter.reaction)) == -1) return -1;
        if (objectCritterCombatDataRead(&(obj->data.critter.combat), stream) == -1) return -1;
        if (fileReadInt32(stream, &(obj->data.critter.hp)) == -1) return -1;
        if (fileReadInt32(stream, &(obj->data.critter.radiation)) == -1) return -1;
        if (fileReadInt32(stream, &(obj->data.critter.poison)) == -1) return -1;
    } else {
        if (fileReadInt32(stream, &(obj->data.flags)) == -1) return -1;

        if (obj->data.flags == 0xCCCCCCCC) {
            debugPrint("\nNote: Reading pud: updated_flags was un-Set!");
            obj->data.flags = 0;
        }

        switch (objectTypeFromPid(obj->pid)) {
        case OBJ_TYPE_ITEM:
            if (protoGetProto(obj->pid, &proto) == -1) return -1;

            switch (proto->item.type) {
            case ITEM_TYPE_WEAPON:
                if (fileReadInt32(stream, &(obj->data.item.weapon.ammoQuantity)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.item.weapon.ammoTypePid)) == -1) return -1;
                break;
            case ITEM_TYPE_AMMO:
                if (fileReadInt32(stream, &(obj->data.item.ammo.quantity)) == -1) return -1;
                break;
            case ITEM_TYPE_MISC:
                if (fileReadInt32(stream, &(obj->data.item.misc.charges)) == -1) return -1;
                break;
            case ITEM_TYPE_KEY:
                if (fileReadInt32(stream, &(obj->data.item.key.keyCode)) == -1) return -1;
                break;
            default:
                break;
            }

            break;
        case OBJ_TYPE_SCENERY:
            if (protoGetProto(obj->pid, &proto) == -1) return -1;

            switch (proto->scenery.type) {
            case SCENERY_TYPE_DOOR:
                if (fileReadInt32(stream, &(obj->data.scenery.door.openFlags)) == -1) return -1;
                break;
            case SCENERY_TYPE_STAIRS:
                if (fileReadInt32(stream, &(obj->data.scenery.stairs.destinationBuiltTile)) == -1) return -1;
                if (fileReadInt32Enum<Map>(stream, &(obj->data.scenery.stairs.destinationMap)) == -1) return -1;
                break;
            case SCENERY_TYPE_ELEVATOR:
                if (fileReadInt32(stream, &(obj->data.scenery.elevator.type)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.scenery.elevator.level)) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_UP:
                if (gMapHeader.version == 19) {
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                } else {
                    if (fileReadInt32Enum<Map>(stream, &(obj->data.scenery.ladder.destinationMap)) == -1) return -1;
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                }
                break;
            case SCENERY_TYPE_LADDER_DOWN:
                if (gMapHeader.version == 19) {
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                } else {
                    if (fileReadInt32Enum<Map>(stream, &(obj->data.scenery.ladder.destinationMap)) == -1) return -1;
                    if (fileReadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                }
                break;
            default:
                break;
            }

            break;
        case OBJ_TYPE_MISC:
            if (isExitGridPid(obj->pid)) {
                if (fileReadInt32Enum<Map>(stream, &(obj->data.misc.map)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.misc.tile)) == -1) return -1;
                if (fileReadInt32(stream, &(obj->data.misc.elevation)) == -1) return -1;
                if (fileReadInt32Enum<Rotation>(stream, &(obj->data.misc.rotation)) == -1) return -1;
            }
            break;
        default:
            break;
        }
    }

    return 0;
}

// 0x49F428 proto_write_protoUpdateData
int objectDataWrite(Object* obj, File* stream)
{
    Proto* proto;

    ObjectData* data = &(obj->data);
    if (fileWriteInt32(stream, data->inventory.length) == -1) return -1;
    if (fileWriteInt32(stream, data->inventory.capacity) == -1) return -1;
    // CE: Original code writes inventory items pointer, which is meaningless.
    if (fileWriteInt32(stream, 0) == -1) return -1;

    if (objectTypeFromPid(obj->pid) == OBJ_TYPE_CRITTER) {
        if (fileWriteInt32(stream, data->flags) == -1) return -1;
        if (objectCritterCombatDataWrite(&(obj->data.critter.combat), stream) == -1) return -1;
        if (fileWriteInt32(stream, data->critter.hp) == -1) return -1;
        if (fileWriteInt32(stream, data->critter.radiation) == -1) return -1;
        if (fileWriteInt32(stream, data->critter.poison) == -1) return -1;
    } else {
        if (fileWriteInt32(stream, data->flags) == -1) return -1;

        switch (objectTypeFromPid(obj->pid)) {
        case OBJ_TYPE_ITEM:
            if (protoGetProto(obj->pid, &proto) == -1) return -1;

            switch (proto->item.type) {
            case ITEM_TYPE_WEAPON:
                if (fileWriteInt32(stream, data->item.weapon.ammoQuantity) == -1) return -1;
                if (fileWriteInt32(stream, data->item.weapon.ammoTypePid) == -1) return -1;
                break;
            case ITEM_TYPE_AMMO:
                if (fileWriteInt32(stream, data->item.ammo.quantity) == -1) return -1;
                break;
            case ITEM_TYPE_MISC:
                if (fileWriteInt32(stream, data->item.misc.charges) == -1) return -1;
                break;
            case ITEM_TYPE_KEY:
                if (fileWriteInt32(stream, data->item.key.keyCode) == -1) return -1;
                break;
            default:
                break;
            }
            break;
        case OBJ_TYPE_SCENERY:
            if (protoGetProto(obj->pid, &proto) == -1) return -1;

            switch (proto->scenery.type) {
            case SCENERY_TYPE_DOOR:
                if (fileWriteInt32(stream, data->scenery.door.openFlags) == -1) return -1;
                break;
            case SCENERY_TYPE_STAIRS:
                if (fileWriteInt32(stream, data->scenery.stairs.destinationBuiltTile) == -1) return -1;
                if (fileWriteInt32Enum<Map>(stream, data->scenery.stairs.destinationMap) == -1) return -1;
                break;
            case SCENERY_TYPE_ELEVATOR:
                if (fileWriteInt32(stream, data->scenery.elevator.type) == -1) return -1;
                if (fileWriteInt32(stream, data->scenery.elevator.level) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_UP:
                if (fileWriteInt32Enum<Map>(stream, data->scenery.ladder.destinationMap) == -1) return -1;
                if (fileWriteInt32(stream, data->scenery.ladder.destinationBuiltTile) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_DOWN:
                if (fileWriteInt32Enum<Map>(stream, data->scenery.ladder.destinationMap) == -1) return -1;
                if (fileWriteInt32(stream, data->scenery.ladder.destinationBuiltTile) == -1) return -1;
                break;
            default:
                break;
            }
            break;
        case OBJ_TYPE_MISC:
            if (isExitGridPid(obj->pid)) {
                if (fileWriteInt32Enum<Map>(stream, data->misc.map) == -1) return -1;
                if (fileWriteInt32(stream, data->misc.tile) == -1) return -1;
                if (fileWriteInt32(stream, data->misc.elevation) == -1) return -1;
                if (fileWriteInt32Enum<Rotation>(stream, data->misc.rotation) == -1) return -1;
            }
            break;
        default:
            break;
        }
    }

    return 0;
}

// 0x49F73C proto_update_gen
static int _proto_update_gen(Object* obj)
{
    Proto* proto;

    if (!_protos_been_initialized) {
        return -1;
    }

    ObjectData* data = &(obj->data);
    data->inventory.length = 0;
    data->inventory.capacity = 0;
    data->inventory.items = nullptr;

    if (protoGetProto(obj->pid, &proto) == -1) {
        return -1;
    }

    switch (objectTypeFromPid(obj->pid)) {
    case OBJ_TYPE_ITEM:
        switch (proto->item.type) {
        case ITEM_TYPE_CONTAINER:
            data->flags = 0;
            break;
        case ITEM_TYPE_WEAPON:
            data->item.weapon.ammoQuantity = proto->item.data.weapon.ammoCapacity;
            data->item.weapon.ammoTypePid = proto->item.data.weapon.ammoTypePid;
            break;
        case ITEM_TYPE_AMMO:
            data->item.ammo.quantity = proto->item.data.ammo.quantity;
            break;
        case ITEM_TYPE_MISC:
            data->item.misc.charges = proto->item.data.misc.charges;
            break;
        case ITEM_TYPE_KEY:
            data->item.key.keyCode = proto->item.data.key.keyCode;
            break;
        default:
            break;
        }
        break;
    case OBJ_TYPE_SCENERY:
        switch (proto->scenery.type) {
        case SCENERY_TYPE_DOOR:
            data->scenery.door.openFlags = proto->scenery.data.door.openFlags;
            break;
        case SCENERY_TYPE_STAIRS:
            data->scenery.stairs.destinationBuiltTile = proto->scenery.data.stairs.destinationBuiltTile;
            data->scenery.stairs.destinationMap = proto->scenery.data.stairs.destinationMap;
            break;
        case SCENERY_TYPE_ELEVATOR:
            data->scenery.elevator.type = proto->scenery.data.elevator.type;
            data->scenery.elevator.level = proto->scenery.data.elevator.level;
            break;
        case SCENERY_TYPE_LADDER_UP:
        case SCENERY_TYPE_LADDER_DOWN:
            data->scenery.ladder.destinationBuiltTile = proto->scenery.data.ladder.destinationBuiltTile;
            break;
        default:
            break;
        }
        break;
    case OBJ_TYPE_MISC:
        if (isExitGridPid(obj->pid)) {
            data->misc.tile = -1;
            data->misc.elevation = 0;
            data->misc.rotation = ROTATION_NE;
            data->misc.map = MAP_INVALID;
        }
        break;
    default:
        break;
    }

    return 0;
}

// 0x49F8A0 proto_update_init
int _proto_update_init(Object* obj)
{
    if (!_protos_been_initialized) {
        return -1;
    }

    if (obj == nullptr) {
        return -1;
    }

    if (obj->pid == -1) {
        return -1;
    }

    memset(&(obj->data), 0, sizeof(ObjectData));

    if (objectTypeFromPid(obj->pid) != OBJ_TYPE_CRITTER) {
        return _proto_update_gen(obj);
    }

    ObjectData* data = &(obj->data);
    data->inventory.length = 0;
    data->inventory.capacity = 0;
    data->inventory.items = nullptr;
    _combat_data_init(obj);
    data->critter.hp = critterGetStat(obj, STAT_MAXIMUM_HIT_POINTS);
    data->critter.combat.ap = critterGetStat(obj, STAT_MAXIMUM_ACTION_POINTS);
    critterUpdateDerivedStats(obj);
    obj->data.critter.combat.whoHitMe = nullptr;

    Proto* proto;
    if (protoGetProto(obj->pid, &proto) != -1) {
        data->critter.combat.aiPacket = proto->critter.aiPacket;
        data->critter.combat.team = proto->critter.team;
    }

    return 0;
}

// 0x49F984 proto_dude_update_gender
int _proto_dude_update_gender()
{
    Proto* proto;
    if (protoGetProto(0x1000000, &proto) == -1) {
        return -1;
    }

    DudeNativeLook nativeLook = DUDE_NATIVE_LOOK_TRIBAL;
    if (gameMovieIsSeen(MOVIE_VSUIT)) {
        nativeLook = DUDE_NATIVE_LOOK_JUMPSUIT;
    }

    CritterFrameId frmId;
    if (critterGetStat(gDude, STAT_GENDER) == GENDER_MALE) {
        frmId = _art_vault_person_nums[nativeLook][GENDER_MALE];
    } else {
        frmId = _art_vault_person_nums[nativeLook][GENDER_FEMALE];
    }

    _art_vault_guy_num = frmId;

    if (critterGetArmor(gDude) == nullptr) {
        WeaponAnimation weaponAnimationCode = WEAPON_ANIMATION_NONE;
        if (critterGetItem2(gDude) != nullptr || critterGetItem1(gDude) != nullptr) {
            weaponAnimationCode = weaponAnimationFromFid(gDude->fid);
        }

        int fid = FrmId(_art_vault_guy_num, ANIM_STAND, weaponAnimationCode, ROTATION_NE);
        objectSetFid(gDude, fid, nullptr);
    }

    proto->fid = FrmId(_art_vault_guy_num, ANIM_STAND, WEAPON_ANIMATION_NONE, ROTATION_NE);

    return 0;
}

// proto_dude_init
// 0x49FA64 proto_dude_init
int _proto_dude_init(const char* path)
{
    _retval = 0;

    gDudeProto.fid = FrmId(_art_vault_guy_num, ANIM_STAND, WEAPON_ANIMATION_NONE, ROTATION_NE);

    if (_init_true) {
        _obj_inven_free(&(gDude->data.inventory));
    }

    _init_true = 1;

    Proto* proto;
    if (protoGetProto(0x1000000, &proto) == -1) {
        return -1;
    }

    protoGetProto(gDude->pid, &proto);

    _proto_update_init(gDude);
    gDude->data.critter.combat.aiPacket = 0;
    gDude->data.critter.combat.team = 0;
    _ResetPlayer();

    if (gcdLoad(path) == -1) {
        _retval = -1;
    }

    proto->critter.data.baseStats[STAT_DAMAGE_RESISTANCE_EMP] = 100;
    proto->critter.data.bodyType = BODY_TYPE_BIPED;
    proto->critter.data.experience = 0;
    proto->critter.data.killType = KILL_TYPE_MAN;
    proto->critter.data.damageType = DAMAGE_TYPE_NORMAL;

    _proto_dude_update_gender();
    inventoryResetDude();

    if ((gDude->flags & OBJECT_FLAT) != OBJECT_NONE) {
        _obj_toggle_flat(gDude, nullptr);
    }

    if ((gDude->flags & OBJECT_NO_BLOCK) != OBJECT_NONE) {
        gDude->flags &= ~OBJECT_NO_BLOCK;
    }

    critterUpdateDerivedStats(gDude);
    critterAdjustHitPoints(gDude, 10000);

    if (_retval) {
        debugPrint("\n ** Error in proto_dude_init()! **\n");
    }

    return _retval;
}

// 0x49FBBC proto_scenery_init
int proto_scenery_init(Proto* proto, int pid)
{
    SceneryFrameId num = sceneryFrameIdFromPid(pid);

    proto->scenery.pid = -1;
    proto->scenery.messageId = 100 * num;
    proto->scenery.fid = FrmId(num - 1);
    if (!artExists(proto->scenery.fid)) {
        proto->scenery.fid = FrmId(SCENERY_FRM_ID_FIRST);
    }
    proto->scenery.lightDistance = 0;
    proto->scenery.lightIntensity = 0;
    proto->scenery.flags = PROTO_FLAG_NONE;
    proto->scenery.extendedFlags = PROTO_EXT_FLAG_LOOK;
    proto->scenery.sid = -1;
    proto->scenery.type = SCENERY_TYPE_GENERIC;
    proto_scenery_subdata_init(proto, proto->scenery.type);
    proto->scenery.material = MATERIAL_TYPE_INVALID;
    proto->scenery.soundId = '0';

    return 0;
}

// 0x49FC74 proto_scenery_subdata_init
int proto_scenery_subdata_init(Proto* proto, SceneryType type)
{
    switch (type) {
    case SCENERY_TYPE_DOOR:
        proto->scenery.data.door.openFlags = 0;
        proto->scenery.extendedFlags |= PROTO_EXT_FLAG_CAN_USE;
        break;
    case SCENERY_TYPE_STAIRS:
        proto->scenery.data.stairs.destinationBuiltTile = -1;
        proto->scenery.data.stairs.destinationMap = MAP_INVALID;
        proto->scenery.extendedFlags |= PROTO_EXT_FLAG_CAN_USE;
        break;
    case SCENERY_TYPE_ELEVATOR:
        proto->scenery.data.elevator.type = -1;
        proto->scenery.data.elevator.level = -1;
        proto->scenery.extendedFlags |= PROTO_EXT_FLAG_CAN_USE;
        break;
    case SCENERY_TYPE_LADDER_UP:
        proto->scenery.data.ladder.destinationBuiltTile = -1;
        proto->scenery.extendedFlags |= PROTO_EXT_FLAG_CAN_USE;
        break;
    case SCENERY_TYPE_LADDER_DOWN:
        proto->scenery.data.ladder.destinationBuiltTile = -1;
        proto->scenery.extendedFlags |= PROTO_EXT_FLAG_CAN_USE;
        break;
    default:
        break;
    }

    return 0;
}

// 0x49FCFC proto_wall_init
int proto_wall_init(Proto* proto, int pid)
{
    WallFrameId num = wallFrameIdFromPid(pid);

    proto->wall.pid = -1;
    proto->wall.messageId = 100 * num;
    proto->wall.fid = FrmId(num - 1);
    if (!artExists(proto->wall.fid)) {
        proto->wall.fid = FrmId(WALL_FRM_ID_FIRST);
    }
    proto->wall.lightDistance = 0;
    proto->wall.lightIntensity = 0;
    proto->wall.flags = PROTO_FLAG_NONE;
    proto->wall.extendedFlags = PROTO_EXT_FLAG_LOOK;
    proto->wall.sid = -1;
    proto->wall.material = MATERIAL_TYPE_METAL;

    return 0;
}

// 0x49FD84 proto_tile_init
int proto_tile_init(Proto* proto, int pid)
{
    TileFrameId num = tileFrameIdFromPid(pid);

    proto->tile.pid = -1;
    proto->tile.messageId = 100 * num;
    proto->tile.fid = FrmId(num - 1);
    if (!artExists(proto->tile.fid)) {
        proto->tile.fid = FrmId(TILE_FRM_ID_FIRST);
    }
    proto->tile.flags = PROTO_FLAG_NONE;
    proto->tile.extendedFlags = PROTO_EXT_FLAG_LOOK;
    proto->tile.sid = -1;
    proto->tile.material = MATERIAL_TYPE_METAL;

    return 0;
}

// 0x49FDFC proto_misc_init
int proto_misc_init(Proto* proto, int pid)
{
    MiscFrameId num = miscFrameIdFromPid(pid);

    proto->misc.pid = -1;
    proto->misc.messageId = 100 * num;
    proto->misc.fid = FrmId(num - 1);
    if (!artExists(proto->misc.fid)) {
        proto->misc.fid = FrmId(MISC_FRM_ID_FIRST);
    }
    proto->misc.lightDistance = 0;
    proto->misc.lightIntensity = 0;
    proto->misc.flags = PROTO_FLAG_NONE;
    proto->misc.extendedFlags = PROTO_EXT_FLAG_NONE;

    return 0;
}

// 0x49FE74 proto_copy_proto
int proto_copy_proto(int srcPid, int dstPid)
{
    Proto* src;
    Proto* dst;

    ObjectType srcType = objectTypeFromPid(srcPid);
    ObjectType dstType = objectTypeFromPid(dstPid);
    if (srcType != dstType) {
        return -1;
    }

    if (protoGetProto(srcPid, &src) == -1) {
        return -1;
    }

    if (protoGetProto(dstPid, &dst) == -1) {
        return -1;
    }

    memcpy(dst, src, _proto_sizes[srcType]);
    dst->pid = dstPid;

    return 0;
}

// 0x49FEDC proto_is_subtype
bool proto_is_subtype(Proto* proto, int subtype)
{
    if (subtype == -1) {
        return true;
    }

    switch (objectTypeFromPid(proto->pid)) {
    case OBJ_TYPE_ITEM:
        return proto->item.type == subtype;
    case OBJ_TYPE_SCENERY:
        return proto->scenery.type == subtype;
    default:
        return false;
    }
}

// proto_data_member
// 0x49FFD8 proto_data_member
int protoGetDataMember(int pid, int member, ProtoDataMemberValue* value)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return -1;
    }

    switch (objectTypeFromPid(pid)) {
    case OBJ_TYPE_ITEM:
        switch (member) {
        case ITEM_DATA_MEMBER_PID:
            value->integerValue = proto->pid;
            break;
        case ITEM_DATA_MEMBER_NAME:
            // NOTE: uninline
            value->stringValue = protoGetName(proto->scenery.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case ITEM_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case ITEM_DATA_MEMBER_FID:
            value->integerValue = proto->fid;
            break;
        case ITEM_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->item.lightDistance;
            break;
        case ITEM_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->item.lightIntensity;
            break;
        case ITEM_DATA_MEMBER_FLAGS:
            value->integerValue = proto->item.flags;
            break;
        case ITEM_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->item.extendedFlags;
            break;
        case ITEM_DATA_MEMBER_SID:
            value->integerValue = proto->item.sid;
            break;
        case ITEM_DATA_MEMBER_TYPE:
            value->integerValue = proto->item.type;
            break;
        case ITEM_DATA_MEMBER_MATERIAL:
            value->integerValue = proto->item.material;
            break;
        case ITEM_DATA_MEMBER_SIZE:
            value->integerValue = proto->item.size;
            break;
        case ITEM_DATA_MEMBER_WEIGHT:
            value->integerValue = proto->item.weight;
            break;
        case ITEM_DATA_MEMBER_COST:
            value->integerValue = proto->item.cost;
            break;
        case ITEM_DATA_MEMBER_INVENTORY_FID:
            value->integerValue = proto->item.inventoryFid;
            break;
        case ITEM_DATA_MEMBER_WEAPON_RANGE:
            if (proto->item.type == ITEM_TYPE_WEAPON) {
                value->integerValue = proto->item.data.weapon.maxRange1;
            }
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_CRITTER:
        switch (member) {
        case CRITTER_DATA_MEMBER_PID:
            value->integerValue = proto->critter.pid;
            break;
        case CRITTER_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = protoGetName(proto->critter.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case CRITTER_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->critter.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case CRITTER_DATA_MEMBER_FID:
            value->integerValue = proto->critter.fid;
            break;
        case CRITTER_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->critter.lightDistance;
            break;
        case CRITTER_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->critter.lightIntensity;
            break;
        case CRITTER_DATA_MEMBER_FLAGS:
            value->integerValue = proto->critter.flags;
            break;
        case CRITTER_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->critter.extendedFlags;
            break;
        case CRITTER_DATA_MEMBER_SID:
            value->integerValue = proto->critter.sid;
            break;
        case CRITTER_DATA_MEMBER_HEAD_FID:
            value->integerValue = proto->critter.headFid;
            break;
        case CRITTER_DATA_MEMBER_BODY_TYPE:
            value->integerValue = proto->critter.data.bodyType;
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_SCENERY:
        switch (member) {
        case SCENERY_DATA_MEMBER_PID:
            value->integerValue = proto->scenery.pid;
            break;
        case SCENERY_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = protoGetName(proto->scenery.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case SCENERY_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->scenery.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case SCENERY_DATA_MEMBER_FID:
            value->integerValue = proto->scenery.fid;
            break;
        case SCENERY_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->scenery.lightDistance;
            break;
        case SCENERY_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->scenery.lightIntensity;
            break;
        case SCENERY_DATA_MEMBER_FLAGS:
            value->integerValue = proto->scenery.flags;
            break;
        case SCENERY_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->scenery.extendedFlags;
            break;
        case SCENERY_DATA_MEMBER_SID:
            value->integerValue = proto->scenery.sid;
            break;
        case SCENERY_DATA_MEMBER_TYPE:
            value->integerValue = proto->scenery.type;
            break;
        case SCENERY_DATA_MEMBER_MATERIAL:
            value->integerValue = proto->scenery.material;
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_WALL:
        switch (member) {
        case WALL_DATA_MEMBER_PID:
            value->integerValue = proto->wall.pid;
            break;
        case WALL_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = protoGetName(proto->wall.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case WALL_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->wall.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case WALL_DATA_MEMBER_FID:
            value->integerValue = proto->wall.fid;
            break;
        case WALL_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->wall.lightDistance;
            break;
        case WALL_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->wall.lightIntensity;
            break;
        case WALL_DATA_MEMBER_FLAGS:
            value->integerValue = proto->wall.flags;
            break;
        case WALL_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->wall.extendedFlags;
            break;
        case WALL_DATA_MEMBER_SID:
            value->integerValue = proto->wall.sid;
            break;
        case WALL_DATA_MEMBER_MATERIAL:
            value->integerValue = proto->wall.material;
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_TILE:
        debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
        break;
    case OBJ_TYPE_MISC:
        switch (member) {
        case MISC_DATA_MEMBER_PID:
            value->integerValue = proto->misc.pid;
            break;
        case MISC_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = protoGetName(proto->misc.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case MISC_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = protoGetDescription(proto->misc.pid);
            // FIXME: Errornously report type as int, should be string.
            return PROTO_DATA_MEMBER_TYPE_INT;
        case MISC_DATA_MEMBER_FID:
            value->integerValue = proto->misc.fid;
            return 1;
        case MISC_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->misc.lightDistance;
            return 1;
        case MISC_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->misc.lightIntensity;
            break;
        case MISC_DATA_MEMBER_FLAGS:
            value->integerValue = proto->misc.flags;
            break;
        case MISC_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->misc.extendedFlags;
            break;
        default:
            debugPrint("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    default:
        break;
    }

    return PROTO_DATA_MEMBER_TYPE_INT;
}

// proto_init
// 0x4A0390 proto_init
int protoInit()
{
    size_t len;
    MessageListItem messageListItem;
    char path[COMPAT_MAX_PATH];

    snprintf(path, sizeof(path), "%s\\proto", settings.system.master_patches_path.c_str());
    len = strlen(path);

    compat_mkdir(path);

    strcpy(path + len, "\\critters");
    compat_mkdir(path);

    strcpy(path + len, "\\items");
    compat_mkdir(path);

    // TODO: Get rid of cast.
    proto_critter_init((Proto*)&gDudeProto, 0x1000000);

    gDudeProto.pid = 0x1000000;
    gDudeProto.fid = FrmId(CRITTER_FRM_ID_1, ANIM_STAND, WEAPON_ANIMATION_NONE, ROTATION_NE);

    gDude->pid = 0x1000000;
    gDude->sid = 1;

    for (ObjectType i = OBJ_TYPE_FIRST; i < OBJ_TYPE_PROTO_COUNT; i++) {
        _proto_remove_list(i);
    }

    _proto_header_load();

    _protos_been_initialized = 1;

    _proto_dude_init("premade\\player.gcd");

    for (ObjectType i = OBJ_TYPE_FIRST; i < OBJ_TYPE_PROTO_COUNT; i++) {
        if (!messageListInit(&(_proto_msg_files[i]))) {
            debugPrint("\nError: Initing proto message files!");
            return -1;
        }
    }

    for (ObjectType i = OBJ_TYPE_FIRST; i < OBJ_TYPE_PROTO_COUNT; i++) {
        snprintf(path, sizeof(path), "%spro_%.4s%s", asc_5186C8, artGetObjectTypeName(i), ".msg");

        if (!messageListLoad(&(_proto_msg_files[i]), path)) {
            debugPrint("\nError: Loading proto message files!");
            return -1;
        }
    }

    for (ObjectType i = OBJ_TYPE_FIRST; i < OBJ_TYPE_PROTO_COUNT; i++) {
        messageListRepositorySetProtoMessageList(i, &(_proto_msg_files[i]));
    }

    _mp_critter_stats_list = _aDrugStatSpecia;
    _critter_stats_list = _critter_stats_list_strs;
    _critter_stats_list_None = _aNone_1;
    for (Stat stat = STAT_FIRST; stat < STAT_COUNT; stat++) {
        _critter_stats_list_strs[stat] = statGetName(stat);
        if (_critter_stats_list_strs[stat] == nullptr) {
            debugPrint("\nError: Finding stat names!");
            return -1;
        }
    }

    _mp_perk_code_None = _aNone_1;
    _perk_code_strs = _mp_perk_code_strs;
    for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
        _mp_perk_code_strs[perk] = perkGetName(perk);
        if (_mp_perk_code_strs[perk] == nullptr) {
            debugPrint("\nError: Finding perk names!");
            return -1;
        }
    }

    if (!messageListInit(&gProtoMessageList)) {
        debugPrint("\nError: Initing main proto message file!");
        return -1;
    }

    snprintf(path, sizeof(path), "%sproto.msg", asc_5186C8);

    if (!messageListLoad(&gProtoMessageList, path)) {
        debugPrint("\nError: Loading main proto message file!");
        return -1;
    }

    _proto_none_str = getmsg(&gProtoMessageList, &messageListItem, 10);

    // material type names
    for (MaterialType materialType = MATERIAL_TYPE_FIRST; materialType < MATERIAL_TYPE_COUNT; materialType++) {
        gMaterialTypeNames[materialType] = getmsg(&gProtoMessageList, &messageListItem, 100 + materialType);
    }

    // item type names
    for (ItemType itemType = ITEM_TYPE_FIRST; itemType < ITEM_TYPE_COUNT; itemType++) {
        gItemTypeNames[itemType] = getmsg(&gProtoMessageList, &messageListItem, 150 + itemType);
    }

    // scenery type names
    for (SceneryType sceneryType = SCENERY_TYPE_FIRST; sceneryType < SCENERY_TYPE_COUNT; sceneryType++) {
        gSceneryTypeNames[sceneryType] = getmsg(&gProtoMessageList, &messageListItem, 200 + sceneryType);
    }

    // damage code types
    for (DamageType damageType = DAMAGE_TYPE_FIRST; damageType < DAMAGE_TYPE_COUNT; damageType++) {
        gDamageTypeNames[damageType] = getmsg(&gProtoMessageList, &messageListItem, 250 + damageType);
    }

    // caliber types
    for (CaliberType caliberType = CALIBER_TYPE_FIRST; caliberType < CALIBER_TYPE_COUNT; caliberType++) {
        gCaliberTypeNames[caliberType] = getmsg(&gProtoMessageList, &messageListItem, 300 + caliberType);
    }

    // race types
    for (RaceType raceType = RACE_TYPE_FIRST; raceType < RACE_TYPE_COUNT; raceType++) {
        gRaceTypeNames[raceType] = getmsg(&gProtoMessageList, &messageListItem, 350 + raceType);
    }

    // body types
    for (BodyType bodyType = BODY_TYPE_FIRST; bodyType < BODY_TYPE_COUNT; bodyType++) {
        gBodyTypeNames[bodyType] = getmsg(&gProtoMessageList, &messageListItem, 400 + bodyType);
    }

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_PROTO, &gProtoMessageList);

    return 0;
}

// 0x4A0814 proto_reset
void protoReset()
{
    // TODO: Get rid of cast.
    proto_critter_init((Proto*)&gDudeProto, 0x1000000);
    gDudeProto.pid = 0x1000000;
    gDudeProto.fid = FrmId(CRITTER_FRM_ID_1, ANIM_STAND, WEAPON_ANIMATION_NONE, ROTATION_NE);

    gDude->pid = 0x1000000;
    gDude->sid = -1;
    gDude->flags &= ~OBJECT_FLAG_0xFC000;

    for (ObjectType i = OBJ_TYPE_FIRST; i < OBJ_TYPE_PROTO_COUNT; i++) {
        _proto_remove_list(i);
    }

    _proto_header_load();

    _protos_been_initialized = 1;
    _proto_dude_init("premade\\player.gcd");
}

// 0x4A0898 proto_exit
void protoExit()
{
    for (ObjectType i = OBJ_TYPE_FIRST; i < OBJ_TYPE_PROTO_COUNT; i++) {
        _proto_remove_list(i);
    }

    for (ObjectType i = OBJ_TYPE_FIRST; i < OBJ_TYPE_PROTO_COUNT; i++) {
        messageListRepositorySetProtoMessageList(i, nullptr);
        messageListFree(&(_proto_msg_files[i]));
    }

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_PROTO, nullptr);
    messageListFree(&gProtoMessageList);
}

// Count .pro lines in .lst files.
//
// 0x4A08E0 proto_header_load
static int _proto_header_load()
{
    for (ObjectType index = OBJ_TYPE_FIRST; index < OBJ_TYPE_PROTO_COUNT; index++) {
        ProtoList* ptr = &(_protoLists[index]);
        ptr->head = nullptr;
        ptr->tail = nullptr;
        ptr->length = 0;
        ptr->max_entries_num = 1;

        char path[COMPAT_MAX_PATH];
        proto_make_path(path, index << 24);
        strcat(path, "\\");
        strcat(path, artGetObjectTypeName(index));
        strcat(path, ".lst");

        File* stream = fileOpen(path, "rt");
        if (stream == nullptr) {
            return -1;
        }

        int ch = '\0';
        while (1) {
            ch = fileReadChar(stream);
            if (ch == -1) {
                break;
            }

            if (ch == '\n') {
                ptr->max_entries_num++;
            }
        }

        if (ch != '\n') {
            ptr->max_entries_num++;
        }

        fileClose(stream);
    }

    return 0;
}

// 0x4A0AEC proto_read_item_data
static int protoItemDataRead(ItemProtoData* item_data, ItemType type, File* stream)
{
    switch (type) {
    case ITEM_TYPE_ARMOR:
        if (fileReadInt32(stream, &(item_data->armor.armorClass)) == -1) return -1;
        if (fileReadInt32List(stream, item_data->armor.damageResistance, 7) == -1) return -1;
        if (fileReadInt32List(stream, item_data->armor.damageThreshold, 7) == -1) return -1;
        if (fileReadInt32Enum<Perk>(stream, &(item_data->armor.perk)) == -1) return -1;
        if (fileReadInt32Enum<CritterFrameId>(stream, &(item_data->armor.maleFid)) == -1) return -1;
        if (fileReadInt32Enum<CritterFrameId>(stream, &(item_data->armor.femaleFid)) == -1) return -1;

        return 0;
    case ITEM_TYPE_CONTAINER:
        if (fileReadInt32(stream, &(item_data->container.maxSize)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->container.openFlags)) == -1) return -1;

        return 0;
    case ITEM_TYPE_DRUG:
        if (fileReadInt32Enum<Stat>(stream, &(item_data->drug.stat[0])) == -1) return -1;
        if (fileReadInt32Enum<Stat>(stream, &(item_data->drug.stat[1])) == -1) return -1;
        if (fileReadInt32Enum<Stat>(stream, &(item_data->drug.stat[2])) == -1) return -1;
        if (fileReadInt32List(stream, item_data->drug.amount, 3) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.duration1)) == -1) return -1;
        if (fileReadInt32List(stream, item_data->drug.amount1, 3) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.duration2)) == -1) return -1;
        if (fileReadInt32List(stream, item_data->drug.amount2, 3) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.addictionChance)) == -1) return -1;
        if (fileReadInt32Enum<Perk>(stream, &(item_data->drug.withdrawalEffect)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->drug.withdrawalOnset)) == -1) return -1;

        return 0;
    case ITEM_TYPE_WEAPON:
        if (fileReadInt32Enum<WeaponAnimation>(stream, &(item_data->weapon.animationCode)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.minDamage)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.maxDamage)) == -1) return -1;
        if (fileReadInt32Enum<DamageType>(stream, &(item_data->weapon.damageType)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.maxRange1)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.maxRange2)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.projectilePid)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.minStrength)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.actionPointCost1)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.actionPointCost2)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.criticalFailureType)) == -1) return -1;
        if (fileReadInt32Enum<Perk>(stream, &(item_data->weapon.perk)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.rounds)) == -1) return -1;
        if (fileReadInt32Enum<CaliberType>(stream, &(item_data->weapon.caliber)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.ammoTypePid)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->weapon.ammoCapacity)) == -1) return -1;
        if (fileReadUInt8(stream, &(item_data->weapon.soundCode)) == -1) return -1;

        return 0;
    case ITEM_TYPE_AMMO:
        if (fileReadInt32Enum<CaliberType>(stream, &(item_data->ammo.caliber)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.quantity)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.armorClassModifier)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.damageResistanceModifier)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.damageMultiplier)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->ammo.damageDivisor)) == -1) return -1;

        return 0;
    case ITEM_TYPE_MISC:
        if (fileReadInt32(stream, &(item_data->misc.powerTypePid)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->misc.powerType)) == -1) return -1;
        if (fileReadInt32(stream, &(item_data->misc.charges)) == -1) return -1;

        return 0;
    case ITEM_TYPE_KEY:
        if (fileReadInt32(stream, &(item_data->key.keyCode)) == -1) return -1;

        return 0;
    default:
        return -1;
    }
}

// 0x4A0ED0 proto_read_scenery_data
static int protoSceneryDataRead(SceneryProtoData* scenery_data, SceneryType type, File* stream)
{
    switch (type) {
    case SCENERY_TYPE_DOOR:
        if (fileReadInt32(stream, &(scenery_data->door.openFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(scenery_data->door.keyCode)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_STAIRS:
        if (fileReadInt32(stream, &(scenery_data->stairs.destinationBuiltTile)) == -1) return -1;
        if (fileReadInt32Enum<Map>(stream, &(scenery_data->stairs.destinationMap)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_ELEVATOR:
        if (fileReadInt32(stream, &(scenery_data->elevator.type)) == -1) return -1;
        if (fileReadInt32(stream, &(scenery_data->elevator.level)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_LADDER_UP:
    case SCENERY_TYPE_LADDER_DOWN:
        if (fileReadInt32(stream, &(scenery_data->ladder.destinationBuiltTile)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_GENERIC:
        if (fileReadInt32(stream, &(scenery_data->generic.genericFlags)) == -1) return -1;

        return 0;
    default:
        return -1;
    }
}

// read .pro file
// 0x4A0FA0 proto_read_protoSubNode
static int protoRead(Proto* proto, File* stream)
{
    if (fileReadInt32(stream, &(proto->pid)) == -1) return -1;
    if (fileReadInt32(stream, &(proto->messageId)) == -1) return -1;
    if (fileReadInt32(stream, &(proto->fid)) == -1) return -1;

    switch (objectTypeFromPid(proto->pid)) {
    case OBJ_TYPE_ITEM:
        if (fileReadInt32(stream, &(proto->item.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->item.lightIntensity)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoFlags>(stream, &(proto->item.flags)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoExtendedFlags>(stream, &(proto->item.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.sid)) == -1) return -1;
        if (fileReadInt32Enum<ItemType>(stream, &(proto->item.type)) == -1) return -1;
        if (fileReadInt32Enum<MaterialType>(stream, &(proto->item.material)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.size)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->item.weight)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.cost)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->item.inventoryFid)) == -1) return -1;
        if (fileReadUInt8(stream, &(proto->item.soundId)) == -1) return -1;
        if (protoItemDataRead(&(proto->item.data), proto->item.type, stream) == -1) return -1;

        return 0;
    case OBJ_TYPE_CRITTER:
        if (fileReadInt32(stream, &(proto->critter.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->critter.lightIntensity)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoFlags>(stream, &(proto->critter.flags)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoExtendedFlags>(stream, &(proto->critter.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.sid)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.headFid)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.aiPacket)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->critter.team)) == -1) return -1;

        if (protoCritterDataRead(stream, &(proto->critter.data)) == -1) return -1;

        return 0;
    case OBJ_TYPE_SCENERY:
        if (fileReadInt32(stream, &(proto->scenery.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->scenery.lightIntensity)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoFlags>(stream, &(proto->scenery.flags)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoExtendedFlags>(stream, &(proto->scenery.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->scenery.sid)) == -1) return -1;
        if (fileReadInt32Enum<SceneryType>(stream, &(proto->scenery.type)) == -1) return -1;
        if (fileReadInt32Enum<MaterialType>(stream, &(proto->scenery.material)) == -1) return -1;
        if (fileReadUInt8(stream, &(proto->scenery.soundId)) == -1) return -1;
        if (protoSceneryDataRead(&(proto->scenery.data), proto->scenery.type, stream) == -1) return -1;
        return 0;
    case OBJ_TYPE_WALL:
        if (fileReadInt32(stream, &(proto->wall.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->wall.lightIntensity)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoFlags>(stream, &(proto->wall.flags)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoExtendedFlags>(stream, &(proto->wall.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->wall.sid)) == -1) return -1;
        if (fileReadInt32Enum<MaterialType>(stream, &(proto->wall.material)) == -1) return -1;

        return 0;
    case OBJ_TYPE_TILE:
        if (fileReadUInt32Enum<ProtoFlags>(stream, &(proto->tile.flags)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoExtendedFlags>(stream, &(proto->tile.extendedFlags)) == -1) return -1;
        if (fileReadInt32(stream, &(proto->tile.sid)) == -1) return -1;
        if (fileReadInt32Enum<MaterialType>(stream, &(proto->tile.material)) == -1) return -1;

        return 0;
    case OBJ_TYPE_MISC:
        if (fileReadInt32(stream, &(proto->misc.lightDistance)) == -1) return -1;
        if (_db_freadInt(stream, &(proto->misc.lightIntensity)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoFlags>(stream, &(proto->misc.flags)) == -1) return -1;
        if (fileReadUInt32Enum<ProtoExtendedFlags>(stream, &(proto->misc.extendedFlags)) == -1) return -1;

        return 0;
    default:
        return -1;
    }
}

// 0x4A1390 proto_write_item_data
static int protoItemDataWrite(ItemProtoData* item_data, ItemType type, File* stream)
{
    switch (type) {
    case ITEM_TYPE_ARMOR:
        if (fileWriteInt32(stream, item_data->armor.armorClass) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->armor.damageResistance, 7) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->armor.damageThreshold, 7) == -1) return -1;
        if (fileWriteInt32Enum<Perk>(stream, item_data->armor.perk) == -1) return -1;
        if (fileWriteInt32Enum<CritterFrameId>(stream, item_data->armor.maleFid) == -1) return -1;
        if (fileWriteInt32Enum<CritterFrameId>(stream, item_data->armor.femaleFid) == -1) return -1;

        return 0;
    case ITEM_TYPE_CONTAINER:
        if (fileWriteInt32(stream, item_data->container.maxSize) == -1) return -1;
        if (fileWriteInt32(stream, item_data->container.openFlags) == -1) return -1;

        return 0;
    case ITEM_TYPE_DRUG:
        if (fileWriteInt32Enum<Stat>(stream, item_data->drug.stat[0]) == -1) return -1;
        if (fileWriteInt32Enum<Stat>(stream, item_data->drug.stat[1]) == -1) return -1;
        if (fileWriteInt32Enum<Stat>(stream, item_data->drug.stat[2]) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->drug.amount, 3) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.duration1) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->drug.amount1, 3) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.duration2) == -1) return -1;
        if (fileWriteInt32List(stream, item_data->drug.amount2, 3) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.addictionChance) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.withdrawalEffect) == -1) return -1;
        if (fileWriteInt32(stream, item_data->drug.withdrawalOnset) == -1) return -1;

        return 0;
    case ITEM_TYPE_WEAPON:
        if (fileWriteInt32Enum<WeaponAnimation>(stream, item_data->weapon.animationCode) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.maxDamage) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.minDamage) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.damageType) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.maxRange1) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.maxRange2) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.projectilePid) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.minStrength) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.actionPointCost1) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.actionPointCost2) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.criticalFailureType) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.perk) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.rounds) == -1) return -1;
        if (fileWriteInt32Enum<CaliberType>(stream, item_data->weapon.caliber) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.ammoTypePid) == -1) return -1;
        if (fileWriteInt32(stream, item_data->weapon.ammoCapacity) == -1) return -1;
        if (fileWriteUInt8(stream, item_data->weapon.soundCode) == -1) return -1;

        return 0;
    case ITEM_TYPE_AMMO:
        if (fileWriteInt32Enum<CaliberType>(stream, item_data->ammo.caliber) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.quantity) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.armorClassModifier) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.damageResistanceModifier) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.damageMultiplier) == -1) return -1;
        if (fileWriteInt32(stream, item_data->ammo.damageDivisor) == -1) return -1;

        return 0;
    case ITEM_TYPE_MISC:
        if (fileWriteInt32(stream, item_data->misc.powerTypePid) == -1) return -1;
        if (fileWriteInt32(stream, item_data->misc.powerType) == -1) return -1;
        if (fileWriteInt32(stream, item_data->misc.charges) == -1) return -1;

        return 0;
    case ITEM_TYPE_KEY:
        if (fileWriteInt32(stream, item_data->key.keyCode) == -1) return -1;

        return 0;
    default:
        return -1;
    }
}

// 0x4A16E4 proto_write_scenery_data
static int protoSceneryDataWrite(SceneryProtoData* scenery_data, SceneryType type, File* stream)
{
    switch (type) {
    case SCENERY_TYPE_DOOR:
        if (fileWriteInt32(stream, scenery_data->door.openFlags) == -1) return -1;
        if (fileWriteInt32(stream, scenery_data->door.keyCode) == -1) return -1;

        return 0;
    case SCENERY_TYPE_STAIRS:
        if (fileWriteInt32(stream, scenery_data->stairs.destinationBuiltTile) == -1) return -1;
        if (fileWriteInt32Enum<Map>(stream, scenery_data->stairs.destinationMap) == -1) return -1;

        return 0;
    case SCENERY_TYPE_ELEVATOR:
        if (fileWriteInt32(stream, scenery_data->elevator.type) == -1) return -1;
        if (fileWriteInt32(stream, scenery_data->elevator.level) == -1) return -1;

        return 0;
    case SCENERY_TYPE_LADDER_UP:
    case SCENERY_TYPE_LADDER_DOWN:
        if (fileWriteInt32(stream, scenery_data->ladder.destinationBuiltTile) == -1) return -1;

        return 0;
    case SCENERY_TYPE_GENERIC:
        if (fileWriteInt32(stream, scenery_data->generic.genericFlags) == -1) return -1;

        return 0;
    default:
        return -1;
    }
}

// 0x4A17B4 proto_write_protoSubNode
static int protoWrite(Proto* proto, File* stream)
{
    if (fileWriteInt32(stream, proto->pid) == -1) return -1;
    if (fileWriteInt32(stream, proto->messageId) == -1) return -1;
    if (fileWriteInt32(stream, proto->fid) == -1) return -1;

    switch (objectTypeFromPid(proto->pid)) {
    case OBJ_TYPE_ITEM:
        if (fileWriteInt32(stream, proto->item.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->item.lightIntensity) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoFlags>(stream, proto->item.flags) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoExtendedFlags>(stream, proto->item.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.type) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.material) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.size) == -1) return -1;
        if (_db_fwriteLong(stream, proto->item.weight) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.cost) == -1) return -1;
        if (fileWriteInt32(stream, proto->item.inventoryFid) == -1) return -1;
        if (fileWriteUInt8(stream, proto->item.soundId) == -1) return -1;
        if (protoItemDataWrite(&(proto->item.data), proto->item.type, stream) == -1) return -1;

        return 0;
    case OBJ_TYPE_CRITTER:
        if (fileWriteInt32(stream, proto->critter.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->critter.lightIntensity) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoFlags>(stream, proto->critter.flags) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoExtendedFlags>(stream, proto->critter.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.headFid) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.aiPacket) == -1) return -1;
        if (fileWriteInt32(stream, proto->critter.team) == -1) return -1;
        if (protoCritterDataWrite(stream, &(proto->critter.data)) == -1) return -1;

        return 0;
    case OBJ_TYPE_SCENERY:
        if (fileWriteInt32(stream, proto->scenery.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->scenery.lightIntensity) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoFlags>(stream, proto->scenery.flags) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoExtendedFlags>(stream, proto->scenery.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->scenery.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->scenery.type) == -1) return -1;
        if (fileWriteInt32(stream, proto->scenery.material) == -1) return -1;
        if (fileWriteUInt8(stream, proto->scenery.soundId) == -1) return -1;
        if (protoSceneryDataWrite(&(proto->scenery.data), proto->scenery.type, stream) == -1) return -1;

        return 0;
    case OBJ_TYPE_WALL:
        if (fileWriteInt32(stream, proto->wall.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->wall.lightIntensity) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoFlags>(stream, proto->wall.flags) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoExtendedFlags>(stream, proto->wall.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->wall.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->wall.material) == -1) return -1;

        return 0;
    case OBJ_TYPE_TILE:
        if (fileWriteUInt32Enum<ProtoFlags>(stream, proto->tile.flags) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoExtendedFlags>(stream, proto->tile.extendedFlags) == -1) return -1;
        if (fileWriteInt32(stream, proto->tile.sid) == -1) return -1;
        if (fileWriteInt32(stream, proto->tile.material) == -1) return -1;

        return 0;
    case OBJ_TYPE_MISC:
        if (fileWriteInt32(stream, proto->misc.lightDistance) == -1) return -1;
        if (_db_fwriteLong(stream, proto->misc.lightIntensity) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoFlags>(stream, proto->misc.flags) == -1) return -1;
        if (fileWriteUInt32Enum<ProtoExtendedFlags>(stream, proto->misc.extendedFlags) == -1) return -1;

        return 0;
    default:
        return -1;
    }
}

// 0x4A1B30 proto_save_pid
int _proto_save_pid(int pid)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return -1;
    }

    char path[260];
    proto_make_path(path, pid);
    strcat(path, "\\");

    _proto_list_str(pid, path + strlen(path));

    File* stream = fileOpen(path, "wb");
    if (stream == nullptr) {
        return -1;
    }

    int rc = protoWrite(proto, stream);

    fileClose(stream);

    return rc;
}

// 0x4A1C3C proto_load_pid
static int _proto_load_pid(int pid, Proto** protoPtr)
{
    char path[COMPAT_MAX_PATH];
    proto_make_path(path, pid);
    strcat(path, "\\");

    if (_proto_list_str(pid, path + strlen(path)) == -1) {
        return -1;
    }

    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        debugPrint("\nError: Can't fopen proto!\n");
        *protoPtr = nullptr;
        return -1;
    }

    if (_proto_find_free_subnode(objectTypeFromPid(pid), protoPtr) == -1) {
        fileClose(stream);
        return -1;
    }

    if (protoRead(*protoPtr, stream) != 0) {
        fileClose(stream);
        return -1;
    }

    fileClose(stream);
    return 0;
}

// 0x4A1D98 proto_find_free_subnode
static int _proto_find_free_subnode(ObjectType type, Proto** protoPtr)
{
    Proto* proto = (Proto*)internal_malloc(proto_size(type));
    *protoPtr = proto;
    if (proto == nullptr) {
        return -1;
    }

    ProtoList* protoList = &(_protoLists[type]);
    ProtoListExtent* protoListExtent = protoList->tail;

    if (protoList->head != nullptr) {
        if (protoListExtent->length == PROTO_LIST_EXTENT_SIZE) {
            ProtoListExtent* newExtent = protoListExtent->next = (ProtoListExtent*)internal_malloc(sizeof(ProtoListExtent));
            if (protoListExtent == nullptr) {
                internal_free(proto);
                *protoPtr = nullptr;
                return -1;
            }

            newExtent->length = 0;
            newExtent->next = nullptr;

            protoList->tail = newExtent;
            protoList->length++;

            protoListExtent = newExtent;
        }
    } else {
        protoListExtent = (ProtoListExtent*)internal_malloc(sizeof(ProtoListExtent));
        if (protoListExtent == nullptr) {
            internal_free(proto);
            *protoPtr = nullptr;
            return -1;
        }

        protoListExtent->next = nullptr;
        protoListExtent->length = 0;

        protoList->length = 1;
        protoList->tail = protoListExtent;
        protoList->head = protoListExtent;
    }

    protoListExtent->proto[protoListExtent->length] = proto;
    protoListExtent->length++;

    return 0;
}

// 0x4A1E90 proto_new
int proto_new(int* pid, ObjectType type)
{
    Proto* proto;

    if (_proto_find_free_subnode(type, &proto) == -1) {
        return -1;
    }

    *pid = _proto_new_id(type) | (type << 24);
    switch (type) {
    case OBJ_TYPE_ITEM:
        proto_item_init(proto, *pid);
        proto->item.pid = *pid;
        break;
    case OBJ_TYPE_CRITTER:
        proto_critter_init(proto, *pid);
        proto->critter.pid = *pid;
        break;
    case OBJ_TYPE_SCENERY:
        proto_scenery_init(proto, *pid);
        proto->scenery.pid = *pid;
        break;
    case OBJ_TYPE_WALL:
        proto_wall_init(proto, *pid);
        proto->wall.pid = *pid;
        break;
    case OBJ_TYPE_TILE:
        proto_tile_init(proto, *pid);
        proto->tile.pid = *pid;
        break;
    case OBJ_TYPE_MISC:
        proto_misc_init(proto, *pid);
        proto->misc.pid = *pid;
        break;
    default:
        return -1;
    }

    return 0;
}

// Evict top most proto cache block.
//
// 0x4A2040 proto_remove_some_list
static void _proto_remove_some_list(ObjectType type)
{
    ProtoList* protoList = &(_protoLists[type]);
    ProtoListExtent* protoListExtent = protoList->head;
    if (protoListExtent != nullptr) {
        protoList->length--;
        protoList->head = protoListExtent->next;

        for (int index = 0; index < protoListExtent->length; index++) {
            internal_free(protoListExtent->proto[index]);
        }

        internal_free(protoListExtent);
    }
}

// Clear proto cache of given type.
//
// 0x4A2094 proto_remove_list
static void _proto_remove_list(ObjectType type)
{
    ProtoList* protoList = &(_protoLists[type]);

    ProtoListExtent* curr = protoList->head;
    while (curr != nullptr) {
        ProtoListExtent* next = curr->next;
        for (int index = 0; index < curr->length; index++) {
            internal_free(curr->proto[index]);
        }
        internal_free(curr);
        curr = next;
    }

    protoList->head = nullptr;
    protoList->tail = nullptr;
    protoList->length = 0;
}

// Clear all proto cache.
//
// 0x4A20F4 proto_remove_all
void _proto_remove_all()
{
    for (ObjectType index = OBJ_TYPE_FIRST; index < OBJ_TYPE_PROTO_COUNT; index++) {
        _proto_remove_list(index);
    }
}

// proto_ptr
// 0x4A2108 proto_ptr
int protoGetProto(int pid, Proto** protoPtr)
{
    *protoPtr = nullptr;

    if (pid == -1) {
        return -1;
    }

    if (pid == 0x1000000) {
        *protoPtr = (Proto*)&gDudeProto;
        return 0;
    }

    ProtoList* protoList = &(_protoLists[objectTypeFromPid(pid)]);
    ProtoListExtent* protoListExtent = protoList->head;
    while (protoListExtent != nullptr) {
        for (int index = 0; index < protoListExtent->length; index++) {
            Proto* proto = (Proto*)protoListExtent->proto[index];
            if (pid == proto->pid) {
                *protoPtr = proto;
                return 0;
            }
        }
        protoListExtent = protoListExtent->next;
    }

    if (protoList->head != nullptr && protoList->tail != nullptr) {
        if (PROTO_LIST_EXTENT_SIZE * protoList->length - (PROTO_LIST_EXTENT_SIZE - protoList->tail->length) > PROTO_LIST_MAX_ENTRIES) {
            _proto_remove_some_list(objectTypeFromPid(pid));
        }
    }

    return _proto_load_pid(pid, protoPtr);
}

// 0x4A21DC proto_new_id
static int _proto_new_id(ObjectType type)
{
    int result = _protoLists[type].max_entries_num;
    _protoLists[type].max_entries_num = result + 1;

    return result;
}

// 0x4A2214 proto_max_id
int proto_max_id(ObjectType type)
{
    return _protoLists[type].max_entries_num;
}

// 0x4A22C0 ResetPlayer
int _ResetPlayer()
{
    Proto* proto;
    protoGetProto(gDude->pid, &proto);

    pcStatsReset();
    protoCritterDataResetStats(&(proto->critter.data));

    // SFALL: Fix base EMP DR not being properly initialized.
    proto->critter.data.baseStats[STAT_DAMAGE_RESISTANCE_EMP] = 100;

    critterReset();
    characterEditorReset();
    protoCritterDataResetSkills(&(proto->critter.data));
    skillsReset();
    perksReset();
    traitsReset();
    critterUpdateDerivedStats(gDude);
    return 0;
}

} // namespace fallout
