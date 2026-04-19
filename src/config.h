#ifndef CONFIG_H
#define CONFIG_H

#include "dictionary.h"

namespace fallout {

enum ConfigFlags {
    CONFIG_DEFAULT = 0,
    // Config will be read from DB (VFS)
    CONFIG_IS_DB = (1 << 0),
    // Keep existing comments when overwriting file. Also retains order.
    CONFIG_RETAIN_COMMENTS = (1 << 1),
    // Keep existing order of keys when overwriting file.
    CONFIG_RETAIN_ORDER = (1 << 2),
    // Keep sections and keys that are not in the saved config data when overwriting. Also retains order.
    CONFIG_RETAIN_UNKNOWN = (1 << 3),
    // Keep existing lines as much as possible when overwriting file, only add/update values.
    CONFIG_RETAIN_ALL = CONFIG_RETAIN_COMMENTS | CONFIG_RETAIN_ORDER | CONFIG_RETAIN_UNKNOWN,
};

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
bool configParseCommandLineArguments(Config* config, int argc, char** argv);
// TODO: valuePtr must be const char**
bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr);
// Tries to load a string value from Config into valuePtr without any conversion. If value doesn't exist, or it's an empty string, assigns defaultValue instead.
// No copy is performed. The returned pointer may refer either to an internal string or to defaultValue; it must be treated as read-only, and callers must ensure defaultValue remains valid for the duration of use.
bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr, const char* defaultValue);
bool configSetString(Config* config, const char* sectionKey, const char* key, const char* value);
bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr, unsigned char base = 0);
bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr, int defaultValue, unsigned char base = 0);
bool configGetIntList(Config* config, const char* section, const char* key, int* arr, int count);
bool configSetInt(Config* config, const char* sectionKey, const char* key, int value);
bool configRead(Config* config, const char* filePath, bool isDb);
bool configWrite(Config* config, const char* filePath, bool isDb);
bool configWriteEx(Config* config, const char* filePath, int flags);
bool configGetDouble(Config* config, const char* sectionKey, const char* key, double* valuePtr);
bool configSetDouble(Config* config, const char* sectionKey, const char* key, double value);

bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr);
bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr, bool defaultValue);
bool configSetBool(Config* config, const char* sectionKey, const char* key, bool value);

} // namespace fallout

#endif /* CONFIG_H */
