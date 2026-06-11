#include "mapper/mp_proto.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "art.h"
#include "color.h"
#include "combat_ai.h"
#include "critter.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "mapper/mp_scrpt.h"
#include "mapper/mp_targt.h"
#include "memory.h"
#include "proto.h"
#include "scripts.h"
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
static void proto_critter_flags_redraw(int win, int pid);
static int proto_critter_flags_modify(int pid);
static int mp_pick_kill_type();
void proto_action_redraw(int win, int pid);
static void reg_text_int(int win, int y, int key, const char* title, int value, int* rowY, bool advance);
static void reg_text_str(int win, int key, const char* title, const char* value, int* rowY);
static void reg_text_int_update(int win, int* value, int min, int max, const char* title, int y, int textY);
static bool proto_action_can_look_at(int pid);
static void proto_action_modify(int pid);
static void proto_edit_mod_fid(int win, int objectType, int* fidPtr, int width, int delta = 0);
static int proto_edit_mod_pid(int win, Proto** protoPtr, int width, int delta);
static void reg_mod_flags(int* flags, int* extendedFlags, int objectType, int subtype);
static void proto_choose_fid(int* fidPtr, int objectType, int useFidObjectType);
static void proto_choose_pid(int* pidPtr, int objectType, int hasPid, int subtype);

// proto_change_proto_info (body implemented in disk-persistence feature)
void proto_change_proto_info(int pid, int mode);

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
static const char* anim_code_strs[] = {
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
void reg_text_int_update(int win, int* value, int min, int max, const char* title, int y, int textY)
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

int protoEdit(int protoId)
{
    // TODO: implement proto editor dialog — load proto, show editor UI
    (void)protoId;
    return -1;
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

// protoInstEdit implemented in mp_instance.cc

} // namespace fallout
