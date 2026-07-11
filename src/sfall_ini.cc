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

// The max length of `fileName` chunk in the triplet.
static constexpr size_t kFileNameMaxSize = 63;

// The max length of `section` chunk in the triplet.
static constexpr size_t kSectionMaxSize = 32;

// Special .ini file names which are accessed without adding base path.
static constexpr const char* kSystemConfigFileNames[] = {
    "ddraw.ini",
    "f2_res.ini",
};

static char basePath[COMPAT_MAX_PATH];

// Sfall-style INI paths can be prefixed with ".\", "./", "\" or "/".
// Strip those prefixes when comparing special file names.
static const char* sfall_strip_ini_path_prefix(const char* iniFileName)
{
    const char* fileName = iniFileName;

    if (fileName[0] == '.' && (fileName[1] == '\\' || fileName[1] == '/')) {
        fileName += 2;
    }

    if (fileName[0] == '\\' || fileName[0] == '/') {
        fileName++;
    }

    return fileName;
}

// Parses "fileName|section|key" triplet into parts. `fileName` and `section`
// chunks are copied into appropriate variables. Returns the pointer to `key`,
// or `nullptr` on any error.
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

// Returns `true` if given `fileName` is a special system .ini file name.
static bool is_system_file_name(const char* fileName)
{
    fileName = sfall_strip_ini_path_prefix(fileName);

    for (auto& systemFileName : kSystemConfigFileNames) {
        if (compat_stricmp(systemFileName, fileName) == 0) {
            return true;
        }
    }

    return false;
}

// Sfall treats non-system INI file names as game-root-relative, even when the
// script passes a leading backslash like "\mods\foo.ini".
static void sfall_build_relative_ini_path(const char* iniFileName, char* path, size_t size)
{
    if (is_system_file_name(iniFileName)) {
        snprintf(path, size, "%s", sfall_strip_ini_path_prefix(iniFileName));
        return;
    }

    const char* fileName = sfall_strip_ini_path_prefix(iniFileName);

    snprintf(path, size, ".\\%s", fileName);
}

static std::string sfall_normalize_ini_cache_key(const char* iniFileName)
{
    const char* fileName = sfall_strip_ini_path_prefix(iniFileName);

    if (is_system_file_name(fileName)) {
        return fileName;
    }

    return std::string(".\\") + fileName;
}

// Reads the INI file into `config`, trying the base directory first for
// non-system files and falling back to the working directory on failure.
static bool sfall_read_named_ini(const char* iniFileName, Config* config)
{
    if (basePath[0] != '\0' && !is_system_file_name(iniFileName)) {
        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s", basePath, iniFileName);
        if (configRead(config, path, false)) {
            return true;
        }
    }

    char path[COMPAT_MAX_PATH];
    sfall_build_relative_ini_path(iniFileName, path, sizeof(path));
    return configRead(config, path, false);
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

// Frees a cached `Config` via `configFree` before releasing it.
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

// Maps a requested .ini file name to its parsed `Config`. A `nullptr` value
// marks a file that could not be read, so it isn't retried.
static std::unordered_map<std::string, CachedConfigPtr> iniConfigCache;

// Maps a requested .ini file name to a persistent array of its contents, so
// repeated calls return the same array. Separate caches for file and DAT reads.
static std::unordered_map<std::string, ArrayId> iniConfigArrayCache;
static std::unordered_map<std::string, ArrayId> iniConfigArrayCacheDat;

// Returns the parsed `Config` for `iniFileName`, reading and caching it on first
// access. Returns `nullptr` if the file cannot be read (the miss is cached).
static Config* sfall_get_ini_config(const char* iniFileName)
{
    if (iniFileName == nullptr) {
        return nullptr;
    }

    std::string cacheKey = sfall_normalize_ini_cache_key(iniFileName);

    auto cacheHit = iniConfigCache.find(cacheKey);
    if (cacheHit != iniConfigCache.end()) {
        return cacheHit->second.get();
    }

    CachedConfigPtr config(new Config());
    if (!configInit(config.get()) || !sfall_read_named_ini(iniFileName, config.get())) {
        // Negative cache: remember that this file could not be read.
        iniConfigCache.emplace(std::move(cacheKey), nullptr);
        return nullptr;
    }

    return iniConfigCache.emplace(std::move(cacheKey), std::move(config)).first->second.get();
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

    std::string cacheKey = sfall_normalize_ini_cache_key(fileName);

    // Invalidate cached data so subsequent reads pick up the new value. The
    // persistent array is left for game reset to free (it holds nested sub-array
    // IDs and may still be referenced by a script).
    iniConfigCache.erase(cacheKey);
    iniConfigArrayCache.erase(cacheKey);

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
        sfall_build_relative_ini_path(fileName, path, sizeof(path));
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

// Copies a config section's key/value pairs into an associative array.
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

// Builds a persistent associative array mapping each section name to a nested
// associative array of that section's key/value pairs.
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

    // Return the cached array if it still exists; otherwise drop the stale entry and rebuild.
    auto& arrayCache = isDb ? iniConfigArrayCacheDat : iniConfigArrayCache;
    std::string cacheKey = isDb ? fileName : sfall_normalize_ini_cache_key(fileName);
    auto cacheHit = arrayCache.find(cacheKey);
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

    arrayCache.emplace(std::move(cacheKey), arrayId);
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
