#include "sfall_opcodes.h"

#include <math.h>
#include <string.h>

#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "critter.h"
#include "dbox.h"
#include "debug.h"
#include "game.h"
#include "input.h"
#include "interface.h"
#include "interpreter.h"
#include "inventory.h"
#include "item.h"
#include "light.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "proto.h"
#include "scripts.h"
#include "sfall_arrays.h"
#include "sfall_global_scripts.h"
#include "sfall_global_vars.h"
#include "sfall_ini.h"
#include "sfall_kb_helpers.h"
#include "sfall_lists.h"
#include "sfall_metarules.h"
#include "sfall_script_hooks.h"
#include "stat.h"
#include "svga.h"
#include "tile.h"
#include "worldmap.h"

namespace fallout {

typedef enum ExplosionMetarule {
    EXPL_FORCE_EXPLOSION_PATTERN = 1,
    EXPL_FORCE_EXPLOSION_ART = 2,
    EXPL_FORCE_EXPLOSION_RADIUS = 3,
    EXPL_FORCE_EXPLOSION_DMGTYPE = 4,
    EXPL_STATIC_EXPLOSION_RADIUS = 5,
    EXPL_GET_EXPLOSION_DAMAGE = 6,
    EXPL_SET_DYNAMITE_EXPLOSION_DAMAGE = 7,
    EXPL_SET_PLASTIC_EXPLOSION_DAMAGE = 8,
    EXPL_SET_EXPLOSION_MAX_TARGET = 9,
} ExplosionMetarule;

static constexpr int kVersionMajor = 4;
static constexpr int kVersionMinor = 3;
static constexpr int kVersionPatch = 4;
static constexpr int kSfallPathBufferSize = 3200; // matches rotation path size in animation.cc

// read_byte
static void op_read_byte(Program* program)
{
    int addr = programStackPopInteger(program);

    int value = 0;
    switch (addr) {
    case 0x56D38C:
        value = combatGetTargetHighlight();
        break;
    default:
        debugPrint("%s: attempt to 'read_byte' at 0x%x", program->name, addr);
        break;
    }

    programStackPushInteger(program, value);
}

// set_pc_base_stat
static void op_set_pc_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBaseStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    critterSetBaseStat(gDude, stat, value);
}

static void op_set_critter_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBaseStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    critterSetBaseStat(obj, stat, value);
}

// set_pc_extra_stat
static void op_set_pc_bonus_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBonusStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    critterSetBonusStat(gDude, stat, value);
}

static void op_set_critter_extra_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBonusStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    critterSetBonusStat(obj, stat, value);
}

// get_pc_base_stat
static void op_get_pc_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall obtains value directly from
    // dude's proto. This can have unforeseen consequences when dealing with
    // current stats.
    int stat = programStackPopInteger(program);
    programStackPushInteger(program, critterGetBaseStat(gDude, stat));
}

static void op_get_critter_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall obtains value directly from
    // dude's proto. This can have unforeseen consequences when dealing with
    // current stats.
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    programStackPushInteger(program, critterGetBaseStat(obj, stat));
}

// get_pc_extra_stat
static void op_get_pc_bonus_stat(Program* program)
{
    int stat = programStackPopInteger(program);
    int value = critterGetBonusStat(gDude, stat);
    programStackPushInteger(program, value);
}

static void op_get_critter_extra_stat(Program* program)
{
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    int value = critterGetBonusStat(obj, stat);
    programStackPushInteger(program, value);
}

// tap_key
static void op_tap_key(Program* program)
{
    int key = programStackPopInteger(program);
    sfall_kb_press_key(key);
}

// get_year
static void op_get_year(Program* program)
{
    int year;
    gameTimeGetDate(nullptr, nullptr, &year);
    programStackPushInteger(program, year);
}

// game_loaded
static void op_game_loaded(Program* program)
{
    bool loaded = sfall_gl_scr_is_loaded(program);
    programStackPushInteger(program, loaded ? 1 : 0);
}

// set_global_script_repeat
static void op_set_global_script_repeat(Program* program)
{
    int frames = programStackPopInteger(program);
    sfall_gl_scr_set_repeat(program, frames);
}

// key_pressed
static void op_key_pressed(Program* program)
{
    int key = programStackPopInteger(program);
    bool pressed = sfall_kb_is_key_pressed(key);
    programStackPushInteger(program, pressed ? 1 : 0);
}

// in_world_map
static void op_in_world_map(Program* program)
{
    programStackPushInteger(program, GameMode::isInGameMode(GameMode::kWorldmap) ? 1 : 0);
}

// force_encounter
static void op_force_encounter(Program* program)
{
    int map = programStackPopInteger(program);
    wmForceEncounter(map, 0);
}

// set_world_map_pos
static void op_set_world_map_pos(Program* program)
{
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    wmSetPartyWorldPos(x, y);
}

// get_world_map_x_pos
static void op_get_world_map_x_pos(Program* program)
{
    int x;
    wmGetPartyWorldPos(&x, nullptr);
    programStackPushInteger(program, x);
}

// get_world_map_y_pos
static void op_get_world_map_y_pos(Program* program)
{
    int y;
    wmGetPartyWorldPos(nullptr, &y);
    programStackPushInteger(program, y);
}

// set_map_time_multi
void op_set_map_time_multi(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    wmSetScriptWorldMapMulti(value.asFloat());
}

// active_hand
static void op_active_hand(Program* program)
{
    programStackPushInteger(program, interfaceGetCurrentHand());
}

// toggle_active_hand
static void op_toggle_active_hand(Program* program)
{
    interfaceBarSwapHands(true);
}

// set_global_script_type
static void op_set_global_script_type(Program* program)
{
    int type = programStackPopInteger(program);
    sfall_gl_scr_set_type(program, type);
}

// set_sfall_global
static void op_set_sfall_global(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    ProgramValue variable = programStackPopValue(program);

    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        sfall_gl_vars_store(key, value.integerValue);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        sfall_gl_vars_store(variable.integerValue, value.integerValue);
    }
}

// get_sfall_global_int
static void op_get_sfall_global_int(Program* program)
{
    ProgramValue variable = programStackPopValue(program);

    int value = 0;
    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        sfall_gl_vars_fetch(key, value);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        sfall_gl_vars_fetch(variable.integerValue, value);
    }

    programStackPushInteger(program, value);
}

// get_game_mode
static void op_get_game_mode(Program* program)
{
    programStackPushInteger(program, GameMode::getCurrentGameMode());
}

// get_uptime
static void op_get_uptime(Program* program)
{
    programStackPushInteger(program, getTicks());
}

// set_car_current_town
static void op_set_car_current_town(Program* program)
{
    int area = programStackPopInteger(program);
    wmCarSetCurrentArea(area);
}

// get_bodypart_hit_modifier
static void op_get_bodypart_hit_modifier(Program* program)
{
    int hit_location = programStackPopInteger(program);
    programStackPushInteger(program, combat_get_hit_location_penalty(hit_location));
}

// set_bodypart_hit_modifier
static void op_set_bodypart_hit_modifier(Program* program)
{
    int penalty = programStackPopInteger(program);
    int hit_location = programStackPopInteger(program);
    combat_set_hit_location_penalty(hit_location, penalty);
}

// sqrt
static void op_sqrt(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, sqrtf(programValue.asFloat()));
}

// abs
static void op_abs(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);

    if (programValue.isInt()) {
        programStackPushInteger(program, abs(programValue.integerValue));
    } else {
        programStackPushFloat(program, abs(programValue.asFloat()));
    }
}

// sin
static void op_sin(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, sinf(programValue.asFloat()));
}

// cos
static void op_cos(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, cosf(programValue.asFloat()));
}

// tan
static void op_tan(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, tanf(programValue.asFloat()));
}

// arctan
static void op_arctan(Program* program)
{
    ProgramValue xValue = programStackPopValue(program);
    ProgramValue yValue = programStackPopValue(program);
    programStackPushFloat(program, atan2f(yValue.asFloat(), xValue.asFloat()));
}

// pow (^)
static void op_power(Program* program)
{
    ProgramValue expValue = programStackPopValue(program);
    ProgramValue baseValue = programStackPopValue(program);

    // CE: Implementation is slightly different, check.
    float result = powf(baseValue.asFloat(), expValue.asFloat());

    if (baseValue.isInt() && expValue.isInt()) {
        // Note: this will truncate the result if power is negative.  Keeping it to match sfall.
        programStackPushInteger(program, static_cast<int>(result));
    } else {
        programStackPushFloat(program, result);
    }
}

// log
static void op_log(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, logf(programValue.asFloat()));
}

// ceil
static void op_ceil(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushInteger(program, static_cast<int>(ceilf(programValue.asFloat())));
}

// exp
static void op_exponent(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, expf(programValue.asFloat()));
}

// get_script
static void op_get_script(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    programStackPushInteger(program, obj->scriptIndex + 1);
}

// get_proto_data
static void op_get_proto_data(Program* program)
{
    size_t offset = static_cast<size_t>(programStackPopInteger(program));
    int pid = programStackPopInteger(program);

    Proto* proto;
    if (protoGetProto(pid, &proto) != 0) {
        debugPrint("op_get_proto_data: bad proto %d", pid);
        programStackPushInteger(program, -1);
        return;
    }

    // CE: Make sure the requested offset is within memory bounds and is
    // properly aligned.
    if (offset + sizeof(int) > proto_size(PID_TYPE(pid)) || offset % sizeof(int) != 0) {
        debugPrint("op_get_proto_data: bad offset %d", offset);
        programStackPushInteger(program, -1);
        return;
    }

    int value = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(proto) + offset);
    programStackPushInteger(program, value);
}

// set_proto_data
static void op_set_proto_data(Program* program)
{
    int value = programStackPopInteger(program);
    size_t offset = static_cast<size_t>(programStackPopInteger(program));
    int pid = programStackPopInteger(program);

    Proto* proto;
    if (protoGetProto(pid, &proto) != 0) {
        debugPrint("op_set_proto_data: bad proto %d", pid);
        programStackPushInteger(program, -1);
        return;
    }

    // CE: Make sure the requested offset is within memory bounds and is
    // properly aligned.
    if (offset + sizeof(int) > proto_size(PID_TYPE(pid)) || offset % sizeof(int) != 0) {
        debugPrint("op_set_proto_data: bad offset %d", offset);
        programStackPushInteger(program, -1);
        return;
    }

    *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(proto) + offset) = value;
}

// set_self
static void op_set_self(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int sid = scriptGetSid(program);

    Script* scr;
    if (scriptGetScript(sid, &scr) == 0) {
        scr->overriddenSelf = obj;
    }
}

// list_begin
static void op_list_begin(Program* program)
{
    int listType = programStackPopInteger(program);
    int listId = sfallListsCreate(listType);
    programStackPushInteger(program, listId);
}

// list_next
static void op_list_next(Program* program)
{
    int listId = programStackPopInteger(program);
    Object* obj = sfallListsGetNext(listId);
    programStackPushPointer(program, obj);
}

// list_end
static void op_list_end(Program* program)
{
    int listId = programStackPopInteger(program);
    sfallListsDestroy(listId);
}

// sfall_ver_major
static void op_get_version_major(Program* program)
{
    programStackPushInteger(program, kVersionMajor);
}

// sfall_ver_minor
static void op_get_version_minor(Program* program)
{
    programStackPushInteger(program, kVersionMinor);
}

// sfall_ver_build
static void op_get_version_patch(Program* program)
{
    programStackPushInteger(program, kVersionPatch);
}

// get_weapon_ammo_pid
static void op_get_weapon_ammo_pid(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int pid = -1;
    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_WEAPON:
                pid = weaponGetAmmoTypePid(obj);
                break;
            case ITEM_TYPE_MISC:
                pid = miscItemGetPowerTypePid(obj);
                break;
            }
        }
    }

    programStackPushInteger(program, pid);
}

// There are two problems with this function.
//
// 1. Sfall's implementation changes ammo PID of misc items, which is impossible
// since it's stored in proto, not in the object.
// 2. Changing weapon's ammo PID is done without checking for ammo
// quantity/capacity which can probably lead to bad things.
//
// set_weapon_ammo_pid
static void op_set_weapon_ammo_pid(Program* program)
{
    int ammoTypePid = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_WEAPON:
                obj->data.item.weapon.ammoTypePid = ammoTypePid;
                break;
            }
        }
    }
}

// get_weapon_ammo_count
static void op_get_weapon_ammo_count(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    // CE: Implementation is different.
    int ammoQuantityOrCharges = 0;
    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_AMMO:
            case ITEM_TYPE_WEAPON:
                ammoQuantityOrCharges = ammoGetQuantity(obj);
                break;
            case ITEM_TYPE_MISC:
                ammoQuantityOrCharges = miscItemGetCharges(obj);
                break;
            }
        }
    }

    programStackPushInteger(program, ammoQuantityOrCharges);
}

// set_weapon_ammo_count
static void op_set_weapon_ammo_count(Program* program)
{
    int ammoQuantityOrCharges = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    // CE: Implementation is different.
    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_AMMO:
            case ITEM_TYPE_WEAPON:
                ammoSetQuantity(obj, ammoQuantityOrCharges);
                break;
            case ITEM_TYPE_MISC:
                miscItemSetCharges(obj, ammoQuantityOrCharges);
                break;
            }
        }
    }
}

// get_mouse_x
static void op_get_mouse_x(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, x);
}

// get_mouse_y
static void op_get_mouse_y(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, y);
}

// get_mouse_buttons
static void op_get_mouse_buttons(Program* program)
{
    // CE: Implementation is slightly different - it does not handle middle
    // mouse button.
    programStackPushInteger(program, mouse_get_last_buttons());
}

// get_screen_width
static void op_get_screen_width(Program* program)
{
    programStackPushInteger(program, screenGetWidth());
}

// get_screen_height
static void op_get_screen_height(Program* program)
{
    programStackPushInteger(program, screenGetHeight());
}

// note: might need to be updated when Hero Appearance is implemented
static void op_refresh_pc_art(Program* program)
{
    if (gDude == nullptr) {
        return;
    }

    Rect rect;
    objectGetRect(gDude, &rect);

    int anim = FID_ANIM_TYPE(gDude->fid);
    int rotation = FID_ROTATION(gDude->fid);

    _proto_dude_update_gender();

    int fid = inventoryComputeCritterFid(gDude,
        gDude->pid,
        critterGetItem2(gDude),
        critterGetItem1(gDude),
        critterGetArmor(gDude),
        interfaceGetCurrentHand(),
        anim,
        rotation);

    // CE: When changing gender, the refreshed rect can be smaller than the original one,
    // which can leave a momentary ghost.  We union with old rect to avoid that.
    Rect newRect;
    objectSetFid(gDude, fid, nullptr);
    objectGetRect(gDude, &newRect);
    rectUnion(&rect, &newRect, &rect);
    tileWindowRefreshRect(&rect, gDude->elevation);
}

// create_message_window
static void op_create_message_window(Program* program)
{
    static bool showing = false;

    if (showing) {
        return;
    }

    const char* string = programStackPopString(program);
    if (string == nullptr || string[0] == '\0') {
        return;
    }

    char* copy = internal_strdup(string);

    const char* body[4];
    int count = 0;

    char* pch = strchr(copy, '\n');
    while (pch != nullptr && count < 4) {
        *pch = '\0';
        body[count++] = pch + 1;
        pch = strchr(pch + 1, '\n');
    }

    showing = true;
    showDialogBox(copy,
        body,
        count,
        192,
        116,
        _colorTable[32328],
        nullptr,
        _colorTable[32328],
        DIALOG_BOX_LARGE);
    showing = false;

    internal_free(copy);
}

// get_attack_type
static void op_get_attack_type(Program* program)
{
    int hit_mode;
    if (interface_get_current_attack_mode(&hit_mode)) {
        programStackPushInteger(program, hit_mode);
    } else {
        programStackPushInteger(program, -1);
    }
}

// force_encounter_with_flags
static void op_force_encounter_with_flags(Program* program)
{
    unsigned int flags = programStackPopInteger(program);
    int map = programStackPopInteger(program);
    wmForceEncounter(map, flags);
}

// list_as_array
static void op_list_as_array(Program* program)
{
    int type = programStackPopInteger(program);
    int arrayId = ListAsArray(type);
    programStackPushInteger(program, arrayId);
}

// atoi
static void op_parse_int(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strtol(string, nullptr, 0)));
}

// atof
static void op_atof(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushFloat(program, static_cast<float>(atof(string)));
}

// tile_under_cursor
static void op_tile_under_cursor(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);

    int tile = tileFromScreenXY(x, y);
    programStackPushInteger(program, tile);
}

// get_tile_fid
static void op_get_tile_fid(Program* program)
{
    int tileData = programStackPopInteger(program);
    int tile = tileData & 0xFFFFFF;
    int elevation = (tileData >> 24) & 0x0F;
    int mode = tileData >> 28;

    if (!hexGridTileIsValid(tile) || elevation < 0 || elevation >= ELEVATION_COUNT) {
        debugPrint("%s: op_get_tile_fid invalid tile data: tile=%d elevation=%d", program->name, tile, elevation);
        programStackPushInteger(program, 0);
        return;
    }

    int squareTile = squareTileFromTile(tile);
    if (!squareGridTileIsValid(squareTile)) {
        debugPrint("%s: op_get_tile_fid failed to map tile=%d to square index", program->name, tile);
        programStackPushInteger(program, 0);
        return;
    }

    int squareData = _square[elevation]->field_0[squareTile];

    switch (mode) {
    case 1:
        programStackPushInteger(program, (squareData >> 16) & 0x3FFF);
        break;
    case 2:
        programStackPushInteger(program, squareData);
        break;
    default:
        programStackPushInteger(program, squareData & 0x3FFF);
        break;
    }
}

// substr
static void op_substr(Program* program)
{
    auto length = programStackPopInteger(program);
    auto startPos = programStackPopInteger(program);
    const char* str = programStackPopString(program);

    char buf[5120] = { 0 };

    int len = static_cast<int>(strlen(str));

    if (startPos < 0) {
        startPos += len; // start from end
        if (startPos < 0) {
            startPos = 0;
        }
    }

    if (length < 0) {
        length += len - startPos; // cutoff at end
        if (length == 0) {
            programStackPushString(program, buf);
            return;
        }
        length = abs(length); // length can't be negative
    }

    // check position
    if (startPos >= len) {
        // start position is out of string length, return empty string
        programStackPushString(program, buf);
        return;
    }

    if (length == 0 || length + startPos > len) {
        length = len - startPos; // set the correct length, the length of characters goes beyond the end of the string
    }

    if (length > sizeof(buf) - 1) {
        length = sizeof(buf) - 1;
    }

    memcpy(buf, &str[startPos], length);
    buf[length] = '\0';
    programStackPushString(program, buf);
}

// strlen
static void op_get_string_length(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strlen(string)));
}

// metarule2_explosions
static void op_explosions_metarule(Program* program)
{
    int param2 = programStackPopInteger(program);
    int param1 = programStackPopInteger(program);
    int metarule = programStackPopInteger(program);

    switch (metarule) {
    case EXPL_FORCE_EXPLOSION_PATTERN:
        if (param1 != 0) {
            explosionSetPattern(2, 4);
        } else {
            explosionSetPattern(0, 6);
        }
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_ART:
        explosionSetFrm(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_RADIUS:
        explosionSetRadius(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_DMGTYPE:
        explosionSetDamageType(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_STATIC_EXPLOSION_RADIUS:
        weaponSetGrenadeExplosionRadius(param1);
        weaponSetRocketExplosionRadius(param2);
        programStackPushInteger(program, 0);
        break;
    case EXPL_GET_EXPLOSION_DAMAGE:
        if (1) {
            int minDamage;
            int maxDamage;
            explosiveGetDamage(param1, &minDamage, &maxDamage);

            ArrayId arrayId = CreateTempArray(2, 0);
            SetArray(arrayId, ProgramValue { 0 }, ProgramValue { minDamage }, false, program);
            SetArray(arrayId, ProgramValue { 1 }, ProgramValue { maxDamage }, false, program);

            programStackPushInteger(program, arrayId);
        }
        break;
    case EXPL_SET_DYNAMITE_EXPLOSION_DAMAGE:
        explosiveSetDamage(PROTO_ID_DYNAMITE_I, param1, param2);
        break;
    case EXPL_SET_PLASTIC_EXPLOSION_DAMAGE:
        explosiveSetDamage(PROTO_ID_PLASTIC_EXPLOSIVES_I, param1, param2);
        break;
    case EXPL_SET_EXPLOSION_MAX_TARGET:
        explosionSetMaxTargets(param1);
        break;
    }
}

// message_str_game
static void op_get_message(Program* program)
{
    int messageId = programStackPopInteger(program);
    int messageListId = programStackPopInteger(program);
    char* text = messageListRepositoryGetMsg(messageListId, messageId);
    programStackPushString(program, text);
}

// array_key
static void op_get_array_key(Program* program)
{
    auto index = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    auto value = GetArrayKey(arrayId, index, program);
    programStackPushValue(program, value);
}

// create_array
static void op_create_array(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);
    auto arrayId = CreateArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// temp_array
static void op_temp_array(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);
    auto arrayId = CreateTempArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// fix_array
static void op_fix_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FixArray(arrayId);
}

// string_split
static void op_string_split(Program* program)
{
    auto split = programStackPopString(program);
    auto str = programStackPopString(program);
    auto arrayId = StringSplit(str, split);
    programStackPushInteger(program, arrayId);
}

// set_array
static void op_set_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    SetArray(arrayId, key, value, true, program);
}

// arrayexpr
static void op_stack_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    auto returnValue = StackArray(key, value, program);
    programStackPushInteger(program, returnValue);
}

// scan_array
static void op_scan_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    auto returnValue = ScanArray(arrayId, value, program);
    programStackPushValue(program, returnValue);
}

// get_array
static void op_get_array(Program* program)
{
    auto key = programStackPopValue(program);
    auto arrayId = programStackPopValue(program);

    if (arrayId.isInt()) {
        auto value = GetArray(arrayId.integerValue, key, program);
        programStackPushValue(program, value);
    } else if (arrayId.isString() && key.isInt()) {
        auto pos = key.asInt();
        auto str = programGetString(program, arrayId.opcode, arrayId.integerValue);

        char buf[2] = { 0 };
        if (pos < strlen(str)) {
            buf[0] = str[pos];
            programStackPushString(program, buf);
        } else {
            programStackPushString(program, buf);
        }
    }
}

// free_array
static void op_free_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FreeArray(arrayId);
}

// len_array
static void op_len_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    programStackPushInteger(program, LenArray(arrayId));
}

// resize_array
static void op_resize_array(Program* program)
{
    auto newLen = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    ResizeArray(arrayId, newLen);
}

// party_member_list
static void op_party_member_list(Program* program)
{
    auto includeHidden = programStackPopInteger(program);
    auto objects = get_all_party_members_objects(includeHidden);
    auto arrayId = CreateTempArray(static_cast<int>(objects.size()), SFALL_ARRAYFLAG_RESERVED);
    for (int i = 0; i < LenArray(arrayId); i++) {
        SetArray(arrayId, ProgramValue { i }, ProgramValue { objects[i] }, false, program);
    }
    programStackPushInteger(program, arrayId);
}

// typeof
static void op_type_of(Program* program)
{
    auto value = programStackPopValue(program);
    if (value.isInt()) {
        programStackPushInteger(program, 1);
    } else if (value.isFloat()) {
        programStackPushInteger(program, 2);
    } else {
        programStackPushInteger(program, 3);
    };
}

// round
static void op_round(Program* program)
{
    float floatValue = programStackPopValue(program).asFloat();
    programStackPushInteger(program, static_cast<int>(lroundf(floatValue)));
}

enum BlockType {
    BLOCKING_TYPE_BLOCK,
    BLOCKING_TYPE_SHOOT,
    BLOCKING_TYPE_AI,
    BLOCKING_TYPE_SIGHT,
    BLOCKING_TYPE_SCROLL,
};

PathBuilderCallback* get_blocking_func(int type)
{
    switch (type) {
    case BLOCKING_TYPE_SHOOT:
        return _obj_shoot_blocking_at;
    case BLOCKING_TYPE_AI:
        return _obj_ai_blocking_at;
    case BLOCKING_TYPE_SIGHT:
        return _obj_sight_blocking_at;
    default:
        return _obj_blocking_at;
    }
}

// obj_blocking_line
static void op_make_straight_path(Program* program)
{
    int type = programStackPopInteger(program);
    int dest = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int flags = type == BLOCKING_TYPE_SHOOT ? 32 : 0;

    Object* obstacle = nullptr;
    _make_straight_path_func(object, object->tile, dest, nullptr, &obstacle, flags, get_blocking_func(type));
    programStackPushPointer(program, obstacle);
}

// obj_blocking_tile
static void op_obj_blocking_at(Program* program)
{
    int type = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);

    PathBuilderCallback* func = get_blocking_func(type);
    Object* obstacle = func(nullptr, tile, elevation);
    if (obstacle != nullptr) {
        if (type == BLOCKING_TYPE_SHOOT) {
            if ((obstacle->flags & OBJECT_SHOOT_THRU) != 0) {
                obstacle = nullptr;
            }
        }
    }
    programStackPushPointer(program, obstacle);
}

// tile_light
static void op_tile_light(Program* program)
{
    int tile = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    programStackPushInteger(program, lightGetTileIntensity(elevation, tile));
}

// tile_get_objs
static void op_tile_get_objects(Program* program)
{
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    ArrayId arrayId = CreateTempArray(0, SFALL_ARRAYFLAG_RESERVED);

    if (!hexGridTileIsValid(tile) || elevation < 0 || elevation >= ELEVATION_COUNT) {
        debugPrint("%s: op_tile_get_objects invalid tile data: tile=%d elevation=%d", program->name, tile, elevation);
        programStackPushInteger(program, arrayId);
        return;
    }

    int index = 0;
    for (Object* object = objectFindFirstAtLocation(elevation, tile); object != nullptr; object = objectFindNextAtLocation()) {
        ResizeArray(arrayId, index + 1);
        SetArray(arrayId, ProgramValue(index++), ProgramValue(object), false, program);
    }

    programStackPushInteger(program, arrayId);
}

// path_find_to
static void op_make_path(Program* program)
{
    int type = programStackPopInteger(program);
    int dest = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    ArrayId arrayId = CreateTempArray(0, 0);

    if (object == nullptr
        || !hexGridTileIsValid(dest)
        || object->elevation < 0
        || object->elevation >= ELEVATION_COUNT
        || !hexGridTileIsValid(object->tile)) {
        debugPrint("%s: op_make_path invalid input: object=%p dest=%d elevation=%d", program->name, object, dest, object != nullptr ? object->elevation : -1);
        programStackPushInteger(program, arrayId);
        return;
    }

    // sfall only requires an empty destination tile when the source object is a critter.
    int requireEmptyDest = PID_TYPE(object->pid) == OBJ_TYPE_CRITTER;

    // XXX: pathfinderFindPath does not accept a destination buffer length. Use the
    // same capacity as the engine's AnimationSad::rotations storage so this
    // wrapper is not the limiting factor.  Sfall uses 800 here
    unsigned char rotations[kSfallPathBufferSize];
    int pathLength = pathfinderFindPath(object, object->tile, dest, rotations, requireEmptyDest, get_blocking_func(type));
    ResizeArray(arrayId, pathLength);
    for (int index = 0; index < pathLength; index++) {
        SetArray(arrayId, ProgramValue(index), ProgramValue(static_cast<int>(rotations[index])), false, program);
    }

    programStackPushInteger(program, arrayId);
}

// art_exists
static void op_art_exists(Program* program)
{
    int fid = programStackPopInteger(program);
    programStackPushInteger(program, artExists(fid));
}

// sfall_func0
static void op_sfall_func0(Program* program)
{
    sfall_metarule(program, 0);
}

// sfall_func1
static void op_sfall_func1(Program* program)
{
    sfall_metarule(program, 1);
}

// sfall_func2
static void op_sfall_func2(Program* program)
{
    sfall_metarule(program, 2);
}

// sfall_func3
static void op_sfall_func3(Program* program)
{
    sfall_metarule(program, 3);
}

// sfall_func4
static void op_sfall_func4(Program* program)
{
    sfall_metarule(program, 4);
}

// sfall_func5
static void op_sfall_func5(Program* program)
{
    sfall_metarule(program, 5);
}

// sfall_func6
static void op_sfall_func6(Program* program)
{
    sfall_metarule(program, 6);
}

// sfall_func7
static void op_sfall_func7(Program* program)
{
    sfall_metarule(program, 7);
}

// sfall_func8
static void op_sfall_func8(Program* program)
{
    sfall_metarule(program, 8);
}

// div (/)
static void op_div(Program* program)
{
    ProgramValue divisorValue = programStackPopValue(program);
    ProgramValue dividendValue = programStackPopValue(program);

    if (divisorValue.integerValue == 0) {
        debugPrint("Division by zero");

        // TODO: Looks like execution is not halted in Sfall's div, check.
        programStackPushInteger(program, 0);
        return;
    }

    if (dividendValue.isFloat() || divisorValue.isFloat()) {
        programStackPushFloat(program, dividendValue.asFloat() / divisorValue.asFloat());
    } else {
        // Unsigned divison.
        programStackPushInteger(program, static_cast<unsigned int>(dividendValue.integerValue) / static_cast<unsigned int>(divisorValue.integerValue));
    }
}

static void op_sprintf(Program* program)
{
    auto arg1 = programStackPopValue(program);
    auto arg2 = programStackPopString(program);
    programStackPushValue(program, arg1);
    programStackPushString(program, arg2);
    sprintf_lite(program, 2, "op_sprintf");
}

static void op_charcode(Program* program)
{
    const char* str = programStackPopString(program);
    if (str != nullptr && str[0] != '\0') {
        programStackPushInteger(program, static_cast<int>(str[0]));
    } else {
        programStackPushInteger(program, 0);
    }
}

static void op_show_iface_tag(fallout::Program* program)
{
    int tag = fallout::programStackPopInteger(program);

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        dudeEnableState(tag);
        break;
    default:
        debugPrint("op_show_iface_tag: custom tag %d is not handled", tag);
    }
}

static void op_hide_iface_tag(fallout::Program* program)
{
    int tag = fallout::programStackPopInteger(program);

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        dudeDisableState(tag);
        break;
    default:
        debugPrint("op_hide_iface_tag: custom tag %d is not handled", tag);
    }
}

static void op_is_iface_tag_active(fallout::Program* program)
{
    int tag = fallout::programStackPopInteger(program);
    bool isActive = false;

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        isActive = fallout::dudeHasState(tag);
        break;
    case 1: // POISONED
        isActive = critterGetPoison(gDude) > POISON_INDICATOR_THRESHOLD;
        break;
    case 2: // RADIATED
        isActive = critterGetRadiation(gDude) > RADATION_INDICATOR_THRESHOLD;
        break;
    default:
        debugPrint("op_is_iface_tag_active: custom tag %d is not handled", tag);
    }

    fallout::programStackPushInteger(program, isActive ? 1 : 0);
}

// TODO: move opcodes into several files
// TODO: reduce code duplication by introducing something like OpcodeContext in sfall

static void op_register_hook(Program* program)
{
    constexpr char opcodeName[] = "register_hook";

    int hookId = programStackPopInteger(program);
    if (hookId < 0 || hookId >= HOOK_COUNT) {
        programPrintError("%s: invalid hook ID: %d", opcodeName, hookId);
        return;
    }
    int startProcIndex = programFindProcedure(program, gScriptProcNames[SCRIPT_PROC_START]);
    if (startProcIndex == -1) {
        programPrintError("%s: 'start' procedure not found", opcodeName);
        return;
    }
    if (!scriptHooksRegister(program, static_cast<HookType>(hookId), startProcIndex)) {
        programPrintError("%s(%d, %d): failed", opcodeName, hookId, startProcIndex);
    }
}

static void op_register_hook_proc(Program* program)
{
    constexpr char opcodeName[] = "register_hook_proc";

    int procedureIndex = programStackPopInteger(program);
    int hookId = programStackPopInteger(program);
    if (hookId < 0 || hookId >= HOOK_COUNT) {
        programPrintError("%s: invalid hook ID: %d", opcodeName, hookId);
        return;
    }
    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) {
        programPrintError("%s: procedure index %d is out of range [0; %d]", opcodeName, procedureIndex, program->procedureCount());
        return;
    }

    // Note: in sfall, register_hook_proc by default adds the next hook to the beginning of the hook order.
    // Meaning the last script to be registered will be executed first.
    // There was a special opcode `register_hook_proc_spec` that adds to the end of hook order instead.
    // In CE we assume that this order shouldn't matter, and giving script a choice like that doesn't solve anything, since several scripts from different mods can use either opcode.

    // Global script order is entirely based off script file name sorting and when user installs scripts from different mods, there's no way to ensure a "proper" order,
    // without some kind of script-dependency system, which we don't have.
    // So let's just simply use the direct order.
    if (!scriptHooksRegister(program, static_cast<HookType>(hookId), procedureIndex)) {
        programPrintError("%s(%d, %d): failed", opcodeName, hookId, procedureIndex);
    }
}

ScriptHookCall* hookOpcodeGetCurrentCall(const char* opcodeName)
{
    const auto hookCall = ScriptHookCall::current();
    if (hookCall == nullptr) {
        programPrintError("%s: called outside of a script hook", opcodeName);
    }
    return hookCall;
}

static void op_get_sfall_arg(Program* program)
{
    constexpr char opcodeName[] = "get_sfall_arg";

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    programStackPushValue(program, hookCall != nullptr ? hookCall->getNextArgFromScript() : ProgramValue(0));
}

static void op_get_sfall_args(Program* program)
{
    constexpr char opcodeName[] = "get_sfall_args";

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    ArrayId result = 0;
    if (hookCall != nullptr) {
        result = CreateTempArray(hookCall->numArgs(), 0);
        for (int i = 0; i < hookCall->numArgs(); ++i) {
            SetArray(result, i, hookCall->getArgAt(i), false, program);
        }
    }
    programStackPushInteger(program, static_cast<int>(result));
}

static void op_set_sfall_arg(Program* program)
{
    constexpr char opcodeName[] = "set_sfall_arg";

    const ProgramValue value = programStackPopValue(program);
    const int argNum = programStackPopInteger(program);

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    if (hookCall == nullptr) return;

    if (argNum < 0 || argNum >= hookCall->numArgs()) {
        programPrintError("%s: argNum %d out of range [0, %d]", opcodeName, argNum, hookCall->numArgs() - 1);
        return;
    }
    hookCall->setArgAt(argNum, value);
}

static void op_set_sfall_return(Program* program)
{
    constexpr char opcodeName[] = "set_sfall_return";

    const ProgramValue value = programStackPopValue(program);

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    if (hookCall == nullptr) return;

    if (hookCall->numScriptReturnValues() >= hookCall->maxReturnValues()) {
        programPrintError("%s: trying to add next return value while only %d is expected", opcodeName, hookCall->maxReturnValues());
        return;
    }
    hookCall->addReturnValueFromScript(value);
}

// Note: opcodes should pop arguments off the stack in reverse order
void sfallOpcodesInit()
{
    // ref. https://github.com/sfall-team/sfall/blob/71ecec3d405bd5e945f157954618b169e60068fe/artifacts/scripting/sfall%20opcode%20list.txt#L145
    // Note: we can't really implement these since address space is different.
    // We can potentially special case some of them, but we should try to avoid that.
    // 0x815
    interpreterRegisterOpcode(0x8156, op_read_byte);
    // 0x815
    // 0x815
    // 0x815

    // ^ 0x81cf - void  write_byte(int address, int value)
    // ^ 0x81d0 - void  write_short(int address, int value)
    // ^ 0x81d1 - void  write_int(int address, int value)
    // ^ 0x821b - void  write_string(int address, string value)

    // ^ 0x81d2 - void  call_offset_v0(int address)
    // ^ 0x81d3 - void  call_offset_v1(int address, int arg1)
    // ^ 0x81d4 - void  call_offset_v2(int address, int arg1, int arg2)
    // ^ 0x81d5 - void  call_offset_v3(int address, int arg1, int arg2, int arg3)
    // ^ 0x81d6 - void  call_offset_v4(int address, int arg1, int arg2, int arg3, int arg4)
    // ^ 0x81d7 - int   call_offset_r0(int address)
    // ^ 0x81d8 - int   call_offset_r1(int address, int arg1)
    // ^ 0x81d9 - int   call_offset_r2(int address, int arg1, int arg2)
    // ^ 0x81da - int   call_offset_r3(int address, int arg1, int arg2, int arg3)
    // ^ 0x81db - int   call_offset_r4(int address, int arg1, int arg2, int arg3, int arg4)

    // 0x815
    interpreterRegisterOpcode(0x815A, op_set_pc_base_stat);
    // 0x815
    interpreterRegisterOpcode(0x815B, op_set_pc_bonus_stat);
    // 0x815
    interpreterRegisterOpcode(0x815C, op_get_pc_base_stat);
    // 0x815
    interpreterRegisterOpcode(0x815D, op_get_pc_bonus_stat);

    // 0x815
    interpreterRegisterOpcode(0x815E, op_set_critter_base_stat);
    // 0x815
    interpreterRegisterOpcode(0x815F, op_set_critter_extra_stat);
    // 0x816
    interpreterRegisterOpcode(0x8160, op_get_critter_base_stat);
    // 0x816
    interpreterRegisterOpcode(0x8161, op_get_critter_extra_stat);
    // 0x824
    // 0x824
    // 0x824
    // 0x824
    // 0x824

    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81

    // 0x816
    // 0x816
    interpreterRegisterOpcode(0x816C, op_key_pressed);
    // 0x816
    interpreterRegisterOpcode(0x8162, op_tap_key);
    // 0x821
    interpreterRegisterOpcode(0x821C, op_get_mouse_x);
    // 0x821
    interpreterRegisterOpcode(0x821D, op_get_mouse_y);
    // 0x821
    interpreterRegisterOpcode(0x821E, op_get_mouse_buttons);
    // 0x821

    // 0x816
    interpreterRegisterOpcode(0x8163, op_get_year);

    // 0x816
    interpreterRegisterOpcode(0x8164, op_game_loaded);

    // 0x816
    // 0x816
    // 0x816
    // 0x816
    // 0x816
    // 0x816
    // 0x816
    // 0x816
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81

    // 0x816
    interpreterRegisterOpcode(0x816A, op_set_global_script_repeat);
    // 0x819
    interpreterRegisterOpcode(0x819B, op_set_global_script_type);
    // 0x819

    // 0x817
    interpreterRegisterOpcode(0x8170, op_in_world_map);

    // 0x817
    interpreterRegisterOpcode(0x8171, op_force_encounter);
    // 0x822
    interpreterRegisterOpcode(0x8229, op_force_encounter_with_flags);
    // 0x822
    interpreterRegisterOpcode(0x822A, op_set_map_time_multi);

    // 0x817
    interpreterRegisterOpcode(0x8172, op_set_world_map_pos);
    // 0x817
    interpreterRegisterOpcode(0x8173, op_get_world_map_x_pos);
    // 0x817
    interpreterRegisterOpcode(0x8174, op_get_world_map_y_pos);

    // 0x817
    // 0x817
    // 0x817

    // 0x817
    // 0x817
    // 0x817
    // 0x817
    // 0x817
    // 0x817
    // 0x817
    // 0x817
    // 0x818
    // 0x818
    // 0x818
    // 0x818
    // 0x818
    // 0x818
    // 0x819
    // 0x818
    // 0x818
    // 0x818
    // 0x818
    // 0x824

    // 0x818

    // 0x818
    // 0x818

    // 0x818
    // 0x818
    // 0x819

    // 0x819
    // 0x819

    // 0x819
    interpreterRegisterOpcode(0x8193, op_active_hand);
    // 0x819
    interpreterRegisterOpcode(0x8194, op_toggle_active_hand);

    // 0x819
    // 0x819
    // 0x819
    // 0x819
    // 0x819
    // 0x819

    // 0x819
    interpreterRegisterOpcode(0x819D, op_set_sfall_global);
    // 0x819
    interpreterRegisterOpcode(0x819E, op_get_sfall_global_int);
    // 0x819
    // 0x822
    interpreterRegisterOpcode(0x822D, op_create_array);
    // 0x822
    interpreterRegisterOpcode(0x822E, op_set_array);
    // 0x822
    interpreterRegisterOpcode(0x822F, op_get_array);
    // 0x823
    interpreterRegisterOpcode(0x8230, op_free_array);
    // 0x823
    interpreterRegisterOpcode(0x8231, op_len_array);
    // 0x823
    interpreterRegisterOpcode(0x8232, op_resize_array);
    // 0x823
    interpreterRegisterOpcode(0x8233, op_temp_array);
    // 0x823
    interpreterRegisterOpcode(0x8234, op_fix_array);
    // 0x823
    interpreterRegisterOpcode(0x8239, op_scan_array);
    // 0x825
    // 0x825
    // 0x825
    interpreterRegisterOpcode(0x8256, op_get_array_key);
    // 0x825
    interpreterRegisterOpcode(0x8257, op_stack_array);

    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81

    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81

    // note: these are deprecated; do not implement
    // 0x81
    // 0x81

    // 0x81
    // 0x824

    // 0x81
    // 0x81
    // 0x81
    // 0x81

    // 0x81
    interpreterRegisterOpcode(0x81AC, op_get_ini_setting);
    // 0x81
    interpreterRegisterOpcode(0x81EB, op_get_ini_string);

    // 0x81
    interpreterRegisterOpcode(0x81AF, op_get_game_mode);

    // 0x81
    interpreterRegisterOpcode(0x81B3, op_get_uptime);

    // 0x81
    interpreterRegisterOpcode(0x81B6, op_set_car_current_town);

    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x822

    // 0x81
    // 0x81
    // 0x81
    // 0x81

    // 0x81
    interpreterRegisterOpcode(0x81DC, op_show_iface_tag);
    // 0x81
    interpreterRegisterOpcode(0x81DD, op_hide_iface_tag);
    // 0x81
    interpreterRegisterOpcode(0x81DE, op_is_iface_tag_active);

    // 0x81
    interpreterRegisterOpcode(0x81DF, op_get_bodypart_hit_modifier);
    // 0x81
    interpreterRegisterOpcode(0x81E0, op_set_bodypart_hit_modifier);

    // 0x81
    // 0x81
    // 0x81

    // 0x81
    interpreterRegisterOpcode(0x81e4, op_get_sfall_arg);

    // 0x823
    interpreterRegisterOpcode(0x823c, op_get_sfall_args);

    // 0x823
    interpreterRegisterOpcode(0x823d, op_set_sfall_arg);

    // 0x81
    interpreterRegisterOpcode(0x81e5, op_set_sfall_return);

    // 0x81

    // 0x81
    // 0x81
    // 0x81
    // 0x81

    // 0x81
    interpreterRegisterOpcode(0x81EC, op_sqrt);
    // 0x81
    interpreterRegisterOpcode(0x81ED, op_abs);
    // 0x81
    interpreterRegisterOpcode(0x81EE, op_sin);
    // 0x81
    interpreterRegisterOpcode(0x81EF, op_cos);
    // 0x81
    interpreterRegisterOpcode(0x81F0, op_tan);
    // 0x81
    interpreterRegisterOpcode(0x81F1, op_arctan);
    // 0x826
    interpreterRegisterOpcode(0x8263, op_power);
    // 0x826
    interpreterRegisterOpcode(0x8264, op_log);
    // 0x826
    interpreterRegisterOpcode(0x8265, op_exponent);
    // 0x826
    interpreterRegisterOpcode(0x8266, op_ceil);
    // 0x826
    interpreterRegisterOpcode(0x8267, op_round);
    // 0x827
    interpreterRegisterOpcode(0x827F, op_div);

    // 0x81

    // 0x81
    // 0x81
    // 0x81
    interpreterRegisterOpcode(0x81F5, op_get_script);

    // 0x81

    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x81
    // 0x820
    // 0x820
    // 0x820
    // 0x820
    // 0x820
    // 0x81
    // 0x820
    // 0x820
    // 0x820
    // 0x820

    // 0x820
    interpreterRegisterOpcode(0x8204, op_get_proto_data);
    // 0x820
    interpreterRegisterOpcode(0x8205, op_set_proto_data);

    // 0x820
    interpreterRegisterOpcode(0x8206, op_set_self);
    // 0x820
    interpreterRegisterOpcode(0x8207, op_register_hook);

    // 0x820
    interpreterRegisterOpcode(0x820D, op_list_begin);
    // 0x820
    interpreterRegisterOpcode(0x820E, op_list_next);
    // 0x820
    interpreterRegisterOpcode(0x820F, op_list_end);
    // 0x823
    interpreterRegisterOpcode(0x8236, op_list_as_array);

    // 0x821
    interpreterRegisterOpcode(0x8210, op_get_version_major);
    // 0x821
    interpreterRegisterOpcode(0x8211, op_get_version_minor);
    // 0x821
    interpreterRegisterOpcode(0x8212, op_get_version_patch);

    // 0x821
    // 0x821
    // 0x821

    // 0x821

    // 0x821
    interpreterRegisterOpcode(0x8217, op_get_weapon_ammo_pid);
    // 0x821
    interpreterRegisterOpcode(0x8218, op_set_weapon_ammo_pid);
    // 0x821
    interpreterRegisterOpcode(0x8219, op_get_weapon_ammo_count);
    // 0x821
    interpreterRegisterOpcode(0x821A, op_set_weapon_ammo_count);

    // 0x822
    interpreterRegisterOpcode(0x8220, op_get_screen_width);
    // 0x822
    interpreterRegisterOpcode(0x8221, op_get_screen_height);

    // 0x822
    // 0x822
    // 0x822
    interpreterRegisterOpcode(0x8224, op_create_message_window);

    // 0x822

    // 0x822
    interpreterRegisterOpcode(0x8227, op_refresh_pc_art);

    // 0x822
    interpreterRegisterOpcode(0x8228, op_get_attack_type);

    // 0x822
    // 0x822

    // 0x823
    interpreterRegisterOpcode(0x8235, op_string_split);
    // 0x823
    interpreterRegisterOpcode(0x8237, op_parse_int);
    // 0x823
    interpreterRegisterOpcode(0x8238, op_atof);
    // 0x824
    interpreterRegisterOpcode(0x824E, op_substr);
    // 0x824
    interpreterRegisterOpcode(0x824F, op_get_string_length);
    // 0x825
    interpreterRegisterOpcode(0x8250, op_sprintf);
    // 0x825
    interpreterRegisterOpcode(0x8251, op_charcode);
    // 0x825
    interpreterRegisterOpcode(0x8253, op_type_of);

    // 0x823
    interpreterRegisterOpcode(0x823A, op_get_tile_fid);

    // 0x823

    // 0x823
    // 0x823

    // 0x824

    // 0x824
    // 0x824
    // 0x824

    // 0x824
    interpreterRegisterOpcode(0x824B, op_tile_under_cursor);
    // 0x824
    // 0x824

    // 0x825
    // 0x825
    // 0x825
    // 0x825
    // 0x825
    // 0x825
    // 0x826

    // 0x826
    interpreterRegisterOpcode(0x8261, op_explosions_metarule);

    // 0x826
    interpreterRegisterOpcode(0x8262, op_register_hook_proc);

    // 0x826
    interpreterRegisterOpcode(0x826B, op_get_message);
    // 0x826
    // 0x826
    interpreterRegisterOpcode(0x826D, op_tile_light);
    // 0x826
    interpreterRegisterOpcode(0x826E, op_make_straight_path);
    // 0x826
    interpreterRegisterOpcode(0x826F, op_obj_blocking_at);
    // 0x827
    interpreterRegisterOpcode(0x8270, op_tile_get_objects);
    // 0x827
    interpreterRegisterOpcode(0x8271, op_party_member_list);
    // 0x827
    interpreterRegisterOpcode(0x8272, op_make_path);
    // 0x827
    // 0x827
    interpreterRegisterOpcode(0x8274, op_art_exists);
    // 0x827

    // 0x827
    interpreterRegisterOpcode(0x8276, op_sfall_func0);
    // 0x827
    interpreterRegisterOpcode(0x8277, op_sfall_func1);
    // 0x827
    interpreterRegisterOpcode(0x8278, op_sfall_func2);
    // 0x827
    interpreterRegisterOpcode(0x8279, op_sfall_func3);
    // 0x827
    interpreterRegisterOpcode(0x827A, op_sfall_func4);
    // 0x827
    interpreterRegisterOpcode(0x827B, op_sfall_func5);
    // 0x827
    interpreterRegisterOpcode(0x827C, op_sfall_func6);
    // 0x828
    interpreterRegisterOpcode(0x8280, op_sfall_func7);
    // 0x828
    interpreterRegisterOpcode(0x8281, op_sfall_func8);

    // 0x827
    interpreterRegisterOpcode(0x827d, op_register_hook_proc);
    // 0x827
}

void sfallOpcodesExit()
{
}

} // namespace fallout
