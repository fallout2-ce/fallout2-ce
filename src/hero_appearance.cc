#include "hero_appearance.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "art.h"
#include "animation.h"
#include "color.h"
#include "config.h"
#include "content_config.h"
#include "debug.h"
#include "draw.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "interface.h"
#include "inventory.h"
#include "input.h"
#include "kb.h"
#include "memory.h"
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
constexpr int kSelectWindowPreviewWidth = 128;
constexpr int kSelectWindowPreviewHeight = 132;
constexpr int kSelectWindowDetailsX = 166;
constexpr int kSelectWindowDetailsY = 54;
constexpr int kSelectWindowButtonY = 196;
constexpr int kSelectWindowButtonHeight = 24;
constexpr int kSelectWindowButtonGap = 6;
constexpr int kSelectWindowButtonWidth = 84;
constexpr int kSelectWindowCancelButtonWidth = 92;
constexpr int kSelectWindowFirstButtonX = 9;
constexpr int kSelectWindowButtonRedX = 8;
constexpr int kSelectWindowButtonTextGap = 6;
constexpr int kSelectWindowDetailsPanelX = 158;
constexpr int kSelectWindowDetailsPanelY = 42;
constexpr int kSelectWindowDetailsPanelWidth = 198;
constexpr int kSelectWindowDetailsPanelHeight = 132;
constexpr int kLittleRedButtonUpFrmId = 8;
constexpr int kLittleRedButtonDownFrmId = 9;
constexpr int kDoneBoxFrmId = 209;
constexpr int kDoneBoxCapWidth = 12;
constexpr int kMetalTextureFrmId = 240;
constexpr int kMetalTextureX = 26;
constexpr int kMetalTextureY = 52;
constexpr int kMetalTextureWidth = 145;
constexpr int kMetalTextureHeight = 48;
constexpr int kInsetTextureFrmId = 169;
constexpr int kInsetTextureX = 190;
constexpr int kInsetTextureY = 44;
constexpr int kInsetTextureWidth = 112;
constexpr int kInsetTextureHeight = 100;

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

typedef struct HeroAppearanceSelectButton {
    int id;
    unsigned char* normal;
    unsigned char* pressed;
} HeroAppearanceSelectButton;

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

int heroAppearanceColorRgb(int red, int green, int blue)
{
    return _colorTable[(red << 10) | (green << 5) | blue];
}

int heroAppearanceColorWindowBackground()
{
    return heroAppearanceColorRgb(2, 2, 2);
}

int heroAppearanceColorPanelBackground()
{
    return heroAppearanceColorRgb(1, 2, 1);
}

int heroAppearanceColorPanelBackgroundLight()
{
    return heroAppearanceColorRgb(8, 9, 7);
}

int heroAppearanceColorFrameLight()
{
    return heroAppearanceColorRgb(11, 11, 9);
}

int heroAppearanceColorFrameDark()
{
    return heroAppearanceColorRgb(1, 1, 1);
}

int heroAppearanceColorDivider()
{
    return heroAppearanceColorRgb(4, 4, 3);
}

int heroAppearanceColorInsetShadow()
{
    return heroAppearanceColorRgb(0, 1, 0);
}

int heroAppearanceColorTextGold()
{
    return _colorTable[18979];
}

int heroAppearanceColorTextGreen()
{
    return heroAppearanceColorRgb(2, 17, 3);
}

int heroAppearanceColorTextGreenDim()
{
    return heroAppearanceColorRgb(1, 12, 2);
}

int heroAppearanceColorTextNeutral()
{
    return _colorTable[14723];
}

void heroAppearanceFillTiledFrmTexture(unsigned char* buffer, int pitch, int x, int y, int width, int height, int frmId, int sourceX, int sourceY, int sourceWidth, int sourceHeight, int fallbackColor)
{
    FrmImage texture;
    int fid = buildFid(OBJ_TYPE_INTERFACE, frmId, 0, 0, 0);
    if (!texture.lock(fid)) {
        bufferFill(buffer + pitch * y + x, width, height, pitch, fallbackColor);
        return;
    }

    const int textureX = std::min(sourceX, texture.getWidth() - 1);
    const int textureY = std::min(sourceY, texture.getHeight() - 1);
    const int textureWidth = std::max(1, std::min(sourceWidth, texture.getWidth() - textureX));
    const int textureHeight = std::max(1, std::min(sourceHeight, texture.getHeight() - textureY));
    const unsigned char* source = texture.getData();

    for (int row = 0; row < height; row++) {
        unsigned char* dest = buffer + pitch * (y + row) + x;
        const unsigned char* srcRow = source + texture.getWidth() * (textureY + (row % textureHeight)) + textureX;
        int copied = 0;
        while (copied < width) {
            const int chunk = std::min(textureWidth, width - copied);
            memcpy(dest + copied, srcRow, chunk);
            copied += chunk;
        }
    }
}

void heroAppearanceFillMetalTexture(unsigned char* buffer, int pitch, int x, int y, int width, int height)
{
    heroAppearanceFillTiledFrmTexture(buffer, pitch, x, y, width, height, kMetalTextureFrmId, kMetalTextureX, kMetalTextureY, kMetalTextureWidth, kMetalTextureHeight, heroAppearanceColorWindowBackground());
}

void heroAppearanceFillInsetTexture(unsigned char* buffer, int pitch, int x, int y, int width, int height)
{
    heroAppearanceFillTiledFrmTexture(buffer, pitch, x, y, width, height, kInsetTextureFrmId, kInsetTextureX, kInsetTextureY, kInsetTextureWidth, kInsetTextureHeight, heroAppearanceColorPanelBackground());
}

void heroAppearanceSelectWindowDrawText(int win, const char* text, int x, int y, int maxWidth, int color)
{
    unsigned char* windowBuffer = windowGetBuffer(win);
    int pitch = windowGetWidth(win);
    fontDrawText(windowBuffer + pitch * y + x, text, maxWidth, pitch, color);
}

void heroAppearanceSelectWindowDrawInsetPanel(int win, int x, int y, int width, int height)
{
    int right = x + width - 1;
    int bottom = y + height - 1;
    int light = heroAppearanceColorFrameLight();
    int dark = heroAppearanceColorFrameDark();
    int shadow = heroAppearanceColorInsetShadow();
    unsigned char* windowBuffer = windowGetBuffer(win);
    int pitch = windowGetWidth(win);

    heroAppearanceFillInsetTexture(windowBuffer, pitch, x, y, width, height);
    windowDrawRect(win, x, y, right, bottom, heroAppearanceColorWindowBackground());
    windowDrawLine(win, x + 1, y + 1, right - 1, y + 1, dark);
    windowDrawLine(win, x + 1, y + 1, x + 1, bottom - 1, dark);
    windowDrawLine(win, x + 2, y + 2, right - 2, y + 2, shadow);
    windowDrawLine(win, x + 2, y + 2, x + 2, bottom - 2, shadow);
    windowDrawLine(win, x + 1, bottom - 1, right - 1, bottom - 1, light);
    windowDrawLine(win, right - 1, y + 1, right - 1, bottom - 1, light);
}

void heroAppearanceSelectWindowDrawRaisedFrame(int win, int x, int y, int width, int height)
{
    int right = x + width - 1;
    int bottom = y + height - 1;
    int light = heroAppearanceColorFrameLight();
    int dark = heroAppearanceColorFrameDark();
    int shadow = heroAppearanceColorInsetShadow();

    windowDrawRect(win, x, y, right, bottom, shadow);
    windowDrawLine(win, x + 1, y + 1, right - 1, y + 1, light);
    windowDrawLine(win, x + 1, y + 1, x + 1, bottom - 1, light);
    windowDrawLine(win, x + 1, bottom - 1, right - 1, bottom - 1, dark);
    windowDrawLine(win, right - 1, y + 1, right - 1, bottom - 1, dark);
    windowDrawLine(win, x + 2, y + 2, right - 2, y + 2, dark);
    windowDrawLine(win, x + 2, y + 2, x + 2, bottom - 2, dark);
    windowDrawLine(win, x + 2, bottom - 2, right - 2, bottom - 2, shadow);
    windowDrawLine(win, right - 2, y + 2, right - 2, bottom - 2, shadow);
}

void heroAppearanceDrawDonePlate(unsigned char* buffer, int pitch, int x, int y, int width)
{
    FrmImage doneBox;
    int doneBoxFid = buildFid(OBJ_TYPE_INTERFACE, kDoneBoxFrmId, 0, 0, 0);
    if (!doneBox.lock(doneBoxFid)) {
        bufferFill(buffer + pitch * y + x, width, kSelectWindowButtonHeight, pitch, heroAppearanceColorPanelBackgroundLight());
        return;
    }

    const int sourceWidth = doneBox.getWidth();
    const int sourceHeight = doneBox.getHeight();
    const unsigned char* source = doneBox.getData();

    if (width >= sourceWidth) {
        blitBufferToBufferTrans(
            source,
            sourceWidth,
            sourceHeight,
            sourceWidth,
            buffer + pitch * y + x,
            pitch);
        return;
    }

    const int leftWidth = std::min(kDoneBoxCapWidth, width / 2);
    const int rightWidth = std::min(kDoneBoxCapWidth, width - leftWidth);
    const int middleWidth = width - leftWidth - rightWidth;

    blitBufferToBufferTrans(
        source,
        leftWidth,
        sourceHeight,
        sourceWidth,
        buffer + pitch * y + x,
        pitch);

    if (middleWidth > 0) {
        blitBufferToBufferTrans(
            source + leftWidth,
            middleWidth,
            sourceHeight,
            sourceWidth,
            buffer + pitch * y + x + leftWidth,
            pitch);
    }

    blitBufferToBufferTrans(
        source + sourceWidth - rightWidth,
        rightWidth,
        sourceHeight,
        sourceWidth,
        buffer + pitch * y + x + leftWidth + middleWidth,
        pitch);
}

void heroAppearanceSelectWindowDrawButtonImage(unsigned char* buffer, int width, int height, const char* title, bool pressed)
{
    bufferFill(buffer, width, height, width, heroAppearanceColorWindowBackground());
    heroAppearanceDrawDonePlate(buffer, width, 0, 0, width);

    FrmImage redButton;
    int redFid = buildFid(OBJ_TYPE_INTERFACE, pressed ? kLittleRedButtonDownFrmId : kLittleRedButtonUpFrmId, 0, 0, 0);
    if (redButton.lock(redFid)) {
        int redX = kSelectWindowButtonRedX;
        int redY = (height - redButton.getHeight()) / 2;

        blitBufferToBufferTrans(
            redButton.getData(),
            redButton.getWidth(),
            redButton.getHeight(),
            redButton.getWidth(),
            buffer + width * redY + redX,
            width);
    }

    int oldFont = fontGetCurrent();
    fontSetCurrent(103);

    int textWidth = fontGetStringWidth(title);
    int textX = kSelectWindowButtonRedX + 15 + kSelectWindowButtonTextGap;
    if (textX + textWidth + 4 > width) {
        textX = width - textWidth - 4;
    }
    if (textX < 4) {
        textX = 4;
    }

    int textY = (height - fontGetLineHeight()) / 2;

    fontDrawText(buffer + width * textY + textX,
        title,
        width - textX - 4,
        width,
        _colorTable[18979] | FONT_SHADOW);

    fontSetCurrent(oldFont);
}

void heroAppearanceSelectButtonInit(HeroAppearanceSelectButton* button)
{
    button->id = -1;
    button->normal = nullptr;
    button->pressed = nullptr;
}

void heroAppearanceSelectButtonDestroy(HeroAppearanceSelectButton* button)
{
    if (button->id != -1) {
        buttonDestroy(button->id);
        button->id = -1;
    }

    if (button->normal != nullptr) {
        internal_free(button->normal);
        button->normal = nullptr;
    }

    if (button->pressed != nullptr) {
        internal_free(button->pressed);
        button->pressed = nullptr;
    }
}

void heroAppearanceSelectWindowCreateButton(int win, int x, int y, int width, const char* title, int eventCode, HeroAppearanceSelectButton* button)
{
    heroAppearanceSelectButtonInit(button);

    int size = width * kSelectWindowButtonHeight;
    button->normal = (unsigned char*)internal_malloc(size);
    if (button->normal == nullptr) {
        return;
    }

    button->pressed = (unsigned char*)internal_malloc(size);
    if (button->pressed == nullptr) {
        internal_free(button->normal);
        button->normal = nullptr;
        return;
    }

    heroAppearanceSelectWindowDrawButtonImage(button->normal, width, kSelectWindowButtonHeight, title, false);
    heroAppearanceSelectWindowDrawButtonImage(button->pressed, width, kSelectWindowButtonHeight, title, true);

    button->id = buttonCreate(win,
        x,
        y,
        width,
        kSelectWindowButtonHeight,
        -1,
        -1,
        -1,
        eventCode,
        button->normal,
        button->pressed,
        nullptr,
        0);

    if (button->id == -1) {
        heroAppearanceSelectButtonDestroy(button);
    } else {
        buttonSetCallbacks(button->id, _gsound_red_butt_press, _gsound_red_butt_release);
    }
}

bool heroAppearanceSelectWindowDrawPreviewFid(int win, int fid)
{
    CacheEntry* handle = nullptr;
    int frameWidth = 0;
    int frameHeight = 0;
    unsigned char* frameData = artLockFrameDataReturningSize(fid, &handle, &frameWidth, &frameHeight);
    if (frameData == nullptr || frameWidth <= 0 || frameHeight <= 0) {
        if (handle != nullptr) {
            artUnlock(handle);
        }
        return false;
    }

    int maxWidth = kSelectWindowPreviewWidth - 6;
    int maxHeight = kSelectWindowPreviewHeight - 6;
    int targetWidth = frameWidth * 7 / 4;
    int targetHeight = frameHeight * 7 / 4;

    if (targetWidth <= 0 || targetHeight <= 0) {
        targetWidth = frameWidth;
        targetHeight = frameHeight;
    }

    if (targetWidth > maxWidth || targetHeight > maxHeight) {
        if (targetWidth * maxHeight > targetHeight * maxWidth) {
            targetHeight = targetHeight * maxWidth / targetWidth;
            targetWidth = maxWidth;
        } else {
            targetWidth = targetWidth * maxHeight / targetHeight;
            targetHeight = maxHeight;
        }
    }

    if (targetWidth < 1) {
        targetWidth = 1;
    }

    if (targetHeight < 1) {
        targetHeight = 1;
    }

    int dstX = kSelectWindowPreviewX + (kSelectWindowPreviewWidth - targetWidth) / 2;
    int dstY = kSelectWindowPreviewY + (kSelectWindowPreviewHeight - targetHeight) / 2;

    unsigned char* windowBuffer = windowGetBuffer(win);
    blitBuffer2DScaledTrans(ConstBuffer2D(frameData, frameWidth, frameHeight),
        Buffer2D(windowBuffer, windowGetWidth(win), windowGetHeight(win)),
        dstX,
        dstY,
        targetWidth,
        targetHeight);

    artUnlock(handle);
    return true;
}

void heroAppearanceCopyFittedText(char* dest, size_t size, const char* source, int maxWidth)
{
    if (dest == nullptr || size == 0) {
        return;
    }

    snprintf(dest, size, "%s", source != nullptr ? source : "");
    if (fontGetStringWidth(dest) <= maxWidth) {
        return;
    }

    const char suffix[] = "...";
    size_t length = strlen(dest);
    while (length > 0) {
        length--;
        dest[length] = '\0';

        if (length + strlen(suffix) >= size) {
            continue;
        }

        char candidate[128];
        snprintf(candidate, sizeof(candidate), "%s%s", dest, suffix);
        if (fontGetStringWidth(candidate) <= maxWidth) {
            snprintf(dest, size, "%s", candidate);
            return;
        }
    }

    snprintf(dest, size, "%s", suffix);
}

void heroAppearanceFormatSelectorLabel(char* dest, size_t size, const HeroAppearanceEntry* entry, bool isStyle)
{
    if (entry == nullptr) {
        snprintf(dest, size, "No selectable appearance");
        return;
    }

    if (entry->packageBacked) {
        snprintf(dest, size, "Package #%02d", isStyle ? entry->style : entry->race);
        return;
    }

    snprintf(dest, size, "%s", entry->displayName);
}

void heroAppearanceFormatPackagePath(char* dest, size_t size, const HeroAppearanceEntry* entry)
{
    if (entry == nullptr || !entry->packageBacked || (entry->race == 0 && entry->style == 0)) {
        snprintf(dest, size, "Package native");
        return;
    }

    char genderCode = '?';
    heroAppearanceGetDudeGenderCode(&genderCode);
    snprintf(dest, size, "appearance\\h%cr%02ds%02d.dat", genderCode, entry->race, entry->style);
}

void heroAppearanceSelectWindowDrawPreview(int win, const HeroAppearanceEntry* entry)
{
    heroAppearanceSelectWindowDrawInsetPanel(win,
        kSelectWindowPreviewX,
        kSelectWindowPreviewY,
        kSelectWindowPreviewWidth,
        kSelectWindowPreviewHeight);

    bool previewDrawn = false;
    HeroAppearancePreviewMount mount;
    heroAppearancePreviewMountInit(&mount);

    bool mounted = entry != nullptr && !entry->packageBacked;
    if (entry != nullptr && entry->packageBacked) {
        mounted = heroAppearancePreviewMount(&mount, entry->race, entry->style);
    }

    if (mounted) {
        int fid = heroAppearanceSelectWindowBuildPreviewFid(entry);
        if (fid != -1) {
            previewDrawn = heroAppearanceSelectWindowDrawPreviewFid(win, fid);
        }
    }
    if (entry != nullptr && entry->packageBacked) {
        heroAppearancePreviewUnmount(&mount);
    }

    if (!previewDrawn) {
        const char* text = "NO PREVIEW";
        int textX = kSelectWindowPreviewX + (kSelectWindowPreviewWidth - fontGetStringWidth(text)) / 2;
        int textY = kSelectWindowPreviewY + (kSelectWindowPreviewHeight - fontGetLineHeight()) / 2;
        heroAppearanceSelectWindowDrawText(win,
            text,
            textX,
            textY,
            kSelectWindowPreviewWidth - 8,
            heroAppearanceColorTextGreen());
    }
}

void heroAppearanceSelectWindowDrawStatic(int win, bool isStyle)
{
    int oldFont = fontGetCurrent();
    unsigned char* windowBuffer = windowGetBuffer(win);
    int pitch = windowGetWidth(win);

    heroAppearanceFillMetalTexture(windowBuffer, pitch, 0, 0, kSelectWindowWidth, kSelectWindowHeight);
    heroAppearanceSelectWindowDrawRaisedFrame(win, 4, 4, kSelectWindowWidth - 8, kSelectWindowHeight - 8);

    fontSetCurrent(103);

    const char* title = isStyle ? "HERO STYLE" : "HERO RACE";
    int titleX = (kSelectWindowWidth - fontGetStringWidth(title)) / 2;
    heroAppearanceSelectWindowDrawText(win,
        title,
        titleX,
        14,
        kSelectWindowWidth - titleX - 12,
        _colorTable[18979] | FONT_SHADOW);

    windowDrawLine(win, 18, 35, kSelectWindowWidth - 19, 35, heroAppearanceColorFrameDark());
    windowDrawLine(win, 18, 36, kSelectWindowWidth - 19, 36, heroAppearanceColorInsetShadow());

    heroAppearanceSelectWindowDrawInsetPanel(win,
        kSelectWindowPreviewX,
        kSelectWindowPreviewY,
        kSelectWindowPreviewWidth,
        kSelectWindowPreviewHeight);
    heroAppearanceSelectWindowDrawInsetPanel(win,
        kSelectWindowDetailsPanelX,
        kSelectWindowDetailsPanelY,
        kSelectWindowDetailsPanelWidth,
        kSelectWindowDetailsPanelHeight);

    windowDrawLine(win, 18, 184, kSelectWindowWidth - 19, 184, heroAppearanceColorFrameDark());
    windowDrawLine(win, 18, 185, kSelectWindowWidth - 19, 185, heroAppearanceColorInsetShadow());

    fontSetCurrent(oldFont);
}

void heroAppearanceSelectWindowDrawValue(int win, bool isStyle, const HeroAppearanceEntry* entry, const char* status, bool debugInfo)
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    heroAppearanceSelectWindowDrawPreview(win, entry);
    heroAppearanceSelectWindowDrawInsetPanel(win,
        kSelectWindowDetailsPanelX,
        kSelectWindowDetailsPanelY,
        kSelectWindowDetailsPanelWidth,
        kSelectWindowDetailsPanelHeight);

    const int detailsTextWidth = kSelectWindowWidth - kSelectWindowDetailsX - 24;

    char valueText[80];
    char fittedText[80];
    heroAppearanceFormatSelectorLabel(valueText, sizeof(valueText), entry, isStyle);
    heroAppearanceCopyFittedText(fittedText, sizeof(fittedText), valueText, detailsTextWidth);

    heroAppearanceSelectWindowDrawText(win,
        fittedText,
        kSelectWindowDetailsX,
        kSelectWindowDetailsY,
        detailsTextWidth,
        heroAppearanceColorTextGold() | FONT_SHADOW);

    windowDrawLine(win,
        kSelectWindowDetailsX,
        kSelectWindowDetailsY + 16,
        kSelectWindowDetailsX + 166,
        kSelectWindowDetailsY + 16,
        heroAppearanceColorFrameDark());

    char lineText[128];
    if (entry != nullptr) {
        if (debugInfo) {
            snprintf(lineText, sizeof(lineText), "Raw race %02d  style %02d", entry->race, entry->style);
        } else {
            snprintf(lineText, sizeof(lineText), "Race %d", entry->race);
        }
    } else {
        snprintf(lineText, sizeof(lineText), "%s selector", isStyle ? "Style" : "Race");
    }
    heroAppearanceCopyFittedText(fittedText, sizeof(fittedText), lineText, detailsTextWidth);
    heroAppearanceSelectWindowDrawText(win,
        fittedText,
        kSelectWindowDetailsX,
        kSelectWindowDetailsY + 22,
        detailsTextWidth,
        debugInfo ? heroAppearanceColorTextGreenDim() : heroAppearanceColorTextGold());

    if (entry != nullptr) {
        if (debugInfo) {
            const char* model = heroAppearanceEntryGetModelForDude(entry);
            snprintf(lineText, sizeof(lineText), "Model %s", model != nullptr && model[0] != '\0' ? model : (entry->packageBacked ? "package" : "native"));
            heroAppearanceCopyFittedText(fittedText, sizeof(fittedText), lineText, detailsTextWidth);
            heroAppearanceSelectWindowDrawText(win,
                fittedText,
                kSelectWindowDetailsX,
                kSelectWindowDetailsY + 40,
                detailsTextWidth,
                heroAppearanceColorTextGreenDim());

            heroAppearanceFormatPackagePath(lineText, sizeof(lineText), entry);
            heroAppearanceCopyFittedText(fittedText, sizeof(fittedText), lineText, detailsTextWidth);
            heroAppearanceSelectWindowDrawText(win,
                fittedText,
                kSelectWindowDetailsX,
                kSelectWindowDetailsY + 58,
                detailsTextWidth,
                heroAppearanceColorTextGreenDim());

            snprintf(lineText, sizeof(lineText), "Fallback %s", heroAppearanceFallbackName(entry->fallback));
            heroAppearanceCopyFittedText(fittedText, sizeof(fittedText), lineText, detailsTextWidth);
            heroAppearanceSelectWindowDrawText(win,
                fittedText,
                kSelectWindowDetailsX,
                kSelectWindowDetailsY + 76,
                detailsTextWidth,
                heroAppearanceColorTextNeutral());
        } else {
            snprintf(lineText, sizeof(lineText), "Style %d", entry->style);
            heroAppearanceCopyFittedText(fittedText, sizeof(fittedText), lineText, detailsTextWidth);
            heroAppearanceSelectWindowDrawText(win,
                fittedText,
                kSelectWindowDetailsX,
                kSelectWindowDetailsY + 42,
                detailsTextWidth,
                heroAppearanceColorTextGold());
        }
    }

    if (status != nullptr && status[0] != '\0') {
        heroAppearanceCopyFittedText(fittedText, sizeof(fittedText), status, detailsTextWidth);
        heroAppearanceSelectWindowDrawText(win,
            fittedText,
            kSelectWindowDetailsX,
            kSelectWindowDetailsY + 64,
            detailsTextWidth,
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

static bool heroAppearanceSelectWindowInternal(int raceStyleFlag, bool* acceptedPtr, bool refreshDudeArt)
{
    heroAppearanceInit();

    if (acceptedPtr != nullptr) {
        *acceptedPtr = false;
    }

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
    int win = windowCreate(winX, winY, kSelectWindowWidth, kSelectWindowHeight, heroAppearanceColorWindowBackground(), WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return false;
    }

    bool mouseWasHidden = cursorIsHidden();
    if (mouseWasHidden) {
        mouseShowCursor();
    }

    int oldMouseCursor = gameMouseGetCursor();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    heroAppearanceSelectWindowDrawStatic(win, isStyle);

    HeroAppearanceSelectButton buttons[4];
    int buttonX = kSelectWindowFirstButtonX;
    heroAppearanceSelectWindowCreateButton(win, buttonX, kSelectWindowButtonY, kSelectWindowButtonWidth, "PREV", kSelectWindowPreviousEvent, &(buttons[0]));
    buttonX += kSelectWindowButtonWidth + kSelectWindowButtonGap;
    heroAppearanceSelectWindowCreateButton(win, buttonX, kSelectWindowButtonY, kSelectWindowButtonWidth, "NEXT", kSelectWindowNextEvent, &(buttons[1]));
    buttonX += kSelectWindowButtonWidth + kSelectWindowButtonGap;
    heroAppearanceSelectWindowCreateButton(win, buttonX, kSelectWindowButtonY, kSelectWindowButtonWidth, "DONE", kSelectWindowDoneEvent, &(buttons[2]));
    buttonX += kSelectWindowButtonWidth + kSelectWindowButtonGap;
    const char* cancelButtonTitle = isStyle && !refreshDudeArt ? "BACK" : "CANCEL";
    heroAppearanceSelectWindowCreateButton(win, buttonX, kSelectWindowButtonY, kSelectWindowCancelButtonWidth, cancelButtonTitle, kSelectWindowCancelEvent, &(buttons[3]));

    char status[96];
    status[0] = '\0';

    bool debugInfo = false;
    heroAppearanceSelectWindowDrawValue(win, isStyle, &(gHeroAppearanceEntries[selectedIndex]), status, debugInfo);

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
        } else if (keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
            debugInfo = !debugInfo;
            heroAppearanceSelectWindowDrawValue(win, isStyle, &(gHeroAppearanceEntries[selectedIndex]), status, debugInfo);
        }

        if (direction != 0) {
            int candidateIndex = heroAppearanceSelectWindowNextEntryIndex(isStyle, originalRace, selectedIndex, direction);
            if (candidateIndex != -1) {
                selectedIndex = candidateIndex;
                status[0] = '\0';
            } else {
                snprintf(status, sizeof(status), "No more appearances found.");
            }

            heroAppearanceSelectWindowDrawValue(win, isStyle, &(gHeroAppearanceEntries[selectedIndex]), status, debugInfo);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    for (int index = 0; index < 4; index++) {
        heroAppearanceSelectButtonDestroy(&(buttons[index]));
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

        if (selectedEntry->style != originalStyle || selectedEntry->race != originalRace) {
            if (!heroAppearanceSetStyle(selectedEntry->style)) {
                return false;
            }
        }

        changed = selectedEntry->race != originalRace || selectedEntry->style != originalStyle;
    }

    if (changed && refreshDudeArt) {
        heroAppearanceRefreshDudeArt();
    }

    if (acceptedPtr != nullptr) {
        *acceptedPtr = true;
    }

    return true;
}

bool heroAppearanceSelectWindow(int raceStyleFlag)
{
    return heroAppearanceSelectWindowInternal(raceStyleFlag, nullptr, true);
}

bool heroAppearanceSelectWindowForCharacterCreation(int raceStyleFlag, bool* acceptedPtr)
{
    return heroAppearanceSelectWindowInternal(raceStyleFlag, acceptedPtr, false);
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
