#include "content_config.h"

#include "game_config_migration.h"

namespace fallout {

Config gContentConfig;

constexpr char kConfigPath[] = R"(config\game.cfg)";

void contentConfigInit()
{
    if (gContentConfig.isInitialized()) {
        return;
    }

    // Try to migrate some settings from sfall.
    contentConfigTryMigrateFromSfall(kConfigPath);

    if (!configInit(&gContentConfig)) {
        return;
    }

    configRead(&gContentConfig, kConfigPath, true);
}

void contentConfigExit()
{
    if (!gContentConfig.isInitialized()) return;

    configFree(&gContentConfig);
}

} // namespace fallout