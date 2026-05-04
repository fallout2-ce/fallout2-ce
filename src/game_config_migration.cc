#include "game_config_migration.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "content_config.h"
#include "debug.h"
#include "game_config.h"
#include "platform_compat.h"
#include "settings.h"
#include "sfall_config.h"

namespace fallout {

namespace {

#define F2_RES_CONFIG_FILE_NAME "f2_res.ini"

    struct F2ResMigrationEntry {
        const char* legacySection;
        const char* legacyKey;
        const char* targetSection;
        const char* targetKey;
    };

    static bool gameConfigHasKey(Config* config, const char* section, const char* key);
    static bool gameConfigNeedsF2ResMigration(Config* gameConfig);
    static bool gameConfigMigrateStringKey(Config* legacyConfig, Config* gameConfig, const F2ResMigrationEntry& entry);
    static bool gameConfigMigrateScaleKey(Config* legacyConfig, Config* gameConfig);

    static constexpr F2ResMigrationEntry kF2ResMigrationEntries[] = {
        { "MAIN", "SCR_WIDTH", GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_X_KEY },
        { "MAIN", "SCR_HEIGHT", GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_Y_KEY },
        { "MAIN", "WINDOWED", GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_WINDOWED_KEY },
        { "IFACE", "IFACE_BAR_MODE", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_MODE_KEY },
        { "IFACE", "IFACE_BAR_WIDTH", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_WIDTH_KEY },
        { "IFACE", "IFACE_BAR_SIDE_ART", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_SIDE_ART_KEY },
        { "IFACE", "IFACE_BAR_SIDES_ORI", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_SIDES_ORI_KEY },
        { "MAPS", "IGNORE_MAP_EDGES", GAME_CONFIG_UI_KEY, GAME_CONFIG_IGNORE_MAP_EDGES_KEY },
        { "STATIC_SCREENS", "SPLASH_SCRN_SIZE", GAME_CONFIG_UI_KEY, GAME_CONFIG_SPLASH_SCREEN_SIZE_KEY },
    };

    static bool gameConfigHasKey(Config* config, const char* section, const char* key)
    {
        char* value;
        return configGetString(config, section, key, &value);
    }

    static bool gameConfigNeedsF2ResMigration(Config* gameConfig)
    {
        assert(gameConfig != nullptr);

        return !gameConfigHasKey(gameConfig, GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_X_KEY);
    }

    static bool gameConfigMigrateStringKey(Config* legacyConfig, Config* gameConfig, const F2ResMigrationEntry& entry)
    {
        assert(legacyConfig != nullptr && gameConfig != nullptr);

        if (gameConfigHasKey(gameConfig, entry.targetSection, entry.targetKey)) {
            return false;
        }

        char* value;
        if (!configGetString(legacyConfig, entry.legacySection, entry.legacyKey, &value)) {
            return false;
        }

        return configSetString(gameConfig, entry.targetSection, entry.targetKey, value);
    }

    static bool gameConfigMigrateScaleKey(Config* legacyConfig, Config* gameConfig)
    {
        assert(legacyConfig != nullptr && gameConfig != nullptr);

        if (gameConfigHasKey(gameConfig, GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_SCALE_KEY)) {
            return false;
        }

        int value;
        if (!configGetInt(legacyConfig, "MAIN", "SCALE_2X", &value)) {
            return false;
        }

        return configSetInt(gameConfig, GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_SCALE_KEY, value + 1);
    }

} // namespace

// Migrate settings F2_RES.INI to fallout2.cfg
//
// Only happens a single time, after which fallout2.cfg is the source of truth
bool gameConfigMigrateFromF2Res(const char* gameConfigFilePath, Config* gameConfig)
{
    if (gameConfigFilePath == nullptr || gameConfig == nullptr) {
        return false;
    }

    if (!gameConfigNeedsF2ResMigration(gameConfig)) {
        return false;
    }

    char f2ResFilePath[COMPAT_MAX_PATH];
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(gameConfigFilePath, drive, dir, nullptr, nullptr);
    compat_makepath(f2ResFilePath, drive, dir, F2_RES_CONFIG_FILE_NAME, nullptr);

    Config legacyConfig;
    if (!configInit(&legacyConfig)) {
        return false;
    }

    bool migrated = false;
    if (configRead(&legacyConfig, f2ResFilePath, false)) {
        for (const auto& entry : kF2ResMigrationEntries) {
            if (gameConfigMigrateStringKey(&legacyConfig, gameConfig, entry)) {
                migrated = true;
            }
        }

        if (gameConfigMigrateScaleKey(&legacyConfig, gameConfig)) {
            migrated = true;
        }
    }

    configFree(&legacyConfig);
    return migrated;
}

namespace {

    constexpr char kSfallMisc[] = "Misc";
    constexpr char kSfallInterface[] = "Interface";

    struct SfallMigrationEntry {
        const char* sfallSection;
        const char* sfallKey;
        const char* targetSection;
        const char* targetKey;
        // If the sfall value matches this string, skip migration (value is already the default).
        // nullptr means always migrate when the key is present.
        const char* defaultValue;
    };

    constexpr SfallMigrationEntry kSfallMigrationEntries[] = {
        // [start]
        { kSfallMisc, "StartingMap", CONTENT_CONFIG_START_SECTION, "map", "" },
        { kSfallMisc, "MaleStartModel", CONTENT_CONFIG_START_SECTION, "model_male", "hmwarr" },
        { kSfallMisc, "MaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_male_default", "hmjmps" },
        { kSfallMisc, "FemaleStartModel", CONTENT_CONFIG_START_SECTION, "model_female", "hfprim" },
        { kSfallMisc, "FemaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_female_default", "hfjmps" },
        { kSfallMisc, "PipBoyAvailableAtGameStart", CONTENT_CONFIG_START_SECTION, "pipboy", "0" },
        // [karma]
        { kSfallMisc, "KarmaFRMs", CONTENT_CONFIG_KARMA_SECTION, "frms" },
        { kSfallMisc, "KarmaPoints", CONTENT_CONFIG_KARMA_SECTION, "points" },
        // [dialog]
        { kSfallMisc, "DialogueFix", CONTENT_CONFIG_DIALOG_SECTION, "no_exit_hotkey", "1" },
        { kSfallMisc, "DialogGenderWords", CONTENT_CONFIG_DIALOG_SECTION, "gender_words", "0" },
        // [main_menu]
        { kSfallMisc, "VersionString", CONTENT_CONFIG_MAIN_MENU_SECTION, "version_string" },
        { kSfallMisc, "MainMenuFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "font_color", "0" },
        { kSfallMisc, "MainMenuBigFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "big_font_color", "0" },
        { kSfallMisc, "MainMenuOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_x", "0" },
        { kSfallMisc, "MainMenuOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_y", "0" },
        { kSfallMisc, "MainMenuCreditsOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_x", "0" },
        { kSfallMisc, "MainMenuCreditsOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_y", "0" },
        // [movies]
        { kSfallMisc, "MovieTimer_artimer1", CONTENT_CONFIG_MOVIES_SECTION, "artimer1", "90" },
        { kSfallMisc, "MovieTimer_artimer2", CONTENT_CONFIG_MOVIES_SECTION, "artimer2", "180" },
        { kSfallMisc, "MovieTimer_artimer3", CONTENT_CONFIG_MOVIES_SECTION, "artimer3", "270" },
        { kSfallMisc, "MovieTimer_artimer4", CONTENT_CONFIG_MOVIES_SECTION, "artimer4", "360" },
        // [combat]
        { kSfallMisc, "DamageFormula", CONTENT_CONFIG_COMBAT_SECTION, "damage_formula", "0" },
        { kSfallMisc, "BonusHtHDamageFix", CONTENT_CONFIG_COMBAT_SECTION, "bonus_hth_damage_fix", "1" },
        { kSfallMisc, "RemoveCriticalTimelimits", CONTENT_CONFIG_COMBAT_SECTION, "remove_critical_time_limits", "0" },
        { kSfallMisc, "ScienceOnCritters", CONTENT_CONFIG_COMBAT_SECTION, "science_on_critters", "0" },
        { kSfallMisc, "CheckWeaponAmmoCost", CONTENT_CONFIG_COMBAT_SECTION, "check_weapon_ammo_cost", nullptr },
        { kSfallMisc, "ComputeSprayMod", CONTENT_CONFIG_COMBAT_SECTION, "burst_enabled", "0" },
        { kSfallMisc, "ComputeSpray_CenterMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_mult", "1" },
        { kSfallMisc, "ComputeSpray_CenterDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_div", "3" },
        { kSfallMisc, "ComputeSpray_TargetMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_mult", "1" },
        { kSfallMisc, "ComputeSpray_TargetDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_div", "2" },
        // [explosions]
        { kSfallMisc, "ExplosionsEmitLight", CONTENT_CONFIG_EXPLOSIONS_SECTION, "emit_light", "0" },
        { kSfallMisc, "Dynamite_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_max", "50" },
        { kSfallMisc, "Dynamite_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_min", "30" },
        { kSfallMisc, "PlasticExplosive_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_max", "80" },
        { kSfallMisc, "PlasticExplosive_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_min", "40" },
        // [skilldex]
        { kSfallMisc, "Lockpick", CONTENT_CONFIG_SKILLDEX_SECTION, "lockpick", "293" },
        { kSfallMisc, "Steal", CONTENT_CONFIG_SKILLDEX_SECTION, "steal", "293" },
        { kSfallMisc, "Traps", CONTENT_CONFIG_SKILLDEX_SECTION, "traps", "293" },
        { kSfallMisc, "FirstAid", CONTENT_CONFIG_SKILLDEX_SECTION, "first_aid", "293" },
        { kSfallMisc, "Doctor", CONTENT_CONFIG_SKILLDEX_SECTION, "doctor", "293" },
        { kSfallMisc, "Science", CONTENT_CONFIG_SKILLDEX_SECTION, "science", "293" },
        { kSfallMisc, "Repair", CONTENT_CONFIG_SKILLDEX_SECTION, "repair", "293" },
        // [worldmap]
        { kSfallMisc, "TownMapHotkeysFix", CONTENT_CONFIG_WORLDMAP_SECTION, "town_map_hotkeys_fix", "1" },
        { kSfallMisc, "DisableHorrigan", CONTENT_CONFIG_WORLDMAP_SECTION, "disable_horrigan", "0" },
        { kSfallMisc, "CityRepsList", CONTENT_CONFIG_WORLDMAP_SECTION, "city_reputation_list" },
        { kSfallInterface, "WorldMapTravelMarkers", CONTENT_CONFIG_WORLDMAP_SECTION, "trail_markers", "0" },
        // [characters]
        { kSfallMisc, "PremadePaths", CONTENT_CONFIG_CHARACTERS_SECTION, "premade_paths" },
        { kSfallMisc, "PremadeFIDs", CONTENT_CONFIG_CHARACTERS_SECTION, "premade_fids" },
        // [text]
        { kSfallMisc, "ExtraGameMsgFileList", CONTENT_CONFIG_TEXT_SECTION, "extra_msg_file_list" },
    };

} // anonymous namespace

// Migrate sfall settings from ddraw.ini to game.cfg.
//
// Runs once when no local game.cfg exists at contentConfigFilePath.
// Writes only the settings found in sfallConfig to a new local file.
static bool contentConfigMigrateFromSfall(Config* sfallConfig, const char* contentConfigFilePath)
{
    assert(sfallConfig != nullptr && contentConfigFilePath != nullptr);

    // Skip if a local game.cfg already exists (already migrated or user-managed).
    if (compat_file_exists(contentConfigFilePath)) {
        return false;
    }

    Config migratedConfig;
    if (!configInit(&migratedConfig)) {
        return false;
    }

    bool migrated = false;
    // Migrate start year/month/day only when explicitly set (not the sfall -1 sentinel).
    auto migrateStartInt = [&](const char* sfallKey, const char* targetKey, int defaultValue) {
        int value;
        if (configGetInt(sfallConfig, "Misc", sfallKey, &value) && value >= 0 && value != defaultValue) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", value);
            configSetString(&migratedConfig, CONTENT_CONFIG_START_SECTION, targetKey, buf);
            migrated = true;
        }
    };
    migrateStartInt("StartYear", "year", 2241);
    migrateStartInt("StartMonth", "month", 6);
    migrateStartInt("StartDay", "day", 24);

    for (const auto& entry : kSfallMigrationEntries) {
        char* value;
        if (configGetString(sfallConfig, entry.sfallSection, entry.sfallKey, &value)) {
            if (value[0] == '\0' || entry.defaultValue != nullptr && strcmp(value, entry.defaultValue) == 0) {
                continue;
            }
            configSetString(&migratedConfig, entry.targetSection, entry.targetKey, value);
            migrated = true;
        }
    }

    if (migrated) {
        // Ensure all directory components exist before writing.
        char drive[COMPAT_MAX_DRIVE];
        char dirPart[COMPAT_MAX_DIR];
        char pathWithoutFile[COMPAT_MAX_PATH];
        compat_splitpath(contentConfigFilePath, drive, dirPart, nullptr, nullptr);
        compat_makepath(pathWithoutFile, drive, dirPart, nullptr, nullptr);
        compat_mkdir_recursive(pathWithoutFile);

        if (!configWrite(&migratedConfig, contentConfigFilePath, false)) {
            debugPrint("Failed to write migrated settings to %s!\n", contentConfigFilePath);
        }
    }

    configFree(&migratedConfig);
    return migrated;
}

void contentConfigTryMigrateFromSfall(const char* contentConfigPath)
{
    if (!gSfallConfig.isInitialized() || gSfallConfig.entriesLength == 0) {
        // Nothing to migrate.
        return;
    }
    const auto& masterPatches = settings.system.master_patches_path;
    if (masterPatches.empty()) {
        debugPrint("Failed to migrate from ddraw.ini: no master_patches is set.\n");
        return;
    }
    if (compat_file_exists(masterPatches.c_str())) {
        // Master patches is pointing to a file instead of a folder. This shouldn't normally happen, so don't migrate in this case.
        return;
    }
    char contentCfgPath[COMPAT_MAX_PATH];
    snprintf(contentCfgPath, sizeof(contentCfgPath), "%s\\%s", masterPatches.c_str(), contentConfigPath);
    if (contentConfigMigrateFromSfall(&gSfallConfig, contentCfgPath)) {
        debugPrint("Migrated settings from ddraw.ini to game.cfg.\n");
    }
}

} // namespace fallout
