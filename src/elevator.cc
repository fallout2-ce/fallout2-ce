#include "elevator.h"

#include <ctype.h>
#include <string.h>

#include <algorithm>

#include "art.h"
#include "cycle.h"
#include "debug.h"
#include "delay.h"
#include "draw.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "input.h"
#include "interface.h"
#include "kb.h"
#include "map.h"
#include "pipboy.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_config.h"
#include "svga.h"
#include "touch.h"
#include "window_manager.h"
#include "worldmap.h"

namespace fallout {

// The maximum number of elevator levels.
#define ELEVATOR_LEVEL_MAX (4)

// Max number of elevators that can be loaded from elevators.ini. This limit is
// emposed by Sfall.
#define ELEVATORS_MAX 50

typedef enum ElevatorFrm {
    ELEVATOR_FRM_BUTTON_DOWN,
    ELEVATOR_FRM_BUTTON_UP,
    ELEVATOR_FRM_GAUGE,
    ELEVATOR_FRM_COUNT,
} ElevatorFrm;

typedef struct ElevatorBackground {
    int backgroundFrmId;
    int panelFrmId;
} ElevatorBackground;

typedef struct ElevatorDescription {
    Map map;
    int elevation;
    int tile;
} ElevatorDescription;

static int elevatorWindowInit(int elevator);
static void elevatorWindowFree();
static int elevatorGetLevelFromKeyCode(int elevator, int keyCode);
static int elevatorGetLevelFromEscKey(int elevator, int map);

// 0x43E950 grph_id_2
static const int gElevatorFrmIds[ELEVATOR_FRM_COUNT] = {
    141, // ebut_in.frm - map elevator screen
    142, // ebut_out.frm - map elevator screen
    149, // gaj000.frm - map elevator screen
};

// 0x43E95C intotal
static ElevatorBackground gElevatorBackgrounds[ELEVATORS_MAX] = {
    { 143, -1 },
    { 143, 150 },
    { 144, -1 },
    { 144, 145 },
    { 146, -1 },
    { 146, 147 },
    { 146, -1 },
    { 146, 151 },
    { 148, -1 },
    { 146, -1 },
    { 146, -1 },
    { 146, 147 },
    { 388, -1 },
    { 143, 150 },
    { 148, -1 },
    { 148, -1 },
    { 148, -1 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
};

// Number of levels for eleveators.
//
// 0x43EA1C btncnt
static int gElevatorLevels[ELEVATORS_MAX] = {
    4,
    2,
    3,
    2,
    3,
    2,
    3,
    3,
    3,
    3,
    3,
    2,
    4,
    2,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

// 0x43EA7C retvals
static ElevatorDescription gElevatorDescriptions[ELEVATORS_MAX][ELEVATOR_LEVEL_MAX] = {
    {
        { MAP_KLAMATH_GRAZE, 0, 18940 },
        { MAP_KLAMATH_GRAZE, 1, 18936 },
        { MAP_VAULTCITY_COURTYARD, 0, 21340 },
        { MAP_VAULTCITY_COURTYARD, 1, 21340 },
    },
    {
        { MAP_KLAMATH_TRAPCAVES, 0, 20502 },
        { MAP_KLAMATH_GRAZE, 0, 14912 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_GECKO_JUNKYARD, 0, 12498 },
        { MAP_GECKO_JUNKYARD, 1, 20094 },
        { MAP_GECKO_ACCESS_TUNNELS, 0, 17312 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_GECKO_ACCESS_TUNNELS, 0, 16140 },
        { MAP_GECKO_ACCESS_TUNNELS, 1, 16140 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_MILITARY_BASE_12, 0, 14920 },
        { MAP_MILITARY_BASE_12, 1, 15120 },
        { MAP_MILITARY_BASE_34, 0, 12944 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_MILITARY_BASE_34, 0, 24520 },
        { MAP_MILITARY_BASE_34, 1, 25316 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_NCR_DOWNTOWN, 0, 22526 },
        { MAP_NCR_DOWNTOWN, 1, 22526 },
        { MAP_NCR_DOWNTOWN, 2, 22526 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_NCR_DOWNTOWN, 2, 14086 },
        { MAP_NCR_COUNCIL_1, 0, 14086 },
        { MAP_NCR_COUNCIL_1, 2, 14086 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_VAULT_13, 0, 14104 },
        { MAP_VAULT_13, 1, 22504 },
        { MAP_VAULT_13, 2, 17312 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_KLAMATH_1, 0, 13704 },
        { MAP_KLAMATH_1, 1, 23302 },
        { MAP_KLAMATH_1, 2, 17308 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_SIERRA_123, 0, 19300 },
        { MAP_SIERRA_123, 1, 19300 },
        { MAP_SIERRA_123, 2, 20110 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_SIERRA_123, 2, 20118 },
        { MAP_SIERRA_4, 0, 21710 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_SIERRA_123, 0, 20122 },
        { MAP_SIERRA_123, 1, 20124 },
        { MAP_SIERRA_123, 2, 20940 },
        { MAP_SIERRA_4, 0, 22540 },
    },
    {
        { MAP_KLAMATH_TOXICCAVES, 1, 16052 },
        { MAP_KLAMATH_TOXICCAVES, 2, 14480 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_DEN_ENTRANCE, 0, 14104 },
        { MAP_DEN_ENTRANCE, 1, 22504 },
        { MAP_DEN_ENTRANCE, 2, 17312 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_VAULT_CITY_VAULT, 0, 14104 },
        { MAP_VAULT_CITY_VAULT, 1, 22504 },
        { MAP_VAULT_CITY_VAULT, 2, 17312 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_VAULT_15, 0, 13704 },
        { MAP_VAULT_15, 1, 23302 },
        { MAP_VAULT_15, 2, 17308 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_VAULT_15_EAST_ENTRANCE, 0, 17285 },
        { MAP_VAULT_15, 0, 19472 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_NAVARRO_ENTRANCE, 2, 10701 },
        { MAP_NAVARRO_ENTRANCE, 1, 10705 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_NAVARRO_ENTRANCE, 2, 14697 },
        { MAP_NAVARRO_ENTRANCE, 1, 15099 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_NAVARRO_ENTRANCE, 2, 23877 },
        { MAP_NAVARRO_ENTRANCE, 1, 25481 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_NAVARRO_ENTRANCE, 2, 26130 },
        { MAP_NAVARRO_ENTRANCE, 1, 24721 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_SAN_FRAN_CHINATOWN, 0, 23953 },
        { MAP_SHI_TEMPLE, 1, 16526 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
    {
        { MAP_REDDING_WANAMINGO_ENT, 0, 13901 },
        { MAP_REDDING_WANAMINGO_12, 1, 17923 },
        { MAP_RND_DESERT_1, 0, -1 },
        { MAP_RND_DESERT_1, 0, -1 },
    },
};

// NOTE: These values are also used as key bindings.
//
// 0x43EEFC keytable
static char gElevatorLevelLabels[ELEVATORS_MAX][ELEVATOR_LEVEL_MAX] = {
    { '1', '2', '3', '4' },
    { 'G', '1', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '6', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '4' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
};

// 0x51862C sfxtable
static const char* gElevatorSoundEffects[ELEVATOR_LEVEL_MAX - 1][ELEVATOR_LEVEL_MAX] = {
    {
        "ELV1_1",
        "ELV1_1",
        "ERROR",
        "ERROR",
    },
    {
        "ELV1_2",
        "ELV1_2",
        "ELV1_1",
        "ERROR",
    },
    {
        "ELV1_3",
        "ELV1_3",
        "ELV2_3",
        "ELV1_1",
    },
};

// 0x570A54 elev_win
static int gElevatorWindow;

// 0x570A6C win_buf_2
static unsigned char* gElevatorWindowBuffer;

// 0x570A70 bk_enable_2
static bool gElevatorWindowIsoWasEnabled;

static FrmImage _elevatorFrmImages[ELEVATOR_FRM_COUNT];
static FrmImage _elevatorBackgroundFrmImage;
static FrmImage _elevatorPanelFrmImage;

// Presents elevator dialog for player to pick a desired level.
//
// 0x43EF5C elevator_select
int elevatorSelectLevel(int elevator, Map* mapPtr, int* elevationPtr, int* tilePtr)
{
    if (elevator < 0 || elevator >= ELEVATORS_MAX) {
        return -1;
    }

    // SFALL
    if (elevatorWindowInit(elevator) == -1) {
        return -1;
    }

    const ElevatorDescription* elevatorDescription = gElevatorDescriptions[elevator];
    touch_set_touchscreen_mode(true);

    int index;
    for (index = 0; index < ELEVATOR_LEVEL_MAX; index++) {
        if (elevatorDescription[index].map == *mapPtr) {
            break;
        }
    }

    if (index < ELEVATOR_LEVEL_MAX) {
        int adjustedIndex = *elevationPtr + index;
        debugPrint("\nElevator select: type=%d map=%d inputLevel=%d matchedIndex=%d adjustedIndex=%d",
            elevator,
            *mapPtr,
            *elevationPtr,
            index,
            adjustedIndex);
        if (adjustedIndex >= 0 && adjustedIndex < ELEVATOR_LEVEL_MAX && elevatorDescription[adjustedIndex].tile != -1) {
            *elevationPtr = adjustedIndex;
        }
    } else {
        debugPrint("\nElevator select: type=%d map=%d inputLevel=%d matchedIndex=none",
            elevator,
            *mapPtr,
            *elevationPtr);
    }

    if (elevator == ELEVATOR_SIERRA_2) {
        if (*elevationPtr <= 2) {
            *elevationPtr -= 2;
        } else {
            *elevationPtr -= 3;
        }
    } else if (elevator == ELEVATOR_MILITARY_BASE_LOWER) {
        if (*elevationPtr >= 2) {
            *elevationPtr -= 2;
        }
    } else if (elevator == ELEVATOR_MILITARY_BASE_UPPER && *elevationPtr == 4) {
        *elevationPtr -= 2;
    }

    if (*elevationPtr > 3) {
        *elevationPtr -= 3;
    }

    int clampedElevation = std::clamp(*elevationPtr, 0, gElevatorLevels[elevator] - 1);
    if (clampedElevation != *elevationPtr) {
        debugPrint("\nElevator select: clamping start level type=%d level=%d clamped=%d buttonCount=%d",
            elevator,
            *elevationPtr,
            clampedElevation,
            gElevatorLevels[elevator]);
        *elevationPtr = clampedElevation;
    }

    debugPrint("\n the start elev level %d\n", *elevationPtr);

    int gaugeSliceSize = (_elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth() * _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getHeight()) / 13;
    float gaugeUnitsPerLevel = 12.0f / (float)(gElevatorLevels[elevator] - 1);
    blitBufferToBuffer(
        _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getData() + gaugeSliceSize * (int)((float)(*elevationPtr) * gaugeUnitsPerLevel),
        _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth(),
        _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getHeight() / 13,
        _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth(),
        gElevatorWindowBuffer + _elevatorBackgroundFrmImage.getWidth() * 41 + 121,
        _elevatorBackgroundFrmImage.getWidth());
    windowRefresh(gElevatorWindow);

    bool done = false;
    bool skipGauge = false;
    int keyCode;
    while (!done) {
        sharedFpsLimiter.mark();

        keyCode = inputGetInput();
        if (keyCode == KEY_ESCAPE) {
            keyCode = elevatorGetLevelFromEscKey(elevator, *mapPtr);
            skipGauge = true;
            done = true;
        }

        if (keyCode >= 500 && keyCode < 500 + gElevatorLevels[elevator]) {
            done = true;
        }

        if (keyCode > 0 && keyCode < 500) {
            int level = elevatorGetLevelFromKeyCode(elevator, keyCode);
            if (level != 0) {
                keyCode = 500 + level - 1;
                done = true;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (!skipGauge) {
        keyCode -= 500;
        debugPrint("\nElevator select result: type=%d keyCode=%d skipGauge=%d startLevel=%d",
            elevator,
            keyCode,
            static_cast<int>(skipGauge),
            *elevationPtr);

        if (*elevationPtr != keyCode) {
            float levelStep = (float)(gElevatorLevels[elevator] - 1) / 12.0f;

            const int delay = std::max(static_cast<int>(levelStep * 276.92307f / settings.ui.anim_speed), 1);

            if (keyCode < *elevationPtr) {
                levelStep = -levelStep;
            }

            int numberOfLevelsTravelled = keyCode - *elevationPtr;
            if (numberOfLevelsTravelled < 0) {
                numberOfLevelsTravelled = -numberOfLevelsTravelled;
            }

            debugPrint("\nElevator travel: type=%d from=%d to=%d levels=%d buttonCount=%d",
                elevator,
                *elevationPtr,
                keyCode,
                numberOfLevelsTravelled,
                gElevatorLevels[elevator]);

            soundPlayFile(gElevatorSoundEffects[gElevatorLevels[elevator] - 2][numberOfLevelsTravelled]);

            float targetGaugePosition = (float)keyCode * gaugeUnitsPerLevel;
            float currentGaugePosition = (float)(*elevationPtr) * gaugeUnitsPerLevel;
            do {
                sharedFpsLimiter.mark();

                unsigned int tick = getTicks();
                currentGaugePosition += levelStep;
                blitBufferToBuffer(
                    _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getData() + gaugeSliceSize * (int)currentGaugePosition,
                    _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth(),
                    _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getHeight() / 13,
                    _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth(),
                    gElevatorWindowBuffer + _elevatorBackgroundFrmImage.getWidth() * 41 + 121,
                    _elevatorBackgroundFrmImage.getWidth());

                windowRefresh(gElevatorWindow);

                _GNW95_process_message();
                delay_ms(delay - (getTicks() - tick));
                _GNW95_process_message();

                renderPresent();
                sharedFpsLimiter.throttle();
            } while ((levelStep <= 0.0 || currentGaugePosition < targetGaugePosition) && (levelStep > 0.0 || currentGaugePosition > targetGaugePosition));

            inputPauseForTocks(200);
        }
    }

    elevatorWindowFree();
    touch_set_touchscreen_mode(false);

    if (keyCode >= 0 && keyCode < ELEVATOR_LEVEL_MAX) {
        const ElevatorDescription* description = &(elevatorDescription[keyCode]);
        *mapPtr = description->map;
        *elevationPtr = description->elevation;
        *tilePtr = description->tile;
        debugPrint("\nElevator destination: type=%d index=%d map=%d elevation=%d tile=%d",
            elevator,
            keyCode,
            *mapPtr,
            *elevationPtr,
            *tilePtr);
    } else {
        debugPrint("\nElevator destination: type=%d index=%d canceled=1",
            elevator,
            keyCode);
    }

    return 0;
}

// 0x43F324 elevator_start
static int elevatorWindowInit(int elevator)
{
    gElevatorWindowIsoWasEnabled = isoDisable();
    colorCycleDisable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    gameMouseObjectsHide();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    scriptsDisable();

    int index;
    for (index = 0; index < ELEVATOR_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gElevatorFrmIds[index]);
        if (!_elevatorFrmImages[index].lock(fid)) {
            break;
        }
    }

    if (index != ELEVATOR_FRM_COUNT) {
        for (int reversedIndex = index - 1; reversedIndex >= 0; reversedIndex--) {
            _elevatorFrmImages[reversedIndex].unlock();
        }

        if (gElevatorWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    const ElevatorBackground* elevatorBackground = &(gElevatorBackgrounds[elevator]);
    bool backgroundsLoaded = true;

    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, elevatorBackground->backgroundFrmId);
    if (_elevatorBackgroundFrmImage.lock(backgroundFid)) {
        if (elevatorBackground->panelFrmId != -1) {
            int panelFid = buildFid(OBJ_TYPE_INTERFACE, elevatorBackground->panelFrmId);
            if (!_elevatorPanelFrmImage.lock(panelFid)) {
                backgroundsLoaded = false;
            }
        }
    } else {
        backgroundsLoaded = false;
    }

    if (!backgroundsLoaded) {
        _elevatorBackgroundFrmImage.unlock();
        _elevatorPanelFrmImage.unlock();

        for (int index = 0; index < ELEVATOR_FRM_COUNT; index++) {
            _elevatorFrmImages[index].unlock();
        }

        if (gElevatorWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    int elevatorWindowX = (screenGetWidth() - _elevatorBackgroundFrmImage.getWidth()) / 2;
    int elevatorWindowY = (screenGetHeight() - INTERFACE_BAR_HEIGHT - 1 - _elevatorBackgroundFrmImage.getHeight()) / 2;
    gElevatorWindow = windowCreate(
        elevatorWindowX,
        elevatorWindowY,
        _elevatorBackgroundFrmImage.getWidth(),
        _elevatorBackgroundFrmImage.getHeight(),
        static_cast<ColorWithFlags>(256),
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (gElevatorWindow == -1) {
        _elevatorBackgroundFrmImage.unlock();
        _elevatorPanelFrmImage.unlock();

        for (int index = 0; index < ELEVATOR_FRM_COUNT; index++) {
            _elevatorFrmImages[index].unlock();
        }

        if (gElevatorWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    gElevatorWindowBuffer = windowGetBuffer(gElevatorWindow);
    memcpy(gElevatorWindowBuffer, _elevatorBackgroundFrmImage.getData(), _elevatorBackgroundFrmImage.getWidth() * _elevatorBackgroundFrmImage.getHeight());

    if (_elevatorPanelFrmImage.isLocked()) {
        blitBufferToBuffer(_elevatorPanelFrmImage.getData(),
            _elevatorPanelFrmImage.getWidth(),
            _elevatorPanelFrmImage.getHeight(),
            _elevatorPanelFrmImage.getWidth(),
            gElevatorWindowBuffer + _elevatorBackgroundFrmImage.getWidth() * (_elevatorBackgroundFrmImage.getHeight() - _elevatorPanelFrmImage.getHeight()),
            _elevatorBackgroundFrmImage.getWidth());
    }

    int y = 40;
    for (int level = 0; level < gElevatorLevels[elevator]; level++) {
        int btn = buttonCreate(gElevatorWindow,
            13,
            y,
            _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getWidth(),
            _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getHeight(),
            -1,
            -1,
            -1,
            500 + level,
            _elevatorFrmImages[ELEVATOR_FRM_BUTTON_UP].getData(),
            _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, nullptr);
        }
        y += 60;
    }

    return 0;
}

// 0x43F6D0 elevator_end
static void elevatorWindowFree()
{
    windowDestroy(gElevatorWindow);

    _elevatorBackgroundFrmImage.unlock();
    _elevatorPanelFrmImage.unlock();

    for (int index = 0; index < ELEVATOR_FRM_COUNT; index++) {
        _elevatorFrmImages[index].unlock();
    }

    scriptsEnable();

    if (gElevatorWindowIsoWasEnabled) {
        isoEnable();
    }

    colorCycleEnable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
}

// 0x43F73C Check4Keys
static int elevatorGetLevelFromKeyCode(int elevator, int keyCode)
{
    for (int index = 0; index < ELEVATOR_LEVEL_MAX; index++) {
        char c = gElevatorLevelLabels[elevator][index];
        if (c == '\0') {
            break;
        }

        // consider use std toupper instead of & 0xFF
        if (c == (char)(keyCode & 0xFF)) {
            return index + 1;
        }
    }
    return 0;
}

static int elevatorGetLevelFromEscKey(int elevator, int map)
{
    const ElevatorDescription* exits = gElevatorDescriptions[elevator];
    for (int index = 0; index < ELEVATOR_LEVEL_MAX; index++) {
        if (exits[index].map == map && exits[index].elevation == gElevation) {
            return index;
        }
    }
    return -1;
}

void elevatorsInit()
{
    char* elevatorsFileName;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ELEVATORS_FILE_KEY, &elevatorsFileName);
    if (elevatorsFileName == nullptr || *elevatorsFileName == '\0') {
        return;
    }

    ScopedConfig elevatorsConfig(elevatorsFileName, false);
    if (!elevatorsConfig) {
        return;
    }

    char sectionKey[4];
    char key[32];
    for (int index = 0; index < ELEVATORS_MAX; index++) {
        snprintf(sectionKey, sizeof(sectionKey), "%d", index);

        if (index >= ELEVATOR_COUNT) {
            int levels = 0;
            configGetInt(elevatorsConfig.get(), sectionKey, "ButtonCount", &levels);
            gElevatorLevels[index] = std::clamp(levels, 2, ELEVATOR_LEVEL_MAX);
        }

        configGetInt(elevatorsConfig.get(), sectionKey, "MainFrm", &(gElevatorBackgrounds[index].backgroundFrmId));
        configGetInt(elevatorsConfig.get(), sectionKey, "ButtonsFrm", &(gElevatorBackgrounds[index].panelFrmId));

        for (int level = 0; level < ELEVATOR_LEVEL_MAX; level++) {
            snprintf(key, sizeof(key), "ID%d", level + 1);
            configGetEnum<Map>(elevatorsConfig.get(), sectionKey, key, &(gElevatorDescriptions[index][level].map));

            snprintf(key, sizeof(key), "Elevation%d", level + 1);
            configGetInt(elevatorsConfig.get(), sectionKey, key, &(gElevatorDescriptions[index][level].elevation));

            snprintf(key, sizeof(key), "Tile%d", level + 1);
            configGetInt(elevatorsConfig.get(), sectionKey, key, &(gElevatorDescriptions[index][level].tile));
        }
    }

    // NOTE: Sfall implementation is slightly different. It uses one
    // loop and stores `type` value in a separate lookup table. This
    // value is then used in the certain places to remap from
    // requested elevator to the new one.
    for (int index = 0; index < ELEVATORS_MAX; index++) {
        snprintf(sectionKey, sizeof(sectionKey), "%d", index);

        int type;
        if (configGetInt(elevatorsConfig.get(), sectionKey, "Image", &type)) {
            type = std::clamp(type, 0, ELEVATORS_MAX - 1);
            if (index != type) {
                memcpy(&(gElevatorBackgrounds[index]), &(gElevatorBackgrounds[type]), sizeof(*gElevatorBackgrounds));
                memcpy(&(gElevatorLevels[index]), &(gElevatorLevels[type]), sizeof(*gElevatorLevels));
                memcpy(&(gElevatorLevelLabels[index]), &(gElevatorLevelLabels[type]), sizeof(*gElevatorLevelLabels));
            }
        }
    }
}

} // namespace fallout
