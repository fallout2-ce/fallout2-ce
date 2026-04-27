#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "db.h"
#include "memory.h"
#include "platform_compat.h"

namespace fallout {

#define CONFIG_FILE_MAX_LINE_LENGTH (1024)

// The initial number of sections (or key-value) pairs in the config.
#define CONFIG_INITIAL_CAPACITY (10)

struct CaseInsensitiveLess {
    bool operator()(const std::string& a, const std::string& b) const
    {
        return compat_stricmp(a.c_str(), b.c_str()) < 0;
    }
};

typedef std::set<std::string, CaseInsensitiveLess> StringSet;

static bool configParseLine(Config* config, char* string);
static bool configParseKeyValue(char* string, char* key, char* value);
static bool configEnsureSectionExists(Config* config, const char* sectionKey);
static bool configTrimString(char* string);

static bool configWriteDb(Config* config, const char* filePath);
static bool configWriteStandard(Config* config, const char* filePath);
static bool configWriteSideBySide(Config* config, const char* filePath, int flags);
static void configWriteSection(FILE* stream, const char* sectionName, ConfigSection* section, const StringSet* handledKeys);

// Last section key read from .INI file.
//
// 0x518224
static char gConfigLastSectionKey[CONFIG_FILE_MAX_LINE_LENGTH] = "unknown";

// 0x42BD90
bool configInit(Config* config)
{
    if (config == nullptr) {
        return false;
    }

    if (dictionaryInit(config, CONFIG_INITIAL_CAPACITY, sizeof(ConfigSection), nullptr) != 0) {
        return false;
    }

    return true;
}

// 0x42BDBC
void configFree(Config* config)
{
    if (config == nullptr) {
        return;
    }

    for (int sectionIndex = 0; sectionIndex < config->entriesLength; sectionIndex++) {
        DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);

        ConfigSection* section = (ConfigSection*)sectionEntry->value;
        for (int keyValueIndex = 0; keyValueIndex < section->entriesLength; keyValueIndex++) {
            DictionaryEntry* keyValueEntry = &(section->entries[keyValueIndex]);

            char** value = (char**)keyValueEntry->value;
            internal_free(*value);
            *value = nullptr;
        }

        dictionaryFree(section);
    }

    dictionaryFree(config);
}

// Parses command line argments and adds them into the config.
//
// The expected format of [argv] elements are "[section]key=value", otherwise
// the element is silently ignored.
//
// NOTE: This function trims whitespace in key-value pair, but not in section.
// I don't know if this is intentional or it's bug.
//
// 0x42BE38
bool configParseCommandLineArguments(Config* config, int argc, char** argv)
{
    if (config == nullptr) {
        return false;
    }

    for (int arg = 0; arg < argc; arg++) {
        char* pch;
        char* string = argv[arg];

        // Find opening bracket.
        pch = strchr(string, '[');
        if (pch == nullptr) {
            continue;
        }

        char* sectionKey = pch + 1;

        // Find closing bracket.
        pch = strchr(sectionKey, ']');
        if (pch == nullptr) {
            continue;
        }

        *pch = '\0';

        char key[260];
        char value[260];
        if (configParseKeyValue(pch + 1, key, value)) {
            if (!configSetString(config, sectionKey, key, value)) {
                *pch = ']';
                return false;
            }
        }

        *pch = ']';
    }

    return true;
}

// TODO: use const char** for valuePtr to enforce read-only API
// 0x42BF48
bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || valuePtr == nullptr) {
        return false;
    }

    int sectionIndex = dictionaryGetIndexByKey(config, sectionKey);
    if (sectionIndex == -1) {
        return false;
    }

    DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
    ConfigSection* section = (ConfigSection*)sectionEntry->value;

    int index = dictionaryGetIndexByKey(section, key);
    if (index == -1) {
        return false;
    }

    DictionaryEntry* keyValueEntry = &(section->entries[index]);
    *valuePtr = *(char**)keyValueEntry->value;

    return true;
}

bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr, const char* defaultValue)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || valuePtr == nullptr) {
        return false;
    }
    if (!configGetString(config, sectionKey, key, valuePtr) || (*valuePtr)[0] == '\0') {
        *valuePtr = const_cast<char*>(defaultValue);
    }
    return true;
}

// 0x42BF90
bool configSetString(Config* config, const char* sectionKey, const char* key, const char* value)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || value == nullptr) {
        return false;
    }

    int sectionIndex = dictionaryGetIndexByKey(config, sectionKey);
    if (sectionIndex == -1) {
        if (!configEnsureSectionExists(config, sectionKey)) {
            return false;
        }
        sectionIndex = dictionaryGetIndexByKey(config, sectionKey);
    }

    DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
    ConfigSection* section = (ConfigSection*)sectionEntry->value;

    int index = dictionaryGetIndexByKey(section, key);
    if (index != -1) {
        DictionaryEntry* keyValueEntry = &(section->entries[index]);

        char** existingValue = (char**)keyValueEntry->value;
        internal_free(*existingValue);
        *existingValue = nullptr;

        dictionaryRemoveValue(section, key);
    }

    char* valueCopy = internal_strdup(value);
    if (valueCopy == nullptr) {
        return false;
    }

    if (dictionaryAddValue(section, key, &valueCopy) == -1) {
        internal_free(valueCopy);
        return false;
    }

    return true;
}

// 0x42C05C
bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr, unsigned char base /* = 0 */)
{
    if (valuePtr == nullptr) {
        return false;
    }

    char* stringValue;
    if (!configGetString(config, sectionKey, key, &stringValue)) {
        return false;
    }

    char* end;
    errno = 0;
    long l = strtol(stringValue, &end, base); // see https://stackoverflow.com/a/6154614

    // The link above says right things about converting strings to numbers,
    // however we need to maintain compatibility with atoi implementation and
    // original game data. One example of the problem is worldmap.txt where
    // frequency values expressed as percentages (Frequent=38%). If we handle
    // the result like the link above suggests (and what previous implementation
    // provided), we'll simply end up returning `false`, since there will be
    // unconverted characters left. On the other hand, this function is also
    // used to parse Sfall config values, which uses hexadecimal notation to
    // represent colors. We're not going to need any of these in the long run so
    // for now simply ignore any error that could arise during conversion.

    *valuePtr = l;

    return true;
}

bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr, const int defaultValue, unsigned char base)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || valuePtr == nullptr) {
        return false;
    }
    if (!configGetInt(config, sectionKey, key, valuePtr, base)) {
        *valuePtr = defaultValue;
    }
    return true;
}

// 0x42C090
bool configGetIntList(Config* config, const char* sectionKey, const char* key, int* arr, int count)
{
    if (arr == nullptr || count < 2) {
        return false;
    }

    char* string;
    if (!configGetString(config, sectionKey, key, &string)) {
        return false;
    }

    char temp[CONFIG_FILE_MAX_LINE_LENGTH];
    string = strncpy(temp, string, CONFIG_FILE_MAX_LINE_LENGTH - 1);

    while (1) {
        char* pch = strchr(string, ',');
        if (pch == nullptr) {
            break;
        }

        count--;
        if (count == 0) {
            break;
        }

        *pch = '\0';
        *arr++ = atoi(string);
        string = pch + 1;
    }

    // SFALL: Fix getting last item in a list if the list has less than the
    // requested number of values (for `chem_primary_desire`).
    if (count > 0) {
        *arr = atoi(string);
        count--;
    }

    return count == 0;
}

// 0x42C160
bool configSetInt(Config* config, const char* sectionKey, const char* key, int value)
{
    char stringValue[20];
    compat_itoa(value, stringValue, 10);

    return configSetString(config, sectionKey, key, stringValue);
}

// Reads .INI file into config.
//
// 0x42C280
bool configRead(Config* config, const char* filePath, bool isDb)
{
    if (config == nullptr || filePath == nullptr) {
        return false;
    }

    char string[CONFIG_FILE_MAX_LINE_LENGTH];

    if (isDb) {
        File* stream = fileOpen(filePath, "rb");

        // CE: Return `false` if file does not exists in database.
        if (stream == nullptr) {
            return false;
        }

        while (fileReadString(string, sizeof(string), stream) != nullptr) {
            configParseLine(config, string);
        }
        fileClose(stream);

        // Build patch file path by inserting "#patch" before the extension.
        char patchPath[COMPAT_MAX_PATH];
        const char* dot = strrchr(filePath, '.');
        if (dot != nullptr) {
            snprintf(patchPath, sizeof(patchPath), "%.*s#patch%s", (int)(dot - filePath), filePath, dot);
        } else {
            snprintf(patchPath, sizeof(patchPath), "%s#patch", filePath);
        }

        struct PatchContext {
            Config* config;
            char string[CONFIG_FILE_MAX_LINE_LENGTH];
        } patchCtx = { config };

        xfileOpenEachReverse(patchPath, "rb", [](XFile* file, void* ctx) {
            auto* pc = static_cast<PatchContext*>(ctx);
            while (fileReadString(pc->string, sizeof(pc->string), file) != nullptr) {
                configParseLine(pc->config, pc->string);
            } }, &patchCtx);
    } else {
        FILE* stream = compat_fopen(filePath, "rt");

        // CE: Return `false` if file does not exists on the file system.
        if (stream == nullptr) {
            return false;
        }

        while (compat_fgets(string, sizeof(string), stream) != nullptr) {
            configParseLine(config, string);
        }
        fclose(stream);
    }

    return true;
}

// Writes config into .INI file.
//
// 0x42C324
bool configWrite(Config* config, const char* filePath, bool isDb)
{
    return configWriteEx(config, filePath, isDb ? CONFIG_IS_DB : CONFIG_DEFAULT);
}

bool configWriteEx(Config* config, const char* filePath, int flags)
{
    if (config == nullptr || filePath == nullptr) {
        return false;
    }

    if (flags & CONFIG_IS_DB) {
        return configWriteDb(config, filePath);
    }

    if (flags & (CONFIG_RETAIN_COMMENTS | CONFIG_RETAIN_ORDER | CONFIG_RETAIN_UNKNOWN)) {
        return configWriteSideBySide(config, filePath, flags);
    }

    return configWriteStandard(config, filePath);
}

static bool configWriteDb(Config* config, const char* filePath)
{
    File* stream = fileOpen(filePath, "wt");
    if (stream == nullptr) {
        return false;
    }

    for (int i = 0; i < config->entriesLength; i++) {
        DictionaryEntry* sectionEntry = &(config->entries[i]);
        filePrintFormatted(stream, "[%s]\n", sectionEntry->key);

        ConfigSection* section = (ConfigSection*)sectionEntry->value;
        for (int j = 0; j < section->entriesLength; j++) {
            DictionaryEntry* keyValueEntry = &(section->entries[j]);
            filePrintFormatted(stream, "%s=%s\n", keyValueEntry->key, *(char**)keyValueEntry->value);
        }
        filePrintFormatted(stream, "\n");
    }

    fileClose(stream);
    return true;
}

static bool configWriteStandard(Config* config, const char* filePath)
{
    FILE* stream = compat_fopen(filePath, "wt");
    if (stream == nullptr) {
        return false;
    }

    for (int i = 0; i < config->entriesLength; i++) {
        configWriteSection(stream, config->entries[i].key, (ConfigSection*)config->entries[i].value, nullptr);
        fprintf(stream, "\n");
    }

    fclose(stream);
    return true;
}

static void configWriteSection(FILE* stream, const char* sectionName, ConfigSection* section, const StringSet* handledKeys)
{
    if (sectionName != nullptr) {
        fprintf(stream, "[%s]\n", sectionName);
    }

    for (int i = 0; i < section->entriesLength; i++) {
        DictionaryEntry* entry = &(section->entries[i]);
        if (handledKeys == nullptr || handledKeys->find(entry->key) == handledKeys->end()) {
            fprintf(stream, "%s=%s\n", entry->key, *(char**)entry->value);
        }
    }
}

static bool configWriteSideBySide(Config* config, const char* filePath, int flags)
{
    bool retainUnknown = (flags & CONFIG_RETAIN_UNKNOWN) != 0;

    FILE* original = compat_fopen(filePath, "rt");
    if (original == nullptr) {
        if (errno == ENOENT) {
            // File doesn't exist, nothing to retain, perform standard write.
            return configWriteStandard(config, filePath);
        }
        // Other error (e.g., permissions), fail as requested.
        return false;
    }

    char tempPath[COMPAT_MAX_PATH];
    int tempPathLength = snprintf(tempPath, sizeof(tempPath), "%s.tmp", filePath);
    if (tempPathLength < 0 || tempPathLength >= (int)sizeof(tempPath)) {
        fclose(original);
        return false;
    }
    FILE* output = compat_fopen(tempPath, "wt");
    if (output == nullptr) {
        fclose(original);
        return false;
    }

    StringSet handledSections;
    StringSet handledKeys;
    std::vector<std::string> pendingLines;

    char currentSectionName[CONFIG_FILE_MAX_LINE_LENGTH] = "";
    ConfigSection* currentSection = nullptr;
    // Treat pre-section preamble as "known" so its comments are preserved.
    bool currentSectionKnown = true;

    char line[CONFIG_FILE_MAX_LINE_LENGTH];
    while (compat_fgets(line, sizeof(line), original) != nullptr) {
        char lineCopy[CONFIG_FILE_MAX_LINE_LENGTH];
        strcpy(lineCopy, line);

        char* trimmed = lineCopy;
        while (isspace(static_cast<unsigned char>(*trimmed))) {
            trimmed++;
        }

        // Buffer comments and empty lines; drop them if inside a suppressed section.
        if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') {
            if (!currentSectionKnown && !retainUnknown) {
                continue;
            }
            if ((flags & CONFIG_RETAIN_COMMENTS) || (*trimmed == '\0')) {
                pendingLines.emplace_back(line);
            }
            continue;
        }

        if (*trimmed == '[') {
            // End current section, write out remaining keys from it.
            if (currentSection != nullptr) {
                configWriteSection(output, nullptr, currentSection, &handledKeys);
                handledKeys.clear();
                currentSection = nullptr;
            }

            // Parse the section name before deciding whether to write.
            currentSectionKnown = false;
            currentSectionName[0] = '\0';

            char* end = strchr(trimmed, ']');
            if (end != nullptr) {
                *end = '\0';
                strcpy(currentSectionName, trimmed + 1);
                configTrimString(currentSectionName);
                *end = ']';

                int sectionIndex = dictionaryGetIndexByKey(config, currentSectionName);
                if (sectionIndex != -1) {
                    currentSection = (ConfigSection*)config->entries[sectionIndex].value;
                    handledSections.insert(currentSectionName);
                    currentSectionKnown = true;
                }
            }

            if (currentSectionKnown || retainUnknown) {
                // Flush buffered comments/empty lines that preceded this header.
                for (const std::string& l : pendingLines) {
                    fprintf(output, "%s", l.c_str());
                }
                pendingLines.clear();
                fprintf(output, "%s", line);
            } else {
                // Suppress unknown section entirely, discard buffered lines.
                pendingLines.clear();
            }
            continue;
        }

        char* equals = strchr(trimmed, '=');
        if (equals != nullptr) {
            *equals = '\0';
            char key[CONFIG_FILE_MAX_LINE_LENGTH];
            strcpy(key, trimmed);
            configTrimString(key);
            *equals = '=';

            char* value = nullptr;
            if (currentSection != nullptr) {
                configGetString(config, currentSectionName, key, &value);
            }

            if (value != nullptr) {
                // Flush lines that were between previous key and this one.
                for (const std::string& l : pendingLines) {
                    fprintf(output, "%s", l.c_str());
                }
                pendingLines.clear();

                const char* k = key;
                char* comment = nullptr;

                if (flags & CONFIG_RETAIN_COMMENTS) {
                    *equals = '\0';
                    k = trimmed;
                    if ((comment = strchr(equals + 1, ';')) == nullptr) {
                        comment = strchr(equals + 1, '#');
                    }
                }

                fprintf(output, "%s=%s%s%s", k, value, comment ? " " : "", comment ? comment : "\n");
                handledKeys.insert(key);
            } else if (retainUnknown) {
                // Key not in config - write verbatim from original file.
                for (const std::string& l : pendingLines) {
                    fprintf(output, "%s", l.c_str());
                }
                pendingLines.clear();
                fprintf(output, "%s", line);
            }
            // else: drop key; keep pending lines buffered so they attach to the
            // next surviving key rather than becoming orphaned.
            continue;
        }

        // Unknown line structure - buffer if retaining comments/structure.
        if ((currentSectionKnown || retainUnknown) && (flags & CONFIG_RETAIN_COMMENTS)) {
            pendingLines.emplace_back(line);
        }
    }

    // Write remaining keys in the last processed section.
    if (currentSection != nullptr) {
        configWriteSection(output, nullptr, currentSection, &handledKeys);
    }

    // Flush remaining comments/empty lines.
    for (const std::string& l : pendingLines) {
        fprintf(output, "%s", l.c_str());
    }
    pendingLines.clear();

    // Write completely new sections, separated from existing content.
    bool firstNewSection = true;
    for (int i = 0; i < config->entriesLength; i++) {
        DictionaryEntry* sectionEntry = &(config->entries[i]);
        if (handledSections.find(sectionEntry->key) == handledSections.end()) {
            if (firstNewSection) {
                fprintf(output, "\n");
                firstNewSection = false;
            }
            configWriteSection(output, sectionEntry->key, (ConfigSection*)sectionEntry->value, nullptr);
            fprintf(output, "\n");
        }
    }

    fclose(output);
    fclose(original);

    std::string backupPath = std::string(filePath) + ".bak";

    if (compat_remove(backupPath.c_str()) != 0 && errno != ENOENT) {
        compat_remove(tempPath);
        return false;
    }

    if (compat_rename(filePath, backupPath.c_str()) != 0) {
        compat_remove(tempPath);
        return false;
    }

    if (compat_rename(tempPath, filePath) != 0) {
        compat_rename(backupPath.c_str(), filePath);
        return false;
    }

    if (compat_remove(backupPath.c_str()) != 0 && errno != ENOENT) {
        return false;
    }
    return true;
}

// Parses a line from .INI file into config.
//
// A line either contains a "[section]" section key or "key=value" pair. In the
// first case section key is not added to config immediately, instead it is
// stored in [gConfigLastSectionKey] for later usage. This prevents empty
// sections in the config.
//
// In case of key-value pair it pretty straight forward - it adds key-value
// pair into previously read section key stored in [gConfigLastSectionKey].
//
// Returns `true` when a section was parsed or key-value pair was parsed and
// added to the config, or `false` otherwise.
//
// 0x42C4BC
static bool configParseLine(Config* config, char* string)
{
    char* pch;

    // Find comment marker and truncate the string.
    pch = strchr(string, ';');
    if (pch == nullptr) {
        pch = strchr(string, '#');
    }

    if (pch != nullptr) {
        *pch = '\0';
    }

    // CE: Original implementation treats any line with brackets as section key.
    // The problem can be seen when loading Olympus settings (ddraw.ini), which
    // contains the following line:
    //
    //  ```ini
    //  VersionString=Olympus 2207 [Complete].
    //  ```
    //
    // It thinks that [Complete] is a start of new section, and puts remaining
    // keys there.

    // Skip leading whitespace.
    while (isspace(static_cast<unsigned char>(*string))) {
        string++;
    }

    // Check if it's a section key.
    if (*string == '[') {
        char* sectionKey = string + 1;

        // Find closing bracket.
        pch = strchr(sectionKey, ']');
        if (pch != nullptr) {
            *pch = '\0';
            strcpy(gConfigLastSectionKey, sectionKey);
            return configTrimString(gConfigLastSectionKey);
        }
    }

    char key[260];
    char value[260];
    if (!configParseKeyValue(string, key, value)) {
        return false;
    }

    return configSetString(config, gConfigLastSectionKey, key, value);
}

// Splits "key=value" pair from [string] and copy appropriate parts into [key]
// and [value] respectively.
//
// Both key and value are trimmed.
//
// 0x42C594
static bool configParseKeyValue(char* string, char* key, char* value)
{
    if (string == nullptr || key == nullptr || value == nullptr) {
        return false;
    }

    // Find equals character.
    char* pch = strchr(string, '=');
    if (pch == nullptr) {
        return false;
    }

    *pch = '\0';

    strcpy(key, string);
    strcpy(value, pch + 1);

    *pch = '=';

    configTrimString(key);
    configTrimString(value);

    return true;
}

// Ensures the config has a section with specified key.
//
// Return `true` if section exists or it was successfully added, or `false`
// otherwise.
//
// 0x42C638
static bool configEnsureSectionExists(Config* config, const char* sectionKey)
{
    if (config == nullptr || sectionKey == nullptr) {
        return false;
    }

    if (dictionaryGetIndexByKey(config, sectionKey) != -1) {
        // Section already exists, no need to do anything.
        return true;
    }

    ConfigSection section;
    if (dictionaryInit(&section, CONFIG_INITIAL_CAPACITY, sizeof(char**), nullptr) == -1) {
        return false;
    }

    if (dictionaryAddValue(config, sectionKey, &section) == -1) {
        return false;
    }

    return true;
}

// Removes leading and trailing whitespace from the specified string.
//
// 0x42C698
static bool configTrimString(char* string)
{
    if (string == nullptr) {
        return false;
    }

    size_t length = strlen(string);
    if (length == 0) {
        return true;
    }

    // Starting from the end of the string, loop while it's a whitespace and
    // decrement string length.
    char* pch = string + length - 1;
    while (length != 0 && isspace(static_cast<unsigned char>(*pch))) {
        length--;
        pch--;
    }

    // pch now points to the last non-whitespace character.
    pch[1] = '\0';

    // Starting from the beginning of the string loop while it's a whitespace
    // and decrement string length.
    pch = string;
    while (isspace(static_cast<unsigned char>(*pch))) {
        pch++;
        length--;
    }

    // pch now points for to the first non-whitespace character.
    memmove(string, pch, length + 1);

    return true;
}

// 0x42C718
bool configGetDouble(Config* config, const char* sectionKey, const char* key, double* valuePtr)
{
    if (valuePtr == nullptr) {
        return false;
    }

    char* stringValue;
    if (!configGetString(config, sectionKey, key, &stringValue)) {
        return false;
    }

    *valuePtr = strtod(stringValue, nullptr);

    return true;
}

// 0x42C74C
bool configSetDouble(Config* config, const char* sectionKey, const char* key, double value)
{
    char stringValue[32];
    snprintf(stringValue, sizeof(stringValue), "%.6f", value);

    return configSetString(config, sectionKey, key, stringValue);
}

// NOTE: Boolean-typed variant of [configGetInt].
bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr)
{
    if (valuePtr == nullptr) {
        return false;
    }

    int integerValue;
    if (!configGetInt(config, sectionKey, key, &integerValue)) {
        return false;
    }

    *valuePtr = integerValue != 0;

    return true;
}

bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr, const bool defaultValue)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || valuePtr == nullptr) {
        return false;
    }
    if (!configGetBool(config, sectionKey, key, valuePtr)) {
        *valuePtr = defaultValue;
    }
    return true;
}

// NOTE: Boolean-typed variant of [configGetInt].
bool configSetBool(Config* config, const char* sectionKey, const char* key, bool value)
{
    return configSetInt(config, sectionKey, key, value ? 1 : 0);
}

} // namespace fallout
