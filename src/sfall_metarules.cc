#include "sfall_metarules.h"

#include <algorithm>
#include <cstdarg>
#include <math.h>
#include <memory>
#include <string.h>
#include <string>

#include "art.h"
#include "color.h"
#include "combat.h"
#include "config.h" // For Config, configInit, configFree
#include "dbox.h"
#include "debug.h"
#include "game.h"
#include "game_dialog.h"
#include "game_mouse.h"
#include "interface.h"
#include "interpreter.h"
#include "inventory.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "platform_compat.h"
#include "scripts.h"
#include "sfall_animation.h"
#include "sfall_arrays.h" // For CreateTempArray, SetArray
#include "sfall_ini.h"
#include "sfall_opcodes.h"
#include "sfall_script_hooks.h"
#include "text_font.h"
#include "tile.h"
#include "window.h"
#include "worldmap.h"

#include <assert.h>

namespace fallout {

static void mf_car_gas_amount(MetaruleContext& ctx);
static void mf_combat_data(MetaruleContext& ctx);
static void mf_critter_inven_obj2(MetaruleContext& ctx);
static void mf_dialog_obj(MetaruleContext& ctx);
static void mf_get_cursor_mode(MetaruleContext& ctx);
static void mf_get_flags(MetaruleContext& ctx);
static void mf_get_object_data(MetaruleContext& ctx);
static void mf_get_sfall_arg_at(MetaruleContext& ctx);
static void mf_get_text_width(MetaruleContext& ctx);
static void mf_intface_redraw(MetaruleContext& ctx);
static void mf_loot_obj(MetaruleContext& ctx);
static void mf_message_box(MetaruleContext& ctx);
static void mf_add_extra_msg_file(MetaruleContext& ctx);
static void mf_art_cache_flush(MetaruleContext& ctx);
static void mf_metarule_exist(MetaruleContext& ctx);
static void mf_obj_under_cursor(MetaruleContext& ctx);
static void mf_opcode_exists(MetaruleContext& ctx);
static void mf_outlined_object(MetaruleContext& ctx);
static void mf_set_cursor_mode(MetaruleContext& ctx);
static void mf_set_flags(MetaruleContext& ctx);
static void mf_set_outline(MetaruleContext& ctx);
static void mf_show_window(MetaruleContext& ctx);
static void mf_tile_by_position(MetaruleContext& ctx);
static void mf_tile_refresh_display(MetaruleContext& ctx);
static void mf_string_compare(MetaruleContext& ctx);
static void mf_string_find(MetaruleContext& ctx);
static void mf_string_to_case(MetaruleContext& ctx);
static void mf_string_format(MetaruleContext& ctx);
static void mf_floor2(MetaruleContext& ctx);

MetaruleContext::MetaruleContext(Program* program, const MetaruleInfo* metaruleInfo, int numArgs)
    : _program(program)
    , _metaruleInfo(metaruleInfo)
    , _numArgs(numArgs)
    , _returnValue(0)
    , _hasReturnValue(false)
{
    assert(numArgs >= 0 && numArgs <= static_cast<int>(METARULE_MAX_ARGS));

    for (int index = _numArgs - 1; index >= 0; index--) {
        _args[index] = programStackPopValue(_program);
    }
}

MetaruleContext::MetaruleContext(Program* program, const MetaruleInfo* metaruleInfo, int numArgs, const ProgramValue* args)
    : _program(program)
    , _metaruleInfo(metaruleInfo)
    , _numArgs(numArgs)
    , _returnValue(0)
    , _hasReturnValue(false)
{
    assert(numArgs >= 0 && numArgs <= static_cast<int>(METARULE_MAX_ARGS));

    for (int index = 0; index < _numArgs; index++) {
        _args[index] = args[_numArgs - index - 1];
    }
}

Program* MetaruleContext::program() const
{
    return _program;
}

const MetaruleInfo* MetaruleContext::metaruleInfo() const
{
    return _metaruleInfo;
}

const char* MetaruleContext::name() const
{
    return _metaruleInfo->name;
}

int MetaruleContext::numArgs() const
{
    return _numArgs;
}

const ProgramValue& MetaruleContext::arg(int index) const
{
    assert(index >= 0 && index < _numArgs);
    return _args[index];
}

int MetaruleContext::intArg(int index) const
{
    return arg(index).asInt();
}

float MetaruleContext::floatArg(int index) const
{
    return arg(index).asFloat();
}

const char* MetaruleContext::stringArg(int index) const
{
    const ProgramValue& value = arg(index);
    if (!value.isString()) {
        programFatalError("MetaruleContext::stringArg: string expected, got %x", value.opcode);
    }

    return programGetString(_program, value.opcode, value.integerValue);
}

void* MetaruleContext::pointerArg(int index) const
{
    const ProgramValue& value = arg(index);
    if (value.opcode == VALUE_TYPE_INT && value.integerValue == 0) {
        return nullptr;
    }

    if (!value.isPointer()) {
        programFatalError("MetaruleContext::pointerArg: pointer expected, got %x", value.opcode);
    }

    return value.pointerValue;
}

void MetaruleContext::setReturn(const ProgramValue& value)
{
    _returnValue = value;
    _hasReturnValue = true;
}

void MetaruleContext::setReturn(int value)
{
    setReturn(ProgramValue(value));
}

void MetaruleContext::setReturn(float value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_FLOAT;
    programValue.floatValue = value;
    setReturn(programValue);
}

void MetaruleContext::setReturn(const char* value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_DYNAMIC_STRING;
    programValue.integerValue = programPushString(_program, value);
    setReturn(programValue);
}

void MetaruleContext::setReturn(void* value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_PTR;
    programValue.pointerValue = value;
    setReturn(programValue);
}

void MetaruleContext::setReturn(Object* value)
{
    setReturn(static_cast<void*>(value));
}

void MetaruleContext::setReturn(Attack* value)
{
    setReturn(static_cast<void*>(value));
}

void MetaruleContext::pushReturnValue() const
{
    programStackPushValue(_program, _hasReturnValue ? _returnValue : ProgramValue(0));
}

void MetaruleContext::printError(const char* format, ...) const
{
    va_list args;
    va_start(args, format);
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    programPrintError("%s", message);
}

bool MetaruleContext::validateArguments() const
{
    for (int index = 0; index < _numArgs; index++) {
        const ProgramValue& value = arg(index);
        switch (_metaruleInfo->argumentTypes[index]) {
        case ARG_ANY:
            continue;
        case ARG_INT:
            if (!value.isInt()) {
                printError("%s() - argument #%d is not an integer.", name(), index + 1);
                return false;
            }
            break;
        case ARG_OBJECT:
            if (value.isPointer()) {
                if (value.pointerValue == nullptr) {
                    printError("%s() - argument #%d is null.", name(), index + 1);
                    return false;
                }
            } else if (value.isInt()) {
                if (value.integerValue == 0) {
                    printError("%s() - argument #%d is null.", name(), index + 1);
                    return false;
                }
            } else {
                printError("%s() - argument #%d is not an object.", name(), index + 1);
                return false;
            }
            break;
        case ARG_STRING:
            if (!value.isString()) {
                printError("%s() - argument #%d is not a string.", name(), index + 1);
                return false;
            }
            break;
        case ARG_INTSTR:
            if (!value.isInt() && !value.isString()) {
                printError("%s() - argument #%d is not an integer or a string.", name(), index + 1);
                return false;
            }
            break;
        case ARG_NUMBER:
            if (!value.isInt() && !value.isFloat()) {
                printError("%s() - argument #%d is not a number.", name(), index + 1);
                return false;
            }
            break;
        }
    }

    return true;
}

// ref. https://github.com/sfall-team/sfall/blob/42556141127895c27476cd5242a73739cbb0fade/sfall/Modules/Scripting/Handlers/Metarule.cpp#L72
// Note: metarules should pop arguments off the stack in natural order

// TODO: argument validation, standard error return value
// TODO: reduce code complexity using something like MetaruleContext in sfall
const MetaruleInfo kMetarules[] = {
    { "add_extra_msg_file", mf_add_extra_msg_file, 1, 2, -1, { ARG_STRING, ARG_INT } },
    // {"add_iface_tag",             mf_add_iface_tag,             0, 0},
    // {"add_g_timer_event",         mf_add_g_timer_event,         2, 2, -1, {ARG_INT, ARG_INT}},
    // {"add_trait",                 mf_add_trait,                 1, 1, -1, {ARG_INT}},
    { "art_cache_clear", mf_art_cache_flush, 0, 0 },
    // {"art_frame_data",            mf_art_frame_data,            1, 3,  0, {ARG_INTSTR, ARG_INT, ARG_INT}},
    // {"attack_is_aimed",           mf_attack_is_aimed,           0, 0},
    { "car_gas_amount", mf_car_gas_amount, 0, 0 },
    { "combat_data", mf_combat_data, 0, 0 },
    // {"create_win",                mf_create_win,                5, 6, -1, {ARG_STRING, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    { "critter_inven_obj2", mf_critter_inven_obj2, 2, 2, 0, { ARG_OBJECT, ARG_INT } },
    // {"dialog_message",            mf_dialog_message,            1, 1, -1, {ARG_STRING}},
    { "dialog_obj", mf_dialog_obj, 0, 0 },
    // {"display_stats",             mf_display_stats,             0, 0}, // refresh
    // {"draw_image",                mf_draw_image,                1, 5, -1, {ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"draw_image_scaled",         mf_draw_image_scaled,         1, 6, -1, {ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"exec_map_update_scripts",   mf_exec_map_update_scripts,   0, 0},
    { "floor2", mf_floor2, 1, 1, 0, { ARG_NUMBER } },
    // {"get_can_rest_on_map",       mf_get_rest_on_map,           2, 2, -1, {ARG_INT, ARG_INT}},
    // {"get_combat_free_move",      mf_get_combat_free_move,      0, 0},
    // {"get_current_inven_size",    mf_get_current_inven_size,    1, 1,  0, {ARG_OBJECT}},
    { "get_cursor_mode", mf_get_cursor_mode, 0, 0 },
    { "get_flags", mf_get_flags, 1, 1, 0, { ARG_OBJECT } },
    // {"get_ini_config",            mf_get_ini_config,            2, 2,  0, {ARG_STRING, ARG_INT}},
    { "get_ini_section", mf_get_ini_section, 2, 2, -1, { ARG_STRING, ARG_STRING } },
    { "get_ini_sections", mf_get_ini_sections, 1, 1, -1, { ARG_STRING } },
    // {"get_inven_ap_cost",         mf_get_inven_ap_cost,         0, 0},
    // {"get_map_enter_position",    mf_get_map_enter_position,    0, 0},
    // {"get_metarule_table",        mf_get_metarule_table,        0, 0},
    // {"get_object_ai_data",        mf_get_object_ai_data,        2, 2, -1, {ARG_OBJECT, ARG_INT}},
    { "get_object_data", mf_get_object_data, 2, 2, 0, { ARG_OBJECT, ARG_INT } },
    // {"get_outline",               mf_get_outline,               1, 1,  0, {ARG_OBJECT}},
    { "get_sfall_arg_at", mf_get_sfall_arg_at, 1, 1, 0, { ARG_INT } },
    // {"get_stat_max",              mf_get_stat_max,              1, 2,  0, {ARG_INT, ARG_INT}},
    // {"get_stat_min",              mf_get_stat_min,              1, 2,  0, {ARG_INT, ARG_INT}},
    // {"get_string_pointer",        mf_get_string_pointer,        1, 1,  0, {ARG_STRING}}, // note: deprecated; do not implement
    // {"get_terrain_name",          mf_get_terrain_name,          0, 2, -1, {ARG_INT, ARG_INT}},
    { "get_text_width", mf_get_text_width, 1, 1, 0, { ARG_STRING } },
    // {"get_window_attribute",      mf_get_window_attribute,      1, 2, -1, {ARG_INT, ARG_INT}},
    // {"has_fake_perk_npc",         mf_has_fake_perk_npc,         2, 2,  0, {ARG_OBJECT, ARG_STRING}},
    // {"has_fake_trait_npc",        mf_has_fake_trait_npc,        2, 2,  0, {ARG_OBJECT, ARG_STRING}},
    // {"hide_window",               mf_hide_window,               0, 1, -1, {ARG_STRING}},
    // {"interface_art_draw",        mf_interface_art_draw,        4, 6, -1, {ARG_INT, ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"interface_overlay",         mf_interface_overlay,         2, 6, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"interface_print",           mf_interface_print,           5, 6, -1, {ARG_STRING, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"intface_hide",              mf_intface_hide,              0, 0},
    // {"intface_is_hidden",         mf_intface_is_hidden,         0, 0},
    { "intface_redraw", mf_intface_redraw, 0, 1 },
    // {"intface_show",              mf_intface_show,              0, 0},
    // {"inventory_redraw",          mf_inventory_redraw,          0, 1, -1, {ARG_INT}},
    // {"item_make_explosive",       mf_item_make_explosive,       3, 4, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"item_weight",               mf_item_weight,               1, 1,  0, {ARG_OBJECT}},
    // {"lock_is_jammed",            mf_lock_is_jammed,            1, 1,  0, {ARG_OBJECT}},
    { "loot_obj", mf_loot_obj, 0, 0 },
    { "message_box", mf_message_box, 1, 4, -1, { ARG_STRING, ARG_INT, ARG_INT, ARG_INT } },
    { "metarule_exist", mf_metarule_exist, 1, 1 },
    // {"npc_engine_level_up",       mf_npc_engine_level_up,       1, 1},
    // {"obj_is_openable",           mf_obj_is_openable,           1, 1,  0, {ARG_OBJECT}},
    { "obj_under_cursor", mf_obj_under_cursor, 2, 2, 0, { ARG_INT, ARG_INT } },
    // {"objects_in_radius",         mf_objects_in_radius,         3, 4,  0, {ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    { "outlined_object", mf_outlined_object, 0, 0 },
    // {"real_dude_obj",             mf_real_dude_obj,             0, 0},
    { "reg_anim_animate_and_move", mf_reg_anim_animate_and_move, 4, 4, -1, { ARG_OBJECT, ARG_INT, ARG_INT, ARG_INT } },
    // {"remove_timer_event",        mf_remove_timer_event,        0, 1, -1, {ARG_INT}},
    // {"set_spray_settings",        mf_set_spray_settings,        4, 4, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"set_can_rest_on_map",       mf_set_rest_on_map,           3, 3, -1, {ARG_INT, ARG_INT, ARG_INT}},
    // {"set_car_intface_art",       mf_set_car_intface_art,       1, 1, -1, {ARG_INT}},
    // {"set_combat_free_move",      mf_set_combat_free_move,      1, 1, -1, {ARG_INT}},
    { "set_cursor_mode", mf_set_cursor_mode, 1, 1, -1, { ARG_INT } },
    // {"set_drugs_data",            mf_set_drugs_data,            3, 3, -1, {ARG_INT, ARG_INT, ARG_INT}},
    // {"set_dude_obj",              mf_set_dude_obj,              1, 1, -1, {ARG_INT}},
    // {"set_fake_perk_npc",         mf_set_fake_perk_npc,         5, 5, -1, {ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING}},
    // {"set_fake_trait_npc",        mf_set_fake_trait_npc,        5, 5, -1, {ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING}},
    { "set_flags", mf_set_flags, 2, 2, -1, { ARG_OBJECT, ARG_INT } },
    // {"set_iface_tag_text",        mf_set_iface_tag_text,        3, 3, -1, {ARG_INT, ARG_STRING, ARG_INT}},
    { "set_ini_setting", mf_set_ini_setting, 2, 2, -1, { ARG_STRING, ARG_INTSTR } },
    // {"set_map_enter_position",    mf_set_map_enter_position,    3, 3, -1, {ARG_INT, ARG_INT, ARG_INT}},
    // {"set_object_data",           mf_set_object_data,           3, 3, -1, {ARG_OBJECT, ARG_INT, ARG_INT}},
    { "set_outline", mf_set_outline, 2, 2, -1, { ARG_OBJECT, ARG_INT } },
    // {"set_quest_failure_value",   mf_set_quest_failure_value,   2, 2, -1, {ARG_INT, ARG_INT}},
    // {"set_rest_heal_time",        mf_set_rest_heal_time,        1, 1, -1, {ARG_INT}},
    // {"set_worldmap_heal_time",    mf_set_worldmap_heal_time,    1, 1, -1, {ARG_INT}},
    // {"set_rest_mode",             mf_set_rest_mode,             1, 1, -1, {ARG_INT}},
    // {"set_scr_name",              mf_set_scr_name,              0, 1, -1, {ARG_STRING}},
    // {"set_selectable_perk_npc",   mf_set_selectable_perk_npc,   5, 5, -1, {ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING}},
    // {"set_terrain_name",          mf_set_terrain_name,          3, 3, -1, {ARG_INT, ARG_INT, ARG_STRING}},
    // {"set_town_title",            mf_set_town_title,            2, 2, -1, {ARG_INT, ARG_STRING}},
    // {"set_unique_id",             mf_set_unique_id,             1, 2, -1, {ARG_OBJECT, ARG_INT}},
    // {"set_unjam_locks_time",      mf_set_unjam_locks_time,      1, 1, -1, {ARG_INT}},
    // {"set_window_flag",           mf_set_window_flag,           3, 3, -1, {ARG_INTSTR, ARG_INT, ARG_INT}},
    { "show_window", mf_show_window, 0, 1, -1, { ARG_STRING } },
    // {"signal_close_game",         mf_signal_close_game,         0, 0},
    // {"spatial_radius",            mf_spatial_radius,            1, 1,  0, {ARG_OBJECT}},
    { "string_compare", mf_string_compare, 2, 3, 0, { ARG_STRING, ARG_STRING, ARG_INT } },
    { "string_find", mf_string_find, 2, 3, -1, { ARG_STRING, ARG_STRING, ARG_INT } },
    { "string_format", mf_string_format, 2, 8, 0, { ARG_STRING, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY } },
    { "string_to_case", mf_string_to_case, 2, 2, -1, { ARG_STRING, ARG_INT } },
    { "tile_by_position", mf_tile_by_position, 2, 2, -1, { ARG_INT, ARG_INT } },
    { "tile_refresh_display", mf_tile_refresh_display, 0, 0 },
    // {"unjam_lock",                mf_unjam_lock,                1, 1, -1, {ARG_OBJECT}},
    // {"unwield_slot",              mf_unwield_slot,              2, 2, -1, {ARG_OBJECT, ARG_INT}},
    // {"win_fill_color",            mf_win_fill_color,            0, 5, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    { "opcode_exists", mf_opcode_exists, 1, 1 },
};
const std::size_t kMetarulesCount = sizeof(kMetarules) / sizeof(kMetarules[0]);

constexpr int kMetarulesMax = sizeof(kMetarules) / sizeof(kMetarules[0]);

void mf_art_cache_flush(MetaruleContext& ctx)
{
    artCacheFlush();
}

void mf_car_gas_amount(MetaruleContext& ctx)
{
    ctx.setReturn(wmCarGasAmount());
}

void mf_combat_data(MetaruleContext& ctx)
{
    if (isInCombat()) {
        ctx.setReturn(combat_get_data());
    } else {
        ctx.setReturn(static_cast<void*>(nullptr));
    }
}

void mf_critter_inven_obj2(MetaruleContext& ctx)
{
    Object* obj = static_cast<Object*>(ctx.pointerArg(0));
    int slot = ctx.intArg(1);

    switch (slot) {
    case 0:
        ctx.setReturn(critterGetArmor(obj));
        break;
    case 1:
        ctx.setReturn(critterGetItem2(obj));
        break;
    case 2:
        ctx.setReturn(critterGetItem1(obj));
        break;
    case -2:
        ctx.setReturn(obj->data.inventory.length);
        break;
    default:
        programFatalError("mf_critter_inven_obj2: invalid type");
    }
}

void mf_dialog_obj(MetaruleContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kDialog)) {
        ctx.setReturn(gGameDialogSpeaker);
    } else {
        ctx.setReturn(static_cast<void*>(nullptr));
    }
}

void mf_get_cursor_mode(MetaruleContext& ctx)
{
    ctx.setReturn(gameMouseGetMode());
}

void mf_get_flags(MetaruleContext& ctx)
{
    Object* object = static_cast<Object*>(ctx.pointerArg(0));
    ctx.setReturn(object->flags);
}

void mf_get_sfall_arg_at(MetaruleContext& ctx)
{
    const int argNum = ctx.intArg(0);

    ProgramValue result(0);
    const auto hookCall = hookOpcodeGetCurrentCall(ctx.name());
    if (hookCall != nullptr) {
        if (argNum >= 0 && argNum < hookCall->numArgs()) {
            result = hookCall->getArgAt(argNum);
        } else {
            ctx.printError("%s: argNum %d out of range [0, %d]", ctx.name(), argNum, hookCall->numArgs() - 1);
        }
    }
    ctx.setReturn(result);
}

void mf_get_object_data(MetaruleContext& ctx)
{
    // TODO: only allow to modify a set of whitelisted object types
    // TODO: map offsets to fields to avoid potential alignment, 64bit issues!
    void* ptr = ctx.pointerArg(0);
    size_t offset = static_cast<size_t>(ctx.intArg(1));

    if (offset % 4 != 0) {
        programFatalError("mf_get_object_data: bad offset %d", offset);
    }

    int value = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(ptr) + offset);
    ctx.setReturn(value);
}

void mf_get_text_width(MetaruleContext& ctx)
{
    ctx.setReturn(fontGetStringWidth(ctx.stringArg(0)));
}

void mf_intface_redraw(MetaruleContext& ctx)
{
    if (ctx.numArgs() == 0) {
        interfaceBarRefresh();
    } else {
        // TODO: Incomplete.
        programFatalError("mf_intface_redraw: not implemented");
    }
}

void mf_loot_obj(MetaruleContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kInventory)) {
        ctx.setReturn(inventoryGetTargetObject());
    } else {
        ctx.setReturn(static_cast<void*>(nullptr));
    }
}

void mf_metarule_exist(MetaruleContext& ctx)
{
    const char* metarule = ctx.stringArg(0);

    for (int index = 0; index < kMetarulesMax; index++) {
        if (strcmp(kMetarules[index].name, metarule) == 0) {
            ctx.setReturn(1);
            return;
        }
    }

    ctx.setReturn(0);
}

void mf_add_extra_msg_file(MetaruleContext& ctx)
{
    if (ctx.numArgs() == 2) {
        programFatalError("op_sfall_func: '%s': explicit fileNumber is not supported in Fallout 2 CE", ctx.name());
    }

    const char* fileName = ctx.stringArg(0);

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", "game", fileName);

    int result = messageListRepositoryAddExtra(path);
    switch (result) {
    case -2:
        ctx.printError("%s() - error loading message file.", ctx.name());
        break;
    case -3:
        ctx.printError("%s() - the limit of adding message files has been exceeded.", ctx.name());
        break;
    }

    ctx.setReturn(result);
}

void mf_opcode_exists(MetaruleContext& ctx)
{
    int opcode = ctx.intArg(0);
    int opcodeIndex = opcode & 0x3FFF;
    if (opcodeIndex < 0 || opcodeIndex >= OPCODE_MAX_COUNT) {
        ctx.setReturn(0);
        return;
    }
    auto opcodeHandler = gInterpreterOpcodeHandlers[opcodeIndex];
    int opcodeExists = opcodeHandler != nullptr ? 1 : 0;
    ctx.setReturn(opcodeExists);
}

void mf_obj_under_cursor(MetaruleContext& ctx)
{
    int includeDude = ctx.intArg(0);
    int onlyCritter = ctx.intArg(1);

    Object* object = gameMouseGetObjectUnderCursor(onlyCritter ? OBJ_TYPE_CRITTER : -1, includeDude, gElevation);

    ctx.setReturn(object);
}

void mf_outlined_object(MetaruleContext& ctx)
{
    ctx.setReturn(gmouse_get_outlined_object());
}

void mf_set_cursor_mode(MetaruleContext& ctx)
{
    int mode = ctx.intArg(0);
    gameMouseSetMode(mode);
}

void mf_set_flags(MetaruleContext& ctx)
{
    Object* object = static_cast<Object*>(ctx.pointerArg(0));
    int flags = ctx.intArg(1);

    object->flags = flags;
}

void mf_set_outline(MetaruleContext& ctx)
{
    Object* object = static_cast<Object*>(ctx.pointerArg(0));
    int outline = ctx.intArg(1);
    object->outline = outline;
}

void mf_show_window(MetaruleContext& ctx)
{
    if (ctx.numArgs() == 0) {
        scriptWindowShow();
    } else if (ctx.numArgs() == 1) {
        const char* windowName = ctx.stringArg(0);
        if (!scriptWindowShowNamed(windowName)) {
            debugPrint("show_window: window '%s' is not found", windowName);
        }
    }
}

void mf_tile_refresh_display(MetaruleContext& ctx)
{
    tileWindowRefresh();
}

void mf_tile_by_position(MetaruleContext& ctx)
{
    int x = ctx.intArg(0);
    int y = ctx.intArg(1);
    ctx.setReturn(tileFromScreenXY(x, y));
}

// compares strings case-insensitive with specifics for Fallout
// from sfall: https://github.com/sfall-team/sfall/blob/71ecec3d405bd5e945f157954618b169e60068fe/sfall/Modules/Scripting/Handlers/Utils.cpp#L34
static bool FalloutStringCompare(const char* str1, const char* str2, long codePage)
{
    while (true) {
        unsigned char c1 = *str1;
        unsigned char c2 = *str2;

        if (c1 == 0 && c2 == 0) return true; // end - strings are equal
        if (c1 == 0 || c2 == 0) return false; // strings are not equal
        str1++;
        str2++;
        if (c1 == c2) continue;

        if (codePage == 866) {
            // replace Russian 'x' with English (Fallout specific)
            if (c1 == 229) c1 -= 229 - 'x';
            if (c2 == 229) c2 -= 229 - 'x';
        }

        // 0 - 127 (standard ASCII)
        // upper to lower case
        if (c1 >= 'A' && c1 <= 'Z') c1 |= 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 |= 32;
        if (c1 == c2) continue;
        if (c1 < 128 || c2 < 128) return false;

        // 128 - 255 (international/extended)
        switch (codePage) {
        case 866:
            if (c1 != 149 && c2 != 149) { // code used for the 'bullet' character in Fallout font (the Russian letter 'X' uses Latin letter)
                // upper to lower case
                if (c1 >= 128 && c1 <= 159) {
                    c1 |= 32;
                } else if (c1 >= 224 && c1 <= 239) {
                    c1 -= 48; // shift lower range
                } else if (c1 == 240) {
                    c1++;
                }
                if (c2 >= 128 && c2 <= 159) {
                    c2 |= 32;
                } else if (c2 >= 224 && c2 <= 239) {
                    c2 -= 48; // shift lower range
                } else if (c2 == 240) {
                    c2++;
                }
            }
            break;
        case 1251:
            // upper to lower case
            if (c1 >= 0xC0 && c1 <= 0xDF) c1 |= 32;
            if (c2 >= 0xC0 && c2 <= 0xDF) c2 |= 32;
            if (c1 == 0xA8) c1 += 16;
            if (c2 == 0xA8) c2 += 16;
            break;
        case 1250:
        case 1252:
            if (c1 != 0xD7 && c1 != 0xF7 && c2 != 0xD7 && c2 != 0xF7) {
                if (c1 >= 0xC0 && c1 <= 0xDE) c1 |= 32;
                if (c2 >= 0xC0 && c2 <= 0xDE) c2 |= 32;
            }
            break;
        }
        if (c1 != c2) return false; // strings are not equal
    }
}

void mf_string_compare(MetaruleContext& ctx)
{
    // compare str1 to str3 case insensitively
    // if args == 3, use FalloutStringCompare
    const char* str1 = ctx.stringArg(0);
    const char* str2 = ctx.stringArg(1);
    int codePage = 0;
    if (ctx.numArgs() == 3) {
        codePage = ctx.intArg(2);
    }
    bool result = false;
    if (ctx.numArgs() < 3) {
        // default case-insensitive comparison
        result = compat_stricmp(str1, str2) == 0;
    } else {
        // Fallout specific case-insensitive comparison
        result = FalloutStringCompare(str1, str2, codePage);
    }
    if (result) {
        ctx.setReturn(1); // strings are equal
    } else {
        ctx.setReturn(0); // strings are not equal
    }
}

void mf_string_find(MetaruleContext& ctx)
{
    const char* str = ctx.stringArg(0);
    const char* substr = ctx.stringArg(1);
    int startPos = 0;

    if (ctx.numArgs() == 3) {
        startPos = ctx.intArg(2);
    }

    if (startPos < 0 || startPos >= strlen(str)) {
        debugPrint("string_find: invalid start position %d", startPos);
        ctx.setReturn(-1);
        return;
    }

    const char* found = strstr(str + startPos, substr);
    if (found) {
        ctx.setReturn(static_cast<int>(found - str));
    } else {
        ctx.setReturn(-1);
    }
}

void mf_string_to_case(MetaruleContext& ctx)
{
    auto buf = ctx.stringArg(0);
    std::string s(buf);
    auto caseType = ctx.intArg(1);
    if (caseType == 1) {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    } else if (caseType == 0) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    } else {
        debugPrint("string_to_case: invalid case type %d", caseType);
    }
    ctx.setReturn(s.c_str());
}

void mf_string_format(MetaruleContext& ctx)
{
    ProgramValue formatArgs[7];
    for (int index = 1; index < ctx.numArgs(); index++) {
        formatArgs[index - 1] = ctx.arg(index);
    }

    const char* format = ctx.stringArg(0);
    int args = ctx.numArgs();
    int fmtLen = static_cast<int>(strlen(format));
    if (fmtLen == 0) {
        ctx.setReturn("");
        return;
    }
    if (fmtLen > 1024) {
        debugPrint("%s(): format string exceeds maximum length of 1024 characters.", "string_format");
        ctx.setReturn("Error");
        return;
    }
    int newFmtLen = fmtLen;

    for (int i = 0; i < fmtLen; i++) {
        if (format[i] == '%') newFmtLen++;
    }

    auto newFmt = std::make_unique<char[]>(newFmtLen + 1);

    bool conversion = false;
    int j = 0;
    int valIdx = 0;

    char out[5120 + 1] = { 0 };
    int bufCount = sizeof(out) - 1;
    char* outBuf = out;

    int numArgs = args;

    for (int i = 0; i < fmtLen; i++) {
        char c = format[i];
        if (!conversion) {
            if (c == '%') conversion = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '%') {
            int partLen;
            if (c == '%') {
                newFmt[j] = '\0';
                strncpy(outBuf, newFmt.get(), std::min(j, bufCount - 1));
                partLen = j;
            } else {
                if (c == 'h' || c == 'l' || c == 'j' || c == 'z' || c == 't' || c == 'w' || c == 'L' || c == 'I') continue;
                if (++valIdx == numArgs) {
                    debugPrint("%s() - format string contains more conversions than passed arguments (%d): %s",
                        "string_format", numArgs - 1, format);
                }
                const auto& arg = formatArgs[std::min(valIdx - 1, numArgs - 2)];

                if (c == 'S' || c == 'Z') {
                    c = 's';
                }
                if ((c == 's' && !arg.isString()) || c == 'n') {
                    c = 'd';
                }
                newFmt[j++] = c;
                newFmt[j] = '\0';
                partLen = arg.isFloat()
                    ? snprintf(outBuf, bufCount, newFmt.get(), arg.floatValue)
                    : arg.isInt()    ? snprintf(outBuf, bufCount, newFmt.get(), arg.integerValue)
                    : arg.isString() ? snprintf(outBuf, bufCount, newFmt.get(),
                                           programGetString(ctx.program(), arg.opcode, arg.integerValue))
                                     : snprintf(outBuf, bufCount, newFmt.get(), "<UNSUPPORTED TYPE>");
            }
            outBuf += partLen;
            bufCount -= partLen;
            conversion = false;
            j = 0;
            if (bufCount <= 0) {
                break;
            }
            continue;
        }
        newFmt[j++] = c;
    }

    if (bufCount > 0) {
        newFmt[j] = '\0';
        if (strlen(newFmt.get()) < bufCount) {
            strcpy(outBuf, newFmt.get());
        } else {
            strncpy(outBuf, newFmt.get(), bufCount - 1);
            outBuf[bufCount - 1] = '\0';
        }
    }

    ctx.setReturn(out);
}

void mf_floor2(MetaruleContext& ctx)
{
    ctx.setReturn(static_cast<int>(floor(ctx.arg(0).asFloat())));
}

void sprintf_lite(Program* program, int args, const char* infoOpcodeName)
{
    ProgramValue formatArgs[7]; // 8 arguments total, 1 for format string

    for (int index = 0; index < args - 1; index++) {
        formatArgs[index] = programStackPopValue(program);
    }

    auto format = programStackPopString(program); // Pop the format string

    int fmtLen = static_cast<int>(strlen(format));
    if (fmtLen == 0) {
        programStackPushString(program, "");
        return;
    }
    if (fmtLen > 1024) {
        debugPrint("%s(): format string exceeds maximum length of 1024 characters.", infoOpcodeName);
        programStackPushString(program, "Error");
        return;
    }
    int newFmtLen = fmtLen;

    for (int i = 0; i < fmtLen; i++) {
        if (format[i] == '%') newFmtLen++; // will possibly be escaped, need space for that
    }

    // parse format to make it safe
    auto newFmt = std::make_unique<char[]>(newFmtLen + 1);

    bool conversion = false;
    int j = 0;
    int valIdx = 0;

    char out[5120 + 1] = { 0 };
    int bufCount = sizeof(out) - 1;
    char* outBuf = out;

    int numArgs = args; // From 2 to 8

    for (int i = 0; i < fmtLen; i++) {
        char c = format[i];
        if (!conversion) {
            // Start conversion.
            if (c == '%') conversion = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '%') {
            int partLen;
            if (c == '%') {
                // escaped % sign, just copy newFmt up to (and including) the leading % sign
                newFmt[j] = '\0';
                // strncpy_s(outBuf, bufCount, newFmt, j);
                strncpy(outBuf, newFmt.get(), std::min(j, bufCount - 1));
                partLen = j;
            } else {
                // ignore size prefixes
                if (c == 'h' || c == 'l' || c == 'j' || c == 'z' || c == 't' || c == 'w' || c == 'L' || c == 'I') continue;
                // Type specifier, perform conversion.
                if (++valIdx == numArgs) {
                    debugPrint("%s() - format string contains more conversions than passed arguments (%d): %s",
                        infoOpcodeName, numArgs - 1, format);
                }
                const auto& arg = formatArgs[std::min(valIdx - 1, numArgs - 2)];

                // ctx.arg(valIdx < numArgs ? valIdx : numArgs - 1);
                if (c == 'S' || c == 'Z') {
                    c = 's'; // don't allow wide strings
                }
                if ((c == 's' && !arg.isString()) || // don't allow treating non-string values as string pointers
                    c == 'n') // don't allow "n" specifier
                {
                    c = 'd';
                }
                newFmt[j++] = c;
                newFmt[j] = '\0';
                partLen = arg.isFloat()
                    ? snprintf(outBuf, bufCount, newFmt.get(), arg.floatValue)
                    : arg.isInt()    ? snprintf(outBuf, bufCount, newFmt.get(), arg.integerValue)
                    : arg.isString() ? snprintf(outBuf, bufCount, newFmt.get(),
                                           programGetString(program, arg.opcode, arg.integerValue))
                                     : snprintf(outBuf, bufCount, newFmt.get(), "<UNSUPPORTED TYPE>");
            }
            outBuf += partLen;
            bufCount -= partLen;
            conversion = false;
            j = 0;
            if (bufCount <= 0) {
                break;
            }
            continue;
        }
        newFmt[j++] = c;
    }
    // Copy the remainder of the string.
    if (bufCount > 0) {
        newFmt[j] = '\0';
        // strcpy_s(outBuf, bufCount, newFmt);
        if (strlen(newFmt.get()) < bufCount) {
            strcpy(outBuf, newFmt.get());
        } else {
            strncpy(outBuf, newFmt.get(), bufCount - 1);
            outBuf[bufCount - 1] = '\0'; // Ensure null-termination
        }
    }

    programStackPushString(program, out);
}

// message_box
void mf_message_box(MetaruleContext& ctx)
{
    static int dialogShowCount = 0;

    const char* string = ctx.stringArg(0);
    if (string == nullptr || string[0] == '\0') {
        ctx.setReturn(-1);
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

    int flags = DIALOG_BOX_LARGE | DIALOG_BOX_YES_NO;
    if (ctx.numArgs() > 1) {
        int flagParam = ctx.intArg(1);
        if (flagParam != -1) {
            flags = flagParam;
        }
    }

    // note: most of the CE code uses colorTable indices, but this metarule expects palette values.
    // Default: yellow (145) = _colorTable[32328]
    int color1 = _colorTable[32328], color2 = _colorTable[32328];
    if (ctx.numArgs() > 2) {
        color1 = ctx.intArg(2);
    }
    if (ctx.numArgs() > 3) {
        color2 = ctx.intArg(3);
    }

    dialogShowCount++;
    scriptsDisable();
    int rc = showDialogBox(copy, body, count, 192, 116, color1, nullptr, color2, flags);
    if (--dialogShowCount == 0) {
        scriptsEnable();
    }

    ctx.setReturn(rc);
    internal_free(copy);
}

void sfall_metarule(Program* program, int args)
{
    static ProgramValue values[8];

    for (int index = 0; index < args; index++) {
        values[index] = programStackPopValue(program);
    }

    ProgramValue metaruleName = programStackPopValue(program);
    if (!metaruleName.isString()) {
        programPrintError("op_sfall_func(name, ...) - name must be string.");
        programStackPushInteger(program, 0);
        return;
    }

    const char* metarule = programGetString(program, metaruleName.opcode, metaruleName.integerValue);

    const MetaruleInfo* metaruleInfo = nullptr;
    for (int index = 0; index < kMetarulesMax; index++) {
        if (strcmp(kMetarules[index].name, metarule) == 0) {
            metaruleInfo = &kMetarules[index];
            break;
        }
    }

    if (metaruleInfo == nullptr) {
        programPrintError("op_sfall_func(\"%s\", ...) - metarule function is unknown.", metarule);
        programStackPushInteger(program, 0);
        return;
    }

    if (args < metaruleInfo->minArgs || args > metaruleInfo->maxArgs) {
        programPrintError("op_sfall_func(\"%s\", ...) - invalid number of arguments (%d), must be from %d to %d.",
            metarule,
            args,
            metaruleInfo->minArgs,
            metaruleInfo->maxArgs);
        programStackPushInteger(program, metaruleInfo->errorReturn);
        return;
    }

    MetaruleContext ctx(program, metaruleInfo, args, values);
    if (!ctx.validateArguments()) {
        ctx.setReturn(metaruleInfo->errorReturn);
        ctx.pushReturnValue();
        return;
    }

    metaruleInfo->handler(ctx);
    ctx.pushReturnValue();
}

} // namespace fallout
