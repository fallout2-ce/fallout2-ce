#include "proto_txt.h"

#include <stdio.h>
#include <string.h>

#include "art.h"
#include "db.h"
#include "debug.h"
#include "mapper/mp_targt.h"
#include "perk.h"
#include "platform_compat.h"
#include "proto.h"
#include "stat.h"

namespace fallout {

// Weapon animation-code labels (text format + mapper editor only).
const char* const anim_code_strs[] = {
    "NONE",
    "D-KNIFE",
    "E-CLUB",
    "F-2_HANDED_CLUB",
    "G-SPEAR",
    "H-PISTOL",
    "I-UZI",
    "J-RIFLE",
    "K-LASER",
    "L-MINIGUN",
    "M-ROCKET",
};

// Writes the perk field when in range; index -1 maps to "None".
static void proto_save_perk(File* stream, int perk)
{
    if (perk >= -1 && perk < 119) {
        filePrintFormatted(stream, "d.perk: %s\n", perk == -1 ? _mp_perk_code_None : _mp_perk_code_strs[perk]);
    }
}

// Drug stat name: index -2 is the special header, -1 is "None".
static const char* proto_drug_stat_name(int stat)
{
    if (stat == -2) {
        return _mp_critter_stats_list;
    }
    return stat == -1 ? _critter_stats_list_None : _critter_stats_list_strs[stat];
}

// Truncate the proto/art list string at its extension dot, in place.
// Returns false when no dot is present.
static bool proto_strip_ext(char* name)
{
    char* dot = strchr(name, '.');
    if (dot == nullptr) {
        return false;
    }
    *dot = '\0';
    return true;
}

// fprnt_pid
static int fprnt_pid(File* stream, int pid)
{
    char name[COMPAT_MAX_PATH];
    if (_proto_list_str(pid, name) == -1) {
        return -1;
    }

    if (!proto_strip_ext(name)) {
        return -1;
    }

    filePrintFormatted(stream, "pid: %u %s\n", pid, name);
    return 0;
}

// fprnt_fid
static int fprnt_fid(File* stream, int fid)
{
    char name[COMPAT_MAX_PATH];
    if (art_list_str(fid, name) == -1) {
        return -1;
    }

    proto_strip_ext(name);
    filePrintFormatted(stream, "fid: %u %s\n", fid, name);
    return 0;
}

// fprnt_fid_txt
static int fprnt_fid_txt(File* stream, int fid, const char* format)
{
    char name[COMPAT_MAX_PATH];
    if (art_list_str(fid, name) == -1) {
        return -1;
    }

    proto_strip_ext(name);
    filePrintFormatted(stream, format, fid, name);
    return 0;
}

// fprnt_pid_str
static int fprnt_pid_str(File* stream, int pid, const char* label)
{
    char name[COMPAT_MAX_PATH];
    if (_proto_list_str(pid, name) == -1) {
        return -1;
    }

    if (!proto_strip_ext(name)) {
        return -1;
    }

    filePrintFormatted(stream, "%s: %u %s\n", label, pid, name);
    return 0;
}

// Writes one weapon/misc pid field, normalizing the uninitialized sentinel to -1.
// Returns -1 only when the underlying list lookup fails for a valid pid.
static int proto_save_pid_field(File* stream, int* pid, const char* label)
{
    if (*pid == static_cast<int>(0xCCCCCCCC)) {
        *pid = -1;
    } else if (*pid != -1) {
        if (fprnt_pid_str(stream, *pid, label) == -1) {
            return -1;
        }
    }
    return 0;
}

// proto_save_text
int proto_save_text(int pid, const char* dir)
{
    Proto* proto;
    if (protoGetProto(pid, &proto) == -1) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    strcpy(path, dir);
    strcat(path, "\\");

    char* fileName = path + strlen(path);
    _proto_list_str(pid, fileName);

    char* dot = strchr(fileName, '.');
    if (dot != nullptr) {
        strcpy(dot + 1, "txt");
    } else {
        strcat(path, ".txt");
    }

    File* stream = fileOpenDirect(path, "wt");
    if (stream == nullptr) {
        return -1;
    }

    int rc = -1;

    if (fprnt_pid(stream, pid) == -1) {
        goto out;
    }

    filePrintFormatted(stream, "name: %s\n", protoGetName(proto->pid));
    filePrintFormatted(stream, "message_num: %d\n", proto->messageId);

    if (fprnt_fid(stream, proto->fid) == -1) {
        goto out;
    }

    switch (PID_TYPE(pid)) {
    case OBJ_TYPE_ITEM:
        filePrintFormatted(stream, "light_distance: %d\n", proto->item.lightDistance);
        filePrintFormatted(stream, "light_intensity: %d\n", proto->item.lightIntensity);
        filePrintFormatted(stream, "flags: %d\n", proto->item.flags);
        filePrintFormatted(stream, "flags_ext: %d\n", proto->item.extendedFlags);
        filePrintFormatted(stream, "sid: %d\n", proto->item.sid);
        filePrintFormatted(stream, "type: %s\n", item_pro_type[proto->item.type]);

        switch (proto->item.type) {
        case ITEM_TYPE_ARMOR:
            filePrintFormatted(stream, "d.ac: %d\n", proto->item.data.armor.armorClass);
            for (int i = 0; i < 7; i++) {
                filePrintFormatted(stream, "d.dam_resist%d: %d\n", i, proto->item.data.armor.damageResistance[i]);
            }
            for (int i = 0; i < 7; i++) {
                filePrintFormatted(stream, "d.dam_thresh%d: %d\n", i, proto->item.data.armor.damageThreshold[i]);
            }
            proto_save_perk(stream, proto->item.data.armor.perk);
            fprnt_fid_txt(stream, proto->item.data.armor.maleFid, "d.male_fid: %u %s\n");
            fprnt_fid_txt(stream, proto->item.data.armor.femaleFid, "d.female_fid: %u %s\n");
            break;
        case ITEM_TYPE_CONTAINER:
            filePrintFormatted(stream, "d.max_size: %d\n", proto->item.data.container.maxSize);
            filePrintFormatted(stream, "d.open_flags: %d\n", proto->item.data.container.openFlags);
            break;
        case ITEM_TYPE_DRUG:
            for (int i = 0; i < 3; i++) {
                int stat = proto->item.data.drug.stat[i];
                if (stat >= -2 && stat < 38) {
                    filePrintFormatted(stream, "d.stat%d: %s\n", i, proto_drug_stat_name(stat));
                }
            }
            for (int i = 0; i < 3; i++) {
                filePrintFormatted(stream, "d.amount%d: %d\n", i, proto->item.data.drug.amount[i]);
            }
            filePrintFormatted(stream, "d.duration1: %d\n", proto->item.data.drug.duration1);
            for (int i = 0; i < 3; i++) {
                filePrintFormatted(stream, "d.amount1_%d: %d\n", i, proto->item.data.drug.amount1[i]);
            }
            filePrintFormatted(stream, "d.duration2: %d\n", proto->item.data.drug.duration2);
            for (int i = 0; i < 3; i++) {
                filePrintFormatted(stream, "d.amount2_%d: %d\n", i, proto->item.data.drug.amount2[i]);
            }
            filePrintFormatted(stream, "d.addiction_chance: %d\n", proto->item.data.drug.addictionChance);
            filePrintFormatted(stream, "d.withdrawal_effect: %d\n", proto->item.data.drug.withdrawalEffect);
            filePrintFormatted(stream, "d.withdrawal_onset: %d\n", proto->item.data.drug.withdrawalOnset);
            break;
        case ITEM_TYPE_WEAPON:
            filePrintFormatted(stream, "d.animation_code: %s\n", anim_code_strs[proto->item.data.weapon.animationCode]);
            filePrintFormatted(stream, "d.min_damage: %d\n", proto->item.data.weapon.minDamage);
            filePrintFormatted(stream, "d.max_damage: %d\n", proto->item.data.weapon.maxDamage);
            filePrintFormatted(stream, "d.dt: %s\n", gDamageTypeNames[proto->item.data.weapon.damageType]);
            filePrintFormatted(stream, "d.max_range1: %d\n", proto->item.data.weapon.maxRange1);
            filePrintFormatted(stream, "d.max_range2: %d\n", proto->item.data.weapon.maxRange2);
            if (proto_save_pid_field(stream, &proto->item.data.weapon.projectilePid, "d.proj_pid") == -1) {
                goto err;
            }
            filePrintFormatted(stream, "d.min_st: %d\n", proto->item.data.weapon.minStrength);
            filePrintFormatted(stream, "d.mp_cost1: %d\n", proto->item.data.weapon.actionPointCost1);
            filePrintFormatted(stream, "d.mp_cost2: %d\n", proto->item.data.weapon.actionPointCost2);
            filePrintFormatted(stream, "d.crit_fail_table: %d\n", proto->item.data.weapon.criticalFailureType);
            proto_save_perk(stream, proto->item.data.weapon.perk);
            filePrintFormatted(stream, "d.rounds: %d\n", proto->item.data.weapon.rounds);
            filePrintFormatted(stream, "d.caliber: %s\n", cal_type_strs[proto->item.data.weapon.caliber]);
            if (proto_save_pid_field(stream, &proto->item.data.weapon.ammoTypePid, "d.ammo_type_pid") == -1) {
                goto err;
            }
            filePrintFormatted(stream, "d.max_ammo: %d\n", proto->item.data.weapon.ammoCapacity);
            filePrintFormatted(stream, "d.sound_id: %c\n", proto->item.data.weapon.soundCode);
            break;
        case ITEM_TYPE_AMMO:
            filePrintFormatted(stream, "d.caliber: %s\n", cal_type_strs[proto->item.data.ammo.caliber]);
            filePrintFormatted(stream, "d.quantity: %d\n", proto->item.data.ammo.quantity);
            filePrintFormatted(stream, "d.ac_adjust: %d\n", proto->item.data.ammo.armorClassModifier);
            filePrintFormatted(stream, "d.dr_adjust: %d\n", proto->item.data.ammo.damageResistanceModifier);
            filePrintFormatted(stream, "d.dam_mult: %d\n", proto->item.data.ammo.damageMultiplier);
            filePrintFormatted(stream, "d.dam_div: %d\n", proto->item.data.ammo.damageDivisor);
            break;
        case ITEM_TYPE_MISC:
            if (proto->item.data.misc.powerTypePid != -1) {
                if (fprnt_pid_str(stream, proto->item.data.misc.powerTypePid, "d.power_type_pid") == -1) {
                    goto err;
                }
            }
            filePrintFormatted(stream, "d.power_type: %d\n", proto->item.data.misc.powerType);
            if (proto->item.data.misc.powerType != 0) {
                filePrintFormatted(stream, "d.charges: %d\n", proto->item.data.misc.charges);
            }
            break;
        case ITEM_TYPE_KEY:
            filePrintFormatted(stream, "d.key_code: %d\n", proto->item.data.key.keyCode);
            break;
        }

        filePrintFormatted(stream, "material: %s\n", gMaterialTypeNames[proto->item.material]);
        filePrintFormatted(stream, "size: %d\n", proto->item.size);
        filePrintFormatted(stream, "weight: %d\n", proto->item.weight);
        filePrintFormatted(stream, "cost: %d\n", proto->item.cost);
        if (fprnt_fid_txt(stream, proto->item.inventoryFid, "inv_fid: %u %s\n") != 0) {
            debugPrint("Err::write item:print inven_fid:%d:%.12s:\n", pid, protoGetName(proto->pid));
        }
        break;
    case OBJ_TYPE_CRITTER:
        filePrintFormatted(stream, "light_distance: %d\n", proto->critter.lightDistance);
        filePrintFormatted(stream, "light_intensity: %d\n", proto->critter.lightIntensity);
        filePrintFormatted(stream, "flags: %d\n", proto->critter.flags);
        filePrintFormatted(stream, "flags_ext: %d\n", proto->critter.extendedFlags);
        filePrintFormatted(stream, "sid: %d\n", proto->critter.sid);
        fprnt_fid_txt(stream, proto->critter.headFid, "head_fid: %u %s\n");
        filePrintFormatted(stream, "ai_packet: %d\n", proto->critter.aiPacket);
        filePrintFormatted(stream, "team_num: %d\n", proto->critter.team);
        filePrintFormatted(stream, "d.body: %d %s\n", proto->critter.data.bodyType, body_type_strs[proto->critter.data.bodyType]);
        filePrintFormatted(stream, "d.flags: %d\n", proto->critter.data.flags);
        for (int i = 0; i < 35; i++) {
            filePrintFormatted(stream, "d.stat_base%d: %d\n", i, proto->critter.data.baseStats[i]);
        }
        for (int i = 0; i < 35; i++) {
            filePrintFormatted(stream, "d.stat_bonus%d: %d\n", i, proto->critter.data.bonusStats[i]);
        }
        for (int i = 0; i < 18; i++) {
            filePrintFormatted(stream, "d.stat_points%d: %d\n", i, proto->critter.data.skills[i]);
        }
        break;
    case OBJ_TYPE_SCENERY:
        filePrintFormatted(stream, "light_distance: %d\n", proto->scenery.lightDistance);
        filePrintFormatted(stream, "light_intensity: %d\n", proto->scenery.lightIntensity);
        filePrintFormatted(stream, "flags: %d\n", proto->scenery.flags);
        filePrintFormatted(stream, "flags_ext: %d\n", proto->scenery.extendedFlags);
        filePrintFormatted(stream, "sid: %d\n", proto->scenery.sid);
        filePrintFormatted(stream, "type: %s\n", gSceneryTypeNames[proto->scenery.type]);

        switch (proto->scenery.type) {
        case SCENERY_TYPE_DOOR:
            filePrintFormatted(stream, "d.open_flags: %d\n", proto->scenery.data.door.openFlags);
            filePrintFormatted(stream, "d.key_code: %d\n", proto->scenery.data.door.keyCode);
            break;
        case SCENERY_TYPE_STAIRS:
        case SCENERY_TYPE_ELEVATOR:
            filePrintFormatted(stream, "d.lower_tile: %d\n", proto->scenery.data.stairs.destinationBuiltTile);
            filePrintFormatted(stream, "d.upper_tile: %d\n", proto->scenery.data.stairs.destinationMap);
            break;
        }

        filePrintFormatted(stream, "material: %s\n", gMaterialTypeNames[proto->scenery.material]);
        break;
    case OBJ_TYPE_WALL:
        filePrintFormatted(stream, "light_distance: %d\n", proto->wall.lightDistance);
        filePrintFormatted(stream, "light_intensity: %d\n", proto->wall.lightIntensity);
        filePrintFormatted(stream, "flags: %d\n", proto->wall.flags);
        filePrintFormatted(stream, "flags_ext: %d\n", proto->wall.extendedFlags);
        filePrintFormatted(stream, "sid: %d\n", proto->wall.sid);
        filePrintFormatted(stream, "material: %s\n", gMaterialTypeNames[proto->wall.material]);
        break;
    case OBJ_TYPE_TILE:
        filePrintFormatted(stream, "flags: %d\n", proto->tile.flags);
        filePrintFormatted(stream, "flags_ext: %d\n", proto->tile.extendedFlags);
        filePrintFormatted(stream, "material: %s\n", gMaterialTypeNames[proto->tile.material]);
        break;
    case OBJ_TYPE_MISC:
        filePrintFormatted(stream, "light_distance: %d\n", proto->misc.lightDistance);
        filePrintFormatted(stream, "light_intensity: %d\n", proto->misc.lightIntensity);
        filePrintFormatted(stream, "flags: %d\n", proto->misc.flags);
        filePrintFormatted(stream, "flags_ext: %d\n", proto->misc.extendedFlags);
        break;
    }

    rc = 0;

out:
    fileWriteString("\n\n", stream);
    fileClose(stream);
    return rc;

err:
    debugPrint("Error:fprnt_pid!\n");
    return -1;
}

// Splits a "key: value" line in place: trims the newline, null-terminates the key at the
// first ':', and returns the value with leading spaces skipped (or nullptr if no ':').
static char* proto_text_split(char* line)
{
    char* nl = strchr(line, '\n');
    if (nl != nullptr) {
        *nl = '\0';
    }

    char* colon = strchr(line, ':');
    if (colon == nullptr) {
        return nullptr;
    }

    *colon = '\0';
    char* value = colon + 1;
    while (*value == ' ') {
        value++;
    }
    return value;
}

// Skips a leading numeric token and the spaces after it, returning the start of the name token.
static const char* proto_text_skip_field(const char* text)
{
    while (*text != ' ') {
        text++;
    }
    while (*text == ' ') {
        text++;
    }
    return text;
}

// Case-insensitive lookup of str in list[count]; returns index, or -1 (logging the error).
static int strlistcmp(const char* str, const char* const* list, int count)
{
    for (int i = 0; i < count; i++) {
        if (compat_stricmp(str, list[i]) == 0) {
            return i;
        }
    }
    debugPrint("Err::read:strlistcmp:==>%s\n", str);
    return -1;
}

// Parses a "material" line into *material via the material name table. Returns true if handled.
static bool proto_read_material(const char* key, const char* value, int* material)
{
    if (strcmp(key, "material") != 0) {
        return false;
    }

    int index = strlistcmp(value, gMaterialTypeNames, MATERIAL_TYPE_COUNT);
    if (index != -1) {
        *material = index;
    }
    return true;
}

// Resolves a perk-code name to a perk value (-1 = None); leaves *outPerk untouched if unknown.
static void proto_read_perk(const char* str, int* outPerk)
{
    if (compat_stricmp(str, _mp_perk_code_None) == 0) {
        *outPerk = -1;
        return;
    }

    int index = strlistcmp(str, _mp_perk_code_strs, PERK_COUNT);
    if (index != -1) {
        *outPerk = index;
    }
}

// Resolves a drug-stat name to a stat value (-2 = special, -1 = None); leaves *outStat untouched if unknown.
static void proto_read_drug_stat(const char* str, int* outStat)
{
    if (compat_stricmp(str, _mp_critter_stats_list) == 0) {
        *outStat = -2;
        return;
    }
    if (compat_stricmp(str, _critter_stats_list_None) == 0) {
        *outStat = -1;
        return;
    }

    int index = strlistcmp(str, _critter_stats_list_strs, STAT_COUNT);
    if (index != -1) {
        *outStat = index;
    }
}

// Parses "<fidNum> <artname>" into *outFid. defaultType is the art type used for the fallback
// fid (255 means "use the fid's own type"). Returns -1 if the art name is not in its list.
static int proto_read_text_fid(const char* text, int* outFid, int defaultType)
{
    if (defaultType == 255) {
        defaultType = -1;
    }

    int fidNum = 0;
    sscanf(text, "%d", &fidNum);

    const char* name = proto_text_skip_field(text);

    int objectType = (fidNum & 0xF000000) >> 24;
    int index = artListIndex(objectType, name);
    if (index == -1) {
        int type = defaultType == -1 ? ((fidNum >> 24) & 0xFF) : defaultType;
        *outFid = buildFid(type, 0, 0, 0, 0);
        return -1;
    }

    int fid = buildFid(objectType, index, (fidNum & 0xFF0000) >> 16, (fidNum & 0xF000) >> 12, 0);
    if (!_art_fid_valid(fid)) {
        int type = defaultType == -1 ? ((fidNum >> 24) & 0xFF) : defaultType;
        fid = buildFid(type, 0, 0, 0, 0);
        debugPrint("\nError: proto_read_text_fid: art-check-valid failed: %s!", name);
    }

    *outFid = fid;
    return 0;
}

// Finds the 1-based line index of "<name>.PRO" in the type's proto .lst; returns it, or -1.
static int proto_find_str(int pid, const char* name)
{
    if (pid == -1) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    proto_make_path(path, pid);
    strcat(path, "\\");
    strcat(path, artGetObjectTypeName(PID_TYPE(pid)));
    strcat(path, ".lst");

    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    char target[COMPAT_MAX_PATH];
    strcpy(target, name);
    compat_strupr(target);
    strcat(target, ".PRO");

    int lineIndex = 0;
    int result = -1;
    char line[260];
    while (fileReadString(line, sizeof(line), stream) != nullptr) {
        if (line[0] != '\n' || line[1] != '\0') {
            lineIndex++;
        }

        char* nl = strchr(line, '\n');
        if (nl != nullptr) {
            *nl = '\0';
        }

        compat_strupr(line);
        if (strcmp(line, target) == 0) {
            result = lineIndex;
            break;
        }
    }

    fileClose(stream);
    return result;
}

// Resolves a "<pidNum> <protoname>" reference to an existing pid (remapped by name via the
// .lst), validated by protoGetProto. Sets *outPid to the pid or -1. Returns 0 on success, -1 otherwise.
static int proto_read_text_pid(char* text, int* outPid)
{
    int pidNum = 0;
    sscanf(text, "%d", &pidNum);

    const char* name = proto_text_skip_field(text);

    int foundId = proto_find_str(pidNum, name);
    if (foundId != -1) {
        int pid = (pidNum & 0xFF000000) | foundId;
        Proto* proto;
        if (protoGetProto(pid, &proto) != -1) {
            *outPid = pid;
            return 0;
        }
    }

    *outPid = -1;
    return -1;
}

// Applies a common proto header field. Returns 1 if handled, 0 if not a common field,
// -1 if the main fid's art name is unresolved (caller should stop parsing).
static int proto_read_common(const char* key, char* value, int pid, Proto* proto,
    int* lightDistance, int* lightIntensity, int* flags, int* extendedFlags, int* sid, bool clampFlags)
{
    int id = pid & 0xFFFFFF;

    if (strcmp(key, "pid") == 0) {
        proto->pid = pid;
        return 1;
    }
    if (strcmp(key, "name") == 0) {
        debugPrint("\n{%d}{}{name: %s}\n", 100 * id, value);
        return 1;
    }
    if (strcmp(key, "message_num") == 0) {
        sscanf(value, "%d", &proto->messageId);
        if (proto->messageId == -1) {
            proto->messageId = 100 * id;
        }
        return 1;
    }
    if (strcmp(key, "fid") == 0) {
        return proto_read_text_fid(value, &proto->fid, PID_TYPE(pid)) == -1 ? -1 : 1;
    }
    if (lightDistance != nullptr && strcmp(key, "light_distance") == 0) {
        sscanf(value, "%d", lightDistance);
        return 1;
    }
    if (lightIntensity != nullptr && strcmp(key, "light_intensity") == 0) {
        sscanf(value, "%d", lightIntensity);
        return 1;
    }
    if (flags != nullptr && strcmp(key, "flags") == 0) {
        sscanf(value, "%d", flags);
        if (clampFlags && *flags == static_cast<int>(0xCCCCCCCC)) {
            *flags = 0;
        }
        return 1;
    }
    if (extendedFlags != nullptr && strcmp(key, "flags_ext") == 0) {
        sscanf(value, "%d", extendedFlags);
        return 1;
    }
    if (sid != nullptr && strcmp(key, "sid") == 0) {
        sscanf(value, "%d", sid);
        return 1;
    }
    return 0;
}

// Matches "<prefix><n>" for n in [0, count) and parses the value into arr[n]. Returns true if matched.
static bool proto_read_indexed_int(const char* key, const char* value, const char* prefix, int* arr, int count)
{
    char name[36];
    for (int i = 0; i < count; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        if (strcmp(key, name) == 0) {
            sscanf(value, "%d", &arr[i]);
            return true;
        }
    }
    return false;
}

static void proto_load_armor_field(const char* key, char* value, Proto* proto)
{
    auto* armor = &proto->item.data.armor;
    if (strcmp(key, "d.ac") == 0) {
        sscanf(value, "%d", &armor->armorClass);
    } else if (strcmp(key, "d.perk") == 0) {
        proto_read_perk(value, &armor->perk);
    } else if (strcmp(key, "d.male_fid") == 0) {
        proto_read_text_fid(value, &armor->maleFid, 1);
    } else if (strcmp(key, "d.female_fid") == 0) {
        proto_read_text_fid(value, &armor->femaleFid, 1);
    } else if (proto_read_indexed_int(key, value, "d.dam_resist", armor->damageResistance, 7)) {
    } else if (proto_read_indexed_int(key, value, "d.dam_thresh", armor->damageThreshold, 7)) {
    }
}

// Matches "d.stat<n>" for n in [0,count) and applies the drug-stat name. Returns true if matched.
static bool proto_read_drug_stat_indexed(const char* key, const char* value, int* stats, int count)
{
    char name[36];
    for (int i = 0; i < count; i++) {
        snprintf(name, sizeof(name), "d.stat%d", i);
        if (strcmp(key, name) == 0) {
            proto_read_drug_stat(value, &stats[i]);
            return true;
        }
    }
    return false;
}

static void proto_load_drug_field(const char* key, char* value, Proto* proto)
{
    auto* drug = &proto->item.data.drug;
    if (strcmp(key, "d.stat") == 0) {
        proto_read_drug_stat(value, &drug->stat[0]);
    } else if (proto_read_drug_stat_indexed(key, value, drug->stat, 3)) {
    } else if (strcmp(key, "d.duration1") == 0) {
        sscanf(value, "%d", &drug->duration1);
    } else if (proto_read_indexed_int(key, value, "d.amount1_", drug->amount1, 3)) {
    } else if (strcmp(key, "d.duration2") == 0) {
        sscanf(value, "%d", &drug->duration2);
    } else if (proto_read_indexed_int(key, value, "d.amount2_", drug->amount2, 3)) {
    } else if (proto_read_indexed_int(key, value, "d.amount", drug->amount, 3)) {
    } else if (strcmp(key, "d.addiction_chance") == 0) {
        sscanf(value, "%d", &drug->addictionChance);
    } else if (strcmp(key, "d.withdrawal_effect") == 0) {
        sscanf(value, "%d", &drug->withdrawalEffect);
    } else if (strcmp(key, "d.withdrawal_onset") == 0) {
        sscanf(value, "%d", &drug->withdrawalOnset);
    }
}

static void proto_load_weapon_field(const char* key, char* value, Proto* proto)
{
    auto* weapon = &proto->item.data.weapon;
    if (strcmp(key, "d.flags") == 0) {
        int f = 0;
        sscanf(value, "%d", &f);
        if ((f & 1) != 0) {
            proto->item.extendedFlags |= PROTO_EXT_FLAG_IS_TWO_HANDED;
        }
    } else if (strcmp(key, "d.animation_code") == 0) {
        int index = strlistcmp(value, anim_code_strs, 11);
        if (index != -1) {
            weapon->animationCode = index;
        }
    } else if (strcmp(key, "d.min_damage") == 0) {
        sscanf(value, "%d", &weapon->minDamage);
    } else if (strcmp(key, "d.max_damage") == 0) {
        sscanf(value, "%d", &weapon->maxDamage);
    } else if (strcmp(key, "d.dt") == 0) {
        int index = strlistcmp(value, gDamageTypeNames, DAMAGE_TYPE_COUNT);
        if (index != -1) {
            weapon->damageType = index;
        }
    } else if (strcmp(key, "d.max_range1") == 0) {
        sscanf(value, "%d", &weapon->maxRange1);
    } else if (strcmp(key, "d.max_range2") == 0) {
        sscanf(value, "%d", &weapon->maxRange2);
    } else if (strcmp(key, "d.proj_pid") == 0) {
        proto_read_text_pid(value, &weapon->projectilePid);
    } else if (strcmp(key, "d.min_st") == 0) {
        sscanf(value, "%d", &weapon->minStrength);
    } else if (strcmp(key, "d.mp_cost1") == 0) {
        sscanf(value, "%d", &weapon->actionPointCost1);
    } else if (strcmp(key, "d.mp_cost2") == 0) {
        sscanf(value, "%d", &weapon->actionPointCost2);
    } else if (strcmp(key, "d.crit_fail_table") == 0) {
        sscanf(value, "%d", &weapon->criticalFailureType);
    } else if (strcmp(key, "d.perk") == 0) {
        proto_read_perk(value, &weapon->perk);
    } else if (strcmp(key, "d.rounds") == 0) {
        sscanf(value, "%d", &weapon->rounds);
    } else if (strcmp(key, "d.caliber") == 0) {
        int index = strlistcmp(value, cal_type_strs, CALIBER_TYPE_COUNT);
        if (index != -1) {
            weapon->caliber = index;
        }
    } else if (strcmp(key, "d.ammo_type_pid") == 0) {
        proto_read_text_pid(value, &weapon->ammoTypePid);
    } else if (strcmp(key, "d.max_ammo") == 0) {
        sscanf(value, "%d", &weapon->ammoCapacity);
    } else if (strcmp(key, "d.sound_id") == 0) {
        sscanf(value, "%c", reinterpret_cast<char*>(&weapon->soundCode));
    }
}

// Returns -1 if the inventory fid art name is unresolved (caller should stop parsing).
static int proto_load_item_field(const char* key, char* value, int pid, Proto* proto)
{
    if (strcmp(key, "type") == 0) {
        int index = strlistcmp(value, item_pro_type, ITEM_TYPE_COUNT);
        if (index != -1) {
            proto->item.type = index;
        } else if (strcmp(value, "Consumable") == 0) {
            proto->item.type = ITEM_TYPE_DRUG;
        } else if (strcmp(value, "Generic Item") == 0) {
            proto->item.type = ITEM_TYPE_MISC;
        }
        proto_item_subdata_init(proto, proto->item.type);
        return 0;
    }
    if (proto_read_material(key, value, &proto->item.material)) {
        return 0;
    }
    if (strcmp(key, "size") == 0) {
        sscanf(value, "%d", &proto->item.size);
        return 0;
    }
    if (strcmp(key, "weight") == 0) {
        sscanf(value, "%d", &proto->item.weight);
        return 0;
    }
    if (strcmp(key, "cost") == 0) {
        sscanf(value, "%d", &proto->item.cost);
        return 0;
    }
    if (strcmp(key, "inv_fid") == 0) {
        return proto_read_text_fid(value, &proto->item.inventoryFid, 7) == -1 ? -1 : 0;
    }

    if (proto->item.type == -1) {
        return 0;
    }

    switch (proto->item.type) {
    case ITEM_TYPE_ARMOR:
        proto_load_armor_field(key, value, proto);
        break;
    case ITEM_TYPE_CONTAINER:
        if (strcmp(key, "d.max_size") == 0) {
            sscanf(value, "%d", &proto->item.data.container.maxSize);
        } else if (strcmp(key, "d.open_flags") == 0) {
            sscanf(value, "%d", &proto->item.data.container.openFlags);
        }
        break;
    case ITEM_TYPE_DRUG:
        proto_load_drug_field(key, value, proto);
        break;
    case ITEM_TYPE_WEAPON:
        proto_load_weapon_field(key, value, proto);
        break;
    case ITEM_TYPE_AMMO:
        if (strcmp(key, "d.caliber") == 0) {
            int index = strlistcmp(value, cal_type_strs, CALIBER_TYPE_COUNT);
            if (index != -1) {
                proto->item.data.ammo.caliber = index;
            }
        } else if (strcmp(key, "d.quantity") == 0) {
            sscanf(value, "%d", &proto->item.data.ammo.quantity);
        } else if (strcmp(key, "d.ac_adjust") == 0) {
            sscanf(value, "%d", &proto->item.data.ammo.armorClassModifier);
        } else if (strcmp(key, "d.dr_adjust") == 0) {
            sscanf(value, "%d", &proto->item.data.ammo.damageResistanceModifier);
        } else if (strcmp(key, "d.dam_mult") == 0) {
            sscanf(value, "%d", &proto->item.data.ammo.damageMultiplier);
        } else if (strcmp(key, "d.dam_div") == 0) {
            sscanf(value, "%d", &proto->item.data.ammo.damageDivisor);
        }
        break;
    case ITEM_TYPE_MISC:
        if (strcmp(key, "d.power_type_pid") == 0) {
            proto_read_text_pid(value, &proto->item.data.misc.powerTypePid);
        } else if (strcmp(key, "d.power_type") == 0) {
            sscanf(value, "%d", &proto->item.data.misc.powerType);
        } else if (strcmp(key, "d.charges") == 0) {
            sscanf(value, "%d", &proto->item.data.misc.charges);
        }
        break;
    case ITEM_TYPE_KEY:
        if (strcmp(key, "d.key_code") == 0) {
            sscanf(value, "%d", &proto->item.data.key.keyCode);
        }
        break;
    }
    return 0;
}

// Returns -1 if the head fid art name is unresolved (caller should stop parsing).
static int proto_load_critter_field(const char* key, char* value, Proto* proto)
{
    if (strcmp(key, "head_fid") == 0) {
        return proto_read_text_fid(value, &proto->critter.headFid, 8) == -1 ? -1 : 0;
    }
    if (strcmp(key, "ai_packet") == 0) {
        sscanf(value, "%d", &proto->critter.aiPacket);
    } else if (strcmp(key, "team_num") == 0) {
        sscanf(value, "%d", &proto->critter.team);
    } else if (strcmp(key, "d.flags") == 0) {
        sscanf(value, "%d", &proto->critter.data.flags);
        if (proto->critter.data.flags == static_cast<int>(0xCCCCCCCC)) {
            proto->critter.data.flags = 0;
        }
    } else if (strcmp(key, "d.body") == 0) {
        int body = 0;
        sscanf(value, "%d", &body);
        proto->critter.data.bodyType = body;
        if (body > 3) {
            proto->critter.data.bodyType = 0;
        } else {
            int index = strlistcmp(proto_text_skip_field(value), body_type_strs, BODY_TYPE_COUNT);
            if (index != -1) {
                proto->critter.data.bodyType = index;
            }
        }
    }
    return 0;
}

static void proto_load_scenery_field(const char* key, char* value, Proto* proto)
{
    if (strcmp(key, "type") == 0) {
        int index = strlistcmp(value, gSceneryTypeNames, SCENERY_TYPE_COUNT);
        if (index != -1) {
            proto->scenery.type = index;
        }
        proto_scenery_subdata_init(proto, proto->scenery.type);
        return;
    }
    if (proto_read_material(key, value, &proto->scenery.material)) {
        return;
    }

    if (proto->scenery.type == -1) {
        return;
    }

    switch (proto->scenery.type) {
    case SCENERY_TYPE_DOOR:
        if (strcmp(key, "d.open_flags") == 0) {
            sscanf(value, "%d", &proto->scenery.data.door.openFlags);
        } else if (strcmp(key, "d.key_code") == 0) {
            sscanf(value, "%d", &proto->scenery.data.door.keyCode);
        }
        break;
    case SCENERY_TYPE_STAIRS:
    case SCENERY_TYPE_ELEVATOR:
        if (strcmp(key, "d.lower_tile") == 0) {
            sscanf(value, "%d", &proto->scenery.data.stairs.destinationBuiltTile);
        } else if (strcmp(key, "d.upper_tile") == 0) {
            sscanf(value, "%d", &proto->scenery.data.stairs.destinationMap);
        }
        break;
    }
}

// proto_load_text
int proto_load_text(int pid)
{
    int type = PID_TYPE(pid);

    Proto* proto;
    if (_proto_find_free_subnode(type, &proto) == -1) {
        return -1;
    }

    switch (type) {
    case OBJ_TYPE_ITEM:
        proto_item_init(proto, pid);
        break;
    case OBJ_TYPE_CRITTER:
        proto_critter_init(proto, pid);
        break;
    case OBJ_TYPE_SCENERY:
        proto_scenery_init(proto, pid);
        break;
    case OBJ_TYPE_WALL:
        proto_wall_init(proto, pid);
        break;
    case OBJ_TYPE_TILE:
        proto_tile_init(proto, pid);
        break;
    case OBJ_TYPE_MISC:
        proto_misc_init(proto, pid);
        break;
    }
    proto->pid = pid;

    char path[COMPAT_MAX_PATH];
    target_make_path(path, pid);
    size_t len = strlen(path);
    path[len] = '\\';
    _proto_list_str(pid, path + len + 1);

    char* dot = strchr(path + len, '.');
    if (dot != nullptr) {
        strcpy(dot + 1, "txt");
    } else {
        strcat(path, ".txt");
    }

    File* stream = fileOpenDirect(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    int* lightDistance = nullptr;
    int* lightIntensity = nullptr;
    int* flags = nullptr;
    int* extendedFlags = nullptr;
    int* sid = nullptr;
    bool clampFlags = false;
    switch (type) {
    case OBJ_TYPE_ITEM:
        lightDistance = &proto->item.lightDistance;
        lightIntensity = &proto->item.lightIntensity;
        flags = &proto->item.flags;
        extendedFlags = &proto->item.extendedFlags;
        sid = &proto->item.sid;
        break;
    case OBJ_TYPE_CRITTER:
        lightDistance = &proto->critter.lightDistance;
        lightIntensity = &proto->critter.lightIntensity;
        flags = &proto->critter.flags;
        extendedFlags = &proto->critter.extendedFlags;
        sid = &proto->critter.sid;
        clampFlags = true;
        break;
    case OBJ_TYPE_SCENERY:
        lightDistance = &proto->scenery.lightDistance;
        lightIntensity = &proto->scenery.lightIntensity;
        flags = &proto->scenery.flags;
        extendedFlags = &proto->scenery.extendedFlags;
        sid = &proto->scenery.sid;
        break;
    case OBJ_TYPE_WALL:
        lightDistance = &proto->wall.lightDistance;
        lightIntensity = &proto->wall.lightIntensity;
        flags = &proto->wall.flags;
        extendedFlags = &proto->wall.extendedFlags;
        sid = &proto->wall.sid;
        break;
    case OBJ_TYPE_TILE:
        flags = &proto->tile.flags;
        extendedFlags = &proto->tile.extendedFlags;
        break;
    case OBJ_TYPE_MISC:
        lightDistance = &proto->misc.lightDistance;
        lightIntensity = &proto->misc.lightIntensity;
        flags = &proto->misc.flags;
        extendedFlags = &proto->misc.extendedFlags;
        break;
    }

    char line[120];
    while (fileReadString(line, sizeof(line), stream) != nullptr) {
        char* value = proto_text_split(line);
        if (value == nullptr) {
            continue;
        }

        int rc = proto_read_common(line, value, pid, proto, lightDistance, lightIntensity, flags, extendedFlags, sid, clampFlags);
        if (rc == -1) {
            break;
        }
        if (rc == 1) {
            continue;
        }

        bool abort = false;
        switch (type) {
        case OBJ_TYPE_ITEM:
            abort = proto_load_item_field(line, value, pid, proto) == -1;
            break;
        case OBJ_TYPE_CRITTER:
            abort = proto_load_critter_field(line, value, proto) == -1;
            break;
        case OBJ_TYPE_SCENERY:
            proto_load_scenery_field(line, value, proto);
            break;
        case OBJ_TYPE_WALL:
            proto_read_material(line, value, &proto->wall.material);
            break;
        case OBJ_TYPE_TILE:
            proto_read_material(line, value, &proto->tile.material);
            break;
        }
        if (abort) {
            break;
        }
    }

    if (type == OBJ_TYPE_SCENERY && proto->scenery.type == -1) {
        proto->scenery.type = 0;
    }

    fileClose(stream);
    return 0;
}

} // namespace fallout
