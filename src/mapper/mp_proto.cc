#include "mapper/mp_proto.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "combat_ai.h"
#include "critter.h"
#include "debug.h"
#include "object.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "mapper/mp_scrpt.h"
#include "mapper/mp_targt.h"
#include "memory.h"
#include "proto.h"
#include "scripts.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

#define CRITTER_FLAG_COUNT 10
#define SUBDATA_ROWS_PER_COLUMN 9

#define YES 0
#define NO 1

static int proto_choose_container_flags(Proto* proto);
static int proto_subdata_setup_int_button(const char* title, int key, int value, int min_value, int max_value, int* y, int itemIndex);
static int proto_subdata_setup_fid_button(const char* title, int key, int fid, int* y, int itemIndex);
static int proto_subdata_setup_pid_button(const char* title, int key, int pid, int* y, int itemIndex);
static void proto_subdata_setup_str_button(const char* title, int key, int value, const char* const* strs, int count, int* y, int itemIndex);
static void proto_subdata_setup_x_str_button(const char* title, int key, int value, const char* const* strs, int base, int upper, int* y, int itemIndex);
static void proto_subdata_process_int_button(const char* title, int* value, int min, int max, int itemIndex);
static void proto_subdata_process_str_button(const char* title, int* value, const char* const* strs, int count, int itemIndex);
static void proto_subdata_process_x_str_button(const char* title, int* value, const char* const* strs, int base, int upper, int itemIndex);
static void proto_subdata_process_fid_button(int* fidPtr, int itemIndex);
static void proto_subdata_process_pid_button(const char* title, int* pidPtr, int itemIndex);
static void proto_item_subdata_edit_init(Proto* proto);
static int proto_item_subdata_edit_exit();
static void proto_item_subdata_edit_handle(Proto* proto, int key);
static const char* const* mp_critter_stats_strs();
static void proto_critter_flags_redraw(int win, int pid);
static int proto_critter_flags_modify(int pid);
static int mp_pick_kill_type();
void proto_action_redraw(int win, int pid);
static void reg_text_int(int win, int y, int key, const char* title, int value, int* rowY, bool advance);
static void reg_text_str(int win, int key, const char* title, const char* value, int* rowY);
static void reg_text_int_update(int win, int* value, int min, int max, const char* title, int textY);
static bool proto_action_can_look_at(int pid);
static void proto_action_modify(int pid);
static void proto_edit_mod_fid(int win, int objectType, int* fidPtr, int width, int delta = 0);
static int proto_edit_mod_pid(int win, Proto** protoPtr, int width, int delta);
static void reg_mod_flags(int* flags, int* extendedFlags, int objectType, int subtype);
static void proto_choose_fid(int* fidPtr, int objectType, int useFidObjectType);
static void proto_choose_pid(int* pidPtr, int objectType, int hasPid, int subtype);

static void proto_change_proto_info(int pid, int mode);
static int proto_item_edit(int pid);
static int proto_critter_edit(int pid);
static int proto_scenery_edit(int pid);
static int proto_wall_edit(int pid);
static int proto_tile_edit(int pid);
static int proto_misc_edit(int pid);

static char kYes[] = "YES";
static char kNo[] = "NO";

// 0x53DAFC
static char default_proto_builder_name[36] = "EVERTS SCOTTY";

// 0x559924
char* proto_builder_name = default_proto_builder_name;

// 0x559B94
static const char* wall_light_strs[] = {
    "North/South",
    "East/West",
    "North Corner",
    "South Corner",
    "East Corner",
    "West Corner",
};

// 0x559C50
static char* yesno[] = {
    kYes,
    kNo,
};

// 0x559C58
int edit_window_color = 1;

// 0x559C60
bool can_modify_protos = false;

// 0x559C68
static int subwin = -1;

// 0x559C6C
static int critFlagList[CRITTER_FLAG_COUNT] = {
    CRITTER_NO_STEAL,
    CRITTER_NO_DROP,
    CRITTER_NO_LIMBS,
    CRITTER_NO_AGE,
    CRITTER_NO_HEAL,
    CRITTER_INVULNERABLE,
    CRITTER_FLAT,
    CRITTER_SPECIAL_DEATH,
    CRITTER_LONG_LIMBS,
    CRITTER_NO_KNOCKBACK,
};

// 0x559C94
static const char* critFlagStrs[CRITTER_FLAG_COUNT] = {
    "_Steal",
    "_Drop",
    "_Limbs",
    "_Ages",
    "_Heal",
    "Invuln.,",
    "_Flattens",
    "Special",
    "Rng",
    "_Knock",
};

// Approximated palette indices for the prototype editor (original color
// bytes are runtime BSS with no faithful offset).
constexpr int kProtoEditNormalColor = 32747; // normal field value text
constexpr int kProtoEditTitleColor = 32767; // bright title text
constexpr int kProtoEditBoxBorderColor = 2; // art preview box border

static const char* proto_ed_title = "Prototype Editor";

// proto_action_strs
static const char* proto_action_strs[5] = {
    "Useable",
    "Useable On",
    "Look",
    "Talk",
    "Pick Up",
};

// sound_code_strs
static const char* sound_code_strs[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z", "!", "@", "#", "$",
    "_"
};

// attack_anim_strs
static const char* attack_anim_strs[] = {
    "stand",
    "throw_punch",
    "kick_leg",
    "swing_anim",
    "thrust_anim",
    "throw_anim",
    "fire_single",
    "fire_burst",
    "fire_continuous",
};

// art_anim_strs
const char* const anim_code_strs[] = {
    "stand",
    "walk",
    "jump_begin",
    "jump_end",
    "climb_ladder",
    "falling",
    "up_stairs_right",
    "up_stairs_left",
    "down_stairs_right",
    "down_stairs_left",
    "magic_hands_ground",
    "magic_hands_middle",
    "magic_hands_up",
    "dodge_anim",
    "hit_from_front",
    "hit_from_back",
    "throw_punch",
    "kick_leg",
    "throw_anim",
    "running",
    "fall_back",
    "fall_front",
    "bad_landing",
    "big_hole",
    "charred_body",
    "chunks_of_flesh",
    "dancing_autofire",
    "electrify",
    "sliced_in_half",
    "burned_to_nothing",
    "electrified_to_nothing",
    "exploded_to_nothing",
    "melted_to_nothing",
    "fire_dance",
    "fall_back_blood",
    "fall_front_blood",
    "prone_to_standing",
    "back_to_standing",
    "take_out",
    "put_away",
    "parry_anim",
    "thrust_anim",
    "swing_anim",
    "point",
    "unpoint",
    "fire_single",
    "fire_burst",
    "fire_continuous",
};

// mp_perk_max
constexpr int kPerkMax = PERK_COUNT;

// Drug-stat picker list: "Drug Stat (Special)", "None", then the stat names.
// Indexed with base -2 by proto_subdata_*_x_str_button.
static const char* mp_critter_stats_list[2 + STAT_COUNT];

// mp_critter_stats_list — lazily filled wrapper over the stat names.
const char* const* mp_critter_stats_strs()
{
    if (mp_critter_stats_list[0] == nullptr) {
        mp_critter_stats_list[0] = "Drug Stat (Special)";
        mp_critter_stats_list[1] = "None";
        for (int i = 0; i < STAT_COUNT; i++) {
            mp_critter_stats_list[2 + i] = statGetName(i);
        }
    }
    return mp_critter_stats_list;
}

// 0x4922F8
void init_mapper_protos()
{
    edit_window_color = _colorTable[10570];
    can_modify_protos = target_overriden();
}

// 0x492840
int proto_choose_container_flags(Proto* proto)
{
    int win = windowCreate(320,
        185,
        220,
        205,
        edit_window_color,
        WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    _win_register_text_button(win,
        10,
        11,
        -1,
        -1,
        -1,
        '1',
        "Magic Hands Grnd",
        0);

    if ((proto->item.data.container.openFlags & 0x1) != 0) {
        windowDrawText(win,
            yesno[YES],
            50,
            125,
            15,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(win,
            yesno[NO],
            50,
            125,
            15,
            _colorTable[32747] | 0x10000);
    }

    _win_register_text_button(win,
        10,
        32,
        -1,
        -1,
        -1,
        '2',
        "Cannot Pick Up",
        0);

    if (_proto_action_can_pickup(proto->pid)) {
        windowDrawText(win,
            yesno[YES],
            50,
            125,
            36,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(win,
            yesno[NO],
            50,
            125,
            36,
            _colorTable[32747] | 0x10000);
    }

    windowDrawBorder(win);
    windowRefresh(win);

    while (1) {
        sharedFpsLimiter.mark();

        int input = inputGetInput();
        if (input == KEY_ESCAPE
            || input == KEY_BAR
            || input == KEY_RETURN) {
            break;
        }

        if (input == '1') {
            proto->item.data.container.openFlags ^= 0x1;

            if ((proto->item.data.container.openFlags & 0x1) != 0) {
                windowDrawText(win,
                    yesno[YES],
                    50,
                    125,
                    15,
                    _colorTable[32747] | 0x10000);
            } else {
                windowDrawText(win,
                    yesno[NO],
                    50,
                    125,
                    15,
                    _colorTable[32747] | 0x10000);
            }

            windowRefresh(win);
        } else if (input == '2') {
            proto->item.extendedFlags ^= PROTO_EXT_FLAG_CAN_PICK_UP;

            if (_proto_action_can_pickup(proto->pid)) {
                windowDrawText(win,
                    yesno[YES],
                    50,
                    125,
                    36,
                    _colorTable[32747] | 0x10000);
            } else {
                windowDrawText(win,
                    yesno[NO],
                    50,
                    125,
                    36,
                    _colorTable[32747] | 0x10000);
            }

            windowRefresh(win);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);

    return 0;
}

// 0x492A3C
int proto_subdata_setup_int_button(const char* title, int key, int value, int min_value, int max_value, int* y, int itemIndex)
{
    char text[36];
    int button_x;
    int value_offset_x;

    button_x = 10;
    value_offset_x = 90;

    if (itemIndex == SUBDATA_ROWS_PER_COLUMN) {
        *y -= 189;
    }

    if (itemIndex >= SUBDATA_ROWS_PER_COLUMN) {
        button_x = 165;
        value_offset_x -= 16;
    }

    _win_register_text_button(subwin,
        button_x,
        *y,
        -1,
        -1,
        -1,
        key,
        title,
        0);

    if (value >= min_value && value < max_value) {
        sprintf(text, "%d", value);
        windowDrawText(subwin,
            text,
            38,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(subwin,
            "<ERROR>",
            38,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[31744] | 0x10000);
    }

    *y += 21;

    return 0;
}

// 0x492B28
int proto_subdata_setup_fid_button(const char* title, int key, int fid, int* y, int itemIndex)
{
    char text[36];
    char* pch;
    int button_x;
    int value_offset_x;

    button_x = 10;
    value_offset_x = 90;

    if (itemIndex == SUBDATA_ROWS_PER_COLUMN) {
        *y -= 189;
    }

    if (itemIndex >= SUBDATA_ROWS_PER_COLUMN) {
        button_x = 165;
        value_offset_x -= 16;
    }

    _win_register_text_button(subwin,
        button_x,
        *y,
        -1,
        -1,
        -1,
        key,
        title,
        0);

    if (art_list_str(fid, text) != -1) {
        pch = strchr(text, '.');
        if (pch != nullptr) {
            *pch = '\0';
        }

        windowDrawText(subwin,
            text,
            80,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(subwin,
            "None",
            80,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[992] | 0x10000);
    }

    *y += 21;

    return 0;
}

// 0x492C20
int proto_subdata_setup_pid_button(const char* title, int key, int pid, int* y, int itemIndex)
{
    int button_x;
    int value_offset_x;

    button_x = 10;
    value_offset_x = 90;

    if (itemIndex == SUBDATA_ROWS_PER_COLUMN) {
        *y -= 189;
    }

    if (itemIndex >= SUBDATA_ROWS_PER_COLUMN) {
        button_x = 165;
        value_offset_x = 74;
    }

    _win_register_text_button(subwin,
        button_x,
        *y,
        -1,
        -1,
        -1,
        key,
        title,
        0);

    if (pid != -1) {
        windowDrawText(subwin,
            protoGetName(pid),
            49,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[32747] | 0x10000);
    } else {
        windowDrawText(subwin,
            "None",
            49,
            button_x + value_offset_x,
            *y + 4,
            _colorTable[992] | 0x10000);
    }

    *y += 21;

    return 0;
}

// 0x492CF0
void proto_subdata_setup_str_button(const char* title, int key, int value, const char* const* strs, int count, int* y, int itemIndex)
{
    proto_subdata_setup_x_str_button(title, key, value, strs, 0, count, y, itemIndex);
}

// 0x492DFC
void proto_subdata_setup_x_str_button(const char* title, int key, int value, const char* const* strs, int base, int upper, int* y, int itemIndex)
{
    int textWidth = 135;
    int button_x = 10;
    int value_offset_x = 90;

    if (itemIndex == SUBDATA_ROWS_PER_COLUMN) {
        *y -= 189;
    }

    if (itemIndex > 8) {
        textWidth = 110;
        button_x = 165;
        value_offset_x = 74;
    }

    _win_register_text_button(subwin, button_x, *y, -1, -1, -1, key, title, 0);

    const char* text;
    int color;
    if (value >= base && value < upper) {
        text = strs[value - base];
        color = _colorTable[32747];
    } else if (value == -1) {
        text = "None";
        color = _colorTable[992];
    } else {
        text = "<ERROR>";
        color = _colorTable[31744];
    }

    windowDrawText(subwin, text, textWidth, button_x + value_offset_x, *y + 4, color | 0x10000);

    *y += 21;
}

// 0x49397C
void proto_subdata_process_int_button(const char* title, int* value, int min, int max, int itemIndex)
{
    int button_x = 10;
    int value_offset_x = 90;

    if (itemIndex > 8) {
        button_x = 165;
        itemIndex -= 9;
        value_offset_x = 74;
    }

    int num = *value;
    if (win_get_num_i(&num, min, max, false, title, 100, 100) != -1) {
        char text[36];
        snprintf(text, sizeof(text), "%d", num);
        windowDrawText(subwin, text, 38, button_x + value_offset_x, 21 * itemIndex + 15, _colorTable[32747] | 0x10000);
        *value = num;
    }
}

// 0x493C14
void proto_subdata_process_str_button(const char* title, int* value, const char* const* strs, int count, int itemIndex)
{
    proto_subdata_process_x_str_button(title, value, strs, 0, count, itemIndex);
}

// 0x493CD8
void proto_subdata_process_x_str_button(const char* title, int* value, const char* const* strs, int base, int upper, int itemIndex)
{
    int value_offset_x = 90;
    int textWidth = 135;
    int button_x = 10;

    if (itemIndex > 8) {
        button_x = 165;
        itemIndex -= 9;
        textWidth = 110;
        value_offset_x = 74;
    }

    int sel = _win_list_select(title, strs, upper - base, nullptr, 340, 200, kProtoEditNormalColor | 0x10000);
    if (sel != -1) {
        windowDrawText(subwin, strs[sel], textWidth, value_offset_x + button_x, 21 * itemIndex + 15, _colorTable[32747] | 0x10000);
        *value = base + sel;
    }
}

// Resolves an art name without its extension, falling back to "None".
static void art_name_no_ext(int fid, char* buf)
{
    if (art_list_str(fid, buf) == -1) {
        strcpy(buf, "None");
    } else {
        char* pch = strchr(buf, '.');
        if (pch != nullptr) {
            *pch = '\0';
        }
    }
}

// 0x493A40
void proto_subdata_process_fid_button(int* fidPtr, int itemIndex)
{
    int button_x = 10;
    int value_offset_x = 90;

    if (itemIndex > 8) {
        itemIndex -= 9;
        value_offset_x = 74;
        button_x = 165;
    }

    int fid = *fidPtr;
    proto_choose_fid(&fid, 1, 1);

    char text[36];
    art_name_no_ext(fid, text);
    windowDrawText(subwin, text, 80, value_offset_x + button_x, 21 * itemIndex + 15, _colorTable[32747] | 0x10000);
    *fidPtr = fid;
}

// 0x493B3C
void proto_subdata_process_pid_button(const char* title, int* pidPtr, int itemIndex)
{
    int chooseSubtype = 4;
    int button_x = 10;
    int value_offset_x = 90;
    int chooseObjectType = 0;

    if (itemIndex > 8) {
        itemIndex -= 9;
        value_offset_x = 74;
        button_x = 165;
    }

    int pid = *pidPtr;
    if (strcmp(title, "Projectile Pid:") == 0) {
        chooseSubtype = -1;
        chooseObjectType = 5;
    }

    proto_choose_pid(&pid, chooseObjectType, 1, chooseSubtype);
    if (pid != -1) {
        windowDrawText(subwin, protoGetName(pid), 49, value_offset_x + button_x, 21 * itemIndex + 15, _colorTable[32747] | 0x10000);
        *pidPtr = pid;
    }
}

// 0x493954
int proto_item_subdata_edit_exit()
{
    if (subwin == -1) {
        return -1;
    }
    windowDestroy(subwin);
    subwin = -1;
    return 0;
}

// 0x492F34
void proto_item_subdata_edit_init(Proto* proto)
{
    if (subwin != -1) {
        proto_item_subdata_edit_exit();
    }

    subwin = windowCreate(280, 175, 360, 205, edit_window_color, WINDOW_MOVE_ON_TOP);
    if (subwin == -1) {
        return;
    }

    windowDrawBorder(subwin);

    char text[36];
    int y = 11;

    switch (proto->item.type) {
    case ITEM_TYPE_ARMOR: {
        proto_subdata_setup_int_button("AC", '1', proto->item.data.armor.armorClass, -120, 120, &y, 0);

        _win_register_text_button(subwin, 10, y, -1, -1, -1, '2', "Dmg Resists", 0);
        int labelY = y;
        int valueX = 100;
        for (int i = 0; i < 7; i++) {
            snprintf(text, sizeof(text), "%d", proto->item.data.armor.damageResistance[i]);
            windowDrawText(subwin, text, 26, valueX, y + 4, kProtoEditNormalColor | 0x10000);
            windowDrawText(subwin, gDamageTypeNames[i], 35, valueX - 6, labelY + 14, kProtoEditNormalColor);
            valueX += 37;
        }

        y += 21;
        _win_register_text_button(subwin, 10, y, -1, -1, -1, '3', "Dmg Threshs", 0);
        valueX = 100;
        for (int i = 0; i < 7; i++) {
            snprintf(text, sizeof(text), "%d", proto->item.data.armor.damageThreshold[i]);
            windowDrawText(subwin, text, 26, valueX, y + 4, kProtoEditNormalColor | 0x10000);
            valueX += 37;
        }

        y += 21;
        proto_subdata_setup_x_str_button("Perk", '4', proto->item.data.armor.perk, _mp_perk_code_strs, -1, kPerkMax, &y, 0);
        proto_subdata_setup_fid_button("Male Fid", '5', proto->item.data.armor.maleFid, &y, 4);
        proto_subdata_setup_fid_button("Female Fid", '6', proto->item.data.armor.femaleFid, &y, 5);
        break;
    }
    case ITEM_TYPE_CONTAINER:
        proto_subdata_setup_int_button("Max Size", '1', proto->item.data.container.maxSize, 0, 15000, &y, 0);
        proto_subdata_setup_int_button("Open Flags", '2', proto->item.data.container.openFlags, 0, 0xFFFFFF, &y, 0);
        break;
    case ITEM_TYPE_DRUG: {
        const char* const* stats = mp_critter_stats_strs();
        proto_subdata_setup_x_str_button("Stat 0", '1', proto->item.data.drug.stat[0], stats, -2, 38, &y, 0);
        proto_subdata_setup_int_button("Amount 1", '3', proto->item.data.drug.amount1[0], -500, 500, &y, 2);
        proto_subdata_setup_x_str_button("Stat 1", '5', proto->item.data.drug.stat[1], stats, -2, 38, &y, 4);
        proto_subdata_setup_int_button("Amount 1", '7', proto->item.data.drug.amount1[1], -500, 500, &y, 6);
        proto_subdata_setup_x_str_button("Stat 2", '9', proto->item.data.drug.stat[2], stats, -2, 38, &y, 8);
        proto_subdata_setup_int_button("Amount 1", '!', proto->item.data.drug.amount1[2], -500, 500, &y, 10);
        proto_subdata_setup_int_button("Duration 1", '@', proto->item.data.drug.duration1, 0, 32000, &y, 1);
        proto_subdata_setup_int_button("Duration 2", '#', proto->item.data.drug.duration2, 0, 32000, &y, 3);
        proto_subdata_setup_int_button("Addiction", '$', proto->item.data.drug.addictionChance, 0, 100, &y, 5);
        proto_subdata_setup_int_button("Amount 0", '2', proto->item.data.drug.amount[0], -500, 500, &y, 7);
        proto_subdata_setup_int_button("Amount 2", '4', proto->item.data.drug.amount2[0], -500, 500, &y, 9);
        proto_subdata_setup_int_button("Amount 0", '6', proto->item.data.drug.amount[1], -500, 500, &y, 11);
        proto_subdata_setup_int_button("Amount 2", '8', proto->item.data.drug.amount2[1], -500, 500, &y, 13);
        proto_subdata_setup_int_button("Amount 0", '%', proto->item.data.drug.amount[2], -500, 500, &y, 15);
        proto_subdata_setup_int_button("Amount 2", '^', proto->item.data.drug.amount2[2], -500, 500, &y, 17);
        proto_subdata_setup_x_str_button("W. Effect", '&', proto->item.data.drug.withdrawalEffect, _mp_perk_code_strs, -1, kPerkMax, &y, 14);
        proto_subdata_setup_int_button("W. Onset", '*', proto->item.data.drug.withdrawalOnset, -32000, 32000, &y, 16);
        break;
    }
    case ITEM_TYPE_WEAPON: {
        _win_register_text_button(subwin, 10, y, -1, -1, -1, '1', "Flags", 0);
        windowDrawText(subwin, (proto->item.extendedFlags & PROTO_EXT_FLAG_IS_TWO_HANDED) != 0 ? "2Hnd" : "1Hnd", 30, 100, y + 4, kProtoEditNormalColor | 0x10000);
        y += 21;
        proto_subdata_setup_str_button("Anim Code", '2', proto->item.data.weapon.animationCode, anim_code_strs, 11, &y, 1);
        proto_subdata_setup_int_button("Min Dmg", '3', proto->item.data.weapon.minDamage, 0, 32000, &y, 2);
        proto_subdata_setup_int_button("Max Dmg", '4', proto->item.data.weapon.maxDamage, 0, 32000, &y, 3);
        proto_subdata_setup_str_button("Dmg Type", '5', proto->item.data.weapon.damageType, gDamageTypeNames, 7, &y, 4);
        proto_subdata_setup_int_button("Max Range 1", '6', proto->item.data.weapon.maxRange1, 0, 32000, &y, 5);
        proto_subdata_setup_int_button("Max Range 2", '7', proto->item.data.weapon.maxRange2, 0, 32000, &y, 6);
        proto_subdata_setup_pid_button("Proj Pid", '8', proto->item.data.weapon.projectilePid, &y, 7);
        proto_subdata_setup_int_button("Min ST", '9', proto->item.data.weapon.minStrength, 0, 10, &y, 8);
        proto_subdata_setup_int_button("MP Cost 1", '!', proto->item.data.weapon.actionPointCost1, 0, 100, &y, 9);
        proto_subdata_setup_int_button("MP Cost 2", '@', proto->item.data.weapon.actionPointCost2, 0, 100, &y, 10);
        proto_subdata_setup_int_button("Crit Fail", '#', proto->item.data.weapon.criticalFailureType, 0, 100, &y, 11);
        proto_subdata_setup_x_str_button("Perk", '$', proto->item.data.weapon.perk, _mp_perk_code_strs, -1, kPerkMax, &y, 12);
        proto_subdata_setup_int_button("Rounds", '%', proto->item.data.weapon.rounds, 0, 32000, &y, 13);
        proto_subdata_setup_str_button("Caliber", '^', proto->item.data.weapon.caliber, gCaliberTypeNames, 19, &y, 14);
        proto_subdata_setup_pid_button("Ammo Pid", '&', proto->item.data.weapon.ammoTypePid, &y, 15);
        proto_subdata_setup_int_button("Max Ammo", '*', proto->item.data.weapon.ammoCapacity, 0, 32000, &y, 16);
        _win_register_text_button(subwin, 165, y, -1, -1, -1, '(', "Sound ID", 0);
        if (proto->item.data.weapon.soundCode != 0) {
            snprintf(text, sizeof(text), "%c", proto->item.data.weapon.soundCode);
            windowDrawText(subwin, text, 25, 239, y + 4, kProtoEditNormalColor | 0x10000);
        } else {
            windowDrawText(subwin, "None", 25, 239, y + 4, _colorTable[31744] | 0x10000);
        }
        break;
    }
    case ITEM_TYPE_AMMO:
        proto_subdata_setup_str_button("Caliber", '1', proto->item.data.ammo.caliber, gCaliberTypeNames, 19, &y, 0);
        proto_subdata_setup_int_button("Quantity", '2', proto->item.data.ammo.quantity, 0, 32000, &y, 0);
        proto_subdata_setup_int_button("AC Adjust", '3', proto->item.data.ammo.armorClassModifier, -250, 250, &y, 0);
        proto_subdata_setup_int_button("DR Adjust", '4', proto->item.data.ammo.damageResistanceModifier, -250, 250, &y, 0);
        proto_subdata_setup_int_button("Dam Mult", '5', proto->item.data.ammo.damageMultiplier, 1, 250, &y, 0);
        proto_subdata_setup_int_button("Dam Div", '6', proto->item.data.ammo.damageDivisor, 1, 250, &y, 0);
        break;
    case ITEM_TYPE_MISC:
        proto_subdata_setup_pid_button("Power Pid", '1', proto->item.data.misc.powerTypePid, &y, 0);
        proto_subdata_setup_str_button("Power Type", '2', proto->item.data.misc.powerType, gCaliberTypeNames, 19, &y, 0);
        proto_subdata_setup_int_button("Charges", '3', proto->item.data.misc.charges, 0, 32000, &y, 0);
        break;
    case ITEM_TYPE_KEY:
        break;
    default:
        return;
    }

    windowRefresh(subwin);
}

// 0x493DFC
void proto_item_subdata_edit_handle(Proto* proto, int key)
{
    char text[36];

    switch (proto->item.type) {
    case ITEM_TYPE_ARMOR:
        switch (key) {
        case '1':
            proto_subdata_process_int_button("AC:", &proto->item.data.armor.armorClass, -120, 120, 0);
            break;
        case '2': {
            int rowY = 100;
            for (int i = 0; i < 7; i++) {
                snprintf(text, sizeof(text), "%s Damage Resistance", gDamageTypeNames[i]);
                int num = proto->item.data.armor.damageResistance[i];
                if (win_get_num_i(&num, -999, 999, false, text, 100, 100) == -1) {
                    break;
                }
                snprintf(text, sizeof(text), "%d", num);
                windowDrawText(subwin, text, 26, rowY, 36, kProtoEditNormalColor | 0x10000);
                windowRefresh(subwin);
                proto->item.data.armor.damageResistance[i] = num;
                rowY += 37;
            }
            break;
        }
        case '3': {
            int rowY = 100;
            for (int i = 0; i < 7; i++) {
                snprintf(text, sizeof(text), "%s Damage Threshold", gDamageTypeNames[i]);
                int num = proto->item.data.armor.damageThreshold[i];
                if (win_get_num_i(&num, -999, 999, false, text, 100, 100) == -1) {
                    break;
                }
                snprintf(text, sizeof(text), "%d", num);
                windowDrawText(subwin, text, 26, rowY, 57, kProtoEditNormalColor | 0x10000);
                windowRefresh(subwin);
                proto->item.data.armor.damageThreshold[i] = num;
                rowY += 37;
            }
            break;
        }
        case '4':
            proto_subdata_process_x_str_button("Perk:", &proto->item.data.armor.perk, _mp_perk_code_strs, -1, kPerkMax, 3);
            break;
        case '5':
            proto_subdata_process_fid_button(&proto->item.data.armor.maleFid, 4);
            break;
        case '6':
            proto_subdata_process_fid_button(&proto->item.data.armor.femaleFid, 5);
            break;
        default:
            return;
        }
        break;
    case ITEM_TYPE_CONTAINER:
        if (key == '1') {
            proto_subdata_process_int_button("Max Size:", &proto->item.data.container.maxSize, 0, 15000, 0);
        } else if (key == '2') {
            proto_choose_container_flags(proto);
            snprintf(text, sizeof(text), "%d", proto->item.data.container.openFlags);
            windowDrawText(subwin, text, 50, 100, 36, kProtoEditNormalColor | 0x10000);
        } else {
            return;
        }
        break;
    case ITEM_TYPE_DRUG: {
        const char* const* stats = mp_critter_stats_strs();
        switch (key) {
        case '1':
            proto_subdata_process_x_str_button("Stat 0:", &proto->item.data.drug.stat[0], stats, -2, 38, 0);
            break;
        case '3':
            proto_subdata_process_int_button("Amount 1:", &proto->item.data.drug.amount1[0], -500, 500, 1);
            break;
        case '5':
            proto_subdata_process_x_str_button("Stat 1:", &proto->item.data.drug.stat[1], stats, -2, 38, 2);
            break;
        case '7':
            proto_subdata_process_int_button("Amount 1:", &proto->item.data.drug.amount1[1], -500, 500, 3);
            break;
        case '9':
            proto_subdata_process_x_str_button("Stat 2:", &proto->item.data.drug.stat[2], stats, -2, 38, 4);
            break;
        case '!':
            proto_subdata_process_int_button("Amount 1:", &proto->item.data.drug.amount1[2], -500, 500, 5);
            break;
        case '@':
            proto_subdata_process_int_button("Duration 1:", &proto->item.data.drug.duration1, 0, 32000, 6);
            break;
        case '#':
            proto_subdata_process_int_button("Duration 2:", &proto->item.data.drug.duration2, 0, 32000, 7);
            break;
        case '$':
            proto_subdata_process_int_button("Addiction:", &proto->item.data.drug.addictionChance, 0, 100, 8);
            break;
        case '2':
            proto_subdata_process_int_button("Amount 0:", &proto->item.data.drug.amount[0], -500, 500, 9);
            break;
        case '4':
            proto_subdata_process_int_button("Amount 2:", &proto->item.data.drug.amount2[0], -500, 500, 10);
            break;
        case '6':
            proto_subdata_process_int_button("Amount 0:", &proto->item.data.drug.amount[1], -500, 500, 11);
            break;
        case '8':
            proto_subdata_process_int_button("Amount 2:", &proto->item.data.drug.amount2[1], -500, 500, 12);
            break;
        case '%':
            proto_subdata_process_int_button("Amount 0:", &proto->item.data.drug.amount[2], -500, 500, 13);
            break;
        case '^':
            proto_subdata_process_int_button("Amount 2:", &proto->item.data.drug.amount2[2], -500, 500, 14);
            break;
        case '&':
            proto_subdata_process_x_str_button("Withdrawal Effect:", &proto->item.data.drug.withdrawalEffect, _mp_perk_code_strs, -1, kPerkMax, 15);
            break;
        case '*':
            proto_subdata_process_int_button("Withdrawal Onset:", &proto->item.data.drug.withdrawalOnset, -32000, 32000, 16);
            break;
        default:
            return;
        }
        break;
    }
    case ITEM_TYPE_WEAPON:
        switch (key) {
        case '1': {
            proto->item.extendedFlags ^= PROTO_EXT_FLAG_IS_TWO_HANDED;
            const char* label = (proto->item.extendedFlags & PROTO_EXT_FLAG_IS_TWO_HANDED) != 0 ? "2Hnd" : "1Hnd";
            windowDrawText(subwin, label, 50, 100, 15, kProtoEditNormalColor | 0x10000);
            break;
        }
        case '2':
            proto_subdata_process_str_button("Animation Code:", &proto->item.data.weapon.animationCode, anim_code_strs, 11, 1);
            break;
        case '3':
            proto_subdata_process_int_button("Minimum Damage:", &proto->item.data.weapon.minDamage, 0, 32000, 2);
            break;
        case '4':
            proto_subdata_process_int_button("Maximum Damage:", &proto->item.data.weapon.maxDamage, 0, 32000, 3);
            break;
        case '5':
            proto_subdata_process_str_button("Damage Type:", &proto->item.data.weapon.damageType, gDamageTypeNames, 7, 4);
            break;
        case '6':
            proto_subdata_process_int_button("Max Range 1:", &proto->item.data.weapon.maxRange1, 0, 32000, 5);
            break;
        case '7':
            proto_subdata_process_int_button("Max Range 2:", &proto->item.data.weapon.maxRange2, 0, 32000, 6);
            break;
        case '8':
            proto_subdata_process_pid_button("Projectile Pid:", &proto->item.data.weapon.projectilePid, 7);
            break;
        case '9':
            proto_subdata_process_int_button("Minimum Strength Required:", &proto->item.data.weapon.minStrength, 0, 10, 8);
            break;
        case '!':
            proto_subdata_process_int_button("Movement Point Cost 1:", &proto->item.data.weapon.actionPointCost1, 0, 100, 9);
            break;
        case '@':
            proto_subdata_process_int_button("Movement Point Cost 2:", &proto->item.data.weapon.actionPointCost2, 0, 100, 10);
            break;
        case '#':
            proto_subdata_process_int_button("Critical Failure Table:", &proto->item.data.weapon.criticalFailureType, 0, 100, 11);
            break;
        case '$':
            proto_subdata_process_x_str_button("Perk:", &proto->item.data.weapon.perk, _mp_perk_code_strs, -1, kPerkMax, 12);
            break;
        case '%':
            proto_subdata_process_int_button("Rounds:", &proto->item.data.weapon.rounds, 0, 32000, 13);
            break;
        case '^':
            proto_subdata_process_str_button("Weapon Caliber:", &proto->item.data.weapon.caliber, gCaliberTypeNames, 19, 14);
            break;
        case '&':
            proto_subdata_process_pid_button("Ammo Pid:", &proto->item.data.weapon.ammoTypePid, 15);
            break;
        case '*':
            proto_subdata_process_int_button("Maximum Ammo:", &proto->item.data.weapon.ammoCapacity, 0, 32000, 16);
            break;
        case '(': {
            int sel = _win_list_select("Pick Sound ID Code:", sound_code_strs, 41, nullptr, 340, 200, kProtoEditNormalColor | 0x10000);
            if (sel != -1) {
                proto->item.data.weapon.soundCode = sound_code_strs[sel][0];
                snprintf(text, sizeof(text), "%c", proto->item.data.weapon.soundCode);
                windowDrawText(subwin, text, 25, 239, 183, kProtoEditNormalColor | 0x10000);
            }
            break;
        }
        default:
            return;
        }
        break;
    case ITEM_TYPE_AMMO:
        switch (key) {
        case '1':
            proto_subdata_process_str_button("Caliber:", &proto->item.data.ammo.caliber, gCaliberTypeNames, 19, 0);
            break;
        case '2':
            proto_subdata_process_int_button("Quantity:", &proto->item.data.ammo.quantity, 0, 32000, 1);
            break;
        case '3':
            proto_subdata_process_int_button("AC Adjust:", &proto->item.data.ammo.armorClassModifier, -250, 250, 2);
            break;
        case '4':
            proto_subdata_process_int_button("DR Adjust:", &proto->item.data.ammo.damageResistanceModifier, -250, 250, 3);
            break;
        case '5':
            proto_subdata_process_int_button("Damage Multiplier:", &proto->item.data.ammo.damageMultiplier, 1, 250, 4);
            break;
        case '6':
            proto_subdata_process_int_button("Damage Divisor:", &proto->item.data.ammo.damageDivisor, 1, 250, 5);
            break;
        default:
            return;
        }
        break;
    case ITEM_TYPE_MISC:
        if (key == '1') {
            proto_subdata_process_pid_button("Power Type Pid:", &proto->item.data.misc.powerTypePid, 0);
        } else if (key == '2') {
            proto_subdata_process_str_button("Power Type:", &proto->item.data.misc.powerType, gCaliberTypeNames, 19, 1);
        } else if (key == '3') {
            proto_subdata_process_int_button("Charges:", &proto->item.data.misc.charges, 0, 32000, 2);
        } else {
            return;
        }
        break;
    case ITEM_TYPE_KEY:
        break;
    default:
        return;
    }

    windowRefresh(subwin);
}

// 0x495438
const char* proto_wall_light_str(int flags)
{
    if ((flags & PROTO_EXT_FLAG_HIDDEN) != 0) {
        return wall_light_strs[1];
    }

    if ((flags & PROTO_EXT_FLAG_NORTH_CORNER) != 0) {
        return wall_light_strs[2];
    }

    if ((flags & PROTO_EXT_FLAG_SOUTH_CORNER) != 0) {
        return wall_light_strs[3];
    }

    if ((flags & PROTO_EXT_FLAG_EAST_CORNER) != 0) {
        return wall_light_strs[4];
    }

    if ((flags & PROTO_EXT_FLAG_WEST_CORNER) != 0) {
        return wall_light_strs[5];
    }

    return wall_light_strs[0];
}

// proto_edit_init
int proto_edit_init(int* outWin, int pid, Proto** outProto, unsigned char** outImgPos, int width)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return -1;
    }
    *outProto = proto;

    int winWidth = _scr_size.right - _scr_size.left + 1;
    int winHeight = _scr_size.bottom - _scr_size.top - 99;

    int type = PID_TYPE(pid);
    int flags = type != 0 ? WINDOW_MOVE_ON_TOP : 0;
    int win = windowCreate(0, 0, winWidth, winHeight, edit_window_color, flags);
    if (win == -1) {
        return -1;
    }
    *outWin = win;

    windowDrawBorder(win);

    int titleWidth = fontGetStringWidth(proto_ed_title);
    windowDrawText(win, proto_ed_title, titleWidth, (width - titleWidth) / 2, 18, kProtoEditTitleColor | FONT_SHADOW);

    char text[80];
    strcpy(text, artGetObjectTypeName(type));
    text[0] = toupper(static_cast<unsigned char>(text[0]));
    int typeWidth = fontGetStringWidth(text);
    windowDrawText(win, text, typeWidth, (width - typeWidth) / 2, 28, kProtoEditTitleColor | FONT_SHADOW);

    unsigned char* buf = windowGetBuffer(win);
    unsigned char* imgPos = buf + width * 26 - 130;
    *outImgPos = imgPos;
    bufferDrawRect(imgPos - width - 1, width, 0, 0, 101, 101, kProtoEditBoxBorderColor);
    bufferFill(imgPos, 100, 100, width, edit_window_color);

    int fid = proto->fid;
    if (artExists(fid)) {
        artRender(fid, imgPos, 100, 100, width);
        if (art_list_str(fid, text) == -1) {
            strcpy(text, "None");
        } else {
            char* pch = strchr(text, '.');
            if (pch != nullptr) {
                *pch = '\0';
            }
        }
        windowDrawText(win, text, 80, width - 110, 130, kProtoEditNormalColor | FONT_SHADOW);
    }

    _win_register_text_button(win, width - 115, 150, -1, -1, KEY_BRACKET_LEFT, -1, "<<", 0);
    _win_register_text_button(win, width - 85, 150, -1, -1, KEY_BRACKET_RIGHT, -1, ">>", 0);
    _win_register_text_button(win, 15, 15, -1, -1, KEY_MINUS, -1, "(*", 0);
    _win_register_text_button(win, 40, 15, -1, -1, KEY_EQUAL, -1, "*)", 0);

    _win_register_text_button(win, 10, 44, -1, -1, -1, KEY_LOWERCASE_N, "Name", 0);
    windowDrawText(win, protoGetName(proto->pid), 130, 90, 48, kProtoEditNormalColor | FONT_SHADOW);

    _win_register_text_button(win, 10, 65, -1, -1, -1, KEY_LOWERCASE_D, "Description", 0);
    windowDrawText(win, protoGetDescription(pid), 400, 90, 67, kProtoEditNormalColor | FONT_SHADOW);

    if (type != OBJ_TYPE_TILE) {
        _win_register_text_button(win, 150, 86, -1, -1, -1, KEY_UPPERCASE_S, "Scripts", 0);
        _win_register_text_button(win, 170, 107, -1, -1, -1, KEY_LOWERCASE_F, "Flags", 0);

        if (type == OBJ_TYPE_WALL || type == OBJ_TYPE_SCENERY) {
            windowDrawText(win, proto_wall_light_str(proto->extendedFlags), 60, 235, 131, kProtoEditNormalColor | FONT_SHADOW);
        }

        _win_register_text_button(win, 260, 107, -1, -1, -1, KEY_UPPERCASE_F, "Flags-Ext", 0);

        if (type == OBJ_TYPE_SCENERY && proto->scenery.type == 0) {
            windowDrawText(win, yesno[(proto->scenery.data.door.openFlags & 0x04) != 0 ? YES : NO], 60, 375, 131, kProtoEditNormalColor | FONT_SHADOW);
            _win_register_text_button(win, 360, 107, -1, -1, -1, KEY_UPPERCASE_W, "WalkThru", 0);
        }
    }

    if (type < OBJ_TYPE_TILE) {
        _win_register_text_button(win, 120, 149, -1, -1, -1, KEY_UPPERCASE_U, "Action Flags", 0);
        proto_action_redraw(win, pid);
    }

    _win_register_text_button(win, 10, 317, -1, -1, -1, KEY_BAR, "Done", 0);
    _win_register_text_button(win, 110, 317, -1, -1, -1, KEY_ESCAPE, "Cancel", 0);

    return 0;
}

// reg_text_int
void reg_text_int(int win, int y, int key, const char* title, int value, int* rowY, bool advance)
{
    char text[48];
    _win_register_text_button(win, y - 80, *rowY, -1, -1, -1, key, title, 0);
    snprintf(text, sizeof(text), "%d", value);
    windowDrawText(win, text, 50, y, *rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
    if (advance) {
        *rowY += 21;
    }
}

// reg_text_str
void reg_text_str(int win, int key, const char* title, const char* value, int* rowY)
{
    _win_register_text_button(win, 10, *rowY, -1, -1, -1, key, title, 0);
    int titleWidth = fontGetStringWidth(title);
    int overflow = titleWidth > 60 ? titleWidth - 60 : 0;
    windowDrawText(win, value, 128 - overflow, overflow + 90, *rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
    *rowY += 21;
}

// reg_text_int_update
void reg_text_int_update(int win, int* value, int min, int max, const char* title, int textY)
{
    int num = *value;
    if (win_get_num_i(&num, min, max, false, title, 100, 100) == -1) {
        return;
    }

    *value = num;

    char text[36];
    snprintf(text, sizeof(text), "%d", num);
    windowDrawText(win, text, 130, 90, textY, kProtoEditNormalColor | FONT_SHADOW);
}

// 0x4960B8
void proto_critter_flags_redraw(int win, int pid)
{
    int index;
    int color;
    int x = 110;

    for (index = 0; index < CRITTER_FLAG_COUNT; index++) {
        if (critterFlagCheck(pid, critFlagList[index])) {
            color = _colorTable[992];
        } else {
            color = _colorTable[10570];
        }

        windowDrawText(win, critFlagStrs[index], 44, x, 195, color | 0x10000);
        x += 48;
    }
}

// 0x496120
int proto_critter_flags_modify(int pid)
{
    Proto* proto;
    int rc;
    int flags = 0;
    int index;

    if (protoGetProto(pid, &proto) == -1) {
        return -1;
    }

    rc = win_yes_no("Can't be stolen from?", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_STEAL;
    }

    rc = win_yes_no("Can't Drop items?", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_DROP;
    }

    rc = win_yes_no("Can't lose limbs?", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_LIMBS;
    }

    rc = win_yes_no("Dead Bodies Can't Age?", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_AGE;
    }

    rc = win_yes_no("Can't Heal by Aging?", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_HEAL;
    }

    rc = win_yes_no("Is Invlunerable????", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_INVULNERABLE;
    }

    rc = win_yes_no("Can't Flatten on Death?", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_FLAT;
    }

    rc = win_yes_no("Has Special Death?", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_SPECIAL_DEATH;
    }

    rc = win_yes_no("Has Extra Hand-To-Hand Range?", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_LONG_LIMBS;
    }

    rc = win_yes_no("Can't be knocked back?", 340, 200, _colorTable[32747] | 0x10000);
    if (rc == -1) {
        return -1;
    }

    if (rc == 1) {
        flags |= CRITTER_NO_KNOCKBACK;
    }

    if (!can_modify_protos) {
        win_timed_msg("Can't modify protos!", _colorTable[31744] | 0x10000);
        return -1;
    }

    for (index = 0; index < CRITTER_FLAG_COUNT; index++) {
        if ((critFlagList[index] & flags) != 0) {
            critterFlagSet(pid, critFlagList[index]);
        } else {
            critterFlagUnset(pid, critFlagList[index]);
        }
    }

    return 0;
}

// 0x497520
int mp_pick_kill_type()
{
    char* names[KILL_TYPE_COUNT];
    int index;

    for (index = 0; index < KILL_TYPE_COUNT; index++) {
        names[index] = killTypeGetName(index);
    }

    return _win_list_select("Kill Type",
        names,
        KILL_TYPE_COUNT,
        nullptr,
        50,
        100,
        _colorTable[32747] | 0x10000);
}

// 0x497568
int proto_pick_ai_packet(int* value)
{
    int count;
    char** names;
    int index;
    int rc;

    count = combat_ai_num();
    if (count <= 0) {
        return -1;
    }

    names = (char**)internal_malloc(sizeof(char*) * count);
    for (index = 0; index < count; index++) {
        names[index] = (char*)internal_malloc(strlen(combat_ai_name(index)) + 1);
        strcpy(names[index], combat_ai_name(index));
    }

    rc = _win_list_select("AI Packet",
        names,
        count,
        nullptr,
        50,
        100,
        _colorTable[32747] | 0x10000);
    if (rc != -1) {
        *value = rc;
    }

    for (index = 0; index < count; index++) {
        internal_free(names[index]);
    }

    internal_free(names);
    return 0;
}

// proto_action_can_look_at
bool proto_action_can_look_at(int pid)
{
    return true;
}

// reg_mod_flags
void reg_mod_flags(int* flags, int* extendedFlags, int objectType, int subtype)
{
    int savedFlags = *flags;
    int savedExtended = *extendedFlags;
    auto* flagBytes = reinterpret_cast<unsigned char*>(flags);
    auto* extBytes = reinterpret_cast<unsigned char*>(extendedFlags);

    bool escaped = false;
    bool rebuild = true;

    while (rebuild) {
        rebuild = false;
        int rowY = 21;
        int win = windowCreate(160, 110, 220, 350, edit_window_color, WINDOW_MOVE_ON_TOP);
        if (win == -1) {
            return;
        }

        reg_text_str(win, '1', "Flat", yesno[(*flags & OBJECT_FLAT) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '2', "NoBlock", yesno[(*flags & OBJECT_NO_BLOCK) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '3', "MultiHex", yesno[(*flags & OBJECT_MULTIHEX) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '4', "Trans None", yesno[(*flags & OBJECT_TRANS_NONE) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '5', "Trans Wall", yesno[(*flags & OBJECT_TRANS_WALL) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '6', "Trans Glass", yesno[(*flags & OBJECT_TRANS_GLASS) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '7', "Trans Steam", yesno[(*flags & OBJECT_TRANS_STEAM) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '8', "Trans Energy", yesno[(*flags & OBJECT_TRANS_ENERGY) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '9', "Trans Red", yesno[(*flags & OBJECT_TRANS_RED) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '0', "Shoot Thru", yesno[(*flags & OBJECT_SHOOT_THRU) != 0 ? YES : NO], &rowY);
        reg_text_str(win, '-', "Light Thru", yesno[(*flags & OBJECT_LIGHT_THRU) != 0 ? YES : NO], &rowY);

        if (objectType == OBJ_TYPE_WALL || objectType == OBJ_TYPE_SCENERY) {
            if (subtype) {
                rowY += 21;
            } else {
                reg_text_str(win, '=', "Wall Light Type", proto_wall_light_str(*extendedFlags), &rowY);
            }
            reg_text_str(win, '_', "Wall Trans End", yesno[(*flags & OBJECT_WALL_TRANS_END) != 0 ? YES : NO], &rowY);
        }

        if (objectType == OBJ_TYPE_ITEM) {
            reg_text_str(win, '!', "No Highlight", yesno[(*flags & OBJECT_NO_HIGHLIGHT) != 0 ? YES : NO], &rowY);
            if (!subtype) {
                reg_text_str(win, '@', "Hidden Item", yesno[(*extendedFlags & PROTO_EXT_FLAG_HIDDEN) != 0 ? YES : NO], &rowY);
            }
        }

        _win_register_text_button(win, 10, 320, -1, -1, -1, KEY_BAR, "Done", 0);
        windowDrawBorder(win);
        windowRefresh(win);

        int keyCode = 0;
        while (true) {
            sharedFpsLimiter.mark();

            keyCode = inputGetInput();
            if (keyCode == KEY_ESCAPE || keyCode == KEY_BAR) {
                break;
            }

            if (keyCode == KEY_RETURN) {
                keyCode = KEY_BAR;
                break;
            }

            int textX = 90;
            int textY = 0;
            int valueIndex = -1;
            int color = kProtoEditNormalColor | FONT_SHADOW;

            switch (keyCode) {
            case '1':
                *flags ^= OBJECT_FLAT;
                textY = 25;
                valueIndex = (*flags & OBJECT_FLAT) != 0 ? YES : NO;
                break;
            case '2':
                *flags ^= OBJECT_NO_BLOCK;
                textY = 46;
                valueIndex = (*flags & OBJECT_NO_BLOCK) != 0 ? YES : NO;
                break;
            case '3':
                flagBytes[1] ^= 8;
                textY = 67;
                valueIndex = (flagBytes[1] & 8) != 0 ? YES : NO;
                break;
            case '4':
                *flags &= 0xFFF0BFFF;
                flagBytes[1] ^= 0x80;
                textY = 88;
                valueIndex = (flagBytes[1] & 0x80) != 0 ? YES : NO;
                break;
            case '5':
                *flags &= 0xFFF13FFF;
                flagBytes[2] ^= 1;
                textY = 109;
                valueIndex = (flagBytes[2] & 1) != 0 ? YES : NO;
                break;
            case '6':
                *flags &= 0xFFF23FFF;
                flagBytes[2] ^= 2;
                textY = 130;
                valueIndex = (flagBytes[2] & 2) != 0 ? YES : NO;
                break;
            case '7':
                *flags &= 0xFFF43FFF;
                flagBytes[2] ^= 4;
                textY = 151;
                valueIndex = (flagBytes[2] & 4) != 0 ? YES : NO;
                break;
            case '8':
                *flags &= 0xFFF83FFF;
                flagBytes[2] ^= 8;
                textY = 172;
                valueIndex = (flagBytes[2] & 8) != 0 ? YES : NO;
                break;
            case '9':
                *flags &= 0xFFF07FFF;
                flagBytes[1] ^= 0x40;
                textY = 193;
                valueIndex = (flagBytes[1] & 0x40) != 0 ? YES : NO;
                break;
            case '0':
                flagBytes[3] ^= 0x80;
                textY = 214;
                valueIndex = (flagBytes[3] & 0x80) != 0 ? YES : NO;
                break;
            case '-':
                flagBytes[3] ^= 0x20;
                textY = 235;
                valueIndex = (flagBytes[3] & 0x20) != 0 ? YES : NO;
                break;
            case '!':
                flagBytes[1] ^= 0x10;
                textX = 93;
                textY = 256;
                valueIndex = (flagBytes[1] & 0x10) != 0 ? YES : NO;
                break;
            case '_':
                flagBytes[3] ^= 0x10;
                textX = 108;
                textY = 277;
                valueIndex = (flagBytes[3] & 0x10) != 0 ? YES : NO;
                break;
            case '@':
                if (!subtype) {
                    extBytes[3] ^= 8;
                    textY = 277;
                    valueIndex = (extBytes[3] & 8) != 0 ? YES : NO;
                }
                break;
            case '=':
                if (!subtype && (objectType == OBJ_TYPE_WALL || objectType == OBJ_TYPE_SCENERY)) {
                    unsigned char b = extBytes[3];
                    if ((b & 8) != 0) {
                        extBytes[3] = (b & 0xE7) | 0x10;
                    } else if ((b & 0x10) != 0) {
                        extBytes[3] = (b & 0xCF) | 0x20;
                    } else if ((b & 0x20) != 0) {
                        extBytes[3] = (b & 0x9F) | 0x40;
                    } else if ((b & 0x40) != 0) {
                        extBytes[3] = (b & 0x3F) | 0x80;
                    } else if ((b & 0x80) == 0) {
                        extBytes[3] = b | 8;
                    } else {
                        extBytes[3] = b & 0x7F;
                    }
                    windowDrawText(win, proto_wall_light_str(*extendedFlags), 80, 110, 256, color);
                    windowRefresh(win);
                }
                break;
            default:
                break;
            }

            if (valueIndex != -1) {
                windowDrawText(win, yesno[valueIndex], 40, textX, textY, color);
                windowRefresh(win);
            }

            // Trans-flag selections are radio-style: rebuild the dialog to
            // refresh every sibling row.
            if (keyCode >= '4' && keyCode <= '8') {
                rebuild = true;
                break;
            }

            renderPresent();
            sharedFpsLimiter.throttle();
        }

        windowDestroy(win);

        if (keyCode == KEY_ESCAPE) {
            escaped = true;
        }
    }

    if (escaped) {
        *flags = savedFlags;
        *extendedFlags = savedExtended;
    }
}

// proto_action_redraw
void proto_action_redraw(int win, int pid)
{
    int colors[5];
    colors[0] = _proto_action_can_use(pid) ? kProtoEditTitleColor : kProtoEditNormalColor;
    colors[1] = _proto_action_can_use_on(pid) ? kProtoEditTitleColor : kProtoEditNormalColor;
    colors[2] = proto_action_can_look_at(pid) ? kProtoEditTitleColor : kProtoEditNormalColor;
    colors[3] = _proto_action_can_talk_to(pid) ? kProtoEditTitleColor : kProtoEditNormalColor;
    colors[4] = _proto_action_can_pickup(pid) ? kProtoEditTitleColor : kProtoEditNormalColor;

    int x = 210;
    for (int index = 0; index < 5; index++) {
        windowDrawText(win, proto_action_strs[index], 34, x, 153, colors[index] | FONT_SHADOW);
        x += 38;
    }
}

// proto_action_modify
void proto_action_modify(int pid)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return;
    }

    int flags = 0;
    int type = PID_TYPE(pid);

    if (type == OBJ_TYPE_ITEM || type == OBJ_TYPE_SCENERY || type == OBJ_TYPE_WALL) {
        int rc = win_yes_no("Useable?", 340, 200, kProtoEditNormalColor);
        if (rc == -1) {
            return;
        }
        if (rc == 1) {
            flags |= PROTO_EXT_FLAG_CAN_USE;
        }

        rc = win_yes_no("Useable On Something?", 340, 200, kProtoEditNormalColor);
        if (rc == -1) {
            return;
        }
        if (rc == 1) {
            flags |= PROTO_EXT_FLAG_CAN_USE_ON;
        }
    }

    int rc = win_yes_no("Can be looked at?", 340, 200, kProtoEditNormalColor);
    if (rc == -1) {
        return;
    }
    if (rc == 1) {
        flags |= PROTO_EXT_FLAG_LOOK;
    }

    if (type == OBJ_TYPE_CRITTER) {
        rc = win_yes_no("Can be talked to?", 340, 200, kProtoEditNormalColor);
        if (rc == -1) {
            return;
        }
        if (rc == 1) {
            flags |= PROTO_EXT_FLAG_CAN_TALK_TO;
        }
    }

    if (type == OBJ_TYPE_ITEM) {
        rc = win_yes_no("Can it be picked up?", 340, 200, kProtoEditNormalColor);
        if (rc == -1) {
            return;
        }
        if (rc == 1) {
            flags |= PROTO_EXT_FLAG_CAN_PICK_UP;
        }
    }

    if (!can_modify_protos) {
        win_timed_msg("Can't modify protos!", _colorTable[31744] | 0x10000);
        return;
    }

    int mask = PROTO_EXT_FLAG_CAN_USE | PROTO_EXT_FLAG_CAN_USE_ON | PROTO_EXT_FLAG_LOOK
        | PROTO_EXT_FLAG_CAN_TALK_TO | PROTO_EXT_FLAG_CAN_PICK_UP;
    proto->item.extendedFlags = (proto->item.extendedFlags & ~mask) | (flags & mask);
}

// proto_edit_mod_fid
void proto_edit_mod_fid(int win, int objectType, int* fidPtr, int width, int delta)
{
    int id = (*fidPtr & 0xFFF) + delta;
    if (id < 0) {
        id = 0;
    }

    int fid;
    while (true) {
        fid = buildFid(objectType, id, (*fidPtr & 0xFF0000) >> 16, 0, 0);
        if (artExists(fid)) {
            break;
        }
        if (--id < 0) {
            break;
        }
    }

    if (artExists(fid)) {
        unsigned char* buf = windowGetBuffer(win);
        unsigned char* imgPos = buf + width * 26 - 130;
        bufferFill(imgPos, 100, 100, width, edit_window_color);
        *fidPtr = fid;
        artRender(fid, imgPos, 100, 100, width);

        Rect rect = { width - 130, 25, width - 30, 125 };
        windowRefreshRect(win, &rect);

        char text[80];
        if (art_list_str(fid, text) != -1) {
            char* pch = strchr(text, '.');
            if (pch != nullptr) {
                *pch = '\0';
            }
            windowDrawText(win, text, 80, width - 110, 130, kProtoEditNormalColor | FONT_SHADOW);

            rect.left = width - 130;
            rect.top = 130;
            rect.right = width - 50;
            rect.bottom = fontGetLineHeight() + 130;
            windowRefreshRect(win, &rect);
        }
    } else if (!artExists(*fidPtr)) {
        *fidPtr = buildFid(objectType, 0, 0, 0, 0);
    }
}

// proto_edit_mod_pid
int proto_edit_mod_pid(int win, Proto** protoPtr, int width, int delta)
{
    Proto* proto = *protoPtr;
    int id = (proto->pid & 0xFFFFFF) + delta;
    if (id == 0) {
        return -1;
    }

    if (id >= proto_max_id(PID_TYPE(proto->pid))) {
        return -1;
    }

    int pid = id | (PID_TYPE(proto->pid) << 24);
    Proto* newProto;
    if (protoGetProto(pid, &newProto) == -1) {
        return -1;
    }

    *protoPtr = newProto;

    unsigned char* buf = windowGetBuffer(win);
    unsigned char* imgPos = buf + width * 26 - 130;
    bufferFill(imgPos, 100, 100, width, edit_window_color);
    artRender(newProto->fid, imgPos, 100, 100, width);

    Rect rect = { width - 130, 25, width - 30, 125 };
    windowRefreshRect(win, &rect);

    char text[80];
    if (_proto_list_str(pid, text) != -1) {
        char* pch = strchr(text, '.');
        if (pch != nullptr) {
            *pch = '\0';
        }
        windowDrawText(win, text, 80, width - 110, 130, kProtoEditNormalColor | FONT_SHADOW);

        rect.left = width - 130;
        rect.top = 130;
        rect.right = width - 50;
        rect.bottom = fontGetLineHeight() + 130;
        windowRefreshRect(win, &rect);
    }

    rect.left = width - 150;
    rect.top = 140;
    rect.right = width - 35;
    rect.bottom = fontGetLineHeight() + 140;
    windowFill(win, rect.left, rect.top, 80, fontGetLineHeight(), kProtoEditNormalColor);
    windowDrawText(win, protoGetName(newProto->pid), 110, rect.left, rect.top, kProtoEditNormalColor | FONT_SHADOW);
    windowRefreshRect(win, &rect);
    return 0;
}

// proto_choose_fid
void proto_choose_fid(int* fidPtr, int objectType, int useFidObjectType)
{
    constexpr int kWinW = 160;
    constexpr int kWinH = 240;

    int win = windowCreate(360, 140, kWinW, kWinH, edit_window_color, WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return;
    }

    windowDrawBorder(win);
    _win_register_text_button(win, 45, 150, -1, -1, KEY_BRACKET_LEFT, -1, "<<", 0);
    _win_register_text_button(win, 75, 150, -1, -1, KEY_BRACKET_RIGHT, -1, ">>", 0);

    unsigned char* buf = windowGetBuffer(win);
    int width = kWinW;

    if (!useFidObjectType && artExists(*fidPtr)) {
        // keep current fid
    } else {
        *fidPtr = buildFid(objectType, 0, 0, 0, 0);
    }

    int fid = *fidPtr;
    bufferDrawRect(buf + width * 26 - 130 - width - 1, width, 0, 0, 101, 101, kProtoEditBoxBorderColor);
    proto_edit_mod_fid(win, objectType, &fid, width);
    _win_register_text_button(win, kWinW - 50, kWinH - 65, -1, -1, -1, KEY_LOWERCASE_N, "None", 0);
    _win_register_text_button(win, 10, kWinH - 40, -1, -1, -1, KEY_BAR, "Done", 0);
    _win_register_text_button(win, kWinW - 60, kWinH - 40, -1, -1, -1, KEY_ESCAPE, "Cancel", 0);
    windowRefresh(win);

    while (true) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        bool done = false;

        if (keyCode == KEY_ESCAPE || keyCode == KEY_BAR) {
            done = true;
        } else if (keyCode == KEY_RETURN) {
            keyCode = KEY_BAR;
            done = true;
        } else if (keyCode == KEY_BRACKET_LEFT) {
            proto_edit_mod_fid(win, objectType, &fid, width, -1);
        } else if (keyCode == KEY_BRACKET_RIGHT) {
            proto_edit_mod_fid(win, objectType, &fid, width, 1);
        } else if (keyCode == KEY_LOWERCASE_N) {
            fid = -1;
            done = true;
        } else if (keyCode == KEY_HOME) {
            proto_edit_mod_fid(win, objectType, &fid, width, -15000);
        } else if (keyCode == KEY_END) {
            proto_edit_mod_fid(win, objectType, &fid, width, art_total(objectType));
        }

        if (done) {
            windowDestroy(win);
            if (keyCode != KEY_ESCAPE) {
                *fidPtr = fid;
            }
            return;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// Steps the chooser in `dir` until a proto of `subtype` is shown or a list
// boundary is hit, then reverses to recover a match if the boundary was hit.
static void proto_step_to_subtype(int win, Proto** protoPtr, int width, int dir, int subtype)
{
    while (proto_edit_mod_pid(win, protoPtr, width, dir) == 0 && !proto_is_subtype(*protoPtr, subtype)) {
    }
    if (!proto_is_subtype(*protoPtr, subtype)) {
        while (proto_edit_mod_pid(win, protoPtr, width, -dir) == 0 && !proto_is_subtype(*protoPtr, subtype)) {
        }
    }
}

// proto_choose_pid
void proto_choose_pid(int* pidPtr, int objectType, int hasPid, int subtype)
{
    constexpr int kWinW = 160;
    constexpr int kWinH = 240;

    Proto* proto;
    if (!hasPid) {
        if (protoGetProto(*pidPtr, &proto) == -1) {
            *pidPtr = (objectType << 24) | 1;
        }
    } else {
        *pidPtr = (objectType << 24) | 1;
    }

    if (protoGetProto(*pidPtr, &proto) == -1) {
        return;
    }

    int win = windowCreate(360, 140, kWinW, kWinH, edit_window_color, WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return;
    }

    windowDrawBorder(win);
    _win_register_text_button(win, 45, 150, -1, -1, KEY_BRACKET_LEFT, -1, "<<", 0);
    _win_register_text_button(win, 75, 150, -1, -1, KEY_BRACKET_RIGHT, -1, ">>", 0);

    unsigned char* buf = windowGetBuffer(win);
    int width = kWinW;

    bufferDrawRect(buf + width * 26 - 130 - width - 1, width, 0, 0, 101, 101, kProtoEditBoxBorderColor);
    proto_edit_mod_pid(win, &proto, width, 0);

    while (proto_edit_mod_pid(win, &proto, width, 1) == 0 && !proto_is_subtype(proto, subtype)) {
    }

    _win_register_text_button(win, 110, kWinH - 65, -1, -1, -1, KEY_LOWERCASE_N, "None", 0);
    _win_register_text_button(win, 10, kWinH - 40, -1, -1, -1, KEY_BAR, "Done", 0);
    _win_register_text_button(win, 100, kWinH - 40, -1, -1, -1, KEY_ESCAPE, "Cancel", 0);
    windowRefresh(win);

    while (true) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        bool done = false;

        if (keyCode == KEY_ESCAPE || keyCode == KEY_BAR) {
            done = true;
        } else if (keyCode == KEY_RETURN) {
            keyCode = KEY_BAR;
            done = true;
        } else if (keyCode == KEY_BRACKET_LEFT) {
            proto_step_to_subtype(win, &proto, width, -1, subtype);
        } else if (keyCode == KEY_BRACKET_RIGHT) {
            proto_step_to_subtype(win, &proto, width, 1, subtype);
        } else if (keyCode == KEY_LOWERCASE_N) {
            proto->pid = -1;
            done = true;
        }

        if (done) {
            windowDestroy(win);
            if (keyCode != KEY_ESCAPE) {
                *pidPtr = proto->pid;
            }
            return;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// 0x49B778
int proto_build_all_type(int type)
{
    // TODO: Incomplete.
    (void)type;
    return 0;
}

int proto_build_all_type_binary(int type)
{
    // TODO: rebuild binary proto list for given object type.
    (void)type;
    return 0;
}

// proto_edit
int protoEdit(int protoId)
{
    int pid = protoId;

    while (true) {
        Proto* proto;
        if (protoGetProto(pid, &proto) == -1) {
            return 0;
        }

        debugPrint("\nproto->fid = %d", proto->fid);

        int type = PID_TYPE(pid);
        int rc;
        switch (type) {
        case OBJ_TYPE_ITEM:
            rc = proto_item_edit(pid);
            break;
        case OBJ_TYPE_CRITTER:
            rc = proto_critter_edit(pid);
            break;
        case OBJ_TYPE_SCENERY:
            rc = proto_scenery_edit(pid);
            break;
        case OBJ_TYPE_WALL:
            rc = proto_wall_edit(pid);
            break;
        case OBJ_TYPE_TILE:
            rc = proto_tile_edit(pid);
            break;
        case OBJ_TYPE_MISC:
            rc = proto_misc_edit(pid);
            break;
        default:
            return 0;
        }

        if (rc == 0) {
            return 0;
        }

        if (rc == -1) {
            proto_remove(pid);
            _proto_load_pid(pid, &proto);
            return 0;
        }

        int id = pid & 0xFFFFFF;
        int typeBits = type << 24;

        if (rc == -2 || rc == -5 || rc == -6) {
            if (!can_modify_protos) {
                win_timed_msg("Not capable of modifying prototypes from here.", _colorTable[31744] | 0x10000);
                proto_remove(pid);
                _proto_load_pid(pid, &proto);
                return 0;
            }

            if (proto_save_text(pid) == -1) {
                return 0;
            }
            _proto_save_pid(pid);
            proto_header_save();

            if (rc == -2) {
                return 0;
            }
            if (rc == -5) {
                if (id > 1) {
                    pid = typeBits | (id - 1);
                }
            } else if (id < proto_max_id(type) - 1) {
                pid = typeBits | (id + 1);
            }
            continue;
        }

        if (rc == -3) {
            if (id > 1) {
                pid = typeBits | (id - 1);
            }
        } else if (rc == -4) {
            if (id < proto_max_id(type) - 1) {
                pid = typeBits | (id + 1);
            }
        }
    }
}

void rebuild_spray_tools()
{
    // TODO: rebuild spray tool (create/update pattern protos)
}

void rebuild_binary()
{
    // TODO: rebuild binary proto files from text sources
}

void art_to_protos()
{
    // TODO: create new proto entries for art files that have no proto yet
}

void swap_protos()
{
    // TODO: swap two prototype slots
}

static unsigned char itemIconsBgColor()
{
    return _colorTable[21];
};

// proto_choose_multi_pids_update
static void protoChooseMultiPidsUpdate(int win, int pidType, int scrollOffset, protoChooseFidCallback fidFunc, int pitch)
{
    constexpr int kGridCols = 4;
    constexpr int kGridRows = 4;
    constexpr int kCellPitchX = 90;
    constexpr int kCellPitchY = 80;
    constexpr int kArtW = 80;
    constexpr int kArtH = 60;
    constexpr int kGridX = 8;
    constexpr int kGridY = 9;

    unsigned char* buf = windowGetBuffer(win);

    for (int row = 0; row < kGridRows; row++) {
        for (int col = 0; col < kGridCols; col++) {
            int idx = scrollOffset + row * kGridCols + col;
            int pid = idx | (pidType << 24);
            int cellX = kGridX + col * kCellPitchX + 1;
            int cellY = kGridY + row * kCellPitchY + 1;

            bufferFill(buf + cellY * pitch + cellX, kArtW, kArtH, pitch, itemIconsBgColor());
            Proto* proto;
            if (protoGetProto(pid, &proto) != -1) {
                int fid = fidFunc ? fidFunc(proto) : proto->fid;
                artRender(fid, buf + cellY * pitch + cellX, kArtW, kArtH, pitch);

                const char* name = protoGetName(pid);
                int textY = cellY + kArtH + 5;
                bufferFill(buf + textY * pitch + cellX, kCellPitchX, fontGetLineHeight(), pitch, edit_window_color);
                windowDrawText(win, name, 80, cellX, textY, _colorTable[32747] | FONT_SHADOW);
            }
        }
    }

    windowRefresh(win);
}

// proto_choose_multi_pids_func
int protoChooseMultiPids(int pidType, protoChooseFidCallback fidFunc, protoChooseAddCallback addFunc)
{
    constexpr int kGridCols = 4;
    constexpr int kGridRows = 4;
    constexpr int kCells = kGridCols * kGridRows;
    constexpr int kCellBorderW = 82;
    constexpr int kCellBorderH = 62;
    constexpr int kCellPitchX = 90;
    constexpr int kCellPitchY = 80;
    constexpr int kWinW = 440;
    constexpr int kWinH = 380;
    constexpr int kGridX = 8;
    constexpr int kGridY = 9;
    constexpr int kBaseKey = 160;

    int win = windowCreate(60, 40, kWinW, kWinH, edit_window_color, WINDOW_MOVE_ON_TOP);
    if (win == -1) return -1;

    int pitch = windowGetWidth(win);

    windowDrawBorder(win);

    _win_register_text_button(win, 345, 350, -1, -1, -1, KEY_BRACKET_LEFT, "<<", 0);
    _win_register_text_button(win, 375, 350, -1, -1, -1, KEY_BRACKET_RIGHT, ">>", 0);
    _win_register_text_button(win, 10, 340, -1, -1, -1, KEY_BAR, "Done", 0);

    unsigned char* buf = windowGetBuffer(win);

    for (int row = 0; row < kGridRows; row++) {
        for (int col = 0; col < kGridCols; col++) {
            int cellX = kGridX + col * kCellPitchX;
            int cellY = kGridY + row * kCellPitchY;
            int keyCode = kBaseKey + row * kGridCols + col;

            bufferDrawRect(buf, pitch, cellX, cellY, cellX + kCellBorderW - 1, cellY + kCellBorderH - 1, _colorTable[2]);

            int btn = buttonCreate(win, cellX, cellY, kCellBorderW, kCellBorderH, -1, -1, -1, keyCode, nullptr, nullptr, nullptr, 0);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }
        }
    }

    int scrollOffset = 1;
    int maxOffset = proto_max_id(pidType) - kCells;
    if (maxOffset < 1) maxOffset = 1;

    protoChooseMultiPidsUpdate(win, pidType, scrollOffset, fidFunc, pitch);

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_BAR || key == KEY_RETURN) {
            windowDestroy(win);
            return key == KEY_ESCAPE ? -1 : 0;
        }

        if (key >= kBaseKey && key < kBaseKey + kCells) {
            int pidIndex = scrollOffset + key - kBaseKey;
            int pid = pidIndex | (pidType << 24);

            Proto* proto;
            if (protoGetProto(pid, &proto) == -1) continue;

            char prompt[128];
            snprintf(prompt, sizeof(prompt), "How many: %s?", protoGetName(pid));
            int quantity = 1;
            if (win_get_num_i(&quantity, 1, 32000, false, prompt, 100, 100) != -1) {
                addFunc(pid, quantity);
            }
        } else if (key == KEY_BRACKET_RIGHT) {
            if (scrollOffset + kCells <= maxOffset) {
                scrollOffset += kCells;
                protoChooseMultiPidsUpdate(win, pidType, scrollOffset, fidFunc, pitch);
            }
        } else if (key == KEY_END) {
            scrollOffset = maxOffset;
            protoChooseMultiPidsUpdate(win, pidType, scrollOffset, fidFunc, pitch);
        } else if (key == KEY_BRACKET_LEFT) {
            if (scrollOffset > 1) {
                int prev = scrollOffset - kCells;
                scrollOffset = prev < 1 ? 1 : prev;
                protoChooseMultiPidsUpdate(win, pidType, scrollOffset, fidFunc, pitch);
            }
        } else if (key == KEY_HOME) {
            scrollOffset = 1;
            protoChooseMultiPidsUpdate(win, pidType, scrollOffset, fidFunc, pitch);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

constexpr int kProtoLightScale = 0x10000;

// Maps a per-type editor exit key to the dispatcher return code.
// modified: prototype was changed during this session.
static int proto_edit_exit_code(int win, int key, bool modified)
{
    windowDestroy(win);

    if (key == KEY_ESCAPE) {
        return -1;
    }

    if (!modified) {
        if (key == KEY_MINUS) {
            return -3;
        }
        if (key == KEY_EQUAL) {
            return -4;
        }
        return 0;
    }

    if (key == KEY_MINUS) {
        return -5;
    }
    if (key == KEY_EQUAL) {
        return -6;
    }
    return -2;
}

// Edits light distance/intensity in place (intensity stored as fixed point).
static void proto_edit_light(int win, int* lightDistance, int* lightIntensity)
{
    int dist = *lightDistance;
    win_get_num_i(&dist, 0, 8, false, "Light distance in hexes (0-8)", 100, 100);
    *lightDistance = dist;

    int pct = *lightIntensity * 100 / kProtoLightScale;
    win_get_num_i(&pct, 0, 100, false, "Light intensity (0-100 percent)", 100, 100);
    *lightIntensity = pct * kProtoLightScale / 100;
}

// Resolves a packed script index to its display name. Returns -1 on failure.
static int proto_scr_name(int packedIndex, char* name, size_t size)
{
    name[0] = '\0';
    return scriptsGetFileName(packedIndex & 0xFFFFFF, name, size);
}

// Picks a script for the prototype and redraws its name. Returns false if the
// chooser was cancelled (window only needs a refresh).
static bool proto_edit_script(int win, Proto* proto, char* name, size_t size)
{
    int sel = scr_choose(3);
    if (sel == -1) {
        windowRefresh(win);
        return false;
    }
    if (sel == -2) {
        strncpy(name, "None", size);
        proto->sid = -1;
    } else {
        proto_scr_name(sel, name, size);
        proto->sid = sel;
    }
    windowDrawText(win, name, 130, 240, 90, kProtoEditNormalColor | FONT_SHADOW);
    windowRefresh(win);
    return true;
}

// Picks a material from the list and redraws its label.
static void proto_edit_material(int win, int* material)
{
    int sel = _win_list_select("Materials", gMaterialTypeNames, 8, nullptr, 100, 100, kProtoEditNormalColor | 0x10000);
    *material = sel == -1 ? 0 : sel;
    windowDrawText(win, gMaterialTypeNames[*material], 130, 90, 132, kProtoEditNormalColor | FONT_SHADOW);
    windowRefresh(win);
}

// proto_change_proto_info
static void proto_change_proto_info(int pid, int mode)
{
    static const char* const titles[] = { "Name", "Description" };
    if (mode != 0 && mode != 1) {
        return;
    }

    char buffer[MESSAGE_LIST_ITEM_FIELD_MAX_SIZE];
    buffer[0] = '\0';

    int maxLength = mode == 0 ? 39 : 49;
    if (_win_get_str(buffer, maxLength, titles[mode], 100, 100) == -1) {
        return;
    }

    int type = PID_TYPE(pid);

    MessageListItem entry;
    entry.num = mode + 100 * (pid & 0xFFFFFF);
    entry.audio = const_cast<char*>("");
    entry.text = buffer;
    if (!message_insert(&_proto_msg_files[type], &entry)) {
        debugPrint("\nError attempting to message_insert prototype name!");
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%spro_%.4s%s", "game\\", artGetObjectTypeName(type), ".msg");
    if (!message_save(&_proto_msg_files[type], path)) {
        debugPrint("\nError attempting to message_save prototype name!");
    }
}

// Edits the prototype name and redraws it.
static void proto_edit_name(int win, int pid)
{
    if (can_modify_protos) {
        proto_change_proto_info(pid, 0);
        windowDrawText(win, protoGetName(pid), 130, 90, 48, kProtoEditNormalColor | FONT_SHADOW);
    }
    windowRefresh(win);
}

// Edits the prototype description and redraws it.
static void proto_edit_description(int win, int pid)
{
    if (can_modify_protos) {
        proto_change_proto_info(pid, 1);
        windowFill(win, 90, 67, 280, fontGetLineHeight(), edit_window_color);
        windowDrawText(win, protoGetDescription(pid), 280, 90, 67, kProtoEditNormalColor | FONT_SHADOW);
    }
    windowRefresh(win);
}

// proto_tile_edit
static int proto_tile_edit(int pid)
{
    int win;
    Proto* proto;
    unsigned char* imgPos;
    int width = _scr_size.right - _scr_size.left + 1;
    if (proto_edit_init(&win, pid, &proto, &imgPos, width) == -1) {
        return -1;
    }

    if (proto->tile.material < 0 || proto->tile.material > 8) {
        win_timed_msg("Proto Tile Error: Material out of range!", _colorTable[31744] | 0x10000);
        windowDestroy(win);
        return -1;
    }

    int rowY = 128;
    reg_text_str(win, KEY_LOWERCASE_M, "Material", gMaterialTypeNames[proto->tile.material], &rowY);
    windowRefresh(win);

    int objectType = PID_TYPE(pid);
    bool modified = false;

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN || key == KEY_BAR || key == KEY_MINUS || key == KEY_EQUAL) {
            int exitKey = key == KEY_RETURN ? KEY_BAR : key;
            return proto_edit_exit_code(win, exitKey, modified);
        }

        switch (key) {
        case KEY_BRACKET_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_TILE, &proto->tile.fid, width, -1);
            modified = true;
            break;
        case KEY_BRACKET_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_TILE, &proto->tile.fid, width, 1);
            modified = true;
            break;
        case KEY_LOWERCASE_M:
        case KEY_UPPERCASE_M:
            proto_edit_material(win, &proto->tile.material);
            modified = true;
            break;
        case KEY_LOWERCASE_F:
            reg_mod_flags(&proto->tile.flags, &proto->tile.extendedFlags, objectType, 0);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_LOWERCASE_N:
        case KEY_UPPERCASE_N:
            proto_edit_name(win, pid);
            modified = true;
            break;
        case KEY_BRACE_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->tile.fid, width, -10);
            modified = true;
            break;
        case KEY_BRACE_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->tile.fid, width, 10);
            modified = true;
            break;
        case KEY_HOME:
            proto_edit_mod_fid(win, OBJ_TYPE_TILE, &proto->tile.fid, width, -15000);
            modified = true;
            break;
        case KEY_END:
            proto_edit_mod_fid(win, OBJ_TYPE_TILE, &proto->tile.fid, width, art_total(OBJ_TYPE_TILE));
            modified = true;
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// proto_misc_edit
static int proto_misc_edit(int pid)
{
    int win;
    Proto* proto;
    unsigned char* imgPos;
    int width = _scr_size.right - _scr_size.left + 1;
    if (proto_edit_init(&win, pid, &proto, &imgPos, width) == -1) {
        return -1;
    }

    int rowY = 86;
    reg_text_int(win, 90, KEY_LOWERCASE_L, "Light Dist/Int", proto->misc.lightDistance, &rowY, false);
    windowRefresh(win);

    int objectType = PID_TYPE(pid);
    bool modified = false;

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN || key == KEY_BAR || key == KEY_MINUS || key == KEY_EQUAL) {
            int exitKey = key == KEY_RETURN ? KEY_BAR : key;
            return proto_edit_exit_code(win, exitKey, modified);
        }

        switch (key) {
        case KEY_BRACKET_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_MISC, &proto->misc.fid, width, -1);
            modified = true;
            break;
        case KEY_BRACKET_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_MISC, &proto->misc.fid, width, 1);
            modified = true;
            break;
        case KEY_LOWERCASE_F:
            reg_mod_flags(&proto->misc.flags, &proto->misc.extendedFlags, objectType, 0);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_LOWERCASE_L:
        case KEY_UPPERCASE_L:
            proto_edit_light(win, &proto->misc.lightDistance, &proto->misc.lightIntensity);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_LOWERCASE_D:
            proto_edit_description(win, pid);
            modified = true;
            break;
        case KEY_LOWERCASE_N:
        case KEY_UPPERCASE_N:
            proto_edit_name(win, pid);
            modified = true;
            break;
        case KEY_BRACE_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->misc.fid, width, -10);
            modified = true;
            break;
        case KEY_BRACE_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->misc.fid, width, 10);
            modified = true;
            break;
        case KEY_HOME:
            proto_edit_mod_fid(win, OBJ_TYPE_MISC, &proto->misc.fid, width, -15000);
            modified = true;
            break;
        case KEY_END:
            proto_edit_mod_fid(win, OBJ_TYPE_MISC, &proto->misc.fid, width, art_total(OBJ_TYPE_MISC));
            modified = true;
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// proto_wall_edit
static int proto_wall_edit(int pid)
{
    int win;
    Proto* proto;
    unsigned char* imgPos;
    int width = _scr_size.right - _scr_size.left + 1;
    if (proto_edit_init(&win, pid, &proto, &imgPos, width) == -1) {
        return -1;
    }

    int rowY = 86;
    reg_text_int(win, 90, KEY_LOWERCASE_L, "Light Dist/Int", proto->wall.lightDistance, &rowY, false);

    char scriptName[36];
    if (proto->wall.sid != -1) {
        if (proto_scr_name(proto->wall.sid, scriptName, sizeof(scriptName)) == -1) {
            strcpy(scriptName, "Error: Bad script index!");
            win_timed_msg(scriptName, _colorTable[31744] | 0x10000);
            windowDrawText(win, scriptName, 130, 240, rowY + 4, _colorTable[31744] | 0x10000);
        } else {
            windowDrawText(win, scriptName, 40, 240, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
        }
    }

    rowY += 42;
    if (proto->wall.material < 0 || proto->wall.material > 8) {
        win_timed_msg("Proto Wall Error: Material out of range!", _colorTable[31744] | 0x10000);
        windowDestroy(win);
        return -1;
    }

    reg_text_str(win, KEY_LOWERCASE_M, "Material", gMaterialTypeNames[proto->wall.material], &rowY);
    windowRefresh(win);

    int objectType = PID_TYPE(pid);
    bool modified = false;

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN || key == KEY_BAR || key == KEY_MINUS || key == KEY_EQUAL) {
            int exitKey = key == KEY_RETURN ? KEY_BAR : key;
            return proto_edit_exit_code(win, exitKey, modified);
        }

        switch (key) {
        case KEY_UPPERCASE_S:
            proto_edit_script(win, proto, scriptName, sizeof(scriptName));
            modified = true;
            break;
        case KEY_UPPERCASE_U:
            proto_action_modify(pid);
            proto_action_redraw(win, pid);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_BRACKET_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_WALL, &proto->wall.fid, width, -1);
            modified = true;
            break;
        case KEY_BRACKET_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_WALL, &proto->wall.fid, width, 1);
            modified = true;
            break;
        case KEY_LOWERCASE_F:
            reg_mod_flags(&proto->wall.flags, &proto->wall.extendedFlags, objectType, 0);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_LOWERCASE_M:
        case KEY_UPPERCASE_M:
            proto_edit_material(win, &proto->wall.material);
            modified = true;
            break;
        case KEY_LOWERCASE_N:
        case KEY_UPPERCASE_N:
            proto_edit_name(win, pid);
            modified = true;
            break;
        case KEY_LOWERCASE_D:
            proto_edit_description(win, pid);
            modified = true;
            break;
        case KEY_LOWERCASE_L:
        case KEY_UPPERCASE_L:
            proto_edit_light(win, &proto->wall.lightDistance, &proto->wall.lightIntensity);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_BRACE_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->wall.fid, width, -10);
            modified = true;
            break;
        case KEY_BRACE_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->wall.fid, width, 10);
            modified = true;
            break;
        case KEY_HOME:
            proto_edit_mod_fid(win, OBJ_TYPE_WALL, &proto->wall.fid, width, -15000);
            modified = true;
            break;
        case KEY_END:
            proto_edit_mod_fid(win, OBJ_TYPE_WALL, &proto->wall.fid, width, art_total(OBJ_TYPE_WALL));
            modified = true;
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// proto_scenery_edit
static int proto_scenery_edit(int pid)
{
    int win;
    Proto* proto;
    unsigned char* imgPos;
    int width = _scr_size.right - _scr_size.left + 1;
    if (proto_edit_init(&win, pid, &proto, &imgPos, width) == -1) {
        return -1;
    }

    int rowY = 86;
    reg_text_int(win, 90, KEY_LOWERCASE_L, "Light Dist/Int", proto->scenery.lightDistance, &rowY, false);

    char text[80];
    if (proto->scenery.sid != -1) {
        if (proto_scr_name(proto->scenery.sid, text, sizeof(text)) == -1) {
            strcpy(text, "Error: Bad script index!");
            win_timed_msg(text, _colorTable[31744] | 0x10000);
            windowDrawText(win, text, 130, 240, rowY + 4, _colorTable[31744] | 0x10000);
        } else {
            windowDrawText(win, text, 130, 240, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
        }
    }

    rowY += 21;
    if (proto->scenery.type < 0 || proto->scenery.type > 6) {
        win_timed_msg("Proto Scenery Error: Type out of range!", _colorTable[31744] | 0x10000);
        windowDestroy(win);
        return -1;
    }

    reg_text_str(win, KEY_LOWERCASE_T, "Type", gSceneryTypeNames[proto->scenery.type], &rowY);

    if (proto->scenery.material < 0 || proto->scenery.material > 8) {
        win_timed_msg("Proto Scenery Error: Material out of range!", _colorTable[31744] | 0x10000);
        windowDestroy(win);
        return -1;
    }

    reg_text_str(win, KEY_LOWERCASE_M, "Material", gMaterialTypeNames[proto->scenery.material], &rowY);

    if (proto->scenery.soundId != 0) {
        snprintf(text, sizeof(text), "%c", proto->scenery.soundId);
    } else {
        strcpy(text, "Error");
    }
    reg_text_str(win, KEY_LOWERCASE_I, "Sound ID", text, &rowY);
    windowRefresh(win);

    int objectType = PID_TYPE(pid);
    bool modified = false;

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN || key == KEY_BAR || key == KEY_MINUS || key == KEY_EQUAL) {
            int exitKey = key == KEY_RETURN ? KEY_BAR : key;
            return proto_edit_exit_code(win, exitKey, modified);
        }

        switch (key) {
        case KEY_UPPERCASE_S:
            proto_edit_script(win, proto, text, sizeof(text));
            modified = true;
            break;
        case KEY_UPPERCASE_U:
            proto_action_modify(pid);
            proto_action_redraw(win, pid);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_UPPERCASE_W:
            if (proto->scenery.type == 0) {
                proto->scenery.data.door.openFlags ^= 0x04;
                bool walkThru = (proto->scenery.data.door.openFlags & 0x04) != 0;
                snprintf(text, sizeof(text), "Walk Thru: %s", walkThru ? "Yes" : "No");
                windowDrawText(win, yesno[walkThru ? YES : NO], 60, 375, 131, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
            }
            break;
        case KEY_UPPERCASE_F:
            proto->scenery.extendedFlags ^= 1;
            snprintf(text, sizeof(text), "Magic Hands %s", (proto->scenery.extendedFlags & 1) != 0 ? "Ground" : "Middle");
            win_timed_msg(text, _colorTable[31744] | 0x10000);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_BRACKET_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_SCENERY, &proto->scenery.fid, width, -1);
            modified = true;
            break;
        case KEY_BRACKET_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_SCENERY, &proto->scenery.fid, width, 1);
            modified = true;
            break;
        case KEY_LOWERCASE_F:
            reg_mod_flags(&proto->scenery.flags, &proto->scenery.extendedFlags, objectType, 0);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_LOWERCASE_M:
        case KEY_UPPERCASE_M:
            proto_edit_material(win, &proto->scenery.material);
            modified = true;
            break;
        case KEY_LOWERCASE_T: {
            int sel = _win_list_select("Type", gSceneryTypeNames, 6, nullptr, 100, 100, kProtoEditNormalColor | 0x10000);
            if (sel == -1) {
                sel = 0;
            }
            if (sel != proto->scenery.type) {
                windowDrawText(win, "No", 130, 90, 300, kProtoEditNormalColor | FONT_SHADOW);
                proto->scenery.type = sel;
                proto_scenery_subdata_init(proto, sel);
                windowDrawText(win, gSceneryTypeNames[sel], 130, 90, 111, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
            }
            break;
        }
        case KEY_LOWERCASE_I: {
            int sel = _win_list_select("Pick Sound ID Code:", sound_code_strs, 41, nullptr, 340, 200, kProtoEditNormalColor | 0x10000);
            if (sel != -1) {
                proto->scenery.soundId = sound_code_strs[sel][0];
                snprintf(text, sizeof(text), "%c", proto->scenery.soundId);
                windowDrawText(win, text, 280, 90, 153, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
            }
            break;
        }
        case KEY_LOWERCASE_L:
        case KEY_UPPERCASE_L:
            proto_edit_light(win, &proto->scenery.lightDistance, &proto->scenery.lightIntensity);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_LOWERCASE_N:
        case KEY_UPPERCASE_N:
            proto_edit_name(win, pid);
            modified = true;
            break;
        case KEY_LOWERCASE_D:
            proto_edit_description(win, pid);
            modified = true;
            break;
        case KEY_BRACE_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->scenery.fid, width, -10);
            modified = true;
            break;
        case KEY_BRACE_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->scenery.fid, width, 10);
            modified = true;
            break;
        case KEY_HOME:
            proto_edit_mod_fid(win, OBJ_TYPE_SCENERY, &proto->scenery.fid, width, -15000);
            modified = true;
            break;
        case KEY_END:
            proto_edit_mod_fid(win, OBJ_TYPE_SCENERY, &proto->scenery.fid, width, art_total(OBJ_TYPE_SCENERY));
            modified = true;
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// Creates an ad-hoc prompt window sized to the label. Returns -1 on failure.
static int proto_item_temp_win(const char* prompt)
{
    int width = fontGetStringWidth(prompt) + 50;
    int height = fontGetLineHeight() + 120;
    int temp = windowCreate(340 - width, 200, width, height, edit_window_color, WINDOW_MOVE_ON_TOP);
    if (temp == -1) {
        debugPrint("\nError creating temp_win!");
        return -1;
    }

    windowDrawText(temp, prompt, fontGetStringWidth(prompt), 10, 10, kProtoEditNormalColor | FONT_SHADOW);
    windowRefresh(temp);
    return temp;
}

// Shows a label in an ad-hoc window, then lets the user pick an attack
// animation. Returns the selected index, or -1 if cancelled.
static int proto_item_attack_popup(const char* prompt, const char* title)
{
    int temp = proto_item_temp_win(prompt);
    if (temp == -1) {
        return -1;
    }

    int sel = _win_list_select(title, attack_anim_strs, 9, nullptr, 340, 200, kProtoEditNormalColor | 0x10000);
    windowDestroy(temp);
    return sel;
}

// Redraws the three weapon attack-type labels (primary/secondary/big gun).
static void proto_item_redraw_attack(int win, Proto* proto)
{
    windowDrawText(win, attack_anim_strs[proto->item.extendedFlags & 0xF], 59, 330, 107, kProtoEditNormalColor | 0x10000);
    windowDrawText(win, attack_anim_strs[(proto->item.extendedFlags & 0xF0) >> 4], 59, 330, 117, kProtoEditNormalColor | 0x10000);
    windowDrawText(win, (proto->item.extendedFlags & PROTO_EXT_FLAG_BIG_GUN) != 0 ? "Big Gun" : "", 59, 330, 127, kProtoEditNormalColor | 0x10000);
}

// Picks an inventory FID for the item and redraws its name.
static void proto_item_choose_inv_fid(int win, Proto* proto)
{
    proto_choose_fid(&proto->item.inventoryFid, OBJ_TYPE_INVENTORY, proto->item.inventoryFid == -1);

    char text[80];
    art_name_no_ext(proto->item.inventoryFid, text);
    windowDrawText(win, text, 80, 235, 132, kProtoEditNormalColor | FONT_SHADOW);
    windowRefresh(win);
}

// Edits weapon primary/secondary attack types (+ Big Gun flag) via popups.
static void proto_item_edit_attack(int win, Proto* proto)
{
    char prompt[80];

    snprintf(prompt, sizeof(prompt), "Primary Attack Type: %s", attack_anim_strs[proto->item.extendedFlags & 0xF]);
    int primary = proto_item_attack_popup(prompt, "Primary Attack Type");
    if (primary == -1) {
        return;
    }
    primary &= 0xF;

    snprintf(prompt, sizeof(prompt), "Secondary Attack Type: %s", attack_anim_strs[(proto->item.extendedFlags & 0xF0) >> 4]);
    int secondary = proto_item_attack_popup(prompt, "Secondary Attack Type");
    if (secondary == -1) {
        return;
    }

    int packed = (16 * (secondary & 0xF)) | primary;
    if ((packed & 0xF) > 5 || ((packed & 0xF0) >> 4) > 5) {
        snprintf(prompt, sizeof(prompt), "Big Gun: %s", yesno[(proto->item.extendedFlags & PROTO_EXT_FLAG_BIG_GUN) != 0 ? YES : NO]);
        int temp = proto_item_temp_win(prompt);
        if (temp == -1) {
            return;
        }

        int bigGun = win_yes_no("Is this a Big Gun?", 340, 200, kProtoEditNormalColor | 0x10000);
        windowDestroy(temp);
        if (bigGun == -1) {
            return;
        }
        if (bigGun) {
            packed |= PROTO_EXT_FLAG_BIG_GUN;
        } else {
            packed &= ~PROTO_EXT_FLAG_BIG_GUN;
        }
    }

    proto->item.extendedFlags = (proto->item.extendedFlags & ~0x1FF) | packed;
    proto_item_redraw_attack(win, proto);
    windowRefresh(win);
}

// proto_item_edit
static int proto_item_edit(int pid)
{
    int win;
    Proto* proto;
    unsigned char* imgPos;
    int width = _scr_size.right - _scr_size.left + 1;
    if (proto_edit_init(&win, pid, &proto, &imgPos, width) == -1) {
        return -1;
    }

    char text[80];
    int rowY = 86;
    reg_text_int(win, 90, KEY_LOWERCASE_L, "Light Dist/Int", proto->item.lightDistance, &rowY, false);

    if (proto->item.sid != -1) {
        if (proto_scr_name(proto->item.sid, text, sizeof(text)) == -1) {
            strcpy(text, "Error: Bad script index!");
            win_timed_msg(text, _colorTable[31744] | 0x10000);
            windowDrawText(win, text, 130, 240, rowY + 4, _colorTable[31744] | 0x10000);
        } else {
            windowDrawText(win, text, 130, 240, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
        }
    }

    rowY += 21;
    if (proto->item.type < 0 || proto->item.type >= ITEM_TYPE_COUNT) {
        win_timed_msg("Proto Item Error: Type out of range!", _colorTable[31744] | 0x10000);
        proto_item_subdata_edit_exit();
        windowDestroy(win);
        return -1;
    }

    reg_text_str(win, KEY_LOWERCASE_T, "Type", gItemTypeNames[proto->item.type], &rowY);
    proto_item_subdata_edit_init(proto);

    _win_register_text_button(win, 170, rowY, -1, -1, -1, KEY_UPPERCASE_I, "Inv Fid", 0);
    art_name_no_ext(proto->item.inventoryFid, text);
    windowDrawText(win, text, 80, 235, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);

    if (proto->item.type == ITEM_TYPE_WEAPON) {
        proto_item_redraw_attack(win, proto);
    }

    if (proto->item.material < 0 || proto->item.material > 8) {
        win_timed_msg("Proto Item Error: Material out of range!", _colorTable[31744] | 0x10000);
        proto_item_subdata_edit_exit();
        windowDestroy(win);
        return -1;
    }

    reg_text_str(win, KEY_LOWERCASE_M, "Material", gMaterialTypeNames[proto->item.material], &rowY);
    reg_text_int(win, 90, KEY_LOWERCASE_S, "Size", proto->item.size, &rowY, true);
    reg_text_int(win, 90, KEY_LOWERCASE_W, "Weight", proto->item.weight, &rowY, true);
    reg_text_int(win, 90, KEY_LOWERCASE_V, "Cost", proto->item.cost, &rowY, true);

    if (proto->item.soundId != 0) {
        snprintf(text, sizeof(text), "%c", proto->item.soundId);
    } else {
        strcpy(text, "Error");
    }
    reg_text_str(win, KEY_LOWERCASE_I, "Sound ID", text, &rowY);
    windowRefresh(win);

    int objectType = PID_TYPE(pid);
    bool modified = false;

    while (true) {
        sharedFpsLimiter.mark();

        int key = inputGetInput();

        if (key == KEY_ESCAPE || key == KEY_RETURN || key == KEY_BAR || key == KEY_MINUS || key == KEY_EQUAL) {
            int exitKey = key == KEY_RETURN ? KEY_BAR : key;
            proto_item_subdata_edit_exit();
            return proto_edit_exit_code(win, exitKey, modified);
        }

        switch (key) {
        case KEY_UPPERCASE_S:
            proto_edit_script(win, proto, text, sizeof(text));
            modified = true;
            break;
        case KEY_UPPERCASE_U:
            proto_action_modify(pid);
            proto_action_redraw(win, pid);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_UPPERCASE_I:
            proto_item_choose_inv_fid(win, proto);
            modified = true;
            break;
        case KEY_UPPERCASE_F:
            if (proto->item.type == ITEM_TYPE_WEAPON) {
                proto_item_edit_attack(win, proto);
                modified = true;
            }
            break;
        case KEY_LOWERCASE_T: {
            int sel = _win_list_select("Item Type", gItemTypeNames, ITEM_TYPE_COUNT, nullptr, 100, 100, kProtoEditNormalColor | 0x10000);
            if (sel != -1 && sel != proto->item.type) {
                windowDrawText(win, "No", 130, 90, 300, kProtoEditNormalColor | FONT_SHADOW);
                proto->item.type = sel;
                proto_item_subdata_init(proto, sel);
                proto_item_subdata_edit_init(proto);
                windowDrawText(win, gItemTypeNames[sel], 130, 90, 111, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
            }
            break;
        }
        case KEY_LOWERCASE_I: {
            int sel = _win_list_select("Pick Sound ID Code:", sound_code_strs, 41, nullptr, 340, 200, kProtoEditNormalColor | 0x10000);
            if (sel != -1) {
                proto->item.soundId = sound_code_strs[sel][0];
                snprintf(text, sizeof(text), "%c", proto->item.soundId);
                windowDrawText(win, text, 280, 90, 216, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
            }
            break;
        }
        case KEY_LOWERCASE_M:
        case KEY_UPPERCASE_M:
            proto_edit_material(win, &proto->item.material);
            modified = true;
            break;
        case KEY_LOWERCASE_S:
            reg_text_int_update(win, &proto->item.size, 0, 4095, "Size", 153);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_UPPERCASE_W:
        case KEY_LOWERCASE_W:
            reg_text_int_update(win, &proto->item.weight, 0, 4095, "Weight", 174);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_UPPERCASE_V:
        case KEY_LOWERCASE_V:
            reg_text_int_update(win, &proto->item.cost, 0, 0xFFFFFF, "Cost", 195);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_LOWERCASE_L:
        case KEY_UPPERCASE_L:
            proto_edit_light(win, &proto->item.lightDistance, &proto->item.lightIntensity);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_LOWERCASE_F:
            reg_mod_flags(&proto->item.flags, &proto->item.extendedFlags, objectType, 0);
            windowRefresh(win);
            modified = true;
            break;
        case KEY_UPPERCASE_N:
        case KEY_LOWERCASE_N:
            proto_edit_name(win, pid);
            modified = true;
            break;
        case KEY_LOWERCASE_D:
            proto_edit_description(win, pid);
            modified = true;
            break;
        case KEY_BRACKET_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->item.fid, width, -1);
            modified = true;
            break;
        case KEY_BRACKET_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->item.fid, width, 1);
            modified = true;
            break;
        case KEY_BRACE_LEFT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->item.fid, width, -10);
            modified = true;
            break;
        case KEY_BRACE_RIGHT:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->item.fid, width, 10);
            modified = true;
            break;
        case KEY_HOME:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->item.fid, width, -15000);
            modified = true;
            break;
        case KEY_END:
            proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->item.fid, width, art_total(OBJ_TYPE_ITEM));
            modified = true;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '!':
        case '@':
        case '#':
        case '$':
        case '%':
        case '^':
        case '&':
        case '(':
        case '*':
            proto_item_subdata_edit_handle(proto, key);
            windowRefresh(win);
            modified = true;
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// Draws an AI combat packet name into the critter editor.
static void proto_critter_redraw_ai(int win, int aiPacket)
{
    char text[80];
    strcpy(text, combat_ai_name(aiPacket));
    windowDrawText(win, text, 80, 100, 174, kProtoEditNormalColor | FONT_SHADOW);
}

// Edits a numeric critter bonus stat in place and redraws it at the given spot.
static void proto_critter_edit_bonus(int win, int* value, int min, int max, const char* title, int drawWidth, int x, int y)
{
    int num = *value;
    win_get_num_i(&num, min, max, false, title, 100, 100);
    *value = num;
    char text[36];
    snprintf(text, sizeof(text), "%d", num);
    windowDrawText(win, text, drawWidth, x, y, kProtoEditNormalColor | FONT_SHADOW);
}

// Copies armor class, resistances and thresholds from a stamped armor proto
// into the critter's bonus stats and redraws the resistance/threshold columns.
static void proto_critter_stamp_armor(int win, Proto* proto, Proto* armor)
{
    char text[80];
    proto->critter.data.bonusStats[STAT_ARMOR_CLASS] = armor->item.data.armor.armorClass;

    int x = 265;
    for (int i = 0; i < 7; i++) {
        proto->critter.data.bonusStats[STAT_DAMAGE_RESISTANCE + i] = armor->item.data.armor.damageResistance[i];
        snprintf(text, sizeof(text), "%d", proto->critter.data.bonusStats[STAT_DAMAGE_RESISTANCE + i]);
        windowDrawText(win, text, 26, x, 258, kProtoEditNormalColor | FONT_SHADOW);

        proto->critter.data.bonusStats[STAT_DAMAGE_THRESHOLD + i] = armor->item.data.armor.damageThreshold[i];
        snprintf(text, sizeof(text), "%d", proto->critter.data.bonusStats[STAT_DAMAGE_THRESHOLD + i]);
        windowDrawText(win, text, 26, x, 279, kProtoEditNormalColor | FONT_SHADOW);
        x += 37;
    }
}

// Edits one of the seven damage resistances (kind 0) or thresholds (kind 1).
static void proto_critter_edit_damage(int win, Proto* proto, bool threshold)
{
    int base = threshold ? STAT_DAMAGE_THRESHOLD : STAT_DAMAGE_RESISTANCE;
    int textY = threshold ? 279 : 258;
    char text[80];
    int x = 265;
    for (int i = 0; i < 7; i++) {
        snprintf(text, sizeof(text), "%s Damage %s", gDamageTypeNames[i], threshold ? "Threshold" : "Resistance");
        int value = proto->critter.data.bonusStats[base + i];
        if (win_get_num_i(&value, -999, 999, false, text, 100, 100) == -1) {
            break;
        }
        snprintf(text, sizeof(text), "%d", value);
        windowDrawText(win, text, 26, x, textY, kProtoEditNormalColor | FONT_SHADOW);
        proto->critter.data.bonusStats[base + i] = value;
        windowRefresh(win);
        x += 37;
    }
}

// proto_critter_edit
static int proto_critter_edit(int pid)
{
    int width = _scr_size.right - _scr_size.left + 1;
    int objectType = PID_TYPE(pid);

    int exitKey = KEY_BAR;
    bool modified = false;
    bool advancedReentry = false;

    do {
        int win;
        Proto* proto;
        unsigned char* imgPos;
        if (proto_edit_init(&win, pid, &proto, &imgPos, width) == -1) {
            return -1;
        }

        char text[80];
        int rowY = 86;
        reg_text_int(win, 90, KEY_LOWERCASE_L, "Light Dist/Int", proto->critter.lightDistance, &rowY, false);

        if (proto->critter.sid != -1) {
            if (proto_scr_name(proto->critter.sid, text, sizeof(text)) == -1) {
                strcpy(text, "Error: Bad script index!");
                win_timed_msg(text, _colorTable[31744] | 0x10000);
                windowDrawText(win, text, 130, 240, rowY + 4, _colorTable[31744] | 0x10000);
            } else {
                windowDrawText(win, text, 130, 240, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
            }
        }

        rowY += 21;
        _win_register_text_button(win, 10, rowY, -1, -1, -1, KEY_1, "Advanced", 0);
        _win_register_text_button(win, 110, rowY, -1, -1, -1, KEY_2, "Gender", 0);

        rowY += 21;
        _win_register_text_button(win, 10, rowY, -1, -1, -1, KEY_LOWERCASE_H, "Head Fid", 0);
        art_name_no_ext(proto->critter.headFid, text);
        windowDrawText(win, text, 80, 80, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);

        rowY += 21;
        _win_register_text_button(win, 10, rowY, -1, -1, -1, KEY_LOWERCASE_B, "Body Type", 0);
        if (proto->critter.data.bodyType < 0 || proto->critter.data.bodyType >= BODY_TYPE_COUNT) {
            win_timed_msg("Proto Critter Error: Body out of range!", _colorTable[31744] | 0x10000);
            windowDestroy(win);
            return -1;
        }
        windowDrawText(win, gBodyTypeNames[proto->critter.data.bodyType], 80, 85, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);

        rowY += 21;
        _win_register_text_button(win, 10, rowY, -1, -1, -1, KEY_UPPERCASE_A, "AI Packet", 0);
        proto_critter_redraw_ai(win, proto->critter.aiPacket);
        _win_register_text_button(win, 220, rowY, -1, -1, -1, KEY_UPPERCASE_T, "Team Num", 0);
        snprintf(text, sizeof(text), "%d", proto->critter.team);
        windowDrawText(win, text, 80, 300, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);

        rowY += 21;
        _win_register_text_button(win, 10, rowY, -1, -1, -1, KEY_EXCLAMATION, "Critter Flags", 0);
        proto_critter_flags_redraw(win, pid);

        rowY += 21;
        _win_register_text_button(win, 10, rowY, -1, -1, -1, KEY_UPPERCASE_X, "Exp. Val", 0);
        snprintf(text, sizeof(text), "%d", proto->critter.data.experience);
        windowDrawText(win, text, 80, 80, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
        _win_register_text_button(win, 110, rowY, -1, -1, -1, KEY_UPPERCASE_B, "Barter", 0);
        windowDrawText(win, yesno[(proto->critter.data.flags & 2) != 0 ? YES : NO], 50, 200, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
        _win_register_text_button(win, 230, rowY, -1, -1, -1, KEY_UPPERCASE_D, "Dmg Type", 0);
        windowDrawText(win, gDamageTypeNames[proto->critter.data.damageType], 50, 320, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);

        rowY += 21;
        _win_register_text_button(win, 10, rowY, -1, -1, -1, KEY_UPPERCASE_K, "Kill Type", 0);
        const char* killName = killTypeGetName(proto->critter.data.killType);
        if (killName != nullptr) {
            windowDrawText(win, killName, 80, 80, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
        }
        _win_register_text_button(win, 110, rowY, -1, -1, -1, KEY_LOWERCASE_C, "Crit. Bonus", 0);
        snprintf(text, sizeof(text), "%d", proto->critter.data.bonusStats[STAT_CRITICAL_CHANCE]);
        windowDrawText(win, text, 60, 200, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
        _win_register_text_button(win, 310, rowY, -1, -1, -1, KEY_PERCENT, "Stamp Armor", 0);

        rowY += 21;
        _win_register_text_button(win, 10, rowY, -1, -1, -1, KEY_UPPERCASE_H, "HP Bonus", 0);
        snprintf(text, sizeof(text), "%d", proto->critter.data.bonusStats[STAT_MAXIMUM_HIT_POINTS]);
        windowDrawText(win, text, 60, 100, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
        _win_register_text_button(win, 110, rowY, -1, -1, -1, KEY_LOWERCASE_S, "Seq. Bonus", 0);
        snprintf(text, sizeof(text), "%d", proto->critter.data.bonusStats[STAT_SEQUENCE]);
        windowDrawText(win, text, 60, 200, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);

        rowY += 21;
        _win_register_text_button(win, 10, rowY, -1, -1, -1, KEY_LOWERCASE_A, "AP Bonus", 0);
        snprintf(text, sizeof(text), "%d", proto->critter.data.bonusStats[STAT_MAXIMUM_ACTION_POINTS]);
        windowDrawText(win, text, 60, 100, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
        _win_register_text_button(win, 110, rowY, -1, -1, -1, KEY_LOWERCASE_M, "Melee Dam", 0);
        snprintf(text, sizeof(text), "%d", proto->critter.data.bonusStats[STAT_MELEE_DAMAGE]);
        windowDrawText(win, text, 60, 200, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);

        rowY -= 21;
        _win_register_text_button(win, 210, rowY, -1, -1, -1, KEY_LOWERCASE_R, "Dm Res", 0);
        int x = 265;
        for (int i = 0; i < 7; i++) {
            snprintf(text, sizeof(text), "%d", proto->critter.data.bonusStats[STAT_DAMAGE_RESISTANCE + i]);
            windowDrawText(win, text, 26, x, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
            windowDrawText(win, gDamageTypeNames[i], 35, x, rowY + 14, kProtoEditNormalColor);
            x += 37;
        }

        rowY += 21;
        _win_register_text_button(win, 210, rowY, -1, -1, -1, KEY_UPPERCASE_R, "Dm Thr", 0);
        x = 265;
        for (int i = 0; i < 7; i++) {
            snprintf(text, sizeof(text), "%d", proto->critter.data.bonusStats[STAT_DAMAGE_THRESHOLD + i]);
            windowDrawText(win, text, 26, x, rowY + 4, kProtoEditNormalColor | FONT_SHADOW);
            x += 37;
        }

        windowRefresh(win);

        bool reenter = false;
        while (true) {
            sharedFpsLimiter.mark();

            int key = inputGetInput();

            if (key == KEY_ESCAPE || key == KEY_BAR || key == KEY_MINUS || key == KEY_EQUAL) {
                exitKey = key;
                break;
            }
            if (key == KEY_RETURN) {
                exitKey = KEY_BAR;
                break;
            }

            switch (key) {
            case KEY_1:
            case KEY_2: {
                windowDestroy(win);
                Proto* dudeProto;
                if (protoGetProto(gDude->pid, &dudeProto) == -1) {
                    return -1;
                }
                critterProtoDataCopy(&dudeProto->critter.data, &proto->critter.data);
                characterEditorShow(key != KEY_1);
                critterProtoDataCopy(&proto->critter.data, &dudeProto->critter.data);
                modified = true;
                _proto_dude_init("premade\\blank.gcd");
                if (key == KEY_1) {
                    reenter = true;
                }
                break;
            }
            case KEY_UPPERCASE_S: {
                int sel = scr_choose(4);
                if (sel == -1) {
                    break;
                }
                if (sel == -2) {
                    strcpy(text, "None");
                    proto->critter.sid = -1;
                } else {
                    if (proto_scr_name(sel, text, sizeof(text)) == -1) {
                        win_timed_msg("Error: Bad script index!", _colorTable[31744] | 0x10000);
                    }
                    proto->critter.sid = sel;
                }
                windowDrawText(win, text, 130, 240, 90, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
                break;
            }
            case KEY_LOWERCASE_H: {
                proto_choose_fid(&proto->critter.headFid, OBJ_TYPE_HEAD, 1);
                art_name_no_ext(proto->critter.headFid, text);
                windowDrawText(win, text, 80, 80, 132, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
                break;
            }
            case KEY_LOWERCASE_B: {
                int sel = _win_list_select("Body Type", gBodyTypeNames, BODY_TYPE_COUNT, nullptr, 100, 100, kProtoEditNormalColor | 0x10000);
                if (sel == -1) {
                    sel = 0;
                }
                proto->critter.data.bodyType = sel;
                windowDrawText(win, gBodyTypeNames[sel], 80, 85, 153, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
                break;
            }
            case KEY_UPPERCASE_A:
                proto_pick_ai_packet(&proto->critter.aiPacket);
                proto_critter_redraw_ai(win, proto->critter.aiPacket);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_UPPERCASE_T: {
                int value = proto->critter.team;
                win_get_num_i(&value, 0, 32000, false, "Team Num", 100, 100);
                proto->critter.team = value;
                snprintf(text, sizeof(text), "%d", value);
                windowDrawText(win, text, 80, 300, 174, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
                break;
            }
            case KEY_EXCLAMATION:
                if (proto_critter_flags_modify(pid) == 0) {
                    proto_critter_flags_redraw(win, pid);
                    windowRefresh(win);
                    modified = true;
                }
                break;
            case KEY_UPPERCASE_X: {
                int value = proto->critter.data.experience;
                if (win_get_num_i(&value, 0, 32000, false, "Experience Value", 200, 200) == -1) {
                    break;
                }
                proto->critter.data.experience = value;
                snprintf(text, sizeof(text), "%d", value);
                windowDrawText(win, text, 80, 80, 216, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
                break;
            }
            case KEY_UPPERCASE_B:
                proto->critter.data.flags ^= 2;
                windowDrawText(win, yesno[(proto->critter.data.flags & 2) != 0 ? YES : NO], 50, 200, 216, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_UPPERCASE_D: {
                int sel = _win_list_select("Damage Type", gDamageTypeNames, 7, nullptr, 340, 200, kProtoEditNormalColor | 0x10000);
                if (sel == -1) {
                    break;
                }
                proto->critter.data.damageType = sel;
                windowDrawText(win, gDamageTypeNames[sel], 50, 320, 216, kProtoEditNormalColor | FONT_SHADOW);
                windowRefresh(win);
                modified = true;
                break;
            }
            case KEY_UPPERCASE_K: {
                int sel = mp_pick_kill_type();
                if (sel == -1) {
                    break;
                }
                proto->critter.data.killType = sel;
                const char* name = killTypeGetName(sel);
                if (name != nullptr) {
                    windowDrawText(win, name, 80, 80, 237, kProtoEditNormalColor | FONT_SHADOW);
                }
                windowRefresh(win);
                modified = true;
                break;
            }
            case KEY_LOWERCASE_C:
                proto_critter_edit_bonus(win, &proto->critter.data.bonusStats[STAT_CRITICAL_CHANCE], -180, 180, "Critical Chance Bonus", 60, 200, 237);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_PERCENT: {
                int armorPid;
                proto_choose_pid(&armorPid, OBJ_TYPE_ITEM, 1, 0);
                if (armorPid == -1) {
                    break;
                }
                Proto* armor;
                if (protoGetProto(armorPid, &armor) == -1) {
                    return -1;
                }
                proto_critter_stamp_armor(win, proto, armor);
                windowRefresh(win);
                modified = true;
                break;
            }
            case KEY_UPPERCASE_H:
                proto_critter_edit_bonus(win, &proto->critter.data.bonusStats[STAT_MAXIMUM_HIT_POINTS], -200, 9000, "Hit Point Bonus", 60, 100, 258);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_LOWERCASE_S:
                proto_critter_edit_bonus(win, &proto->critter.data.bonusStats[STAT_SEQUENCE], -180, 180, "Sequence Bonus", 60, 200, 258);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_LOWERCASE_A:
                proto_critter_edit_bonus(win, &proto->critter.data.bonusStats[STAT_MAXIMUM_ACTION_POINTS], -80, 80, "Action Point Bonus", 60, 100, 279);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_LOWERCASE_M:
                proto_critter_edit_bonus(win, &proto->critter.data.bonusStats[STAT_MELEE_DAMAGE], -180, 180, "Melee Damage Bonus", 60, 200, 279);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_LOWERCASE_L:
            case KEY_UPPERCASE_L:
                proto_edit_light(win, &proto->critter.lightDistance, &proto->critter.lightIntensity);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_LOWERCASE_R:
                proto_critter_edit_damage(win, proto, false);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_UPPERCASE_R:
                proto_critter_edit_damage(win, proto, true);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_LOWERCASE_N:
            case KEY_UPPERCASE_N:
                proto_edit_name(win, pid);
                modified = true;
                break;
            case KEY_LOWERCASE_D:
                proto_edit_description(win, pid);
                modified = true;
                break;
            case KEY_UPPERCASE_U:
                proto_action_modify(pid);
                proto_action_redraw(win, pid);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_LOWERCASE_F:
                reg_mod_flags(&proto->critter.flags, &proto->critter.extendedFlags, objectType, 0);
                windowRefresh(win);
                modified = true;
                break;
            case KEY_BRACKET_LEFT:
                proto_edit_mod_fid(win, OBJ_TYPE_CRITTER, &proto->critter.fid, width, -1);
                modified = true;
                break;
            case KEY_BRACKET_RIGHT:
                proto_edit_mod_fid(win, OBJ_TYPE_CRITTER, &proto->critter.fid, width, 1);
                modified = true;
                break;
            case KEY_BRACE_LEFT:
                proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->critter.fid, width, -10);
                modified = true;
                break;
            case KEY_BRACE_RIGHT:
                proto_edit_mod_fid(win, OBJ_TYPE_ITEM, &proto->critter.fid, width, 10);
                modified = true;
                break;
            case KEY_HOME:
                proto_edit_mod_fid(win, OBJ_TYPE_CRITTER, &proto->critter.fid, width, -15000);
                modified = true;
                break;
            case KEY_END:
                proto_edit_mod_fid(win, OBJ_TYPE_CRITTER, &proto->critter.fid, width, art_total(OBJ_TYPE_CRITTER));
                modified = true;
                break;
            }

            if (reenter) {
                break;
            }

            renderPresent();
            sharedFpsLimiter.throttle();
        }

        if (reenter) {
            advancedReentry = true;
            continue;
        }

        advancedReentry = false;
        int code = proto_edit_exit_code(win, exitKey, modified);

        // Saving paths first validate the proto can spawn an object.
        if (code == -2 || code == -5 || code == -6) {
            Object* obj;
            if (objectCreateWithFidPid(&obj, -1, pid) != -1) {
                objectDestroy(obj, nullptr);
            }
        }
        return code;
    } while (advancedReentry);

    return 0;
}

// protoInstEdit implemented in mp_instance.cc

} // namespace fallout
