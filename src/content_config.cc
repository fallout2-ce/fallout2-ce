#include "content_config.h"

#include "game_config_migration.h"

namespace fallout {

Config gContentConfig;

constexpr char kConfigPath[] = R"(config\game.cfg)";
constexpr char kConfigPatchPath[] = R"(config\game#patch.cfg)";

void contentConfigInit()
{
    if (gContentConfig.isInitialized()) {
        return;
    }

    // Try to migrate some settings from sfall.
    contentConfigTryMigrateFromSfall(kConfigPatchPath);

    if (!configInit(&gContentConfig)) {
        return;
    }

    configRead(&gContentConfig, kConfigPath, true);
    // Patch config allows to override only certain fields, without replacing the whole file.
    // TODO: remove this after config patching by mods is implemented inside configRead
    configRead(&gContentConfig, kConfigPatchPath, true);
}

void contentConfigExit()
{
    if (!gContentConfig.isInitialized()) return;

    configFree(&gContentConfig);
}

} // namespace fallout