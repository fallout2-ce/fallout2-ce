#include "sfall_metarules.h"

#include <algorithm>
#include <math.h>
#include <memory>
#include <string.h>
#include <string>

#include "art.h"
#include "automap.h"
#include "character_editor.h"
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
#include "item.h"
#include "mainmenu.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "opcode_context.h"
#include "options.h"
#include "pipboy.h"
#include "platform_compat.h"
#include "proto_instance.h"
#include "scripts.h"
#include "sfall_animation.h"
#include "sfall_arrays.h" // For CreateTempArray, SetArray
#include "sfall_ini.h"
#include "sfall_opcodes.h"
#include "sfall_script_hooks.h"
#include "skilldex.h"
#include "text_font.h"
#include "tile.h"
#include "window.h"
#include "window_manager.h"
#include "worldmap.h"

#include <assert.h>

namespace fallout {

static void mf_attack_is_aimed(OpcodeContext& ctx);
static void mf_car_gas_amount(OpcodeContext& ctx);
static void mf_combat_data(OpcodeContext& ctx);
static void mf_create_win(OpcodeContext& ctx);
static void mf_critter_inven_obj2(OpcodeContext& ctx);
static void mf_dialog_obj(OpcodeContext& ctx);
static void mf_dialog_message(OpcodeContext& ctx);
static void mf_display_stats(OpcodeContext& ctx);
static void mf_draw_image(OpcodeContext& ctx);
static void mf_draw_image_scaled(OpcodeContext& ctx);
static void mf_get_combat_free_move(OpcodeContext& ctx);
static void mf_get_cursor_mode(OpcodeContext& ctx);
static void mf_get_flags(OpcodeContext& ctx);
static void mf_get_inven_ap_cost(OpcodeContext& ctx);
static void mf_get_object_data(OpcodeContext& ctx);
static void mf_get_outline(OpcodeContext& ctx);
static void mf_get_sfall_arg_at(OpcodeContext& ctx);
static void mf_get_text_width(OpcodeContext& ctx);
static void mf_get_window_attribute(OpcodeContext& ctx);
static void mf_hide_window(OpcodeContext& ctx);
static void mf_interface_art_draw(OpcodeContext& ctx);
static void mf_intface_redraw(OpcodeContext& ctx);
static void mf_inventory_redraw(OpcodeContext& ctx);
static void mf_item_weight(OpcodeContext& ctx);
static void mf_loot_obj(OpcodeContext& ctx);
static void mf_message_box(OpcodeContext& ctx);
static void mf_add_extra_msg_file(OpcodeContext& ctx);
static void mf_add_iface_tag(OpcodeContext& ctx);
static void mf_art_frame_data(OpcodeContext& ctx);
static void mf_art_cache_flush(OpcodeContext& ctx);
static void mf_metarule_exist(OpcodeContext& ctx);
static void mf_obj_is_openable(OpcodeContext& ctx);
static void mf_obj_under_cursor(OpcodeContext& ctx);
static void mf_objects_in_radius(OpcodeContext& ctx);
static void mf_opcode_exists(OpcodeContext& ctx);
static void mf_outlined_object(OpcodeContext& ctx);
static void mf_real_dude_obj(OpcodeContext& ctx);
static void mf_set_combat_free_move(OpcodeContext& ctx);
static void mf_set_cursor_mode(OpcodeContext& ctx);
static void mf_set_flags(OpcodeContext& ctx);
static void mf_set_iface_tag_text(OpcodeContext& ctx);
static void mf_set_outline(OpcodeContext& ctx);
static void mf_set_window_flag(OpcodeContext& ctx);
static void mf_set_unique_id(OpcodeContext& ctx);
static void mf_show_window(OpcodeContext& ctx);
static void mf_signal_close_game(OpcodeContext& ctx);
static void mf_tile_by_position(OpcodeContext& ctx);
static void mf_tile_refresh_display(OpcodeContext& ctx);
static void mf_unwield_slot(OpcodeContext& ctx);
static void mf_win_fill_color(OpcodeContext& ctx);
static void mf_string_compare(OpcodeContext& ctx);
static void mf_string_find(OpcodeContext& ctx);
static void mf_string_to_case(OpcodeContext& ctx);
void mf_string_format(OpcodeContext& ctx);
static void mf_floor2(OpcodeContext& ctx);

// ref. https://github.com/sfall-team/sfall/blob/42556141127895c27476cd5242a73739cbb0fade/sfall/Modules/Scripting/Handlers/Metarule.cpp#L72

// TODO: reduce duplication further once this context is shared with opcode handlers too.
const MetaruleInfo kMetarules[] = {
    { "add_extra_msg_file", mf_add_extra_msg_file, 1, 2, -1, { ARG_STRING, ARG_INT } },
    { "add_iface_tag", mf_add_iface_tag, 0, 0 },
    // {"add_g_timer_event",         mf_add_g_timer_event,         2, 2, -1, {ARG_INT, ARG_INT}},
    // {"add_trait",                 mf_add_trait,                 1, 1, -1, {ARG_INT}},
    { "art_cache_clear", mf_art_cache_flush, 0, 0 },
    { "art_frame_data", mf_art_frame_data, 1, 3, 0, { ARG_INTSTR, ARG_INT, ARG_INT } },
    { "attack_is_aimed", mf_attack_is_aimed, 0, 0 },
    { "car_gas_amount", mf_car_gas_amount, 0, 0 },
    { "combat_data", mf_combat_data, 0, 0 },
    { "create_win", mf_create_win, 5, 6, -1, { ARG_STRING, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "critter_inven_obj2", mf_critter_inven_obj2, 2, 2, 0, { ARG_OBJECT, ARG_INT } },
    { "dialog_message", mf_dialog_message, 1, 1, -1, { ARG_STRING } },
    { "dialog_obj", mf_dialog_obj, 0, 0 },
    { "display_stats", mf_display_stats, 0, 0 }, // refresh
    { "draw_image", mf_draw_image, 1, 5, -1, { ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "draw_image_scaled", mf_draw_image_scaled, 1, 6, -1, { ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    // {"exec_map_update_scripts",   mf_exec_map_update_scripts,   0, 0},
    { "floor2", mf_floor2, 1, 1, 0, { ARG_NUMBER } },
    // {"get_can_rest_on_map",       mf_get_rest_on_map,           2, 2, -1, {ARG_INT, ARG_INT}},
    { "get_combat_free_move", mf_get_combat_free_move, 0, 0 },
    // {"get_current_inven_size",    mf_get_current_inven_size,    1, 1,  0, {ARG_OBJECT}},
    { "get_cursor_mode", mf_get_cursor_mode, 0, 0 },
    { "get_flags", mf_get_flags, 1, 1, 0, { ARG_OBJECT } },
    { "get_ini_config", mf_get_ini_config, 2, 2, 0, { ARG_STRING, ARG_INT } },
    { "get_ini_section", mf_get_ini_section, 2, 2, -1, { ARG_STRING, ARG_STRING } },
    { "get_ini_sections", mf_get_ini_sections, 1, 1, -1, { ARG_STRING } },
    { "get_inven_ap_cost", mf_get_inven_ap_cost, 0, 0 },
    // {"get_map_enter_position",    mf_get_map_enter_position,    0, 0},
    // {"get_metarule_table",        mf_get_metarule_table,        0, 0},
    // {"get_object_ai_data",        mf_get_object_ai_data,        2, 2, -1, {ARG_OBJECT, ARG_INT}},
    { "get_object_data", mf_get_object_data, 2, 2, 0, { ARG_OBJECT, ARG_INT } },
    { "get_outline", mf_get_outline, 1, 1, 0, { ARG_OBJECT } },
    { "get_sfall_arg_at", mf_get_sfall_arg_at, 1, 1, 0, { ARG_INT } },
    // {"get_stat_max",              mf_get_stat_max,              1, 2,  0, {ARG_INT, ARG_INT}},
    // {"get_stat_min",              mf_get_stat_min,              1, 2,  0, {ARG_INT, ARG_INT}},
    // {"get_string_pointer",        mf_get_string_pointer,        1, 1,  0, {ARG_STRING}}, // note: deprecated; do not implement
    // {"get_terrain_name",          mf_get_terrain_name,          0, 2, -1, {ARG_INT, ARG_INT}},
    { "get_text_width", mf_get_text_width, 1, 1, 0, { ARG_STRING } },
    { "get_window_attribute", mf_get_window_attribute, 1, 2, -1, { ARG_INT, ARG_INT } },
    // {"has_fake_perk_npc",         mf_has_fake_perk_npc,         2, 2,  0, {ARG_OBJECT, ARG_STRING}},
    // {"has_fake_trait_npc",        mf_has_fake_trait_npc,        2, 2,  0, {ARG_OBJECT, ARG_STRING}},
    { "hide_window", mf_hide_window, 0, 1, -1, { ARG_STRING } },
    { "interface_art_draw", mf_interface_art_draw, 4, 6, -1, { ARG_INT, ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    // {"interface_overlay",         mf_interface_overlay,         2, 6, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"interface_print",           mf_interface_print,           5, 6, -1, {ARG_STRING, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"intface_hide",              mf_intface_hide,              0, 0},
    // {"intface_is_hidden",         mf_intface_is_hidden,         0, 0},
    { "intface_redraw", mf_intface_redraw, 0, 1 },
    // {"intface_show",              mf_intface_show,              0, 0},
    { "inventory_redraw", mf_inventory_redraw, 0, 1, -1, { ARG_INT } },
    // {"item_make_explosive",       mf_item_make_explosive,       3, 4, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    { "item_weight", mf_item_weight, 1, 1, 0, { ARG_OBJECT } },
    // {"lock_is_jammed",            mf_lock_is_jammed,            1, 1,  0, {ARG_OBJECT}},
    { "loot_obj", mf_loot_obj, 0, 0 },
    { "message_box", mf_message_box, 1, 4, -1, { ARG_STRING, ARG_INT, ARG_INT, ARG_INT } },
    { "metarule_exist", mf_metarule_exist, 1, 1 },
    // {"npc_engine_level_up",       mf_npc_engine_level_up,       1, 1},
    { "obj_is_openable", mf_obj_is_openable, 1, 1, 0, { ARG_OBJECT } },
    { "obj_under_cursor", mf_obj_under_cursor, 2, 2, 0, { ARG_INT, ARG_INT } },
    { "objects_in_radius", mf_objects_in_radius, 3, 4, 0, { ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "outlined_object", mf_outlined_object, 0, 0 },
    { "real_dude_obj", mf_real_dude_obj, 0, 0 },
    { "reg_anim_animate_and_move", mf_reg_anim_animate_and_move, 4, 4, -1, { ARG_OBJECT, ARG_INT, ARG_INT, ARG_INT } },
    // {"remove_timer_event",        mf_remove_timer_event,        0, 1, -1, {ARG_INT}},
    // {"set_spray_settings",        mf_set_spray_settings,        4, 4, -1, {ARG_INT, ARG_INT, ARG_INT, ARG_INT}},
    // {"set_can_rest_on_map",       mf_set_rest_on_map,           3, 3, -1, {ARG_INT, ARG_INT, ARG_INT}},
    // {"set_car_intface_art",       mf_set_car_intface_art,       1, 1, -1, {ARG_INT}},
    { "set_combat_free_move", mf_set_combat_free_move, 1, 1, -1, { ARG_INT } },
    { "set_cursor_mode", mf_set_cursor_mode, 1, 1, -1, { ARG_INT } },
    // {"set_drugs_data",            mf_set_drugs_data,            3, 3, -1, {ARG_INT, ARG_INT, ARG_INT}},
    // {"set_dude_obj",              mf_set_dude_obj,              1, 1, -1, {ARG_INT}},
    // {"set_fake_perk_npc",         mf_set_fake_perk_npc,         5, 5, -1, {ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING}},
    // {"set_fake_trait_npc",        mf_set_fake_trait_npc,        5, 5, -1, {ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING}},
    { "set_flags", mf_set_flags, 2, 2, -1, { ARG_OBJECT, ARG_INT } },
    { "set_iface_tag_text", mf_set_iface_tag_text, 3, 3, -1, { ARG_INT, ARG_STRING, ARG_INT } },
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
    { "set_unique_id", mf_set_unique_id, 1, 2, -1, { ARG_OBJECT, ARG_INT } },
    // {"set_unjam_locks_time",      mf_set_unjam_locks_time,      1, 1, -1, {ARG_INT}},
    { "set_window_flag", mf_set_window_flag, 3, 3, -1, { ARG_INTSTR, ARG_INT, ARG_INT } },
    { "show_window", mf_show_window, 0, 1, -1, { ARG_STRING } },
    { "signal_close_game", mf_signal_close_game, 0, 0 },
    // {"spatial_radius",            mf_spatial_radius,            1, 1,  0, {ARG_OBJECT}},
    { "string_compare", mf_string_compare, 2, 3, 0, { ARG_STRING, ARG_STRING, ARG_INT } },
    { "string_find", mf_string_find, 2, 3, -1, { ARG_STRING, ARG_STRING, ARG_INT } },
    { "string_format", mf_string_format, 2, 8, 0, { ARG_STRING, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY } },
    { "string_to_case", mf_string_to_case, 2, 2, -1, { ARG_STRING, ARG_INT } },
    { "tile_by_position", mf_tile_by_position, 2, 2, -1, { ARG_INT, ARG_INT } },
    { "tile_refresh_display", mf_tile_refresh_display, 0, 0 },
    // {"unjam_lock",                mf_unjam_lock,                1, 1, -1, {ARG_OBJECT}},
    { "unwield_slot", mf_unwield_slot, 2, 2, -1, { ARG_OBJECT, ARG_INT } },
    { "win_fill_color", mf_win_fill_color, 0, 5, -1, { ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "opcode_exists", mf_opcode_exists, 1, 1 },
};
const std::size_t kMetarulesCount = sizeof(kMetarules) / sizeof(kMetarules[0]);

constexpr int kMetarulesMax = sizeof(kMetarules) / sizeof(kMetarules[0]);

enum class InterfaceWindowLookupResult {
    Found,
    Missing,
    Invalid,
};

/*
// Valid window types for get_window_attribute
#define WINTYPE_INVENTORY    (0) // any inventory window (player/loot/use/barter)
#define WINTYPE_DIALOG       (1)
#define WINTYPE_PIPBOY       (2)
#define WINTYPE_WORLDMAP     (3)
#define WINTYPE_IFACEBAR     (4) // the interface bar
#define WINTYPE_CHARACTER    (5)
#define WINTYPE_SKILLDEX     (6)
#define WINTYPE_ESCMENU      (7) // escape menu
#define WINTYPE_AUTOMAP      (8)
*/
static InterfaceWindowLookupResult getInterfaceWindowByType(int winType, int& window)
{
    switch (winType) {
    case 0:
        window = inventoryGetWindow();
        break;
    case 1:
        window = gameDialogGetWindow();
        break;
    case 2:
        window = pipboyGetWindow();
        break;
    case 3:
        window = worldmapGetWindow();
        break;
    case 4:
        window = windowGetWindow(gInterfaceBarWindow) != nullptr ? gInterfaceBarWindow : -1;
        break;
    case 5:
        window = characterEditorGetWindow();
        break;
    case 6:
        window = skilldexGetWindow();
        break;
    case 7:
        window = optionsGetWindow();
        break;
    case 8:
        window = automapGetWindow();
        break;
    default:
        window = -1;
        return InterfaceWindowLookupResult::Invalid;
    }

    return window != -1 ? InterfaceWindowLookupResult::Found : InterfaceWindowLookupResult::Missing;
}

static int getCurrentInterfaceWindow()
{
    int window = -1;
    if (GameMode::isInGameMode(GameMode::kInventory)
        || GameMode::isInGameMode(GameMode::kUseOn)
        || GameMode::isInGameMode(GameMode::kLoot)
        || GameMode::isInGameMode(GameMode::kBarter)) {
        window = inventoryGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kDialog)) {
        window = gameDialogGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kPipboy)) {
        window = pipboyGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kWorldmap)) {
        window = worldmapGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kEditor)) {
        window = characterEditorGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kSkilldex)) {
        window = skilldexGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kOptions)) {
        window = optionsGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kAutomap)) {
        window = automapGetWindow();
    } else if (windowGetWindow(gInterfaceBarWindow) != nullptr) {
        window = gInterfaceBarWindow;
    }

    return window;
}

static bool clampWindowFillRect(int windowWidth, int windowHeight, int& x, int& y, int& width, int& height)
{
    if (x < 0) {
        width += x;
        x = 0;
    }

    if (y < 0) {
        height += y;
        y = 0;
    }

    if (x >= windowWidth || y >= windowHeight) {
        width = 0;
        height = 0;
        return true;
    }

    bool truncated = false;
    if (x + width > windowWidth) {
        width = windowWidth - x;
        truncated = true;
    }

    if (y + height > windowHeight) {
        height = windowHeight - y;
        truncated = true;
    }

    return truncated;
}

static bool applyWindowFlag(int windowId, int bitFlag, bool enabled)
{
    return scriptWindowSetFlag(windowId, bitFlag, enabled);
}

void mf_art_cache_flush(OpcodeContext& ctx)
{
    artCacheFlush();
}

void mf_art_frame_data(OpcodeContext& ctx)
{
    int frame = ctx.numArgs() > 1 ? ctx.arg(1).asInt() : 0;
    int direction = ctx.numArgs() > 2 ? ctx.arg(2).asInt() : 0;

    if (ctx.arg(0).isInt() && ctx.arg(0).asInt() == -1) {
        ctx.setReturn(-1);
        return;
    }

    FrmImage image;
    if (ctx.arg(0).isInt()) {
        int fid = ctx.arg(0).asInt();
        if (!image.lock(fid, frame, direction)) {
            ctx.printError("%s() - cannot load art by FID: %d", ctx.name(), fid);
            ctx.setReturn(-1);
            return;
        }
    } else {
        const char* path = ctx.stringArg(0);
        if (!image.lock(path, frame, direction)) {
            ctx.printError("%s() - cannot load art from file: %s", ctx.name(), path);
            ctx.setReturn(-1);
            return;
        }
    }

    if (image.getWidth() <= 0 || image.getHeight() <= 0) {
        ctx.setReturn(-1);
        return;
    }

    ArrayId arrayId = CreateTempArray(2, 0);
    SetArray(arrayId, ProgramValue(0), ProgramValue(image.getWidth()), false, ctx.program());
    SetArray(arrayId, ProgramValue(1), ProgramValue(image.getHeight()), false, ctx.program());
    ctx.setReturn(ProgramValue(arrayId));
}

void mf_add_iface_tag(OpcodeContext& ctx)
{
    int result = interfaceTagAdd();
    if (result == -1) {
        ctx.printError("%s() - cannot add new tag as the maximum limit has been reached.", ctx.name());
    }
    ctx.setReturn(result);
}

void mf_attack_is_aimed(OpcodeContext& ctx)
{
    int hitMode;
    bool aiming;

    if (interfaceGetCurrentHitMode(&hitMode, &aiming) == -1) {
        ctx.setReturn(0);
    } else {
        ctx.setReturn(aiming ? 1 : 0);
    }
}

void mf_car_gas_amount(OpcodeContext& ctx)
{
    ctx.setReturn(wmCarGasAmount());
}

void mf_combat_data(OpcodeContext& ctx)
{
    if (isInCombat()) {
        ctx.setReturn(combat_get_data());
    } else {
        ctx.setReturn(nullptr);
    }
}

void mf_create_win(OpcodeContext& ctx)
{
    int flags = ctx.numArgs() > 5
        ? ctx.arg(5).asInt()
        : WINDOW_MOVE_ON_TOP;

    int color = (flags & WINDOW_TRANSPARENT) != 0 ? 0 : 256;
    if (scriptWindowCreate(ctx.stringArg(0),
            ctx.arg(1).asInt(),
            ctx.arg(2).asInt(),
            ctx.arg(3).asInt(),
            ctx.arg(4).asInt(),
            color,
            flags)
        == -1) {
        ctx.printError("%s() - couldn't create window.", ctx.name());
        ctx.setReturn(-1);
    }
}

void mf_display_stats(OpcodeContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kInventory)) {
        inventoryDisplayStats();
    } else if (GameMode::isInGameMode(GameMode::kEditor)) {
        characterEditorDisplayStats();
    }
}

void mf_critter_inven_obj2(OpcodeContext& ctx)
{
    Object* obj = ctx.arg(0).asObject();
    int slot = ctx.arg(1).asInt();

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

void mf_dialog_obj(OpcodeContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kDialog)) {
        ctx.setReturn(gGameDialogSpeaker);
    } else {
        ctx.setReturn(nullptr);
    }
}

void mf_dialog_message(OpcodeContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kDialog)
        && !GameMode::isInGameMode(GameMode::kDialogReview)) {
        gameDialogRenderSupplementaryMessage(ctx.stringArg(0));
    }
}

void mf_get_combat_free_move(OpcodeContext& ctx)
{
    ctx.setReturn(_combat_free_move);
}

void mf_get_inven_ap_cost(OpcodeContext& ctx)
{
    ctx.setReturn(inventoryGetInvenApCost());
}

void mf_get_cursor_mode(OpcodeContext& ctx)
{
    ctx.setReturn(gameMouseGetMode());
}

void mf_get_flags(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    ctx.setReturn(object->flags);
}

void mf_get_outline(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    ctx.setReturn(object->outline);
}

void mf_get_sfall_arg_at(OpcodeContext& ctx)
{
    const int argNum = ctx.arg(0).asInt();

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

void mf_get_object_data(OpcodeContext& ctx)
{
    // TODO: only allow to modify a set of whitelisted object types
    // TODO: map offsets to fields to avoid potential alignment, 64bit issues!
    Object* ptr = ctx.arg(0).asObject();
    size_t offset = static_cast<size_t>(ctx.arg(1).asInt());

    if (offset % 4 != 0) {
        programFatalError("mf_get_object_data: bad offset %d", offset);
    }

    int value = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(ptr) + offset);
    ctx.setReturn(value);
}

void mf_get_text_width(OpcodeContext& ctx)
{
    ctx.setReturn(fontGetStringWidth(ctx.stringArg(0)));
}

void mf_get_window_attribute(OpcodeContext& ctx)
{
    int attrType = ctx.numArgs() > 1 ? ctx.arg(1).asInt() : 0;
    int window = -1;
    InterfaceWindowLookupResult lookup = getInterfaceWindowByType(ctx.arg(0).asInt(), window);
    if (lookup == InterfaceWindowLookupResult::Missing) {
        if (attrType != 0) {
            ctx.printError("%s() - failed to get the interface window.", ctx.name());
            ctx.setReturn(-1);
        } else {
            ctx.setReturn(0);
        }
        return;
    }

    if (lookup == InterfaceWindowLookupResult::Invalid) {
        ctx.printError("%s() - invalid window type number.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    Rect rect;
    if (windowGetRect(window, &rect) == -1) {
        ctx.setReturn(-1);
        return;
    }

    switch (attrType) {
    case -1: {
        ArrayId arrayId = CreateTempArray(-1, 0);
        SetArray(arrayId, programMakeString(ctx.program(), "left"), ProgramValue(rect.left), false, ctx.program());
        SetArray(arrayId, programMakeString(ctx.program(), "top"), ProgramValue(rect.top), false, ctx.program());
        SetArray(arrayId, programMakeString(ctx.program(), "right"), ProgramValue(rect.right), false, ctx.program());
        SetArray(arrayId, programMakeString(ctx.program(), "bottom"), ProgramValue(rect.bottom), false, ctx.program());
        ctx.setReturn(ProgramValue(arrayId));
        break;
    }
    case 0: // basically an existence check
        ctx.setReturn(1);
        break;
    case 1:
        ctx.setReturn(rect.left);
        break;
    case 2:
        ctx.setReturn(rect.top);
        break;
    case 3:
        ctx.setReturn(windowGetWidth(window));
        break;
    case 4:
        ctx.setReturn(windowGetHeight(window));
        break;
    case 5:
        ctx.setReturn(window);
        break;
    default:
        ctx.setReturn(0);
        break;
    }
}

static bool loadSfallArtImage(OpcodeContext& ctx, int artArg, int frame, int direction, FrmImage& image, int& fid)
{
    if (ctx.arg(artArg).isInt() && ctx.arg(artArg).asInt() == -1) {
        return false;
    }

    if (ctx.arg(artArg).isInt()) {
        fid = ctx.arg(artArg).asInt();
        int frameDirection = 0;
        int lockFid = fid;
        if (FID_TYPE(fid) == OBJ_TYPE_CRITTER) {
            frameDirection = direction >= 0 ? direction : FID_ROTATION(fid);
            if (direction >= 0) {
                lockFid = (direction << 28) | (fid & 0x0FFFFFFF);
            }
        }

        if (!image.lock(lockFid, frame, frameDirection)) {
            ctx.printError("%s() - cannot load art by FID: %d", ctx.name(), fid);
            return false;
        }
    } else {
        const char* path = ctx.stringArg(artArg);
        int frameDirection = direction >= 0 ? direction : 0;
        if (!image.lock(path, frame, frameDirection)) {
            ctx.printError("%s() - cannot load art from file: %s", ctx.name(), path);
            return false;
        }
    }

    return true;
}

static int drawSfallArtImage(OpcodeContext& ctx, int window, FrmImage& image, int x, int y, int width, int height, bool transparent, bool refresh)
{
    int windowWidth = windowGetWidth(window);
    int windowHeight = windowGetHeight(window);
    if (windowWidth <= 0 || windowHeight <= 0 || windowGetBuffer(window) == nullptr) {
        ctx.printError("%s() - no created or selected window.", ctx.name());
        return 0;
    }

    if (width <= 0 || height <= 0) {
        return 0;
    }

    // sfall clamps interface_art_draw left/top coordinates instead of clipping
    // the source image, but for draw_image does not clamp.  It's best to clamp
    // instead of potentially writing out of bounds, but if we're regularly
    // calling this with negative x/y we should implement intelligent clipping instead.
    if (x < 0) {
        x = 0;
    }

    if (y < 0) {
        y = 0;
    }

    if (x + width > windowWidth || y + height > windowHeight) {
        ctx.printError("%s() - attempt to draw beyond window bounds (%d, %d)", ctx.name(), windowWidth, windowHeight);
        return -1;
    }

    unsigned char* dest = windowGetBuffer(window) + windowWidth * y + x;
    if (width != image.getWidth() || height != image.getHeight()) {
        if (transparent) {
            blitBufferToBufferStretchTrans(image.getData(), image.getWidth(), image.getHeight(), image.getWidth(), dest, width, height, windowWidth);
        } else {
            blitBufferToBufferStretch(image.getData(), image.getWidth(), image.getHeight(), image.getWidth(), dest, width, height, windowWidth);
        }
    } else if (transparent) {
        blitBufferToBufferTrans(image.getData(), image.getWidth(), image.getHeight(), image.getWidth(), dest, windowWidth);
    } else {
        blitBufferToBuffer(image.getData(), image.getWidth(), image.getHeight(), image.getWidth(), dest, windowWidth);
    }

    if (refresh) {
        Rect rect = { x, y, x + width - 1, y + height - 1 };
        windowRefreshRect(window, &rect);
    }

    return 1;
}

static int drawSfallImageToScriptWindow(OpcodeContext& ctx, bool scaled)
{
    int window = scriptWindowGetWindow(ctx.program()->windowId);
    if (window == -1) {
        ctx.printError("%s() - no created or selected window.", ctx.name());
        return 0;
    }

    int frame = ctx.numArgs() > 1 ? ctx.arg(1).asInt() : 0;

    FrmImage image;
    int fid = -1;
    if (!loadSfallArtImage(ctx, 0, frame, -1, image, fid)) {
        return -1;
    }

    if (scaled && ctx.numArgs() < 3) {
        return drawSfallArtImage(ctx, window, image, 0, 0, windowGetWidth(window), windowGetHeight(window), false, true);
    }

    int x = ctx.numArgs() > 2 ? ctx.arg(2).asInt() : 0;
    int y = ctx.numArgs() > 3 ? ctx.arg(3).asInt() : 0;
    int width = image.getWidth();
    int height = image.getHeight();
    bool transparent = true;

    if (scaled) {
        if (ctx.numArgs() > 4) {
            width = ctx.arg(4).asInt();
            height = ctx.numArgs() > 5 ? ctx.arg(5).asInt() : -1;
        }

        if (width <= -1 && height > 0) {
            width = height * image.getWidth() / image.getHeight();
        } else if (height <= -1 && width > 0) {
            height = width * image.getHeight() / image.getWidth();
        }
    } else {
        x += image.getXOffset();
        y += image.getYOffset();
        transparent = ctx.numArgs() > 4 ? ctx.arg(4).asInt() == 0 : true;
    }

    return drawSfallArtImage(ctx, window, image, x, y, width, height, transparent, true);
}

static void mf_draw_image(OpcodeContext& ctx)
{
    ctx.setReturn(drawSfallImageToScriptWindow(ctx, false));
}

static void mf_draw_image_scaled(OpcodeContext& ctx)
{
    ctx.setReturn(drawSfallImageToScriptWindow(ctx, true));
}

static void mf_interface_art_draw(OpcodeContext& ctx)
{
    int window = -1;
    InterfaceWindowLookupResult lookup = getInterfaceWindowByType(ctx.arg(0).asInt() & 0xFF, window);
    if (lookup != InterfaceWindowLookupResult::Found) {
        ctx.printError("%s() - the game interface window is not created or invalid window type number.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    int frame = ctx.numArgs() > 4 ? ctx.arg(4).asInt() : 0;
    int direction = -1;
    int scaledWidth = -1;
    int scaledHeight = -1;
    if (ctx.numArgs() > 5) {
        int arrayId = ctx.arg(5).asInt();
        if (ArrayExists(arrayId)) {
            direction = GetArray(arrayId, ProgramValue(0), ctx.program()).asInt();

            int arrayLength = LenArray(arrayId);
            if (arrayLength > 1) {
                scaledWidth = GetArray(arrayId, ProgramValue(1), ctx.program()).asInt();
            }

            if (arrayLength > 2) {
                scaledHeight = GetArray(arrayId, ProgramValue(2), ctx.program()).asInt();
            }
        }
    }

    FrmImage image;
    int fid = -1;
    if (!loadSfallArtImage(ctx, 1, frame, direction, image, fid)) {
        ctx.setReturn(-1);
        return;
    }

    int xOffset = 0;
    int yOffset = 0;
    if (ctx.arg(1).isInt() && FID_TYPE(fid) == OBJ_TYPE_CRITTER && direction >= 0) {
        xOffset = image.getXOffset();
        yOffset = image.getYOffset();
    }

    int x = std::max(ctx.arg(2).asInt() + xOffset, 0);
    int y = std::max(ctx.arg(3).asInt() + yOffset, 0);

    int width = scaledWidth >= 0 ? scaledWidth : image.getWidth();
    int height = scaledHeight >= 0 ? scaledHeight : image.getHeight();
    ctx.setReturn(drawSfallArtImage(ctx, window, image, x, y, width, height, true, (ctx.arg(0).asInt() & 0x1000000) == 0));
}

void mf_intface_redraw(OpcodeContext& ctx)
{
    if (ctx.numArgs() == 0) {
        interfaceBarRefresh();
    } else {
        // TODO: Incomplete.
        programFatalError("mf_intface_redraw: not implemented");
    }
}

void mf_inventory_redraw(OpcodeContext& ctx)
{
    inventoryRedraw(ctx.numArgs() > 0 ? ctx.arg(0).asInt() : -1);
}

void mf_item_weight(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    if (PID_TYPE(object->pid) != OBJ_TYPE_ITEM) {
        ctx.printError("%s() - expected item object.", ctx.name());
        ctx.setReturn(0);
        return;
    }

    ctx.setReturn(itemGetWeight(object));
}

void mf_loot_obj(OpcodeContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kLoot)) {
        ctx.setReturn(inventoryGetTargetObject());
    } else {
        ctx.setReturn(nullptr);
    }
}

void mf_metarule_exist(OpcodeContext& ctx)
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

void mf_add_extra_msg_file(OpcodeContext& ctx)
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

void mf_opcode_exists(OpcodeContext& ctx)
{
    int opcode = ctx.arg(0).asInt();
    int opcodeIndex = opcode & 0x3FFF;
    if (opcodeIndex < 0 || opcodeIndex >= OPCODE_MAX_COUNT) {
        ctx.setReturn(0);
        return;
    }
    auto opcodeHandler = gInterpreterOpcodeHandlers[opcodeIndex];
    int opcodeExists = opcodeHandler != nullptr ? 1 : 0;
    ctx.setReturn(opcodeExists);
}

void mf_obj_under_cursor(OpcodeContext& ctx)
{
    int onlyCritter = ctx.arg(0).asInt();
    int includeDude = ctx.arg(1).asInt();

    Object* object = gameMouseGetObjectUnderCursor(onlyCritter ? OBJ_TYPE_CRITTER : -1, includeDude, gElevation);

    ctx.setReturn(object);
}

void mf_obj_is_openable(OpcodeContext& ctx)
{
    ctx.setReturn(objectIsOpenable(ctx.arg(0).asObject()));
}

static int objectsInRadiusFirstTile(int sourceTile, int radius, int* endTile)
{
    int hexRadius = HEX_GRID_WIDTH * (radius + 1);

    *endTile = std::min(sourceTile + hexRadius + 1, HEX_GRID_SIZE);
    return std::max(sourceTile - hexRadius, 0);
}

static void mf_objects_in_radius(OpcodeContext& ctx)
{
    int sourceTile = ctx.arg(0).asInt();
    int radius = std::clamp(ctx.arg(1).asInt(), 0, 50);
    int elevation = std::clamp(ctx.arg(2).asInt(), 0, ELEVATION_COUNT - 1);
    int type = ctx.numArgs() > 3 ? ctx.arg(3).asInt() : -1;

    ArrayId arrayId = CreateTempArray(0, 0);

    if (!hexGridTileIsValid(sourceTile)) {
        ctx.setReturn(arrayId);
        return;
    }

    int index = 0;
    int endTile;
    for (int tile = objectsInRadiusFirstTile(sourceTile, radius, &endTile); tile < endTile; tile++) {
        for (Object* object = objectFindFirstAtLocation(elevation, tile); object != nullptr; object = objectFindNextAtLocation()) {
            if (type != -1 && (object->pid == -1 || PID_TYPE(object->pid) != type)) {
                continue;
            }

            int extraRange = (object->flags & OBJECT_MULTIHEX) != 0 ? 1 : 0;
            if (tileDistanceBetween(sourceTile, object->tile) > radius + extraRange) {
                continue;
            }

            ResizeArray(arrayId, index + 1);
            SetArray(arrayId, ProgramValue(index), ProgramValue(object), false, ctx.program());
            index++;
        }
    }

    ctx.setReturn(arrayId);
}

void mf_outlined_object(OpcodeContext& ctx)
{
    ctx.setReturn(gmouse_get_outlined_object());
}

void mf_real_dude_obj(OpcodeContext& ctx)
{
    ctx.setReturn(gDude);
}

void mf_set_combat_free_move(OpcodeContext& ctx)
{
    int value = ctx.arg(0).asInt();
    if (value < 0) {
        value = 0;
    }

    _combat_free_move = value;

    if (isInCombat() && combat_get_data()->attacker == gDude) {
        interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
    }
}

void mf_set_cursor_mode(OpcodeContext& ctx)
{
    int mode = ctx.arg(0).asInt();
    gameMouseSetMode(mode);
}

void mf_set_flags(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    int flags = ctx.arg(1).asInt();

    object->flags = flags;
}

void mf_set_iface_tag_text(OpcodeContext& ctx)
{
    int boxTag = ctx.arg(0).asInt();

    if (boxTag > 4 && boxTag <= interfaceTagGetMax()) {
        interfaceTagSetText(boxTag, ctx.stringArg(1), ctx.arg(2).asInt());
    } else {
        ctx.printError("%s() - tag value must be in the range of 5 to %d.", ctx.name(), interfaceTagGetMax());
        ctx.setReturn(-1);
    }
}

void mf_set_outline(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    int outline = ctx.arg(1).asInt();
    object->outline = outline;
}

void mf_set_window_flag(OpcodeContext& ctx)
{
    int bitFlag = ctx.arg(1).asInt();
    switch (bitFlag) {
    case WINDOW_DONT_MOVE_TOP:
    case WINDOW_MOVE_ON_TOP:
    case WINDOW_HIDDEN:
    case WINDOW_MODAL:
    case WINDOW_TRANSPARENT:
        break;
    default:
        return;
    }

    bool enabled = ctx.arg(2).asInt() != 0;
    if (ctx.arg(0).isString()) {
        const char* windowName = ctx.stringArg(0);
        if (!scriptWindowSetNamedFlag(windowName, bitFlag, enabled)) {
            ctx.printError("%s() - window '%s' is not found.", ctx.name(), windowName);
        }
        return;
    }

    int windowId = ctx.arg(0).asInt();
    if (windowId <= 0) {
        windowId = getCurrentInterfaceWindow();
    }

    if (windowId == -1) {
        return;
    }

    applyWindowFlag(windowId, bitFlag, enabled);
}

void mf_set_unique_id(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    if (ctx.numArgs() > 1 && ctx.arg(1).asInt() == -1) {
        // unassign unique_id only if it has one
        if (object->id > OBJECT_ID_UNIQUE_START) {
            object->id = scriptsNewObjectId();
            scriptsSyncObjectId(object);
        }
        ctx.setReturn(object->id);
        return;
    }

    ctx.setReturn(scriptsSetUniqueObjectId(object));
}

void mf_show_window(OpcodeContext& ctx)
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

void mf_hide_window(OpcodeContext& ctx)
{
    if (ctx.numArgs() == 0) {
        scriptWindowHide();
    } else {
        const char* windowName = ctx.stringArg(0);
        if (!scriptWindowHideNamed(windowName)) {
            ctx.printError("%s() - window '%s' is not found.", ctx.name(), windowName);
        }
    }
}

void mf_win_fill_color(OpcodeContext& ctx)
{
    int window = scriptWindowGetWindow(ctx.program()->windowId);
    if (window == -1) {
        ctx.printError("%s() - no created or selected window.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    int windowWidth = windowGetWidth(window);
    int windowHeight = windowGetHeight(window);
    if (ctx.numArgs() == 0) {
        windowFill(window, 0, 0, windowWidth, windowHeight, 0);
        windowRefresh(window);
        return;
    }

    if (ctx.numArgs() != 5) {
        ctx.printError("%s() - invalid number of arguments (%d), must be 0 or 5.", ctx.name(), ctx.numArgs());
        return;
    }

    int x = ctx.arg(0).asInt();
    int y = ctx.arg(1).asInt();
    int width = ctx.arg(2).asInt();
    int height = ctx.arg(3).asInt();
    int color = ctx.arg(4).asInt();
    bool truncated = clampWindowFillRect(windowWidth, windowHeight, x, y, width, height);
    if (width > 0 && height > 0) {
        windowFill(window, x, y, width, height, color);
        Rect rect = { x, y, x + width - 1, y + height - 1 };
        windowRefreshRect(window, &rect);
    }

    if (truncated) {
        ctx.printError("%s() - the fill area is truncated because it exceeds the current window.", ctx.name());
    }
}

void mf_tile_refresh_display(OpcodeContext& ctx)
{
    tileWindowRefresh();
}

void mf_unwield_slot(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    int slot = ctx.arg(1).asInt();

    if (slot < static_cast<int>(InvenSlot::Armor) || slot > static_cast<int>(InvenSlot::LeftHand)) {
        ctx.printError("%s() - slot must be 0, 1, or 2.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        ctx.printError("%s() - the object is not a critter.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    if (inventoryUnwieldSlot(critter, static_cast<InvenSlot>(slot)) == -1) {
        ctx.setReturn(-1);
    }
}

void mf_tile_by_position(OpcodeContext& ctx)
{
    int x = ctx.arg(0).asInt();
    int y = ctx.arg(1).asInt();
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

void mf_signal_close_game(OpcodeContext&)
{
    mainMenuRequestExit();
    _game_user_wants_to_quit = GAME_QUIT_REQUEST_MAIN_MENU;
}

void mf_string_compare(OpcodeContext& ctx)
{
    // compare str1 to str3 case insensitively
    // if args == 3, use FalloutStringCompare
    const char* str1 = ctx.stringArg(0);
    const char* str2 = ctx.stringArg(1);
    int codePage = 0;
    if (ctx.numArgs() == 3) {
        codePage = ctx.arg(2).asInt();
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

void mf_string_find(OpcodeContext& ctx)
{
    const char* str = ctx.stringArg(0);
    const char* substr = ctx.stringArg(1);
    int startPos = 0;

    if (ctx.numArgs() == 3) {
        startPos = ctx.arg(2).asInt();
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

void mf_string_to_case(OpcodeContext& ctx)
{
    auto buf = ctx.stringArg(0);
    std::string s(buf);
    auto caseType = ctx.arg(1).asInt();
    if (caseType == 1) {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    } else if (caseType == 0) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    } else {
        debugPrint("string_to_case: invalid case type %d", caseType);
    }
    ctx.setReturn(s.c_str());
}

void mf_string_format(OpcodeContext& ctx)
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
                    : arg.isString() ? snprintf(outBuf, bufCount, newFmt.get(), arg.asString(ctx.program()))
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

void mf_floor2(OpcodeContext& ctx)
{
    ctx.setReturn(static_cast<int>(floor(ctx.arg(0).asFloat())));
}

// message_box
void mf_message_box(OpcodeContext& ctx)
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
        int flagParam = ctx.arg(1).asInt();
        if (flagParam != -1) {
            flags = flagParam;
        }
    }

    // note: most of the CE code uses colorTable indices, but this metarule expects palette values.
    // Default: yellow (145) = _colorTable[32328]
    int color1 = _colorTable[32328], color2 = _colorTable[32328];
    if (ctx.numArgs() > 2) {
        color1 = ctx.arg(2).asInt();
    }
    if (ctx.numArgs() > 3) {
        color2 = ctx.arg(3).asInt();
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
    // TODO: make OpcodeContext handle the stack.  This will be easier once it is used for all opcodes.
    static ProgramValue values[METARULE_MAX_ARGS];

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

    OpcodeContext ctx(program, metaruleInfo, args, values);
    if (!ctx.validateArguments()) {
        ctx.setReturn(metaruleInfo->errorReturn);
        ctx.pushReturnValue();
        return;
    }

    metaruleInfo->handler(ctx);
    ctx.pushReturnValue();
}

} // namespace fallout
