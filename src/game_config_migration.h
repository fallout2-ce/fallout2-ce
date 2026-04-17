#ifndef FALLOUT_GAME_CONFIG_MIGRATION_H_
#define FALLOUT_GAME_CONFIG_MIGRATION_H_

#include "config.h"

namespace fallout {

bool gameConfigMigrateFromHiRes(const char* gameConfigFilePath, const char* hiResConfigFileName, Config* gameConfig);

} // namespace fallout

#endif /* FALLOUT_GAME_CONFIG_MIGRATION_H_ */
