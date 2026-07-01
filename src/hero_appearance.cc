#include "hero_appearance.h"

#include <stdio.h>
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

typedef struct HeroAppearancePreviewMount {
    char paths[2][COMPAT_MAX_PATH];
    int pathsLength;
} HeroAppearancePreviewMount;

bool heroAppearanceIsValidSelectorValue(int value)
{
    return value >= 0 && value <= kMaxSelectorValue;
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

    bool mounted = false;
    char genderCode;
    if (heroAppearanceGetDudeGenderCode(&genderCode)) {
        mounted = heroAppearanceMountOne(genderCode);
    } else {
        bool mountedMale = heroAppearanceMountOne('m');
        bool mountedFemale = heroAppearanceMountOne('f');
        mounted = mountedMale || mountedFemale;
    }

    if (!mounted && (gHeroAppearanceRace != 0 || gHeroAppearanceStyle != 0)) {
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

int heroAppearanceSelectWindowBuildPreviewFid(int race, int style)
{
    if (gDude == nullptr) {
        return -1;
    }

    int baseFid = heroAppearanceRemoveCritterArtOffset(gDude->fid);
    int frmId = heroAppearanceGetNormalizedCritterIndex(baseFid);
    if (frmId < 0) {
        return -1;
    }

    if (race != 0 || style != 0) {
        int offset = artGetHeroAppearanceCritterArtOffset();
        if (offset <= 0) {
            return -1;
        }

        frmId += offset;
        if (frmId > 0xFFF) {
            return -1;
        }
    }

    return buildFid(OBJ_TYPE_CRITTER, frmId, ANIM_STAND, 0, ROTATION_SW);
}

void heroAppearanceSelectWindowDrawPreview(int win, int race, int style)
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
    if (heroAppearancePreviewMount(&mount, race, style)) {
        int fid = heroAppearanceSelectWindowBuildPreviewFid(race, style);
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
    heroAppearancePreviewUnmount(&mount);

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

void heroAppearanceSelectWindowDrawValue(int win, bool isStyle, int race, int style, const char* status)
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    windowFill(win, 18, 36, kSelectWindowWidth - 36, 132, _colorTable[_GNW_wcolor[0]]);
    heroAppearanceSelectWindowDrawPreview(win, race, style);

    char valueText[80];
    snprintf(valueText, sizeof(valueText), "%s %02d", isStyle ? "Style" : "Race", isStyle ? style : race);
    windowDrawText(win,
        valueText,
        kSelectWindowWidth - kSelectWindowDetailsX - 24,
        kSelectWindowDetailsX,
        kSelectWindowDetailsY,
        _colorTable[_GNW_wcolor[3]]);

    char packageText[80];
    snprintf(packageText, sizeof(packageText), "Package r%02d s%02d", race, style);
    windowDrawText(win,
        packageText,
        kSelectWindowWidth - kSelectWindowDetailsX - 24,
        kSelectWindowDetailsX,
        kSelectWindowDetailsY + 20,
        _colorTable[_GNW_wcolor[3]]);

    if (status != nullptr && status[0] != '\0') {
        windowDrawText(win,
            status,
            kSelectWindowWidth - kSelectWindowDetailsX - 24,
            kSelectWindowDetailsX,
            kSelectWindowDetailsY + 46,
            _colorTable[31744]);
    }

    fontSetCurrent(oldFont);
    windowRefresh(win);
}

bool heroAppearanceSelectWindowCanUseValue(bool isStyle, int fixedRace, int value)
{
    if (!heroAppearanceIsValidSelectorValue(value)) {
        return false;
    }

    int race = isStyle ? fixedRace : value;
    int style = isStyle ? value : 0;
    return heroAppearancePackageAvailableForDude(race, style);
}

void heroAppearanceSelectWindowDisplayValues(bool isStyle, int fixedRace, int value, int* race, int* style)
{
    if (isStyle) {
        *race = fixedRace;
        *style = value;
    } else {
        *race = value;
        *style = 0;
    }
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
    return gHeroAppearanceEnabled && (gHeroAppearanceRace != 0 || gHeroAppearanceStyle != 0) && gMountedPathsLength > 0;
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

bool heroAppearanceSelectWindow(int raceStyleFlag)
{
    heroAppearanceInit();

    if (!gHeroAppearanceEnabled) {
        return false;
    }

    bool isStyle = raceStyleFlag != 0;
    int originalRace = gHeroAppearanceRace;
    int originalStyle = gHeroAppearanceStyle;
    int selectedValue = isStyle ? originalStyle : originalRace;
    if (!heroAppearanceSelectWindowCanUseValue(isStyle, originalRace, selectedValue)) {
        selectedValue = 0;
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

    int displayRace;
    int displayStyle;
    heroAppearanceSelectWindowDisplayValues(isStyle, originalRace, selectedValue, &displayRace, &displayStyle);
    heroAppearanceSelectWindowDrawValue(win, isStyle, displayRace, displayStyle, status);

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
            int candidateValue = selectedValue + direction;
            while (heroAppearanceIsValidSelectorValue(candidateValue)
                && !heroAppearanceSelectWindowCanUseValue(isStyle, originalRace, candidateValue)) {
                candidateValue += direction;
            }

            if (heroAppearanceIsValidSelectorValue(candidateValue)) {
                selectedValue = candidateValue;
                status[0] = '\0';
            } else {
                snprintf(status, sizeof(status), "No more packages found.");
            }

            heroAppearanceSelectWindowDisplayValues(isStyle, originalRace, selectedValue, &displayRace, &displayStyle);
            heroAppearanceSelectWindowDrawValue(win, isStyle, displayRace, displayStyle, status);
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

    bool changed = false;
    if (isStyle) {
        if (!heroAppearanceSetStyle(selectedValue)) {
            return false;
        }
        changed = selectedValue != originalStyle;
    } else {
        if (!heroAppearanceSetRace(selectedValue)) {
            return false;
        }

        if (selectedValue != originalRace) {
            if (!heroAppearanceSetStyle(0)) {
                return false;
            }
        }

        changed = selectedValue != originalRace;
    }

    if (changed) {
        heroAppearanceRefreshDudeArt();
    }

    return true;
}

int heroAppearanceApplyCritterArtOffset(int fid)
{
    if (!heroAppearanceIsActive()) {
        return fid;
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

    return (fid & ~0xFFF) | heroFrmId;
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
