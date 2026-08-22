#include "perk.h"

#include <stdio.h>

#include "debug.h"
#include "game.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "party_member.h"
#include "platform_compat.h"
#include "sfall_config.h"
#include "skill.h"
#include "stat.h"

#include <algorithm>
#include <string>

namespace fallout {

enum PerkParamMode {
    PERK_PARAM_MODE_FIRST_ONLY,
    PERK_PARAM_MODE_OR,
    PERK_PARAM_MODE_AND,
};

typedef struct PerkDescription {
    char* name;
    char* description;
    int frmId;
    int maxRank;
    int minLevel;
    // Critter stat to modify for every perk rank.
    Stat stat;
    // Stat modifier for every perk rank.
    int statModifier;
    // Skill number, normally. If bit 0x4000000 is set, will be treated as global var number instead.
    int param1;
    // Required value of a skill or global var.
    int value1;
    // Specifies wether to require both params, either one or just use the first one.
    int paramMode;
    // Skill or gvar number, see param1.
    int param2;
    // Required value of a skill or global var.
    int value2;
    // Required minimum value for every primary stat.
    int stats[PRIMARY_STAT_COUNT];
} PerkDescription;

typedef struct PerkRankData {
    int ranks[PERK_COUNT];
} PerkRankData;

typedef struct PerkTweaks {
    int nightVisionBonus = 20;
    int survivalistBonus = 25;
    int masterTraderBonus = 25;
    int educatedBonus = 2;
    int healerMinBonus = 4;
    int healerMaxBonus = 10;
    int lifegiverBonus = 4;
    int mrFixitBonus = 10;
    int medicFirstAidBonus = 10;
    int medicDoctorBonus = 10;
    int masterThiefBonus = 15;
    int speakerBonus = 20;
    int ghostBonus = 20;
    int rangerOutdoorsmanBonus = 15;
    int weaponLongRangeBonus = 4;
    int weaponAccurateBonus = 20;
    int weaponScopeRangePenalty = 8;
    int weaponScopeRangeBonus = 5;
    int vaultCityInoculationsPoisonBonus = 10;
    int vaultCityInoculationsRadBonus = 10;
    int cautiousNatureBonus = 3;
    int comprehensionBonus = 50;
    int demolitionExpertBonus = 10;
    int gamblerBonus = 20;
    int harmlessBonus = 20;
    int livingAnatomyBonus = 5;
    int livingAnatomyDoctorBonus = 10;
    int negotiatorBonus = 10;
    int pyromaniacBonus = 5;
    int salesmanBonus = 20;
    int stonewallPercent = 50;
    int thiefBonus = 10;
    int weaponHandlingBonus = 3;
    int vaultCityTrainingFirstAidBonus = 5;
    int vaultCityTrainingDoctorBonus = 5;
    int expertExcrementExpeditorBonus = 5;
} PerkTweaks;

static PerkRankData* perkGetRankData(Object* critter);
static bool perkCanAddAtLevel(Object* critter, Perk perk, int level);
static void perkResetRanks();
static void perksLoadSfallConfig();
static void perksLoadSfallTweaks(Config* config);
static void perksLoadSfallData(Config* config);
static bool perksGetLimitedInt(Config* config, const char* key, int defaultValue, int minValue, int maxValue, int* valuePtr);
static void perksLoadSfallPerkInt(Config* config, const char* sectionKey, const char* key, int* valuePtr);
static void perksLoadSfallPerkStat(Config* config, const char* sectionKey, const char* key, Stat* valuePtr);

// 0x519DCC perk_data
static PerkDescription gPerkDescriptions[PERK_COUNT] = {
    { nullptr, nullptr, 72, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 5, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 73, 1, 15, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 6, 0 },
    { nullptr, nullptr, 74, 3, 3, STAT_MELEE_DAMAGE, 2, -1, 0, 0, -1, 0, 6, 0, 0, 0, 0, 6, 0 },
    { nullptr, nullptr, 75, 2, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 76, 2, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 6, 6 },
    { nullptr, nullptr, 77, 1, 15, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 6, 7, 0 },
    { nullptr, nullptr, 78, 3, 3, STAT_SEQUENCE, 2, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 79, 3, 3, STAT_HEALING_RATE, 2, -1, 0, 0, -1, 0, 0, 0, 6, 0, 0, 0, 0 },
    { nullptr, nullptr, 80, 3, 6, STAT_CRITICAL_CHANCE, 5, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 6 },
    { nullptr, nullptr, 81, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 82, 3, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 6, 0, 0, 0 },
    { nullptr, nullptr, 83, 2, 6, STAT_RADIATION_RESISTANCE, 15, -1, 0, 0, -1, 0, 0, 0, 6, 0, 4, 0, 0 },
    { nullptr, nullptr, 84, 3, 3, STAT_DAMAGE_RESISTANCE, 10, -1, 0, 0, -1, 0, 0, 0, 6, 0, 0, 0, 6 },
    { nullptr, nullptr, 85, 3, 3, STAT_CARRY_WEIGHT, 50, -1, 0, 0, -1, 0, 6, 0, 6, 0, 0, 0, 0 },
    { nullptr, nullptr, 86, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 7, 0, 0, 6, 0, 0 },
    { nullptr, nullptr, 87, 1, 6, STAT_INVALID, 0, 8, 50, 0, -1, 0, 0, 0, 0, 0, 0, 6, 0 },
    { nullptr, nullptr, 88, 1, 3, STAT_INVALID, 0, 17, 40, 0, -1, 0, 0, 0, 6, 0, 6, 0, 0 },
    { nullptr, nullptr, 89, 1, 12, STAT_INVALID, 0, 15, 75, 0, -1, 0, 0, 0, 0, 7, 0, 0, 0 },
    { nullptr, nullptr, 90, 3, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 6, 0, 0 },
    { nullptr, nullptr, 91, 2, 3, STAT_INVALID, 0, 6, 40, 0, -1, 0, 0, 7, 0, 0, 5, 6, 0 },
    { nullptr, nullptr, 92, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 8 },
    { nullptr, nullptr, 93, 1, 9, STAT_BETTER_CRITICALS, 20, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 4, 6 },
    { nullptr, nullptr, 94, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 7, 0, 0, 5, 0, 0 },
    { nullptr, nullptr, 95, 1, 24, STAT_INVALID, 0, 3, 80, 0, -1, 0, 8, 0, 0, 0, 0, 8, 0 },
    { nullptr, nullptr, 96, 1, 24, STAT_INVALID, 0, 0, 80, 0, -1, 0, 0, 8, 0, 0, 0, 8, 0 },
    { nullptr, nullptr, 97, 1, 18, STAT_INVALID, 0, 8, 80, 2, 3, 80, 0, 0, 0, 0, 0, 10, 0 },
    { nullptr, nullptr, 98, 2, 12, STAT_MAXIMUM_ACTION_POINTS, 1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 99, 1, 310, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 100, 2, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 4, 0, 0, 0, 0 },
    { nullptr, nullptr, 101, 1, 9, STAT_ARMOR_CLASS, 5, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 6, 0 },
    { nullptr, nullptr, 102, 2, 6, STAT_POISON_RESISTANCE, 25, -1, 0, 0, -1, 0, 0, 0, 3, 0, 0, 0, 0 },
    { nullptr, nullptr, 103, 1, 12, STAT_INVALID, 0, 13, 40, 1, 12, 40, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 104, 1, 12, STAT_INVALID, 0, 6, 40, 1, 7, 40, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 105, 1, 12, STAT_INVALID, 0, 10, 50, 2, 9, 50, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 106, 1, 9, STAT_INVALID, 0, 14, 50, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 107, 3, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, -9, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 108, 1, 310, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 4, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 109, 1, 15, STAT_INVALID, 0, 10, 80, 0, -1, 0, 0, 0, 0, 0, 0, 8, 0 },
    { nullptr, nullptr, 110, 1, 6, STAT_INVALID, 0, 8, 60, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 111, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 10, 0, 0, 0 },
    { nullptr, nullptr, 112, 1, 310, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 8 },
    { nullptr, nullptr, 113, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 114, 1, 310, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 5, 0, 0, 0, 0 },
    { nullptr, nullptr, 115, 2, 6, STAT_INVALID, 0, 17, 40, 0, -1, 0, 0, 0, 6, 0, 0, 0, 0 },
    { nullptr, nullptr, 116, 1, 310, STAT_INVALID, 0, 17, 25, 0, -1, 0, 0, 0, 0, 0, 5, 0, 0 },
    { nullptr, nullptr, 117, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 7, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 118, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 4 },
    { nullptr, nullptr, 119, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 120, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 121, 3, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 4, 0, 0 },
    { nullptr, nullptr, 122, 3, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 4, 0, 0 },
    { nullptr, nullptr, 123, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 124, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 125, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 126, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, -2, 0, -2, 0, 0, -3, 0 },
    { nullptr, nullptr, 127, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -3, -2, 0 },
    { nullptr, nullptr, 128, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -2, 0, 0 },
    { nullptr, nullptr, 129, -1, 1, STAT_RADIATION_RESISTANCE, -20, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 130, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 131, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 132, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 133, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 134, -1, 1, STAT_RADIATION_RESISTANCE, 30, -1, 0, 0, -1, 0, 3, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 135, -1, 1, STAT_RADIATION_RESISTANCE, 20, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 136, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 137, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 138, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 139, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 140, -1, 1, STAT_RADIATION_RESISTANCE, 60, -1, 0, 0, -1, 0, 4, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 141, -1, 1, STAT_RADIATION_RESISTANCE, 75, -1, 0, 0, -1, 0, 4, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 136, -1, 1, STAT_MAXIMUM_ACTION_POINTS, -1, -1, 0, 0, -1, 0, -1, -1, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 149, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, -2, 0, 0, -1, 0, -1 },
    { nullptr, nullptr, 154, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 2, 0, 0, 0 },
    { nullptr, nullptr, 158, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 157, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 157, -1, 1, STAT_CHARISMA, -1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 168, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 168, -1, 1, STAT_CHARISMA, -1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 172, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 155, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, -10, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 156, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 122, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 6, 0, 0 },
    { nullptr, nullptr, 39, 1, 9, STAT_INVALID, 0, 11, 75, 0, -1, 0, 0, 0, 0, 0, 0, 4, 0 },
    { nullptr, nullptr, 44, 1, 6, STAT_INVALID, 0, 16, 50, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 0, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, -10, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 1, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, -10, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 2, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, -10, 0, 0, 0, 0 },
    { nullptr, nullptr, 3, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, -10, 0, 0, 0 },
    { nullptr, nullptr, 4, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -10, 0, 0 },
    { nullptr, nullptr, 5, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, -10, 0 },
    { nullptr, nullptr, 6, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -10 },
    { nullptr, nullptr, 160, 1, 6, STAT_INVALID, 0, 10, 50, 2, 0x4000000, 50, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 161, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 159, 1, 12, STAT_INVALID, 0, 3, 75, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 163, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 5, 0, 0, 5, 0 },
    { nullptr, nullptr, 162, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 6, 0, 0, 0 },
    { nullptr, nullptr, 164, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 5 },
    { nullptr, nullptr, 165, 1, 12, STAT_INVALID, 0, 7, 60, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 166, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, -10, 0, 0, 0 },
    { nullptr, nullptr, 43, 1, 6, STAT_INVALID, 0, 15, 50, 2, 14, 50, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 167, 1, 6, STAT_CARRY_WEIGHT, 50, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 169, 1, 9, STAT_INVALID, 0, 1, 75, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 170, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 121, 1, 6, STAT_INVALID, 0, 15, 50, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 171, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 6, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 38, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 173, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, -7, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 104, -1, 1, STAT_INVALID, 0, 7, 75, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 142, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 142, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 52, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 52, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 104, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 104, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 35, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 35, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 154, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 154, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 64, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
};

// An array of perk ranks for each party member.
//
// 0x51C120 perkLevelDataList
static PerkRankData* gPartyMemberPerkRanks = nullptr;

// Amount of experience points granted when player selected "Here and now"
// perk.
//
// 0x51C124 hereAndNowExps
static int hereAndNowBonusExperience = 0;

// perk.msg
//
// 0x6642D4 perk_message_file
static MessageList gPerksMessageList;

static PerkTweaks perkTweaks;
static std::string perkOverrideNames[PERK_COUNT];
static std::string perkOverrideDescriptions[PERK_COUNT];

// 0x4965A0 perk_init
int perksInit()
{
    gPartyMemberPerkRanks = (PerkRankData*)internal_malloc(sizeof(*gPartyMemberPerkRanks) * gPartyMemberDescriptionsLength);
    if (gPartyMemberPerkRanks == nullptr) {
        return -1;
    }

    perkResetRanks();

    if (!messageListInit(&gPerksMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "perk.msg");

    if (!messageListLoad(&gPerksMessageList, path)) {
        return -1;
    }

    for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
        MessageListItem messageListItem;

        messageListItem.num = 101 + perk;
        if (messageListGetItem(&gPerksMessageList, &messageListItem)) {
            gPerkDescriptions[perk].name = messageListItem.text;
        }

        messageListItem.num = 1101 + perk;
        if (messageListGetItem(&gPerksMessageList, &messageListItem)) {
            gPerkDescriptions[perk].description = messageListItem.text;
        }
    }

    perksLoadSfallConfig();

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_PERK, &gPerksMessageList);

    return 0;
}

static void perksLoadSfallConfig()
{
    char* perksFile = nullptr;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PERKS_FILE_KEY, &perksFile);
    if (perksFile == nullptr || perksFile[0] == '\0') {
        return;
    }

    ScopedConfig config { perksFile, false };
    if (!config) {
        debugPrint("Perks config %s not found.\n", perksFile);
        return;
    }

    perksLoadSfallTweaks(config.get());

    int enabled = 1;
    configGetInt(config.get(), "Perks", "Enable", &enabled, 1);
    if (enabled != 0) {
        perksLoadSfallData(config.get());
    }
}

static bool perksGetLimitedInt(Config* config, const char* key, int defaultValue, int minValue, int maxValue, int* valuePtr)
{
    int value = 0;
    if (!configGetInt(config, "PerksTweak", key, &value, defaultValue) || value < minValue) {
        return false;
    }

    *valuePtr = std::min(value, maxValue);
    return true;
}

static void perksLoadSfallTweaks(Config* config)
{
    perksGetLimitedInt(config, "NightVisionBonus", 20, 0, 100, &(perkTweaks.nightVisionBonus));
    perksGetLimitedInt(config, "SurvivalistBonus", 25, 0, 125, &(perkTweaks.survivalistBonus));
    perksGetLimitedInt(config, "MasterTraderBonus", 25, 0, 999, &(perkTweaks.masterTraderBonus));
    perksGetLimitedInt(config, "EducatedBonus", 2, 0, 125, &(perkTweaks.educatedBonus));
    perksGetLimitedInt(config, "HealerMinBonus", 4, 0, 999, &(perkTweaks.healerMinBonus));
    perksGetLimitedInt(config, "HealerMaxBonus", 10, 0, 999, &(perkTweaks.healerMaxBonus));
    perksGetLimitedInt(config, "LifegiverBonus", 4, 0, 125, &(perkTweaks.lifegiverBonus));
    perksGetLimitedInt(config, "MrFixitBonus", 10, 0, 125, &(perkTweaks.mrFixitBonus));
    perksGetLimitedInt(config, "MedicFirstAidBonus", 10, 0, 125, &(perkTweaks.medicFirstAidBonus));
    perksGetLimitedInt(config, "MedicDoctorBonus", 10, 0, 125, &(perkTweaks.medicDoctorBonus));
    perksGetLimitedInt(config, "MasterThiefBonus", 15, 0, 125, &(perkTweaks.masterThiefBonus));
    perksGetLimitedInt(config, "SpeakerBonus", 20, 0, 125, &(perkTweaks.speakerBonus));
    perksGetLimitedInt(config, "GhostBonus", 20, 0, 125, &(perkTweaks.ghostBonus));
    perksGetLimitedInt(config, "RangerOutdoorsmanBonus", 15, 0, 125, &(perkTweaks.rangerOutdoorsmanBonus));
    perksGetLimitedInt(config, "WeaponLongRangeBonus", 4, 2, 100, &(perkTweaks.weaponLongRangeBonus));
    perksGetLimitedInt(config, "WeaponAccurateBonus", 20, 0, 125, &(perkTweaks.weaponAccurateBonus));
    perksGetLimitedInt(config, "WeaponScopeRangePenalty", 8, 0, 100, &(perkTweaks.weaponScopeRangePenalty));
    perksGetLimitedInt(config, "WeaponScopeRangeBonus", 5, 2, 100, &(perkTweaks.weaponScopeRangeBonus));
    perksGetLimitedInt(config, "VaultCityInoculationsPoisonBonus", 10, -100, 100, &(perkTweaks.vaultCityInoculationsPoisonBonus));
    perksGetLimitedInt(config, "VaultCityInoculationsRadBonus", 10, -100, 100, &(perkTweaks.vaultCityInoculationsRadBonus));
    perksGetLimitedInt(config, "CautiousNatureBonus", 3, -12, 20, &(perkTweaks.cautiousNatureBonus));
    perksGetLimitedInt(config, "ComprehensionBonus", 50, 0, 999, &(perkTweaks.comprehensionBonus));
    perksGetLimitedInt(config, "DemolitionExpertBonus", 10, 0, 999, &(perkTweaks.demolitionExpertBonus));
    perksGetLimitedInt(config, "GamblerBonus", 20, 0, 125, &(perkTweaks.gamblerBonus));
    perksGetLimitedInt(config, "HarmlessBonus", 20, 0, 125, &(perkTweaks.harmlessBonus));
    perksGetLimitedInt(config, "LivingAnatomyBonus", 5, 0, 125, &(perkTweaks.livingAnatomyBonus));
    perksGetLimitedInt(config, "LivingAnatomyDoctorBonus", 10, 0, 125, &(perkTweaks.livingAnatomyDoctorBonus));
    perksGetLimitedInt(config, "NegotiatorBonus", 10, 0, 125, &(perkTweaks.negotiatorBonus));
    perksGetLimitedInt(config, "PyromaniacBonus", 5, 0, 125, &(perkTweaks.pyromaniacBonus));
    perksGetLimitedInt(config, "SalesmanBonus", 20, 0, 999, &(perkTweaks.salesmanBonus));
    perksGetLimitedInt(config, "StonewallPercent", 50, 0, 100, &(perkTweaks.stonewallPercent));
    perksGetLimitedInt(config, "ThiefBonus", 10, 0, 125, &(perkTweaks.thiefBonus));
    perksGetLimitedInt(config, "WeaponHandlingBonus", 3, 0, 10, &(perkTweaks.weaponHandlingBonus));
    perksGetLimitedInt(config, "VaultCityTrainingFirstAidBonus", 5, 0, 125, &(perkTweaks.vaultCityTrainingFirstAidBonus));
    perksGetLimitedInt(config, "VaultCityTrainingDoctorBonus", 5, 0, 125, &(perkTweaks.vaultCityTrainingDoctorBonus));
    perksGetLimitedInt(config, "ExpertExcrementExpeditorBonus", 5, 0, 125, &(perkTweaks.expertExcrementExpeditorBonus));
}

static void perksLoadSfallPerkInt(Config* config, const char* sectionKey, const char* key, int* valuePtr)
{
    int value = 0;
    if (configGetIntBase(config, sectionKey, key, &value, -99999, 0) && value != -99999) {
        *valuePtr = value;
    }
}

static void perksLoadSfallPerkStat(Config* config, const char* sectionKey, const char* key, Stat* valuePtr)
{
    int value = 0;
    if (configGetIntBase(config, sectionKey, key, &value, -99999, 0) && value != -99999) {
        *valuePtr = static_cast<Stat>(value);
    }
}

static void perksLoadSfallData(Config* config)
{
    char sectionKey[8];
    for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
        snprintf(sectionKey, sizeof(sectionKey), "%d", perk);

        PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

        char* string = nullptr;
        if (configGetString(config, sectionKey, "Name", &string) && string != nullptr) {
            perkOverrideNames[perk] = string;
            perkDescription->name = perkOverrideNames[perk].data();
        }

        if (configGetString(config, sectionKey, "Desc", &string) && string != nullptr) {
            perkOverrideDescriptions[perk] = string;
            perkDescription->description = perkOverrideDescriptions[perk].data();
        }

        perksLoadSfallPerkInt(config, sectionKey, "Image", &(perkDescription->frmId));
        perksLoadSfallPerkInt(config, sectionKey, "Ranks", &(perkDescription->maxRank));
        perksLoadSfallPerkInt(config, sectionKey, "Level", &(perkDescription->minLevel));
        perksLoadSfallPerkStat(config, sectionKey, "Stat", &(perkDescription->stat));
        perksLoadSfallPerkInt(config, sectionKey, "StatMag", &(perkDescription->statModifier));
        perksLoadSfallPerkInt(config, sectionKey, "Skill1", &(perkDescription->param1));
        perksLoadSfallPerkInt(config, sectionKey, "Skill1Mag", &(perkDescription->value1));
        perksLoadSfallPerkInt(config, sectionKey, "Type", &(perkDescription->paramMode));
        perksLoadSfallPerkInt(config, sectionKey, "Skill2", &(perkDescription->param2));
        perksLoadSfallPerkInt(config, sectionKey, "Skill2Mag", &(perkDescription->value2));
        perksLoadSfallPerkInt(config, sectionKey, "STR", &(perkDescription->stats[STAT_STRENGTH]));
        perksLoadSfallPerkInt(config, sectionKey, "PER", &(perkDescription->stats[STAT_PERCEPTION]));
        perksLoadSfallPerkInt(config, sectionKey, "END", &(perkDescription->stats[STAT_ENDURANCE]));
        perksLoadSfallPerkInt(config, sectionKey, "CHR", &(perkDescription->stats[STAT_CHARISMA]));
        perksLoadSfallPerkInt(config, sectionKey, "INT", &(perkDescription->stats[STAT_INTELLIGENCE]));
        perksLoadSfallPerkInt(config, sectionKey, "AGL", &(perkDescription->stats[STAT_AGILITY]));
        perksLoadSfallPerkInt(config, sectionKey, "LCK", &(perkDescription->stats[STAT_LUCK]));
    }
}

// 0x4966B0 perk_reset
void perksReset()
{
    perkResetRanks();
}

// 0x4966B8 perk_exit
void perksExit()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_PERK, nullptr);
    messageListFree(&gPerksMessageList);

    if (gPartyMemberPerkRanks != nullptr) {
        internal_free(gPartyMemberPerkRanks);
        gPartyMemberPerkRanks = nullptr;
    }
}

// 0x4966E4 perk_load
int perksLoad(File* stream)
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        PerkRankData* ranksData = &(gPartyMemberPerkRanks[index]);
        for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
            if (fileReadInt32(stream, &(ranksData->ranks[perk])) == -1) {
                return -1;
            }
        }
    }

    return 0;
}

// 0x496738 perk_save
int perksSave(File* stream)
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        PerkRankData* ranksData = &(gPartyMemberPerkRanks[index]);
        for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
            if (fileWriteInt32(stream, ranksData->ranks[perk]) == -1) {
                return -1;
            }
        }
    }

    return 0;
}

// perkGetLevelData
// 0x49678C perkGetLevelData
static PerkRankData* perkGetRankData(Object* critter)
{
    if (critter == gDude) {
        return gPartyMemberPerkRanks;
    }

    for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
        if (critter->pid == gPartyMemberPids[index]) {
            return gPartyMemberPerkRanks + index;
        }
    }

    debugPrint("\nError: perkGetLevelData: Can't find party member match!");

    return gPartyMemberPerkRanks;
}

// 0x49680C perk_can_add
static bool perkCanAddAtLevel(Object* critter, Perk perk, int level)
{
    if (!perkIsValid(perk)) {
        return false;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    if (perkDescription->maxRank == -1) {
        return false;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    if (ranksData->ranks[perk] >= perkDescription->maxRank) {
        return false;
    }

    if (critter == gDude) {
        if (level < perkDescription->minLevel) {
            return false;
        }

        if (perk == PERK_HERE_AND_NOW && pcGetExperienceForLevel(pcGetStat(PC_STAT_LEVEL) + 1) == -1) {
            return false;
        }
    }

    bool req1Fulfilled = true;

    int param1 = perkDescription->param1;
    if (param1 != -1) {
        bool isVariable = false;
        if ((param1 & 0x4000000) != 0) {
            isVariable = true;
            param1 &= ~0x4000000;
        }

        int value1 = perkDescription->value1;
        if (value1 < 0) {
            if (isVariable) {
                if (gameGetGlobalVar(static_cast<GameGlobalVar>(param1)) >= -value1) {
                    req1Fulfilled = false;
                }
            } else {
                if (skillGetValue(critter, static_cast<Skill>(param1)) >= -value1) {
                    req1Fulfilled = false;
                }
            }
        } else {
            if (isVariable) {
                if (gameGetGlobalVar(static_cast<GameGlobalVar>(param1)) < value1) {
                    req1Fulfilled = false;
                }
            } else {
                if (skillGetValue(critter, static_cast<Skill>(param1)) < value1) {
                    req1Fulfilled = false;
                }
            }
        }
    }

    if (!req1Fulfilled || perkDescription->paramMode == PERK_PARAM_MODE_AND) {
        if (perkDescription->paramMode == PERK_PARAM_MODE_FIRST_ONLY) {
            return false;
        }

        if (!req1Fulfilled && perkDescription->paramMode == PERK_PARAM_MODE_AND) {
            return false;
        }

        int param2 = perkDescription->param2;
        bool isVariable = false;
        if (param2 != -1) {
            if ((param2 & 0x4000000) != 0) {
                isVariable = true;
                param2 &= ~0x4000000;
            }
        }

        if (param2 == -1) {
            return false;
        }

        int value2 = perkDescription->value2;
        if (value2 < 0) {
            if (isVariable) {
                if (gameGetGlobalVar(static_cast<GameGlobalVar>(param2)) >= -value2) {
                    return false;
                }
            } else {
                if (skillGetValue(critter, static_cast<Skill>(param2)) >= -value2) {
                    return false;
                }
            }
        } else {
            if (isVariable) {
                if (gameGetGlobalVar(static_cast<GameGlobalVar>(param2)) < value2) {
                    return false;
                }
            } else {
                if (skillGetValue(critter, static_cast<Skill>(param2)) < value2) {
                    return false;
                }
            }
        }
    }

    for (Stat stat = STAT_FIRST; stat < PRIMARY_STAT_COUNT; stat++) {
        if (perkDescription->stats[stat] < 0) {
            if (critterGetStat(critter, stat) >= -perkDescription->stats[stat]) {
                return false;
            }
        } else {
            if (critterGetStat(critter, stat) < perkDescription->stats[stat]) {
                return false;
            }
        }
    }

    return true;
}

// Resets party member perks.
//
// 0x496A0C perk_defaults
static void perkResetRanks()
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        PerkRankData* ranksData = &(gPartyMemberPerkRanks[index]);
        for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
            ranksData->ranks[perk] = 0;
        }
    }
}

// 0x496A5C perk_add
int perkAdd(Object* critter, Perk perk)
{
    return perkAddAtLevel(critter, perk, pcGetStat(PC_STAT_LEVEL));
}

int perkAddAtLevel(Object* critter, Perk perk, int level)
{
    if (!perkIsValid(perk)) {
        return -1;
    }

    if (!perkCanAddAtLevel(critter, perk, level)) {
        return -1;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    ranksData->ranks[perk] += 1;

    perkAddEffect(critter, perk);

    return 0;
}

// perk_add_force
// 0x496A9C perk_add_force
int perkAddForce(Object* critter, Perk perk)
{
    if (!perkIsValid(perk)) {
        return -1;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    int value = ranksData->ranks[perk];

    int maxRank = gPerkDescriptions[perk].maxRank;

    if (maxRank != -1 && value >= maxRank) {
        return -1;
    }

    ranksData->ranks[perk] += 1;

    perkAddEffect(critter, perk);

    return 0;
}

// perk_sub
// 0x496AFC perk_sub
int perkRemove(Object* critter, Perk perk)
{
    if (!perkIsValid(perk)) {
        return -1;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    int value = ranksData->ranks[perk];

    if (value < 1) {
        return -1;
    }

    ranksData->ranks[perk] -= 1;

    perkRemoveEffect(critter, perk);

    return 0;
}

// Returns perks available to pick.
//
// 0x496B44 perk_make_list
int perkGetAvailablePerks(Object* critter, int level, Perk* perks)
{
    int count = 0;
    for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
        if (perkCanAddAtLevel(critter, perk, level)) {
            perks[count] = perk;
            count++;
        }
    }
    return count;
}

// has_perk
// 0x496B78 perk_level
int perkGetRank(Object* critter, Perk perk)
{
    if (!perkIsValid(perk)) {
        return 0;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    return ranksData->ranks[perk];
}

// 0x496B90 perk_name
char* perkGetName(Perk perk)
{
    if (!perkIsValid(perk)) {
        return nullptr;
    }
    return gPerkDescriptions[perk].name;
}

// 0x496BB4 perk_description
char* perkGetDescription(Perk perk)
{
    if (!perkIsValid(perk)) {
        return nullptr;
    }
    return gPerkDescriptions[perk].description;
}

// 0x496BD8 perk_skilldex_fid
int perkGetFrmId(Perk perk)
{
    if (!perkIsValid(perk)) {
        return 0;
    }
    return gPerkDescriptions[perk].frmId;
}

bool perkSetProperty(Perk perk, PerkProperty property, int value)
{
    if (!perkIsValid(perk)) {
        return false;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    switch (property) {
    case PerkProperty::FrmId:
        perkDescription->frmId = value;
        break;
    case PerkProperty::MaxRank:
        perkDescription->maxRank = value;
        break;
    case PerkProperty::MinLevel:
        perkDescription->minLevel = value;
        break;
    case PerkProperty::Stat:
        perkDescription->stat = static_cast<Stat>(value);
        break;
    case PerkProperty::StatModifier:
        perkDescription->statModifier = value;
        break;
    case PerkProperty::Param1:
        perkDescription->param1 = value;
        break;
    case PerkProperty::Value1:
        perkDescription->value1 = value;
        break;
    case PerkProperty::ParamMode:
        perkDescription->paramMode = value;
        break;
    case PerkProperty::Param2:
        perkDescription->param2 = value;
        break;
    case PerkProperty::Value2:
        perkDescription->value2 = value;
        break;
    case PerkProperty::Strength:
        perkDescription->stats[STAT_STRENGTH] = value;
        break;
    case PerkProperty::Perception:
        perkDescription->stats[STAT_PERCEPTION] = value;
        break;
    case PerkProperty::Endurance:
        perkDescription->stats[STAT_ENDURANCE] = value;
        break;
    case PerkProperty::Charisma:
        perkDescription->stats[STAT_CHARISMA] = value;
        break;
    case PerkProperty::Intelligence:
        perkDescription->stats[STAT_INTELLIGENCE] = value;
        break;
    case PerkProperty::Agility:
        perkDescription->stats[STAT_AGILITY] = value;
        break;
    case PerkProperty::Luck:
        perkDescription->stats[STAT_LUCK] = value;
        break;
    }

    return true;
}

bool perkSetName(Perk perk, const char* value)
{
    if (!perkIsValid(perk) || value == nullptr) {
        return false;
    }

    perkOverrideNames[perk] = value;
    gPerkDescriptions[perk].name = perkOverrideNames[perk].data();
    return true;
}

bool perkSetDescription(Perk perk, const char* value)
{
    if (!perkIsValid(perk) || value == nullptr) {
        return false;
    }

    perkOverrideDescriptions[perk] = value;
    gPerkDescriptions[perk].description = perkOverrideDescriptions[perk].data();
    return true;
}

// perk_add_effect
// 0x496BFC perk_add_effect
void perkAddEffect(Object* critter, Perk perk)
{
    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        debugPrint("\nERROR: perk_add_effect: Was called on non-critter!");
        return;
    }

    if (!perkIsValid(perk)) {
        return;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    if (perkDescription->stat != -1) {
        int value = critterGetBonusStat(critter, perkDescription->stat);
        critterSetBonusStat(critter, perkDescription->stat, value + perkDescription->statModifier);
    }

    if (perk == PERK_HERE_AND_NOW) {
        PerkRankData* ranksData = perkGetRankData(critter);
        ranksData->ranks[PERK_HERE_AND_NOW] -= 1;

        int level = pcGetStat(PC_STAT_LEVEL);
        int nextLevelExperience = pcGetExperienceForLevel(level + 1);

        hereAndNowBonusExperience = nextLevelExperience >= 0 ? nextLevelExperience - pcGetStat(PC_STAT_EXPERIENCE) : 0;
        pcAddExperienceWithOptions(hereAndNowBonusExperience, false);

        ranksData->ranks[PERK_HERE_AND_NOW] += 1;
    }

    if (perkDescription->maxRank == -1) {
        for (Stat stat = STAT_FIRST; stat < PRIMARY_STAT_COUNT; stat++) {
            int value = critterGetBonusStat(critter, stat);
            critterSetBonusStat(critter, stat, value + perkDescription->stats[stat]);
        }
    }
}

// perk_remove_effect
// 0x496CE0 perk_remove_effect
void perkRemoveEffect(Object* critter, Perk perk)
{
    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        debugPrint("\nERROR: perk_remove_effect: Was called on non-critter!");
        return;
    }

    if (!perkIsValid(perk)) {
        return;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    if (perkDescription->stat != -1) {
        int value = critterGetBonusStat(critter, perkDescription->stat);
        critterSetBonusStat(critter, perkDescription->stat, value - perkDescription->statModifier);
    }

    if (perk == PERK_HERE_AND_NOW) {
        int xp = pcGetStat(PC_STAT_EXPERIENCE);
        pcSetStat(PC_STAT_EXPERIENCE, xp - hereAndNowBonusExperience);
    }

    if (perkDescription->maxRank == -1) {
        for (Stat stat = STAT_FIRST; stat < PRIMARY_STAT_COUNT; stat++) {
            int value = critterGetBonusStat(critter, stat);
            critterSetBonusStat(critter, stat, value - perkDescription->stats[stat]);
        }
    }
}

// Returns modifier to specified skill accounting for perks.
//
// 0x496DD0 perk_adjust_skill
int perkGetSkillModifier(Object* critter, Skill skill)
{
    int modifier = 0;

    switch (skill) {
    case SKILL_FIRST_AID:
        if (perkHasRank(critter, PERK_MEDIC)) {
            modifier += perkTweaks.medicFirstAidBonus;
        }

        if (perkHasRank(critter, PERK_VAULT_CITY_TRAINING)) {
            modifier += perkTweaks.vaultCityTrainingFirstAidBonus;
        }

        break;
    case SKILL_DOCTOR:
        if (perkHasRank(critter, PERK_MEDIC)) {
            modifier += perkTweaks.medicDoctorBonus;
        }

        if (perkHasRank(critter, PERK_LIVING_ANATOMY)) {
            modifier += perkTweaks.livingAnatomyDoctorBonus;
        }

        if (perkHasRank(critter, PERK_VAULT_CITY_TRAINING)) {
            modifier += perkTweaks.vaultCityTrainingDoctorBonus;
        }

        break;
    case SKILL_SNEAK:
        if (perkHasRank(critter, PERK_GHOST)) {
            int lightIntensity = objectGetLightIntensity(gDude);
            if (lightIntensity <= 45875) {
                modifier += perkTweaks.ghostBonus;
            }
        }
        // FALLTHROUGH
    case SKILL_LOCKPICK:
    case SKILL_STEAL:
    case SKILL_TRAPS:
        if (perkHasRank(critter, PERK_THIEF)) {
            modifier += perkTweaks.thiefBonus;
        }

        if (skill == SKILL_LOCKPICK || skill == SKILL_STEAL) {
            if (perkHasRank(critter, PERK_MASTER_THIEF)) {
                modifier += perkTweaks.masterThiefBonus;
            }
        }

        if (skill == SKILL_STEAL) {
            if (perkHasRank(critter, PERK_HARMLESS)) {
                modifier += perkTweaks.harmlessBonus;
            }
        }

        break;
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
        if (perkHasRank(critter, PERK_MR_FIXIT)) {
            modifier += perkTweaks.mrFixitBonus;
        }

        break;
    case SKILL_SPEECH:
        if (perkHasRank(critter, PERK_SPEAKER)) {
            modifier += perkTweaks.speakerBonus;
        }

        if (perkHasRank(critter, PERK_EXPERT_EXCREMENT_EXPEDITOR)) {
            modifier += perkTweaks.expertExcrementExpeditorBonus;
        }

        // FALLTHROUGH
    case SKILL_BARTER:
        if (perkHasRank(critter, PERK_NEGOTIATOR)) {
            modifier += perkTweaks.negotiatorBonus;
        }

        if (skill == SKILL_BARTER) {
            if (perkHasRank(critter, PERK_SALESMAN)) {
                modifier += perkTweaks.salesmanBonus;
            }
        }

        break;
    case SKILL_GAMBLING:
        if (perkHasRank(critter, PERK_GAMBLER)) {
            modifier += perkTweaks.gamblerBonus;
        }

        break;
    case SKILL_OUTDOORSMAN:
        if (perkHasRank(critter, PERK_RANGER)) {
            modifier += perkTweaks.rangerOutdoorsmanBonus;
        }

        if (perkHasRank(critter, PERK_SURVIVALIST)) {
            modifier += perkTweaks.survivalistBonus;
        }

        break;
    default:
        break;
    }

    return modifier;
}

int perkGetNightVisionBonus()
{
    return perkTweaks.nightVisionBonus;
}

int perkGetMasterTraderBonus()
{
    return perkTweaks.masterTraderBonus;
}

int perkGetEducatedBonus()
{
    return perkTweaks.educatedBonus;
}

int perkGetHealerMinBonus()
{
    return perkTweaks.healerMinBonus;
}

int perkGetHealerMaxBonus()
{
    return perkTweaks.healerMaxBonus;
}

int perkGetLifegiverBonus()
{
    return perkTweaks.lifegiverBonus;
}

int perkGetWeaponLongRangeBonus()
{
    return perkTweaks.weaponLongRangeBonus;
}

int perkGetWeaponScopeRangeBonus()
{
    return perkTweaks.weaponScopeRangeBonus;
}

int perkGetWeaponScopeRangePenalty()
{
    return perkTweaks.weaponScopeRangePenalty;
}

int perkGetWeaponAccurateBonus()
{
    return perkTweaks.weaponAccurateBonus;
}

int perkGetVaultCityInoculationsPoisonBonus()
{
    return perkTweaks.vaultCityInoculationsPoisonBonus;
}

int perkGetVaultCityInoculationsRadBonus()
{
    return perkTweaks.vaultCityInoculationsRadBonus;
}

int perkGetCautiousNatureBonus()
{
    return perkTweaks.cautiousNatureBonus;
}

int perkGetComprehensionBonus()
{
    return perkTweaks.comprehensionBonus;
}

int perkGetDemolitionExpertBonus()
{
    return perkTweaks.demolitionExpertBonus;
}

int perkGetLivingAnatomyBonus()
{
    return perkTweaks.livingAnatomyBonus;
}

int perkGetPyromaniacBonus()
{
    return perkTweaks.pyromaniacBonus;
}

int perkGetStonewallPercent()
{
    return perkTweaks.stonewallPercent;
}

int perkGetWeaponHandlingBonus()
{
    return perkTweaks.weaponHandlingBonus;
}

} // namespace fallout
