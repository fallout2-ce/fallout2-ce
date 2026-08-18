#include "ce_save_game.h"

#include <charconv>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

#include "character_editor.h"
#include "config.h"
#include "debug.h"
#include "platform_compat.h"

namespace fallout {

static constexpr char kPerksSection[] = "perks";
static constexpr char kOwedLevelsKey[] = "owed_levels";

static std::string serializeOwedPerkLevels(const std::vector<int>& levels)
{
    std::string value;
    for (size_t index = 0; index < levels.size(); index++) {
        if (index != 0) {
            value += ',';
        }

        value += std::to_string(levels[index]);
    }

    return value;
}

static bool parseOwedPerkLevels(const char* value, std::vector<int>& levels)
{
    levels.clear();

    const char* end = value + strlen(value);
    while (value != end) {
        int level;
        auto result = std::from_chars(value, end, level);
        if (result.ec != std::errc() || result.ptr == value) {
            return false;
        }

        levels.push_back(level);
        if (result.ptr == end) {
            return true;
        }

        if (*result.ptr != ',') {
            return false;
        }

        value = result.ptr + 1;
        if (value == end) {
            return false;
        }
    }

    return true;
}

bool ceSaveGameData(const char* path)
{
    ScopedConfig config;
    if (!config) {
        return false;
    }

    std::string owedLevels = serializeOwedPerkLevels(characterEditorGetOwedPerkLevels());
    if (!configSetString(config.get(), kPerksSection, kOwedLevelsKey, owedLevels.c_str())) {
        return false;
    }

    return configWrite(config.get(), path, false);
}

void ceLoadGameData(const char* path)
{
    if (compat_access(path, 0) == 0) {
        ScopedConfig config { path, false };
        char* owedLevelsValue;
        std::vector<int> owedLevels;
        if (config
            && configGetString(config.get(), kPerksSection, kOwedLevelsKey, &owedLevelsValue)
            && parseOwedPerkLevels(owedLevelsValue, owedLevels)
            && characterEditorSetOwedPerkLevels(owedLevels)) {
            return;
        }

        debugPrint("LOADSAVE (CE): Invalid perk state in %s; using legacy fallback.\n", path);
    }

    characterEditorMigrateLegacyPerkSelectionState();
}

} // namespace fallout
