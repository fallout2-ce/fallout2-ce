#include "proto_txt.h"

#include <stdio.h>
#include <string.h>

#include "art.h"
#include "db.h"
#include "debug.h"
#include "platform_compat.h"
#include "proto.h"

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

    File* stream = fileOpen(path, "wt");
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


} // namespace fallout
