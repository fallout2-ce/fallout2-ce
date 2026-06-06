#include "sfall_ini.h"

#include <algorithm>
#include <cstdio> // for snprintf
#include <cstring> // for strncpy, strlen
#include <memory>
#include <string>
#include <unordered_map>

#include "config.h"
#include "debug.h"
#include "interpreter.h"
#include "opcode_context.h"
#include "platform_compat.h"
#include "sfall_arrays.h"

namespace fallout {

/// The max length of `fileName` chunk in the triplet.
static constexpr size_t kFileNameMaxSize = 63;

/// The max length of `section` chunk in the triplet.
static constexpr size_t kSectionMaxSize = 32;

/// Special .ini file names which are accessed without adding base path.
static constexpr const char* kSystemConfigFileNames[] = {
    "ddraw.ini",
    "f2_res.ini",
};

static char basePath[COMPAT_MAX_PATH];

/// Parses "fileName|section|key" triplet into parts. `fileName` and `section`
/// chunks are copied into appropriate variables. Returns the pointer to `key`,
/// or `nullptr` on any error.
static const char* parse_ini_triplet(const char* triplet, char* fileName, char* section)
{
    const char* fileNameSectionSep = strchr(triplet, '|');
    if (fileNameSectionSep == nullptr) {
        return nullptr;
    }

    size_t fileNameLength = fileNameSectionSep - triplet;
    if (fileNameLength > kFileNameMaxSize) {
        return nullptr;
    }

    const char* sectionKeySep = strchr(fileNameSectionSep + 1, '|');
    if (sectionKeySep == nullptr) {
        return nullptr;
    }

    size_t sectionLength = sectionKeySep - fileNameSectionSep - 1;
    if (sectionLength > kSectionMaxSize) {
        return nullptr;
    }

    strncpy(fileName, triplet, fileNameLength);
    fileName[fileNameLength] = '\0';

    strncpy(section, fileNameSectionSep + 1, sectionLength);
    section[sectionLength] = '\0';

    return sectionKeySep + 1;
}

/// Returns `true` if given `fileName` is a special system .ini file name.
static bool is_system_file_name(const char* fileName)
{
    for (auto& systemFileName : kSystemConfigFileNames) {
        if (compat_stricmp(systemFileName, fileName) == 0) {
            return true;
        }
    }

    return false;
}

/// Resolves the on-disk path for an INI file name (e.g., "myconfig.ini" or
/// "ddraw.ini") into 'path'. The base directory is preferred for non-system
/// files when it actually contains the file; otherwise the current working
/// directory is used.
static void sfall_resolve_ini_path(const char* iniFileName, char* path, size_t size)
{
    if (basePath[0] != '\0' && !is_system_file_name(iniFileName)) {
        snprintf(path, size, "%s\\%s", basePath, iniFileName);
        if (compat_access(path, 0) == 0) {
            return;
        }
    }

    strncpy(path, iniFileName, size - 1);
    path[size - 1] = '\0';
}

void sfall_ini_set_base_path(const char* path)
{
    if (path != nullptr) {
        strcpy(basePath, path);

        size_t length = strlen(basePath);
        if (length > 0) {
            if (basePath[length - 1] == '\\' || basePath[length - 1] == '/') {
                basePath[length - 1] = '\0';
            }
        }
    } else {
        basePath[0] = '\0';
    }
}

/// Custom deleter so cached `Config` objects are freed via `configFree` before
/// the heap allocation is released. Guards on `isInitialized()` so it stays safe
/// even when `configInit` failed.
struct ConfigDeleter {
    void operator()(Config* config) const
    {
        if (config != nullptr) {
            if (config->isInitialized()) {
                configFree(config);
            }
            delete config;
        }
    }
};

using CachedConfigPtr = std::unique_ptr<Config, ConfigDeleter>;

/// Mirror of sfall's `IniReader::_iniCache`: maps a requested .ini file name to
/// its parsed `Config`. A `nullptr` value is a negative cache entry, recording
/// that the file could not be read so it isn't retried on every access (and no
/// `Config` is kept allocated for it). Cleared on game reset (and the relevant
/// entry erased on write).
static std::unordered_map<std::string, CachedConfigPtr> iniConfigCache;

/// Returns the parsed `Config` for `iniFileName`, reading and caching it on first
/// access. Returns `nullptr` if the file cannot be read (the miss is cached).
static Config* sfall_get_ini_config(const char* iniFileName)
{
    if (iniFileName == nullptr) {
        return nullptr;
    }

    auto cacheHit = iniConfigCache.find(iniFileName);
    if (cacheHit != iniConfigCache.end()) {
        return cacheHit->second.get();
    }

    char path[COMPAT_MAX_PATH];
    sfall_resolve_ini_path(iniFileName, path, sizeof(path));

    CachedConfigPtr config(new Config());
    if (!configInit(config.get()) || !configRead(config.get(), path, false)) {
        // Negative cache: remember that this file could not be read.
        iniConfigCache.emplace(iniFileName, nullptr);
        return nullptr;
    }

    Config* result = config.get();
    iniConfigCache.emplace(iniFileName, std::move(config));
    return result;
}

// Returns `false` on triplet parse or config initialization error.
// Returns `true` otherwise, copies the setting value into `value` (or empty
// string if the setting is missing), and optionally reports whether the key was
// found via `found`.
static bool sfall_ini_get_string_internal(const char* triplet, char* value, size_t size, bool* found)
{
    char fileName[kFileNameMaxSize];
    char section[kSectionMaxSize];
    const char* key = parse_ini_triplet(triplet, fileName, section);
    if (key == nullptr) {
        return false;
    }

    if (found != nullptr) {
        *found = false;
    }

    if (size == 0) {
        return false;
    }

    Config* config = sfall_get_ini_config(fileName);

    // NOTE: Sfall's `GetIniSetting` returns error code (-1) only when it cannot
    // parse triplet. Otherwise the default for string settings is empty string.
    value[0] = '\0';

    if (config != nullptr) {
        char* stringValue;
        if (configGetString(config, section, key, &stringValue)) {
            strncpy(value, stringValue, size - 1);
            value[size - 1] = '\0';
            if (found != nullptr) {
                *found = true;
            }
        }
    }

    return true;
}

// false: on error
// true: on key found
// If the key exists but is the empty string, returns true and value to 0
bool sfall_ini_get_int(const char* triplet, int* value)
{
    char string[20];
    bool found;
    if (!sfall_ini_get_string_internal(triplet, string, sizeof(string), &found)) {
        return false;
    }

    if (!found) {
        return false;
    }

    *value = atol(string);

    return true;
}

bool sfall_ini_get_string(const char* triplet, char* value, size_t size)
{
    return sfall_ini_get_string_internal(triplet, value, size, nullptr);
}

bool sfall_ini_set_int(const char* triplet, int value)
{
    char stringValue[20];
    compat_itoa(value, stringValue, 10);

    return sfall_ini_set_string(triplet, stringValue);
}

bool sfall_ini_set_string(const char* triplet, const char* value)
{
    char fileName[kFileNameMaxSize];
    char section[kSectionMaxSize];

    const char* key = parse_ini_triplet(triplet, fileName, section);
    if (key == nullptr) {
        return false;
    }

    // Invalidate the cached config so subsequent reads pick up the new value.
    iniConfigCache.erase(fileName);

    ScopedConfig config;
    if (!config) {
        return false;
    }

    char path[COMPAT_MAX_PATH];
    bool loaded = false;

    if (basePath[0] != '\0' && !is_system_file_name(fileName)) {
        // Attempt to load requested file in base directory.
        snprintf(path, sizeof(path), "%s\\%s", basePath, fileName);
        loaded = configRead(config.get(), path, false);
    }

    if (!loaded) {
        // There was no base path set, requested file is a system config, or
        // non-system config file was not found the base path - attempt to load
        // from current working directory.
        strcpy(path, fileName);
        loaded = configRead(config.get(), path, false);
    }

    configSetString(config.get(), section, key, value);

    bool saved = configWrite(config.get(), path, false);

    return saved;
}

static const ConfigSection* sfall_find_section_in_config(Config* config, const char* section_name)
{
    if (config == nullptr || section_name == nullptr) {
        return nullptr;
    }

    int sectionIndex = dictionaryGetIndexByKey(config, section_name);
    if (sectionIndex == -1) {
        return nullptr;
    }

    DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
    return static_cast<const ConfigSection*>(sectionEntry->value);
}

/// Copies all key/value pairs of a config section into an associative array as string keys mapped to string values.
static void copyConfigSectionToArray(ArrayId arrayId, const ConfigSection* section, Program* program)
{
    for (int i = 0; i < section->entriesLength; ++i) {
        const DictionaryEntry* entry = &(section->entries[i]);
        const char* key = entry->key;
        const char* value = *(static_cast<char**>(entry->value));

        if (key != nullptr && value != nullptr) {
            SetArray(arrayId, programMakeString(program, key), programMakeString(program, value), false, program);
        }
    }
}

// set_ini_setting
void mf_set_ini_setting(OpcodeContext& ctx)
{
    const char* triplet = ctx.stringArg(0);
    ProgramValue value = ctx.arg(1);

    if (value.isString()) {
        const char* stringValue = value.asString(ctx.program());
        if (!sfall_ini_set_string(triplet, stringValue)) {
            debugPrint("set_ini_setting: unable to write '%s' to '%s'",
                stringValue,
                triplet);
        }
    } else {
        int integerValue = value.asInt();
        if (!sfall_ini_set_int(triplet, integerValue)) {
            debugPrint("set_ini_setting: unable to write '%d' to '%s'",
                integerValue,
                triplet);
        }
    }
}

// get_ini_section
void mf_get_ini_section(OpcodeContext& ctx)
{
    // Arguments: file_path (string), section_name (string)

    const char* filePath = ctx.stringArg(0);
    const char* sectionName = ctx.stringArg(1);

    ArrayId arrayId = CreateTempArray(-1, 0);

    if (filePath == nullptr || sectionName == nullptr) {
        ctx.setReturn(arrayId);
        return;
    }

    Config* config = sfall_get_ini_config(filePath);
    if (config != nullptr) {
        const ConfigSection* section = sfall_find_section_in_config(config, sectionName);
        if (section != nullptr) {
            copyConfigSectionToArray(arrayId, section, ctx.program());
        }
    }

    ctx.setReturn(arrayId);
}

// get_ini_sections
void mf_get_ini_sections(OpcodeContext& ctx)
{
    // Arguments: file_path (string)
    const char* filePath = ctx.stringArg(0);
    ArrayId arrayId = -1;

    if (filePath == nullptr) {
        ctx.setReturn(arrayId);
        return;
    }

    Config* config = sfall_get_ini_config(filePath);
    if (config != nullptr && config->entriesLength > 0) {
        arrayId = CreateTempArray(config->entriesLength, 0);
        for (int i = 0; i < config->entriesLength; ++i) {
            const DictionaryEntry* entry = &(config->entries[i]);
            const char* sectionName = entry->key;

            if (sectionName != nullptr) {
                SetArray(arrayId, programMakeInt(ctx.program(), i), programMakeString(ctx.program(), sectionName), false, ctx.program());
            }
        }
    }

    if (arrayId == -1) {
        arrayId = CreateTempArray(0, 0);
    }

    ctx.setReturn(arrayId);
}

/// Maps a requested .ini file name to a persistent Sfall array built from its
/// contents, so repeated `get_ini_config` calls return the same array. Separate
/// caches for filesystem and DAT reads, mirroring sfall. Cleared on game reset.
static std::unordered_map<std::string, ArrayId> iniConfigArrayCache;
static std::unordered_map<std::string, ArrayId> iniConfigArrayCacheDat;

/// Builds a persistent associative array mapping each section name to a nested
/// associative array of that section's key/value pairs.
static ArrayId sfall_build_config_array(const Config* config, Program* program)
{
    ArrayId arrayId = CreateArray(-1, 0);

    for (int i = 0; i < config->entriesLength; ++i) {
        const DictionaryEntry* sectionEntry = &(config->entries[i]);
        const auto* section = static_cast<const ConfigSection*>(sectionEntry->value);

        ArrayId sectionArrayId = CreateArray(-1, 0);
        copyConfigSectionToArray(sectionArrayId, section, program);

        SetArray(arrayId, programMakeString(program, sectionEntry->key), ProgramValue(sectionArrayId), false, program);
    }

    return arrayId;
}

// get_ini_config
void mf_get_ini_config(OpcodeContext& ctx)
{
    // Arguments: file_name (string), from_dat (bool)
    const char* fileName = ctx.stringArg(0);
    bool isDb = ctx.arg(1).asInt() != 0;

    if (fileName == nullptr || fileName[0] == '\0') {
        ctx.printError("%s() - invalid config file path.", ctx.name());
        ctx.setReturn(0);
        return;
    }

    // Return the previously built array if it still exists, otherwise drop the stale entry and rebuild.
    auto& arrayCache = isDb ? iniConfigArrayCacheDat : iniConfigArrayCache;
    auto cacheHit = arrayCache.find(fileName);
    if (cacheHit != arrayCache.end()) {
        if (ArrayExists(cacheHit->second)) {
            ctx.setReturn(cacheHit->second);
            return;
        }
        arrayCache.erase(cacheHit);
    }

    ArrayId arrayId;
    if (isDb) {
        // DAT reads bypass the shared config cache, so read into a local config here.
        ScopedConfig dbConfig(fileName, true);
        if (!dbConfig) {
            ctx.printError("%s() - cannot read config file from DAT: %s", ctx.name(), fileName);
            ctx.setReturn(0);
            return;
        }
        arrayId = sfall_build_config_array(dbConfig.get(), ctx.program());
    } else {
        Config* config = sfall_get_ini_config(fileName);
        if (config == nullptr) {
            ctx.printError("%s() - cannot read config file: %s", ctx.name(), fileName);
            ctx.setReturn(0);
            return;
        }
        arrayId = sfall_build_config_array(config, ctx.program());
    }

    arrayCache.emplace(fileName, arrayId);
    ctx.setReturn(arrayId);
}

// get_ini_setting
void op_get_ini_setting(Program* program)
{
    const char* string = programStackPopString(program);

    int value;
    if (sfall_ini_get_int(string, &value)) {
        programStackPushInteger(program, value);
    } else {
        programStackPushInteger(program, -1);
    }
}

// get_ini_string
void op_get_ini_string(Program* program)
{
    const char* string = programStackPopString(program);

    char value[256];
    if (sfall_ini_get_string(string, value, sizeof(value))) {
        programStackPushString(program, value);
    } else {
        programStackPushInteger(program, -1);
    }
}

void sfall_ini_cache_clear()
{
    iniConfigCache.clear();

    // Cached array IDs are already invalidated by sfallArraysReset(); just drop the now-stale cache entries.
    iniConfigArrayCache.clear();
    iniConfigArrayCacheDat.clear();
}

} // namespace fallout
