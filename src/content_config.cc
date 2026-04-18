#include "content_config.h"

#include "game_config_migration.h"

namespace fallout {

Config gContentConfig;

constexpr char kConfigPath[] = R"(config\game.cfg)";

bool contentConfigInit(const char* baseModPath)
{
    if (gContentConfig.isInitialized()) {
        return false;
    }

    if (!configInit(&gContentConfig)) {
        return false;
    }

    if (!configRead(&gContentConfig, kConfigPath, true)) {
        // Failed to load config, try to migrate some settings from sfall.
        contentConfigTryMigrateFromSfall(baseModPath, kConfigPath);
    }
    return true;
}

void contentConfigExit()
{
    if (!gContentConfig.isInitialized()) return;

    configFree(&gContentConfig);
}

} // namespace fallout