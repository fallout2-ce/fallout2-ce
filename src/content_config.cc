#include "content_config.h"

#include "game_config_migration.h"

namespace fallout {

Config gContentConfig;

constexpr char kConfigPath[] = R"(config\game.cfg)";

void contentConfigInit(const char* baseModPath)
{
    if (gContentConfig.isInitialized()) {
        return;
    }

    if (!configInit(&gContentConfig)) {
        return;
    }

    if (!configRead(&gContentConfig, kConfigPath, true)) {
        // Failed to load config, try to migrate some settings from sfall.
        contentConfigTryMigrateFromSfall(baseModPath, kConfigPath);
    }
}

void contentConfigExit()
{
    if (!gContentConfig.isInitialized()) return;

    configFree(&gContentConfig);
}

} // namespace fallout