#include "game_config_migration.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "game_config.h"
#include "platform_compat.h"

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

} // namespace fallout
