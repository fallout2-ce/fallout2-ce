#ifndef CONFIG_H
#define CONFIG_H

#include "dictionary.h"

namespace fallout {

enum class ConfigFlags : unsigned int {
    Default = 0,
    IsDatabase = 1, // Config will be read from DB (VFS)
    RetainComments = 2, // Keep existing comments when overwriting file. Also retains order.
    RetainOrder = 4, // Keep existing order of keys when overwriting file.
    RetainUnknown = 8, // Keep sections and keys that are not in the saved config data when overwriting. Also retains order.
    RetainAll = RetainComments | RetainOrder | RetainUnknown, // Keep existing lines as much as possible when overwriting file, only add/update values.
};

constexpr inline ConfigFlags operator&(ConfigFlags lhs, ConfigFlags rhs)
{
    return static_cast<ConfigFlags>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline ConfigFlags operator|(ConfigFlags lhs, ConfigFlags rhs)
{
    return static_cast<ConfigFlags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

// A representation of .INI file.
//
// It's implemented as a [Dictionary] whos keys are section names of .INI file,
// and it's values are [ConfigSection] structs.
typedef Dictionary Config;

// Representation of .INI section.
//
// It's implemented as a [Dictionary] whos keys are names of .INI file
// key-pair values, and it's values are pointers to strings (char**).
typedef Dictionary ConfigSection;

bool configInit(Config* config);
void configFree(Config* config);
// Deep-copies into an initialized, empty destination. On failure, the
// destination may contain a partial copy and must still be freed.
bool configCopy(Config* destination, const Config* source);
// Calls onOverride before replacing each successfully parsed section/key.
bool configParseCommandLineArguments(Config* config, int argc, char** argv, void (*onOverride)(const char*, const char*) = nullptr);
// TODO: valuePtr must be const char**
bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr);
// Tries to load a string value from Config into valuePtr without any conversion. If value doesn't exist, or it's an empty string, assigns defaultValue instead.
// No copy is performed. The returned pointer may refer either to an internal string or to defaultValue; it must be treated as read-only, and callers must ensure defaultValue remains valid for the duration of use.
bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr, const char* defaultValue);
bool configSetString(Config* config, const char* sectionKey, const char* key, const char* value);
bool configRemoveKey(Config* config, const char* sectionKey, const char* key);
bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr);
bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr, int defaultValue);
bool configGetIntBase(Config* config, const char* sectionKey, const char* key, int* valuePtr, int defaultValue, int base);

template <typename T>
inline bool configGetEnum(Config* config, const char* sectionKey, const char* key, T* valuePtr)
{
    int temp = 0;
    bool result = configGetInt(config, sectionKey, key, &temp);
    if (result) {
        *valuePtr = static_cast<T>(temp);
    }

    return result;
}

template <typename T>
inline bool configGetEnum(Config* config, const char* sectionKey, const char* key, T* valuePtr, T defaultValue)
{
    int temp = 0;
    bool result = configGetInt(config, sectionKey, key, &temp, static_cast<int>(defaultValue));
    if (result) {
        *valuePtr = static_cast<T>(temp);
    }

    return result;
}

bool configGetIntList(Config* config, const char* section, const char* key, int* arr, int count);
bool configSetInt(Config* config, const char* sectionKey, const char* key, int value);
bool configRead(Config* config, const char* filePath, bool isDb);
bool configWrite(Config* config, const char* filePath, bool isDb);
bool configWriteEx(Config* config, const char* filePath, ConfigFlags flags);
bool configGetDouble(Config* config, const char* sectionKey, const char* key, double* valuePtr);
bool configSetDouble(Config* config, const char* sectionKey, const char* key, double value);

bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr);
bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr, bool defaultValue);
bool configSetBool(Config* config, const char* sectionKey, const char* key, bool value);

class ScopedConfig {
public:
    ScopedConfig();
    ScopedConfig(const char* filePath, bool isDb);
    ~ScopedConfig();

    ScopedConfig(const ScopedConfig&) = delete;
    ScopedConfig& operator=(const ScopedConfig&) = delete;

    Config* get();
    const Config* get() const;

    explicit operator bool() const;

private:
    Config _config = {};
    bool _loaded = false;
};

} // namespace fallout

#endif /* CONFIG_H */
