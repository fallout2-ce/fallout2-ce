#include "game_config_migration.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "content_config.h"
#include "debug.h"
#include "game_config.h"
#include "platform_compat.h"
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
    };

    constexpr SfallMigrationEntry kSfallMigrationEntries[] = {
        // [start]
        { kSfallMisc, "StartingMap", CONTENT_CONFIG_START_SECTION, "map" },
        { kSfallMisc, "MaleStartModel", CONTENT_CONFIG_START_SECTION, "model_male" },
        { kSfallMisc, "MaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_male_default" },
        { kSfallMisc, "FemaleStartModel", CONTENT_CONFIG_START_SECTION, "model_female" },
        { kSfallMisc, "FemaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_female_default" },
        { kSfallMisc, "PipBoyAvailableAtGameStart", CONTENT_CONFIG_START_SECTION, "pipboy" },
        // [karma]
        { kSfallMisc, "KarmaFRMs", CONTENT_CONFIG_KARMA_SECTION, "frms" },
        { kSfallMisc, "KarmaPoints", CONTENT_CONFIG_KARMA_SECTION, "points" },
        // [dialog]
        { kSfallMisc, "DialogueFix", CONTENT_CONFIG_DIALOG_SECTION, "no_exit_hotkey" },
        { kSfallMisc, "DialogGenderWords", CONTENT_CONFIG_DIALOG_SECTION, "gender_words" },
        // [main_menu]
        { kSfallMisc, "VersionString", CONTENT_CONFIG_MAIN_MENU_SECTION, "version_string" },
        { kSfallMisc, "MainMenuFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "font_color" },
        { kSfallMisc, "MainMenuBigFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "big_font_color" },
        { kSfallMisc, "MainMenuOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_x" },
        { kSfallMisc, "MainMenuOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_y" },
        { kSfallMisc, "MainMenuCreditsOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_x" },
        { kSfallMisc, "MainMenuCreditsOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_y" },
        // [movies]
        { kSfallMisc, "MovieTimer_artimer1", CONTENT_CONFIG_MOVIES_SECTION, "artimer1" },
        { kSfallMisc, "MovieTimer_artimer2", CONTENT_CONFIG_MOVIES_SECTION, "artimer2" },
        { kSfallMisc, "MovieTimer_artimer3", CONTENT_CONFIG_MOVIES_SECTION, "artimer3" },
        { kSfallMisc, "MovieTimer_artimer4", CONTENT_CONFIG_MOVIES_SECTION, "artimer4" },
        // [combat]
        { kSfallMisc, "DamageFormula", CONTENT_CONFIG_COMBAT_SECTION, "damage_formula" },
        { kSfallMisc, "BonusHtHDamageFix", CONTENT_CONFIG_COMBAT_SECTION, "bonus_hth_damage_fix" },
        { kSfallMisc, "RemoveCriticalTimelimits", CONTENT_CONFIG_COMBAT_SECTION, "remove_critical_time_limits" },
        { kSfallMisc, "ScienceOnCritters", CONTENT_CONFIG_COMBAT_SECTION, "science_on_critters" },
        { kSfallMisc, "ComputeSpray_CenterMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_mult" },
        { kSfallMisc, "ComputeSpray_CenterDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_div" },
        { kSfallMisc, "ComputeSpray_TargetMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_mult" },
        { kSfallMisc, "ComputeSpray_TargetDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_div" },
        // [explosions]
        { kSfallMisc, "ExplosionsEmitLight", CONTENT_CONFIG_EXPLOSIONS_SECTION, "emit_light" },
        { kSfallMisc, "Dynamite_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_max" },
        { kSfallMisc, "Dynamite_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_min" },
        { kSfallMisc, "PlasticExplosive_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_max" },
        { kSfallMisc, "PlasticExplosive_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_min" },
        // [skilldex]
        { kSfallMisc, "Lockpick", CONTENT_CONFIG_SKILLDEX_SECTION, "lockpick" },
        { kSfallMisc, "Steal", CONTENT_CONFIG_SKILLDEX_SECTION, "steal" },
        { kSfallMisc, "Traps", CONTENT_CONFIG_SKILLDEX_SECTION, "traps" },
        { kSfallMisc, "FirstAid", CONTENT_CONFIG_SKILLDEX_SECTION, "first_aid" },
        { kSfallMisc, "Doctor", CONTENT_CONFIG_SKILLDEX_SECTION, "doctor" },
        { kSfallMisc, "Science", CONTENT_CONFIG_SKILLDEX_SECTION, "science" },
        { kSfallMisc, "Repair", CONTENT_CONFIG_SKILLDEX_SECTION, "repair" },
        // [worldmap]
        { kSfallMisc, "TownMapHotkeysFix", CONTENT_CONFIG_WORLDMAP_SECTION, "town_map_hotkeys_fix" },
        { kSfallMisc, "DisableHorrigan", CONTENT_CONFIG_WORLDMAP_SECTION, "disable_horrigan" },
        { kSfallMisc, "CityRepsList", CONTENT_CONFIG_WORLDMAP_SECTION, "city_reputation_list" },
        { kSfallInterface, "WorldMapTravelMarkers", CONTENT_CONFIG_WORLDMAP_SECTION, "trail_markers" },
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
// Writes only the settings found in sfallConfig to a new local file,
// and updates contentConfig in-memory so the current session uses them.
static bool contentConfigMigrateFromSfall(Config* sfallConfig, Config* contentConfig, const char* contentConfigFilePath)
{
    assert(sfallConfig != nullptr && contentConfig != nullptr && contentConfigFilePath != nullptr);

    // Skip if a local game.cfg already exists (already migrated or user-managed).
    FILE* existing = compat_fopen(contentConfigFilePath, "rt");
    if (existing != nullptr) {
        fclose(existing);
        return false;
    }

    // Migrate start year/month/day only when explicitly set (not the sfall -1 sentinel).
    bool migrated = false;
    Config migratedConfig;
    if (!configInit(&migratedConfig)) {
        return false;
    }

    auto migrateStartInt = [&](const char* sfallKey, const char* targetKey) {
        int value;
        if (configGetInt(sfallConfig, "Misc", sfallKey, &value) && value >= 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", value);
            configSetString(&migratedConfig, CONTENT_CONFIG_START_SECTION, targetKey, buf);
            if (contentConfig->isInitialized())
                configSetString(contentConfig, CONTENT_CONFIG_START_SECTION, targetKey, buf);
            migrated = true;
        }
    };
    migrateStartInt("StartYear", "year");
    migrateStartInt("StartMonth", "month");
    migrateStartInt("StartDay", "day");

    for (const auto& entry : kSfallMigrationEntries) {
        char* value;
        if (configGetString(sfallConfig, entry.sfallSection, entry.sfallKey, &value)) {
            configSetString(&migratedConfig, entry.targetSection, entry.targetKey, value);
            if (contentConfig->isInitialized())
                configSetString(contentConfig, entry.targetSection, entry.targetKey, value);
            migrated = true;
        }
    }

    if (migrated) {
        // Ensure all directory components exist before writing.
        char dir[COMPAT_MAX_PATH];
        char drive[COMPAT_MAX_DRIVE];
        char dirPart[COMPAT_MAX_DIR];
        compat_splitpath(contentConfigFilePath, drive, dirPart, nullptr, nullptr);
        compat_makepath(dir, drive, dirPart, nullptr, nullptr);
        for (char* sep = dir; *sep != '\0'; sep++) {
            if (*sep == '\\' || *sep == '/') {
                char saved = *sep;
                *sep = '\0';
                compat_mkdir(dir);
                *sep = saved;
            }
        }
        compat_mkdir(dir);

        if (!configWrite(&migratedConfig, contentConfigFilePath, false)) {
            debugPrint("Failed to write migrated settings to %s!\n", contentConfigFilePath);
        }
    }

    configFree(&migratedConfig);
    return migrated;
}

void contentConfigTryMigrateFromSfall(const char* baseModPath, const char* contentConfigPath)
{
    FILE* baseAsDat = compat_fopen(baseModPath, "rb");
    if (baseAsDat != nullptr) {
        // Base dat file already exists. This shouldn't normally happen, so don't migrate in this case.
        fclose(baseAsDat);
        return;
    }
    char contentCfgPath[COMPAT_MAX_PATH];
    snprintf(contentCfgPath, sizeof(contentCfgPath), "%s\\%s", baseModPath, contentConfigPath);
    if (contentConfigMigrateFromSfall(&gSfallConfig, &gContentConfig, contentCfgPath)) {
        debugPrint("Migrated settings from ddraw.ini to game.cfg.\n");
    }
}

} // namespace fallout
