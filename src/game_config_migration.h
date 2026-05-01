#ifndef FALLOUT_GAME_CONFIG_MIGRATION_H_
#define FALLOUT_GAME_CONFIG_MIGRATION_H_

#include "config.h"

namespace fallout {

bool gameConfigMigrateFromF2Res(const char* gameConfigFilePath, Config* gameConfig);
bool gameConfigMigrateFromDefaultConfig(const char* defaultConfigFilePath, Config* gameConfig);
void contentConfigTryMigrateFromSfall(const char* contentConfigPath);

} // namespace fallout

#endif /* FALLOUT_GAME_CONFIG_MIGRATION_H_ */
