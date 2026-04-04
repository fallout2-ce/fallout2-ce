#include "sfall_ini.h"

#include <algorithm>
#include <cstdio> // for snprintf
#include <cstring> // for strncpy, strlen

#include "config.h"
#include "debug.h"
#include "interpreter.h"
#include "platform_compat.h"
#include "sfall_arrays.h"
#include "sfall_metarules.h"

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

// Loads an INI file specified by 'ini_file_name' (e.g., "myconfig.ini" or "ddraw.ini")
// into the provided 'config_out' object.
// The 'config_out' object must be initialized by the caller (using configInit).
// The caller is also responsible for freeing 'config_out' (using configFree).
// Returns true if the file was successfully found and read, false otherwise.
static bool sfall_load_named_ini_file(const char* ini_file_name, Config* config_out)
{
    if (ini_file_name == nullptr || config_out == nullptr) {
        return false;
    }

    char path[COMPAT_MAX_PATH];
    bool loaded = false;

    if (basePath[0] != '\0' && !is_system_file_name(ini_file_name)) {
        // Attempt to load requested file in base directory.
        snprintf(path, sizeof(path), "%s\\%s", basePath, ini_file_name);
        loaded = configRead(config_out, path, false);
    }

    if (!loaded) {
        // There was no base path set, requested file is a system config, or
        // non-system config file was not found the base path - attempt to load
        // from current working directory.
        strncpy(path, ini_file_name, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        loaded = configRead(config_out, path, false);
    }

    return loaded;
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

    Config config;
    if (!configInit(&config)) {
        return false;
    }

    bool loaded = sfall_load_named_ini_file(fileName, &config);

    // NOTE: Sfall's `GetIniSetting` returns error code (-1) only when it cannot
    // parse triplet. Otherwise the default for string settings is empty string.
    value[0] = '\0';

    if (loaded) {
        char* stringValue;
        if (configGetString(&config, section, key, &stringValue)) {
            strncpy(value, stringValue, size - 1);
            value[size - 1] = '\0';
            if (found != nullptr) {
                *found = true;
            }
        }
    }

    configFree(&config);

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

    Config config;
    if (!configInit(&config)) {
        return false;
    }

    char path[COMPAT_MAX_PATH];
    bool loaded = false;

    if (basePath[0] != '\0' && !is_system_file_name(fileName)) {
        // Attempt to load requested file in base directory.
        snprintf(path, sizeof(path), "%s\\%s", basePath, fileName);
        loaded = configRead(&config, path, false);
    }

    if (!loaded) {
        // There was no base path set, requested file is a system config, or
        // non-system config file was not found the base path - attempt to load
        // from current working directory.
        strcpy(path, fileName);
        loaded = configRead(&config, path, false);
    }

    configSetString(&config, section, key, value);

    bool saved = configWrite(&config, path, false);

    configFree(&config);

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

// set_ini_setting
void mf_set_ini_setting(MetaruleContext& ctx)
{
    const char* triplet = ctx.stringArg(0);
    ProgramValue value = ctx.arg(1);

    if (value.isString()) {
        const char* stringValue = programGetString(ctx.program(), value.opcode, value.integerValue);
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
void mf_get_ini_section(MetaruleContext& ctx)
{
    // Arguments: file_path (string), section_name (string)

    const char* filePath = ctx.stringArg(0);
    const char* sectionName = ctx.stringArg(1);

    ArrayId arrayId = CreateTempArray(-1, 0);

    if (filePath == nullptr || sectionName == nullptr) {
        ctx.setReturn(static_cast<int>(arrayId));
        return;
    }

    Config iniConfig;
    if (!configInit(&iniConfig)) {
        debugPrint("mf_get_ini_section: Failed to initialize Config structure.");
        ctx.setReturn(static_cast<int>(arrayId));
        return;
    }

    if (sfall_load_named_ini_file(filePath, &iniConfig)) {
        const ConfigSection* section = sfall_find_section_in_config(&iniConfig, sectionName);

        if (section != nullptr) {
            for (int i = 0; i < section->entriesLength; ++i) {
                DictionaryEntry* entry = &(section->entries[i]);
                const char* key = entry->key;
                const char* value = *(static_cast<char**>(entry->value));

                if (key != nullptr && value != nullptr) {
                    SetArray(arrayId, programMakeString(ctx.program(), key), programMakeString(ctx.program(), value), false, ctx.program());
                }
            }
        }
    }

    configFree(&iniConfig);

    ctx.setReturn(static_cast<int>(arrayId));
}

// get_ini_sections
void mf_get_ini_sections(MetaruleContext& ctx)
{
    // Arguments: file_path (string)
    const char* filePath = ctx.stringArg(0);
    ArrayId arrayId = -1;

    if (filePath == nullptr) {
        ctx.setReturn(static_cast<int>(arrayId));
        return;
    }

    Config iniConfig;
    if (!configInit(&iniConfig)) {
        debugPrint("mf_get_ini_sections: Failed to initialize Config structure.");
        ctx.setReturn(static_cast<int>(arrayId));
        return;
    }

    // note: seems to load sections in random order
    if (sfall_load_named_ini_file(filePath, &iniConfig)) {
        if (iniConfig.entriesLength > 0) {
            arrayId = CreateTempArray(iniConfig.entriesLength, 0);
            for (int i = 0; i < iniConfig.entriesLength; ++i) {
                DictionaryEntry* entry = &(iniConfig.entries[i]);
                const char* sectionName = entry->key;

                if (sectionName != nullptr) {
                    SetArray(arrayId, programMakeInt(ctx.program(), i), programMakeString(ctx.program(), sectionName), false, ctx.program());
                }
            }
        }
    }

    configFree(&iniConfig);

    if (arrayId == -1) {
        arrayId = CreateTempArray(0, 0);
    }

    ctx.setReturn(static_cast<int>(arrayId));
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

} // namespace fallout
