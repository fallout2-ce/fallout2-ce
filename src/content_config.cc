#include "content_config.h"

namespace fallout {

Config gContentConfig;

constexpr char kConfigPath[] = R"(config\game.cfg)";

bool contentConfigInit()
{
    if (gContentConfig.isInitialized()) {
        return false;
    }

    if (!configInit(&gContentConfig)) {
        return false;
    }

    configRead(&gContentConfig, kConfigPath, true);
    return true;
}

void contentConfigExit()
{
    if (!gContentConfig.isInitialized()) return;

    configFree(&gContentConfig);
}

} // namespace fallout