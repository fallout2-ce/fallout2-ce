#include "hero_appearance.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "animation.h"
#include "color.h"
#include "config.h"
#include "content_config.h"
#include "debug.h"
#include "game_mouse.h"
#include "interface.h"
#include "inventory.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "obj_types.h"
#include "object.h"
#include "platform_compat.h"
#include "proto.h"
#include "sfall_global_vars.h"
#include "settings.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"
#include "xfile.h"

namespace fallout {

namespace {

constexpr char kRaceGlobalKey[] = "HAp_Race";
constexpr char kStyleGlobalKey[] = "HApStyle";
constexpr int kMaxSelectorValue = 99;
constexpr int kMaxHeroAppearanceEntries = 128;
constexpr int kMaxMountedPaths = 4;
constexpr int kSelectWindowWidth = 380;
constexpr int kSelectWindowHeight = 236;
constexpr int kSelectWindowDoneEvent = 500;
constexpr int kSelectWindowCancelEvent = 501;
constexpr int kSelectWindowPreviousEvent = 502;
constexpr int kSelectWindowNextEvent = 503;
constexpr int kSelectWindowPreviewX = 24;
constexpr int kSelectWindowPreviewY = 42;
constexpr int kSelectWindowPreviewWidth = 112;
constexpr int kSelectWindowPreviewHeight = 124;
constexpr int kSelectWindowDetailsX = 154;
constexpr int kSelectWindowDetailsY = 56;
constexpr int kSelectWindowButtonY = 196;

bool gHeroAppearanceInitialized = false;
bool gHeroAppearanceEnabled = false;
int gHeroAppearanceRace = 0;
int gHeroAppearanceStyle = 0;
char gMountedPaths[kMaxMountedPaths][COMPAT_MAX_PATH];
int gMountedPathsLength = 0;

typedef enum HeroAppearanceFallback {
    HERO_APPEARANCE_FALLBACK_SAFE,
    HERO_APPEARANCE_FALLBACK_NONE,
} HeroAppearanceFallback;

typedef struct HeroAppearanceEntry {
    char displayName[64];
    int race;
    int style;
    char model[13];
    char maleModel[13];
    char femaleModel[13];
    int baseCritterArtId;
    bool selectable;
    bool packageBacked;
    bool requireCompleteCoverage;
    HeroAppearanceFallback fallback;
    char coverage[24];
    char mappingBasis[24];
} HeroAppearanceEntry;

HeroAppearanceEntry gHeroAppearanceEntries[kMaxHeroAppearanceEntries];
int gHeroAppearanceEntriesLength = 0;

typedef struct HeroAppearancePreviewMount {
    char paths[2][COMPAT_MAX_PATH];
    int pathsLength;
} HeroAppearancePreviewMount;

void heroAppearanceCopyString(char* dest, size_t size, const char* value)
{
    if (dest == nullptr || size == 0) {
        return;
    }

    snprintf(dest, size, "%s", value != nullptr ? value : "");
}

char* heroAppearanceTrim(char* string)
{
    if (string == nullptr) {
        return nullptr;
    }

    while (isspace(static_cast<unsigned char>(*string))) {
        string++;
    }

    char* end = string + strlen(string);
    while (end > string && isspace(static_cast<unsigned char>(end[-1]))) {
        end--;
    }

    *end = '\0';
    return string;
}

bool heroAppearanceIsValidSelectorValue(int value)
{
    return value >= 0 && value <= kMaxSelectorValue;
}

const char* heroAppearanceFallbackName(HeroAppearanceFallback fallback)
{
    switch (fallback) {
    case HERO_APPEARANCE_FALLBACK_SAFE:
        return "safe";
    case HERO_APPEARANCE_FALLBACK_NONE:
        return "none";
    }

    return "safe";
}

HeroAppearanceFallback heroAppearanceParseFallback(const char* value)
{
    if (value != nullptr && compat_stricmp(value, "none") == 0) {
        return HERO_APPEARANCE_FALLBACK_NONE;
    }

    return HERO_APPEARANCE_FALLBACK_SAFE;
}

void heroAppearanceEntryInit(HeroAppearanceEntry* entry, const char* displayName, int race, int style)
{
    memset(entry, 0, sizeof(*entry));

    heroAppearanceCopyString(entry->displayName, sizeof(entry->displayName), displayName);
    entry->race = race;
    entry->style = style;
    entry->baseCritterArtId = -1;
    entry->selectable = true;
    entry->packageBacked = race != 0 || style != 0;
    entry->requireCompleteCoverage = true;
    entry->fallback = HERO_APPEARANCE_FALLBACK_SAFE;
    heroAppearanceCopyString(entry->coverage, sizeof(entry->coverage), "player");
    heroAppearanceCopyString(entry->mappingBasis, sizeof(entry->mappingBasis), entry->packageBacked ? "package" : "native");
}

int heroAppearanceFindEntryIndex(int race, int style)
{
    for (int index = 0; index < gHeroAppearanceEntriesLength; index++) {
        if (gHeroAppearanceEntries[index].race == race && gHeroAppearanceEntries[index].style == style) {
            return index;
        }
    }

    return -1;
}

HeroAppearanceEntry* heroAppearanceFindEntry(int race, int style)
{
    int index = heroAppearanceFindEntryIndex(race, style);
    return index != -1 ? &(gHeroAppearanceEntries[index]) : nullptr;
}

bool heroAppearanceAddOrUpdateEntry(const HeroAppearanceEntry* source)
{
    if (source == nullptr
        || !heroAppearanceIsValidSelectorValue(source->race)
        || !heroAppearanceIsValidSelectorValue(source->style)) {
        return false;
    }

    int index = heroAppearanceFindEntryIndex(source->race, source->style);
    if (index == -1) {
        if (gHeroAppearanceEntriesLength >= kMaxHeroAppearanceEntries) {
            return false;
        }

        index = gHeroAppearanceEntriesLength++;
    }

    gHeroAppearanceEntries[index] = *source;
    return true;
}

void heroAppearanceAddNativeRegistryEntry()
{
    HeroAppearanceEntry entry;
    heroAppearanceEntryInit(&entry, "Native", 0, 0);
    entry.packageBacked = false;
    entry.requireCompleteCoverage = false;
    heroAppearanceCopyString(entry.mappingBasis, sizeof(entry.mappingBasis), "native");
    heroAppearanceAddOrUpdateEntry(&entry);
}

void heroAppearanceReadRegistryConfigEntry(const char* entryId)
{
    if (entryId == nullptr || entryId[0] == '\0') {
        return;
    }

    char section[96];
    snprintf(section, sizeof(section), "appearance:%s", entryId);

    int race = -1;
    int style = -1;
    if (!configGetInt(&gContentConfig, section, "race", &race)
        || !configGetInt(&gContentConfig, section, "style", &style)
        || !heroAppearanceIsValidSelectorValue(race)
        || !heroAppearanceIsValidSelectorValue(style)) {
        return;
    }

    char* value = nullptr;
    configGetString(&gContentConfig, section, "name", &value, entryId);

    HeroAppearanceEntry entry;
    heroAppearanceEntryInit(&entry, value, race, style);

    configGetBool(&gContentConfig, section, "selectable", &entry.selectable, true);
    configGetBool(&gContentConfig, section, "package_backed", &entry.packageBacked, entry.packageBacked);
    configGetBool(&gContentConfig, section, "package", &entry.packageBacked, entry.packageBacked);
    configGetBool(&gContentConfig, section, "required_animation_coverage", &entry.requireCompleteCoverage, true);

    configGetInt(&gContentConfig, section, "base_art", &entry.baseCritterArtId, -1);
    configGetInt(&gContentConfig, section, "base_critter_art", &entry.baseCritterArtId, entry.baseCritterArtId);

    configGetString(&gContentConfig, section, "model", &value, "");
    heroAppearanceCopyString(entry.model, sizeof(entry.model), value);
    heroAppearanceCopyString(entry.maleModel, sizeof(entry.maleModel), value);
    heroAppearanceCopyString(entry.femaleModel, sizeof(entry.femaleModel), value);

    configGetString(&gContentConfig, section, "male_model", &value, entry.maleModel);
    heroAppearanceCopyString(entry.maleModel, sizeof(entry.maleModel), value);

    configGetString(&gContentConfig, section, "female_model", &value, entry.femaleModel);
    heroAppearanceCopyString(entry.femaleModel, sizeof(entry.femaleModel), value);

    configGetString(&gContentConfig, section, "coverage", &value, entry.coverage);
    heroAppearanceCopyString(entry.coverage, sizeof(entry.coverage), value);

    configGetString(&gContentConfig, section, "fallback", &value, heroAppearanceFallbackName(entry.fallback));
    entry.fallback = heroAppearanceParseFallback(value);

    const char* defaultMapping = entry.packageBacked ? "package" : (entry.baseCritterArtId >= 0 || entry.model[0] != '\0' ? "model" : "native");
    configGetString(&gContentConfig, section, "mapping", &value, defaultMapping);
    heroAppearanceCopyString(entry.mappingBasis, sizeof(entry.mappingBasis), value);

    heroAppearanceAddOrUpdateEntry(&entry);
}

void heroAppearanceReadRegistryConfig()
{
    char* entries = nullptr;
    configGetString(&gContentConfig, CONTENT_CONFIG_APPEARANCE_SECTION, "entries", &entries, "");
    if (entries == nullptr || entries[0] == '\0') {
        return;
    }

    char temp[1024];
    heroAppearanceCopyString(temp, sizeof(temp), entries);

    char* cursor = temp;
    while (cursor != nullptr && *cursor != '\0') {
        char* next = strpbrk(cursor, ",;");
        if (next != nullptr) {
            *next = '\0';
            next++;
        }

        char* entryId = heroAppearanceTrim(cursor);
        heroAppearanceReadRegistryConfigEntry(entryId);
        cursor = next;
    }
}

void heroAppearanceBuildRegistry()
{
    gHeroAppearanceEntriesLength = 0;
    heroAppearanceAddNativeRegistryEntry();
    heroAppearanceReadRegistryConfig();
}

void heroAppearanceAddDiscoveredPackageEntry(int race, int style)
{
    if (heroAppearanceFindEntryIndex(race, style) != -1) {
        return;
    }

    char displayName[64];
    snprintf(displayName, sizeof(displayName), "Package r%02d s%02d", race, style);

    HeroAppearanceEntry entry;
    heroAppearanceEntryInit(&entry, displayName, race, style);
    entry.packageBacked = race != 0 || style != 0;
    heroAppearanceCopyString(entry.coverage, sizeof(entry.coverage), "package");
    heroAppearanceCopyString(entry.mappingBasis, sizeof(entry.mappingBasis), "package");
    heroAppearanceAddOrUpdateEntry(&entry);
}

bool heroAppearanceIsMountedPath(const char* path)
{
    for (int index = 0; index < gMountedPathsLength; index++) {
        if (compat_stricmp(path, gMountedPaths[index]) == 0) {
            return true;
        }
    }

    return false;
}

bool heroAppearancePathAvailable(const char* path)
{
    if (heroAppearanceIsMountedPath(path)) {
        return true;
    }

    if (!xbaseOpen(path)) {
        return false;
    }

    xbaseClose(path);
    return true;
}

bool heroAppearancePackageAvailableForGender(int race, int style, char genderCode)
{
    char path[COMPAT_MAX_PATH];

    snprintf(path, sizeof(path), "appearance\\h%cr%02ds%02d.dat", genderCode, race, style);
    if (heroAppearancePathAvailable(path)) {
        return true;
    }

    snprintf(path, sizeof(path), "appearance\\h%cr%02ds%02d", genderCode, race, style);
    if (heroAppearancePathAvailable(path)) {
        return true;
    }

    return false;
}

bool heroAppearancePackageAvailable(int race, int style)
{
    if (!heroAppearanceIsValidSelectorValue(race) || !heroAppearanceIsValidSelectorValue(style)) {
        return false;
    }

    if (race == 0 && style == 0) {
        return true;
    }

    return heroAppearancePackageAvailableForGender(race, style, 'm')
        || heroAppearancePackageAvailableForGender(race, style, 'f');
}

bool heroAppearanceGetDudeGenderCode(char* genderCode)
{
    if (gDude == nullptr) {
        return false;
    }

    *genderCode = critterGetStat(gDude, STAT_GENDER) == GENDER_FEMALE ? 'f' : 'm';
    return true;
}

bool heroAppearancePackageAvailableForDude(int race, int style)
{
    if (!heroAppearanceIsValidSelectorValue(race) || !heroAppearanceIsValidSelectorValue(style)) {
        return false;
    }

    if (race == 0 && style == 0) {
        return true;
    }

    char genderCode;
    if (!heroAppearanceGetDudeGenderCode(&genderCode)) {
        return heroAppearancePackageAvailable(race, style);
    }

    return heroAppearancePackageAvailableForGender(race, style, genderCode);
}

void heroAppearanceDiscoverPackagesForSelector(bool isStyle, int fixedRace)
{
    if (isStyle) {
        for (int style = 0; style <= kMaxSelectorValue; style++) {
            if (heroAppearancePackageAvailableForDude(fixedRace, style)) {
                heroAppearanceAddDiscoveredPackageEntry(fixedRace, style);
            }
        }
    } else {
        for (int race = 0; race <= kMaxSelectorValue; race++) {
            if (heroAppearancePackageAvailableForDude(race, 0)) {
                heroAppearanceAddDiscoveredPackageEntry(race, 0);
            }
        }
    }
}

bool heroAppearanceEntryIsNative(const HeroAppearanceEntry* entry)
{
    return entry != nullptr && entry->race == 0 && entry->style == 0;
}

const char* heroAppearanceEntryGetModelForDude(const HeroAppearanceEntry* entry)
{
    if (entry == nullptr) {
        return nullptr;
    }

    char genderCode;
    if (heroAppearanceGetDudeGenderCode(&genderCode)) {
        const char* genderModel = genderCode == 'f' ? entry->femaleModel : entry->maleModel;
        if (genderModel[0] != '\0') {
            return genderModel;
        }
    }

    if (entry->model[0] != '\0') {
        return entry->model;
    }

    if (entry->maleModel[0] != '\0') {
        return entry->maleModel;
    }

    if (entry->femaleModel[0] != '\0') {
        return entry->femaleModel;
    }

    return nullptr;
}

int heroAppearanceEntryResolveBaseCritterArtId(const HeroAppearanceEntry* entry)
{
    if (entry == nullptr) {
        return -1;
    }

    if (entry->baseCritterArtId >= 0) {
        return entry->baseCritterArtId;
    }

    const char* model = heroAppearanceEntryGetModelForDude(entry);
    if (model == nullptr || model[0] == '\0') {
        return -1;
    }

    return artListIndex(OBJ_TYPE_CRITTER, model);
}

bool heroAppearanceEntryHasModelMapping(const HeroAppearanceEntry* entry)
{
    return heroAppearanceEntryResolveBaseCritterArtId(entry) >= 0;
}

bool heroAppearanceEntryAvailableForDude(const HeroAppearanceEntry* entry)
{
    if (entry == nullptr || !entry->selectable) {
        return false;
    }

    if (heroAppearanceEntryIsNative(entry)) {
        return true;
    }

    if (entry->packageBacked && !heroAppearancePackageAvailableForDude(entry->race, entry->style)) {
        return false;
    }

    if (!entry->packageBacked) {
        return heroAppearanceEntryHasModelMapping(entry);
    }

    return true;
}

bool heroAppearanceEntryMatchesSelector(const HeroAppearanceEntry* entry, bool isStyle, int fixedRace)
{
    if (!heroAppearanceEntryAvailableForDude(entry)) {
        return false;
    }

    if (isStyle) {
        return entry->race == fixedRace;
    }

    return entry->style == 0;
}

const HeroAppearanceEntry* heroAppearanceGetSelectedEntry()
{
    return heroAppearanceFindEntry(gHeroAppearanceRace, gHeroAppearanceStyle);
}

int heroAppearanceSelectWindowFindEntryIndex(bool isStyle, int fixedRace, int race, int style)
{
    for (int index = 0; index < gHeroAppearanceEntriesLength; index++) {
        const HeroAppearanceEntry* entry = &(gHeroAppearanceEntries[index]);
        if (entry->race == race
            && entry->style == style
            && heroAppearanceEntryMatchesSelector(entry, isStyle, fixedRace)) {
            return index;
        }
    }

    return -1;
}

int heroAppearanceSelectWindowFirstEntryIndex(bool isStyle, int fixedRace)
{
    for (int index = 0; index < gHeroAppearanceEntriesLength; index++) {
        if (heroAppearanceEntryMatchesSelector(&(gHeroAppearanceEntries[index]), isStyle, fixedRace)) {
            return index;
        }
    }

    return -1;
}

int heroAppearanceSelectWindowNextEntryIndex(bool isStyle, int fixedRace, int selectedIndex, int direction)
{
    int index = selectedIndex + direction;
    while (index >= 0 && index < gHeroAppearanceEntriesLength) {
        if (heroAppearanceEntryMatchesSelector(&(gHeroAppearanceEntries[index]), isStyle, fixedRace)) {
            return index;
        }

        index += direction;
    }

    return -1;
}

void heroAppearancePreviewMountInit(HeroAppearancePreviewMount* mount)
{
    mount->pathsLength = 0;

    for (int index = 0; index < 2; index++) {
        mount->paths[index][0] = '\0';
    }
}

bool heroAppearancePreviewMountRecord(HeroAppearancePreviewMount* mount, const char* path)
{
    if (mount->pathsLength >= 2) {
        return false;
    }

    snprintf(mount->paths[mount->pathsLength], sizeof(mount->paths[mount->pathsLength]), "%s", path);
    mount->pathsLength++;
    return true;
}

bool heroAppearancePreviewMountOne(HeroAppearancePreviewMount* mount, int race, int style, char genderCode)
{
    char path[COMPAT_MAX_PATH];

    snprintf(path, sizeof(path), "appearance\\h%cr%02ds%02d.dat", genderCode, race, style);
    if (heroAppearanceIsMountedPath(path)) {
        return true;
    }

    if (xbaseOpen(path)) {
        return heroAppearancePreviewMountRecord(mount, path);
    }

    snprintf(path, sizeof(path), "appearance\\h%cr%02ds%02d", genderCode, race, style);
    if (heroAppearanceIsMountedPath(path)) {
        return true;
    }

    if (xbaseOpen(path)) {
        return heroAppearancePreviewMountRecord(mount, path);
    }

    return false;
}

bool heroAppearancePreviewMount(HeroAppearancePreviewMount* mount, int race, int style)
{
    heroAppearancePreviewMountInit(mount);

    if (race == 0 && style == 0) {
        return true;
    }

    bool mounted = false;
    char genderCode;
    if (heroAppearanceGetDudeGenderCode(&genderCode)) {
        mounted = heroAppearancePreviewMountOne(mount, race, style, genderCode);
    } else {
        bool mountedMale = heroAppearancePreviewMountOne(mount, race, style, 'm');
        bool mountedFemale = heroAppearancePreviewMountOne(mount, race, style, 'f');
        mounted = mountedMale || mountedFemale;
    }

    if (mount->pathsLength > 0) {
        artCacheFlush();
    }

    return mounted;
}

void heroAppearancePreviewUnmount(HeroAppearancePreviewMount* mount)
{
    for (int index = 0; index < mount->pathsLength; index++) {
        xbaseClose(mount->paths[index]);
        mount->paths[index][0] = '\0';
    }

    if (mount->pathsLength > 0) {
        artCacheFlush();
    }

    mount->pathsLength = 0;
}

void heroAppearanceUnmountPaths()
{
    for (int index = 0; index < gMountedPathsLength; index++) {
        xbaseClose(gMountedPaths[index]);
        gMountedPaths[index][0] = '\0';
    }

    gMountedPathsLength = 0;
}

bool heroAppearanceRecordMount(const char* path)
{
    if (gMountedPathsLength >= kMaxMountedPaths) {
        return false;
    }

    snprintf(gMountedPaths[gMountedPathsLength], sizeof(gMountedPaths[gMountedPathsLength]), "%s", path);
    gMountedPathsLength++;
    return true;
}

bool heroAppearanceMountOne(char genderCode)
{
    char path[COMPAT_MAX_PATH];

    snprintf(path, sizeof(path), "appearance\\h%cr%02ds%02d.dat", genderCode, gHeroAppearanceRace, gHeroAppearanceStyle);
    if (xbaseOpen(path)) {
        return heroAppearanceRecordMount(path);
    }

    snprintf(path, sizeof(path), "appearance\\h%cr%02ds%02d", genderCode, gHeroAppearanceRace, gHeroAppearanceStyle);
    if (xbaseOpen(path)) {
        return heroAppearanceRecordMount(path);
    }

    return false;
}

void heroAppearanceRefreshMounts()
{
    heroAppearanceUnmountPaths();

    if (!gHeroAppearanceEnabled) {
        return;
    }

    const HeroAppearanceEntry* entry = heroAppearanceGetSelectedEntry();
    if (entry != nullptr && !entry->packageBacked) {
        return;
    }

    if (gHeroAppearanceRace == 0 && gHeroAppearanceStyle == 0) {
        return;
    }

    bool mounted = false;
    char genderCode;
    if (heroAppearanceGetDudeGenderCode(&genderCode)) {
        mounted = heroAppearanceMountOne(genderCode);
    } else {
        bool mountedMale = heroAppearanceMountOne('m');
        bool mountedFemale = heroAppearanceMountOne('f');
        mounted = mountedMale || mountedFemale;
    }

    if (!mounted) {
        debugPrint("Hero Appearance package not found for race %d style %d.\n", gHeroAppearanceRace, gHeroAppearanceStyle);
    }
}

int heroAppearanceGetNormalizedCritterIndex(int fid)
{
    if (FID_TYPE(fid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    return fid & 0xFFF;
}

void heroAppearanceReadGlobals()
{
    if (!sfall_gl_vars_is_initialized()) {
        gHeroAppearanceRace = 0;
        gHeroAppearanceStyle = 0;
        return;
    }

    int value = 0;
    if (sfall_gl_vars_fetch(kRaceGlobalKey, value) && heroAppearanceIsValidSelectorValue(value)) {
        gHeroAppearanceRace = value;
    } else {
        gHeroAppearanceRace = 0;
    }

    value = 0;
    if (sfall_gl_vars_fetch(kStyleGlobalKey, value) && heroAppearanceIsValidSelectorValue(value)) {
        gHeroAppearanceStyle = value;
    } else {
        gHeroAppearanceStyle = 0;
    }
}

int heroAppearanceSelectWindowBuildPreviewFid(const HeroAppearanceEntry* entry)
{
    if (gDude == nullptr || entry == nullptr) {
        return -1;
    }

    int baseFid = heroAppearanceRemoveCritterArtOffset(gDude->fid);
    int frmId = heroAppearanceGetNormalizedCritterIndex(baseFid);
    if (frmId < 0) {
        return -1;
    }

    if (!heroAppearanceEntryIsNative(entry) && !entry->packageBacked) {
        int modelFrmId = heroAppearanceEntryResolveBaseCritterArtId(entry);
        if (modelFrmId < 0 || modelFrmId > 0xFFF) {
            return -1;
        }

        frmId = modelFrmId;
    } else if (!heroAppearanceEntryIsNative(entry)) {
        int offset = artGetHeroAppearanceCritterArtOffset();
        if (offset <= 0) {
            return -1;
        }

        frmId += offset;
        if (frmId > 0xFFF) {
            return -1;
        }
    }

    return artResolveCritterFid(buildFid(OBJ_TYPE_CRITTER, frmId, ANIM_STAND, 0, ROTATION_SW));
}

void heroAppearanceSelectWindowDrawPreview(int win, const HeroAppearanceEntry* entry)
{
    windowFill(win,
        kSelectWindowPreviewX,
        kSelectWindowPreviewY,
        kSelectWindowPreviewWidth,
        kSelectWindowPreviewHeight,
        _colorTable[_GNW_wcolor[0]]);
    windowDrawRect(win,
        kSelectWindowPreviewX,
        kSelectWindowPreviewY,
        kSelectWindowPreviewX + kSelectWindowPreviewWidth - 1,
        kSelectWindowPreviewY + kSelectWindowPreviewHeight - 1,
        _colorTable[_GNW_wcolor[2]]);

    bool previewDrawn = false;
    HeroAppearancePreviewMount mount;
    heroAppearancePreviewMountInit(&mount);

    bool mounted = entry != nullptr && !entry->packageBacked;
    if (entry != nullptr && entry->packageBacked) {
        mounted = heroAppearancePreviewMount(&mount, entry->race, entry->style);
    }

    if (mounted) {
        int fid = heroAppearanceSelectWindowBuildPreviewFid(entry);
        if (fid != -1 && artExists(fid)) {
            unsigned char* windowBuffer = windowGetBuffer(win);
            int pitch = windowGetWidth(win);
            artRender(fid,
                windowBuffer + pitch * (kSelectWindowPreviewY + 2) + kSelectWindowPreviewX + 2,
                kSelectWindowPreviewWidth - 4,
                kSelectWindowPreviewHeight - 4,
                pitch);
            previewDrawn = true;
        }
    }
    if (entry != nullptr && entry->packageBacked) {
        heroAppearancePreviewUnmount(&mount);
    }

    if (!previewDrawn) {
        const char* text = "No preview";
        int textX = kSelectWindowPreviewX + (kSelectWindowPreviewWidth - fontGetStringWidth(text)) / 2;
        int textY = kSelectWindowPreviewY + (kSelectWindowPreviewHeight - fontGetLineHeight()) / 2;
        windowDrawText(win, text, kSelectWindowPreviewWidth - 8, textX, textY, _colorTable[_GNW_wcolor[3]]);
    }
}

void heroAppearanceSelectWindowDrawStatic(int win, bool isStyle)
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    windowFill(win, 0, 0, kSelectWindowWidth, kSelectWindowHeight, _colorTable[_GNW_wcolor[0]]);
    windowDrawBorder(win);

    const char* title = isStyle ? "Hero Style" : "Hero Race";
    int titleX = (kSelectWindowWidth - fontGetStringWidth(title)) / 2;
    windowDrawText(win, title, kSelectWindowWidth - 24, titleX, 14, _colorTable[_GNW_wcolor[4]]);

    const char* hint = "Select with Prev/Next, Done saves, Esc cancels.";
    int hintX = (kSelectWindowWidth - fontGetStringWidth(hint)) / 2;
    if (hintX < 12) {
        hintX = 12;
    }
    windowDrawText(win, hint, kSelectWindowWidth - 24, hintX, 174, _colorTable[_GNW_wcolor[3]]);

    fontSetCurrent(oldFont);
}

void heroAppearanceSelectWindowDrawValue(int win, bool isStyle, const HeroAppearanceEntry* entry, const char* status)
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    windowFill(win, 18, 36, kSelectWindowWidth - 36, 132, _colorTable[_GNW_wcolor[0]]);
    heroAppearanceSelectWindowDrawPreview(win, entry);

    char valueText[80];
    if (entry != nullptr) {
        snprintf(valueText, sizeof(valueText), "%s %02d", isStyle ? "Style" : "Race", isStyle ? entry->style : entry->race);
    } else {
        snprintf(valueText, sizeof(valueText), "No appearances");
    }

    windowDrawText(win,
        valueText,
        kSelectWindowWidth - kSelectWindowDetailsX - 24,
        kSelectWindowDetailsX,
        kSelectWindowDetailsY,
        _colorTable[_GNW_wcolor[3]]);

    if (entry != nullptr) {
        windowDrawText(win,
            entry->displayName,
            kSelectWindowWidth - kSelectWindowDetailsX - 24,
            kSelectWindowDetailsX,
            kSelectWindowDetailsY + 20,
            _colorTable[_GNW_wcolor[3]]);

        char mappingText[96];
        snprintf(mappingText, sizeof(mappingText), "%s, fallback %s", entry->mappingBasis, heroAppearanceFallbackName(entry->fallback));
        windowDrawText(win,
            mappingText,
            kSelectWindowWidth - kSelectWindowDetailsX - 24,
            kSelectWindowDetailsX,
            kSelectWindowDetailsY + 40,
            _colorTable[_GNW_wcolor[3]]);
    }

    if (status != nullptr && status[0] != '\0') {
        windowDrawText(win,
            status,
            kSelectWindowWidth - kSelectWindowDetailsX - 24,
            kSelectWindowDetailsX,
            kSelectWindowDetailsY + 64,
            _colorTable[31744]);
    }

    fontSetCurrent(oldFont);
    windowRefresh(win);
}

} // namespace

void heroAppearanceInit()
{
    if (gHeroAppearanceInitialized) {
        heroAppearanceRefreshMounts();
        return;
    }

    gHeroAppearanceInitialized = true;
    gHeroAppearanceEnabled = false;
    gHeroAppearanceRace = 0;
    gHeroAppearanceStyle = 0;

    configGetBool(&gContentConfig, CONTENT_CONFIG_APPEARANCE_SECTION, "hero_appearance", &gHeroAppearanceEnabled, false);
    if (!gHeroAppearanceEnabled) {
        gHeroAppearanceEnabled = settings.qol.enable_hero_appearance_mod;
    }

    heroAppearanceBuildRegistry();
    heroAppearanceReadGlobals();
    heroAppearanceRefreshMounts();
}

void heroAppearanceRefresh()
{
    if (!gHeroAppearanceInitialized) {
        heroAppearanceInit();
        return;
    }

    int previousRace = gHeroAppearanceRace;
    int previousStyle = gHeroAppearanceStyle;

    heroAppearanceReadGlobals();
    heroAppearanceRefreshMounts();

    if (previousRace != gHeroAppearanceRace || previousStyle != gHeroAppearanceStyle) {
        artCacheFlush();
    }
}

void heroAppearanceExit()
{
    heroAppearanceUnmountPaths();
    gHeroAppearanceInitialized = false;
    gHeroAppearanceEnabled = false;
    gHeroAppearanceRace = 0;
    gHeroAppearanceStyle = 0;
    gHeroAppearanceEntriesLength = 0;
}

void heroAppearanceRefreshDudeArt()
{
    if (gDude == nullptr) {
        return;
    }

    Rect rect;
    objectGetRect(gDude, &rect);

    int anim = FID_ANIM_TYPE(gDude->fid);
    int rotation = FID_ROTATION(gDude->fid);

    _proto_dude_update_gender();

    int fid = inventoryComputeCritterFid(gDude,
        gDude->pid,
        critterGetItem2(gDude),
        critterGetItem1(gDude),
        critterGetArmor(gDude),
        interfaceGetCurrentHand(),
        anim,
        rotation);

    Rect newRect;
    objectSetFid(gDude, fid, nullptr);
    objectGetRect(gDude, &newRect);
    // Keep the old rect dirty because a model swap can shrink the rendered bounds.
    rectUnion(&rect, &newRect, &rect);
    tileWindowRefreshRect(&rect, gDude->elevation);
}

bool heroAppearanceIsEnabled()
{
    return gHeroAppearanceEnabled;
}

bool heroAppearanceIsActive()
{
    if (!gHeroAppearanceEnabled || (gHeroAppearanceRace == 0 && gHeroAppearanceStyle == 0)) {
        return false;
    }

    const HeroAppearanceEntry* entry = heroAppearanceGetSelectedEntry();
    if (entry != nullptr && !entry->packageBacked) {
        return heroAppearanceEntryHasModelMapping(entry);
    }

    return gMountedPathsLength > 0;
}

int heroAppearanceGetRace()
{
    return gHeroAppearanceRace;
}

int heroAppearanceGetStyle()
{
    return gHeroAppearanceStyle;
}

bool heroAppearanceSetRace(int race)
{
    if (!heroAppearanceIsValidSelectorValue(race)) {
        return false;
    }

    heroAppearanceInit();

    if (!gHeroAppearanceEnabled) {
        return false;
    }

    gHeroAppearanceRace = race;
    sfall_gl_vars_store(kRaceGlobalKey, race);
    heroAppearanceRefreshMounts();
    artCacheFlush();
    return true;
}

bool heroAppearanceSetStyle(int style)
{
    if (!heroAppearanceIsValidSelectorValue(style)) {
        return false;
    }

    heroAppearanceInit();

    if (!gHeroAppearanceEnabled) {
        return false;
    }

    gHeroAppearanceStyle = style;
    sfall_gl_vars_store(kStyleGlobalKey, style);
    heroAppearanceRefreshMounts();
    artCacheFlush();
    return true;
}

bool heroAppearanceSetDudeModel(int gender, const char* model)
{
    if (gender < 0 || gender >= GENDER_COUNT || model == nullptr || model[0] == '\0') {
        return false;
    }

    return artSetDudeDefaultModel(gender, model) == 0;
}

bool heroAppearanceSelectWindow(int raceStyleFlag)
{
    heroAppearanceInit();

    if (!gHeroAppearanceEnabled) {
        return false;
    }

    bool isStyle = raceStyleFlag != 0;
    int originalRace = gHeroAppearanceRace;
    int originalStyle = gHeroAppearanceStyle;

    heroAppearanceBuildRegistry();
    heroAppearanceDiscoverPackagesForSelector(isStyle, originalRace);

    int selectedIndex = heroAppearanceSelectWindowFindEntryIndex(isStyle, originalRace, originalRace, isStyle ? originalStyle : 0);
    if (selectedIndex == -1) {
        selectedIndex = heroAppearanceSelectWindowFirstEntryIndex(isStyle, originalRace);
    }

    if (selectedIndex == -1) {
        return false;
    }

    int winX = (screenGetWidth() - kSelectWindowWidth) / 2;
    int winY = (screenGetHeight() - kSelectWindowHeight) / 2;
    int win = windowCreate(winX, winY, kSelectWindowWidth, kSelectWindowHeight, _colorTable[_GNW_wcolor[0]], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return false;
    }

    bool mouseWasHidden = cursorIsHidden();
    if (mouseWasHidden) {
        mouseShowCursor();
    }

    int oldMouseCursor = gameMouseGetCursor();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    heroAppearanceSelectWindowDrawStatic(win, isStyle);
    _win_register_text_button(win, 46, kSelectWindowButtonY, -1, -1, -1, kSelectWindowPreviousEvent, "Prev", 0);
    _win_register_text_button(win, 116, kSelectWindowButtonY, -1, -1, -1, kSelectWindowNextEvent, "Next", 0);
    _win_register_text_button(win, 222, kSelectWindowButtonY, -1, -1, -1, kSelectWindowDoneEvent, "Done", 0);
    _win_register_text_button(win, 294, kSelectWindowButtonY, -1, -1, -1, kSelectWindowCancelEvent, "Cancel", 0);

    fontSetCurrent(oldFont);

    char status[96];
    status[0] = '\0';

    heroAppearanceSelectWindowDrawValue(win, isStyle, &(gHeroAppearanceEntries[selectedIndex]), status);

    bool accepted = false;

    while (true) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        int direction = 0;

        if (keyCode == kSelectWindowPreviousEvent || keyCode == KEY_ARROW_UP || keyCode == KEY_ARROW_LEFT) {
            direction = -1;
        } else if (keyCode == kSelectWindowNextEvent || keyCode == KEY_ARROW_DOWN || keyCode == KEY_ARROW_RIGHT) {
            direction = 1;
        } else if (keyCode == kSelectWindowDoneEvent || keyCode == KEY_RETURN) {
            accepted = true;
            break;
        } else if (keyCode == kSelectWindowCancelEvent || keyCode == KEY_ESCAPE) {
            break;
        }

        if (direction != 0) {
            int candidateIndex = heroAppearanceSelectWindowNextEntryIndex(isStyle, originalRace, selectedIndex, direction);
            if (candidateIndex != -1) {
                selectedIndex = candidateIndex;
                status[0] = '\0';
            } else {
                snprintf(status, sizeof(status), "No more appearances found.");
            }

            heroAppearanceSelectWindowDrawValue(win, isStyle, &(gHeroAppearanceEntries[selectedIndex]), status);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);

    gameMouseSetCursor(oldMouseCursor);
    if (mouseWasHidden) {
        mouseHideCursor();
    }

    if (!accepted) {
        return true;
    }

    const HeroAppearanceEntry* selectedEntry = &(gHeroAppearanceEntries[selectedIndex]);

    bool changed = false;
    if (isStyle) {
        if (!heroAppearanceSetStyle(selectedEntry->style)) {
            return false;
        }
        changed = selectedEntry->style != originalStyle;
    } else {
        if (!heroAppearanceSetRace(selectedEntry->race)) {
            return false;
        }

        if (selectedEntry->race != originalRace) {
            if (!heroAppearanceSetStyle(0)) {
                return false;
            }
        }

        changed = selectedEntry->race != originalRace;
    }

    if (changed) {
        heroAppearanceRefreshDudeArt();
    }

    return true;
}

int heroAppearanceResolvePlayerFid(int fid)
{
    if (!heroAppearanceIsActive()) {
        return fid;
    }

    const HeroAppearanceEntry* entry = heroAppearanceGetSelectedEntry();
    if (entry != nullptr && !entry->packageBacked) {
        int frmId = heroAppearanceEntryResolveBaseCritterArtId(entry);
        if (frmId < 0 || frmId > 0xFFF || FID_TYPE(fid) != OBJ_TYPE_CRITTER) {
            return fid;
        }

        int resolvedFid = (fid & ~0xFFF) | frmId;
        return entry->fallback == HERO_APPEARANCE_FALLBACK_SAFE ? artResolveCritterFid(resolvedFid) : resolvedFid;
    }

    int offset = artGetHeroAppearanceCritterArtOffset();
    if (offset <= 0) {
        return fid;
    }

    int frmId = heroAppearanceGetNormalizedCritterIndex(fid);
    if (frmId < 0 || frmId >= offset) {
        return fid;
    }

    int heroFrmId = frmId + offset;
    if (heroFrmId > 0xFFF) {
        return fid;
    }

    int resolvedFid = (fid & ~0xFFF) | heroFrmId;
    return entry == nullptr || entry->fallback == HERO_APPEARANCE_FALLBACK_SAFE ? artResolveCritterFid(resolvedFid) : resolvedFid;
}

int heroAppearanceApplyCritterArtOffset(int fid)
{
    return heroAppearanceResolvePlayerFid(fid);
}

int heroAppearanceRemoveCritterArtOffset(int fid)
{
    int offset = artGetHeroAppearanceCritterArtOffset();
    if (offset <= 0 || FID_TYPE(fid) != OBJ_TYPE_CRITTER) {
        return fid;
    }

    int frmId = fid & 0xFFF;
    if (frmId < offset || frmId >= offset * 2) {
        return fid;
    }

    return (fid & ~0xFFF) | (frmId - offset);
}

} // namespace fallout
