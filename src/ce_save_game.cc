#include "ce_save_game.h"

#include <charconv>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "character_editor.h"
#include "config.h"
#include "debug.h"
#include "platform_compat.h"
#include "skill.h"

namespace fallout {

static constexpr char kPerksSection[] = "perks";
static constexpr char kOwedLevelsKey[] = "owed_levels";
static constexpr char kSkillUsageSection[] = "skill_usage";

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

static std::string serializeCritterSkillUsage(const CritterSkillUsageTable& table)
{
    std::string value;
    bool first = true;
    for (const SkillUsageSlots& slots : table) {
        for (int timestamp : slots) {
            if (!first) {
                value += ',';
            }
            first = false;
            value += std::to_string(timestamp);
        }
    }

    return value;
}

static std::optional<CritterSkillUsageTable> parseCritterSkillUsageTable(std::string_view value)
{
    CritterSkillUsageTable table;
    const char* cur = value.data();
    const char* end = value.data() + value.size();
    bool first = true;
    for (SkillUsageSlots& slots : table) {
        for (int& timestamp : slots) {
            if (!first) {
                if (cur == end || *cur != ',') {
                    return std::nullopt;
                }
                cur++;
            }
            first = false;

            auto result = std::from_chars(cur, end, timestamp);
            if (result.ec != std::errc() || result.ptr == cur) {
                return std::nullopt;
            }
            cur = result.ptr;
        }
    }

    if (cur != end) {
        return std::nullopt;
    }

    return table;
}

static bool loadCritterSkillUsageSection(Config& config)
{
    int sectionIndex = dictionaryGetIndexByKey(&config, kSkillUsageSection);
    if (sectionIndex == -1) {
        return true;
    }

    const ConfigSection& section = *static_cast<const ConfigSection*>(config.entries[sectionIndex].value);
    CritterSkillUsage::TableMap tables;

    for (int index = 0; index < section.entriesLength; index++) {
        std::string_view key = section.entries[index].key;
        int pid = 0;
        auto pidResult = std::from_chars(key.data(), key.data() + key.size(), pid);
        if (pidResult.ec != std::errc() || pidResult.ptr != key.data() + key.size()) {
            return false;
        }

        const char* value = *static_cast<const char* const*>(section.entries[index].value);
        auto table = parseCritterSkillUsageTable(value);
        if (!table) {
            return false;
        }

        tables[pid] = *table;
    }

    CritterSkillUsage::instance().importTables(std::move(tables));
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

    CritterSkillUsage& tracker = CritterSkillUsage::instance();
    tracker.pruneExpired();

    // Store one key per critter PID to avoid INI line-length limits.
    for (const auto& [pid, table] : tracker.tables()) {
        std::string key = std::to_string(pid);
        std::string serialized = serializeCritterSkillUsage(table);
        if (!configSetString(config.get(), kSkillUsageSection, key.c_str(), serialized.c_str())) {
            return false;
        }
    }

    return configWrite(config.get(), path, false);
}

void ceLoadGameData(const char* path)
{
    CritterSkillUsage::instance().clear();

    bool perksLoaded = false;
    if (compat_access(path, 0) == 0) {
        ScopedConfig config { path, false };
        if (config) {
            char* owedLevelsValue;
            std::vector<int> owedLevels;
            if (configGetString(config.get(), kPerksSection, kOwedLevelsKey, &owedLevelsValue)
                && parseOwedPerkLevels(owedLevelsValue, owedLevels)
                && characterEditorSetOwedPerkLevels(owedLevels)) {
                perksLoaded = true;
            } else {
                debugPrint("LOADSAVE (CE): Invalid perk state in %s; using legacy fallback.\n", path);
            }

            if (!loadCritterSkillUsageSection(*config.get())) {
                debugPrint("LOADSAVE (CE): Invalid skill usage state in %s; ignoring.\n", path);
            }
        } else {
            debugPrint("LOADSAVE (CE): Invalid perk state in %s; using legacy fallback.\n", path);
        }
    }

    if (!perksLoaded) {
        characterEditorMigrateLegacyPerkSelectionState();
    }
}

} // namespace fallout
