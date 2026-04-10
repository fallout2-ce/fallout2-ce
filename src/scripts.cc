#include "scripts.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "automap.h"
#include "combat.h"
#include "critter.h"
#include "debug.h"
#include "dialog.h"
#include "elevator.h"
#include "endgame.h"
#include "export.h"
#include "game.h"
#include "game_dialog.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "input.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "party_member.h"
#include "platform_compat.h"
#include "proto.h"
#include "proto_instance.h"
#include "queue.h"
#include "scan_unimplemented.h"
#include "sfall_arrays.h"
#include "sfall_config.h"
#include "sfall_global_scripts.h"
#include "stat.h"
#include "svga.h"
#include "tile.h"
#include "window.h"
#include "window_manager.h"
#include "window_manager_private.h"
#include "worldmap.h"

namespace fallout {

#define SCRIPT_LIST_EXTENT_SIZE 16

// SFALL: Increase number of message lists for scripted dialogs.
// CE: In Sfall this increase is configurable with `BoostScriptDialogLimit`.
#define SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY 10000

typedef struct ScriptsListEntry {
    char name[16];
    int local_vars_num;
} ScriptsListEntry;

typedef struct ScriptListExtent {
    Script scripts[SCRIPT_LIST_EXTENT_SIZE];
    // Number of scripts in the extent
    int length;
    struct ScriptListExtent* next;
} ScriptListExtent;

typedef struct ScriptList {
    ScriptListExtent* head;
    ScriptListExtent* tail;
    // Number of extents in the script list.
    int length;
    int nextScriptId;
} ScriptList;

static Program* scriptsCreateProgramByName(const char* name);
static void _doBkProcesses();
static void _script_chk_critters();
static void _script_chk_timed_events();
static int scriptsClearPendingRequests();
static int scriptLocateProcs(Script* scr);
static int scriptsLoadScriptsList();
static int scriptsFreeScriptsList();
static int scriptsGetFileName(int scriptIndex, char* name, size_t size);
static int _scr_header_load();
static void scriptsCloseNearbyElevatorDoors();
static int scriptsHandleElevatorRequest(bool closeDoorsBeforeMapTransition);
static int scriptWrite(Script* scr, File* stream);
static int scriptListExtentWrite(ScriptListExtent* scriptExtent, File* stream);
static int scriptRead(Script* scr, File* stream);
static int scriptListExtentRead(ScriptListExtent* scriptExtent, File* stream);
static void scriptListExtentClearRuntimeState(ScriptListExtent* scriptExtent);
static int scriptGetNewId(int scriptType);
static int scriptsRemoveLocalVars(Script* script);
static int scriptsGetMessageList(int messageListId, MessageList** outMessageList);

// 0x50D6B8
static char gScriptsErrorText[] = "Error";

// 0x50D6C0
static char gScriptsEmptyText[] = "";

// Number of lines in scripts.lst
//
// 0x51C6AC
static int _num_script_indexes = 0;

// 0x51C6B0
static int gScriptsEnumerationScriptIndex = 0;

// 0x51C6B4
static ScriptListExtent* gScriptsEnumerationScriptListExtent = nullptr;

// 0x51C6B8
static int gScriptsEnumerationElevation = 0;

// 0x51C6BC
static bool gSpatialsEnabled = true;

// 0x51C6C0
static ScriptList gScriptLists[SCRIPT_TYPE_COUNT];

// 0x51C710
static const char* gScriptsBasePath = "scripts\\";

// 0x51C714
static bool gScriptsEnabled = false;

// 0x51C718
static int gCritterProcessingEnabled = 0;

// 0x51C71C
static int gGameModeEnabled = 0;

// Game time in ticks (1/10 second).
//
// 0x51C720
static unsigned int gGameTime = 302400;

// 0x51C724
static const int gGameTimeDaysPerMonth[12] = {
    31, // Jan
    28, // Feb
    31, // Mar
    30, // Apr
    31, // May
    30, // Jun
    31, // Jul
    31, // Aug
    30, // Sep
    31, // Oct
    30, // Nov
    31, // Dec
};

// 0x51C758
const char* gScriptProcNames[SCRIPT_PROC_COUNT] = {
    "no_p_proc",
    "start",
    "spatial_p_proc",
    "description_p_proc",
    "pickup_p_proc",
    "drop_p_proc",
    "use_p_proc",
    "use_obj_on_p_proc",
    "use_skill_on_p_proc",
    "none_x_bad",
    "none_x_bad",
    "talk_p_proc",
    "critter_p_proc",
    "combat_p_proc",
    "damage_p_proc",
    "map_enter_p_proc",
    "map_exit_p_proc",
    "create_p_proc",
    "destroy_p_proc",
    "none_x_bad",
    "none_x_bad",
    "look_at_p_proc",
    "timed_event_p_proc",
    "map_update_p_proc",
    "push_p_proc",
    "is_dropping_p_proc",
    "combat_is_starting_p_proc",
    "combat_is_over_p_proc",
};

// scripts.lst
//
// 0x51C7C8
static ScriptsListEntry* gScriptsListEntries = nullptr;

// 0x51C7CC
static int gScriptsListEntriesLength = 0;

// 0x51C7D4
static int gObjectIdCounter = 4;

// 0x51C7DC
static int gCritterProcessingIndex = 0;

// 0x51C7E0
static int gLastQueueProcessingTime = 0;

// 0x51C7E4
static int gLastMapUpdateTime = 0;

// 0x51C7E8
static Object* _scrQueueTestObj = nullptr;

// 0x51C7EC
static int _scrQueueTestValue = 0;

// 0x51C7F0
static char* gErrorString = gScriptsErrorText;

// 0x51C7F4
static char* gEmptyString = gScriptsEmptyText;

// 0x664954
static unsigned int gScriptsRequests;

// 0x664958
static CombatStartData gScriptsRequestedCSD;

// 0x664980
static CombatStartData gScriptsCSD;

// 0x6649A8
static int gScriptsRequestedElevatorType;

// 0x6649AC
static int gScriptsRequestedElevatorLevel;

// 0x6649B0
static int gScriptsRequestedExplosionTile;

// 0x6649B4
static int gScriptsRequestedExplosionElevation;

// 0x6649B8
static int gScriptsRequestedExplosionMinDamage;

// 0x6649BC
static int gScriptsRequestedExplosionMaxDamage;

// 0x6649C0
static Object* gScriptsRequestedDialogWith;

// 0x6649C4
static Object* gScriptsRequestedLootingBy;

// 0x6649C8
static Object* gScriptsRequestedLootingFrom;

// 0x6649CC
static Object* gScriptsRequestedStealingBy;

// 0x6649D0
static Object* gScriptsRequestedStealingFrom;

// 0x6649D4
static MessageList gScriptDialogMessageLists[SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY];

// scr.msg
//
// 0x667724
static MessageList gScrMessageList;

// 0x667748
static int gLastBackgroundProcessTime;

// 0x66774C
static bool gBackgroundProcessTimeInitialized;

// 0x667750
static char gDebugScriptFileName[20];

static int gStartYear;
static int gStartMonth;
static int gStartDay;

static int gMovieTimerArtimer1;
static int gMovieTimerArtimer2;
static int gMovieTimerArtimer3;
static int gMovieTimerArtimer4;

// Returns game time in ticks (1/10 second).
//
// 0x4A3330
unsigned int gameTimeGetTime()
{
    return gGameTime;
}

// 0x4A3338
void gameTimeGetDate(int* monthPtr, int* dayPtr, int* yearPtr)
{
    int year = (gGameTime / GAME_TIME_TICKS_PER_DAY + gStartDay) / 365 + gStartYear;
    int month = gStartMonth;
    int day = (gGameTime / GAME_TIME_TICKS_PER_DAY + gStartDay) % 365;

    while (1) {
        int daysInMonth = gGameTimeDaysPerMonth[month];
        if (day < daysInMonth) {
            break;
        }

        month++;
        day -= daysInMonth;

        if (month == 12) {
            year++;
            month = 0;
        }
    }

    if (dayPtr != nullptr) {
        *dayPtr = day + 1;
    }

    if (monthPtr != nullptr) {
        *monthPtr = month + 1;
    }

    if (yearPtr != nullptr) {
        *yearPtr = year;
    }
}

// Returns game hour/minute in military format (hhmm).
//
// Examples:
// - 8:00 A.M. -> 800
// - 3:00 P.M. -> 1500
// - 11:59 P.M. -> 2359
//
// game_time_hour
// 0x4A33C8
int gameTimeGetHour()
{
    return 100 * ((gGameTime / 600) / 60 % 24) + (gGameTime / 600) % 60;
}

// Returns time string (h:mm)
//
// 0x4A3420
char* gameTimeGetTimeString()
{
    // 0x66772C
    static char hour_str[7];

    snprintf(hour_str, sizeof(hour_str), "%d:%02d", (gGameTime / 600) / 60 % 24, (gGameTime / 600) % 60);
    return hour_str;
}

// TODO: Make unsigned.
//
// 0x4A347C
void gameTimeSetTime(unsigned int time)
{
    if (time == 0) {
        time = 1;
    }

    gGameTime = time;
}

// 0x4A34CC
void gameTimeAddTicks(int ticks)
{
    gGameTime += ticks;

    bool shouldProcessGameTimeEvent = false;

    unsigned int year = gGameTime / GAME_TIME_TICKS_PER_YEAR;
    if (year >= 13) {
        endgameSetupDeathEnding(ENDGAME_DEATH_ENDING_REASON_TIMEOUT);
        _game_user_wants_to_quit = GAME_QUIT_REQUEST_MAIN_MENU;
    }

    // FIXME: This condition will never be true.
    if (shouldProcessGameTimeEvent) {
        gameTimeEventProcess(nullptr, nullptr);
    }
}

// 0x4A3518
void gameTimeAddSeconds(int seconds)
{
    // NOTE: Uninline.
    gameTimeAddTicks(seconds * 10);
}

// 0x4A3570
int gameTimeScheduleUpdateEvent()
{
    // ticks until midnight
    int delay = 10 * (60 * (60 - (gGameTime / 600) % 60 - 1) + 3600 * (24 - (gGameTime / 600) / 60 % 24 - 1) + 60);
    if (queueAddEvent(delay, nullptr, nullptr, EVENT_TYPE_GAME_TIME) == -1) {
        return -1;
    }

    if (gMapHeader.name[0] != '\0') {
        if (queueAddEvent(600, nullptr, nullptr, EVENT_TYPE_MAP_UPDATE_EVENT) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4A3620
int gameTimeEventProcess(Object* obj, void* data)
{
    int movie_index;
    int stopProcess;

    movie_index = -1;

    debugPrint("\nQUEUE PROCESS: Midnight!");

    if (gameMovieIsPlaying()) {
        return 0;
    }

    objectUnjamAll();

    if (!_gdialogActive()) {
        _scriptsCheckGameEvents(&movie_index, -1);
    }

    stopProcess = critterCheckRadiationEvent(gDude);

    queueClearByEventType(4, nullptr);

    gameTimeScheduleUpdateEvent();

    if (movie_index != -1) {
        stopProcess = 1;
    }

    return stopProcess;
}

// 0x4A3690
int _scriptsCheckGameEvents(int* moviePtr, int window)
{
    int movie = -1;
    int movieFlags = GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC;
    bool endgame = false;
    bool adjustRep = false;

    int day = gGameTime / GAME_TIME_TICKS_PER_DAY;

    if (gameGetGlobalVar(GVAR_ENEMY_ARROYO)) {
        movie = MOVIE_AFAILED;
        movieFlags = GAME_MOVIE_FADE_IN | GAME_MOVIE_STOP_MUSIC;
        endgame = true;
    } else {
        if (day >= gMovieTimerArtimer4 || gameGetGlobalVar(GVAR_FALLOUT_2) >= 3) {
            movie = MOVIE_ARTIMER4;
            if (!gameMovieIsSeen(MOVIE_ARTIMER4)) {
                adjustRep = true;
                wmAreaSetVisibleState(CITY_ARROYO, 0, 1);
                wmAreaSetVisibleState(CITY_DESTROYED_ARROYO, 1, 1);
                wmAreaMarkVisitedState(CITY_DESTROYED_ARROYO, 2);
            }
        } else if (day >= gMovieTimerArtimer3 && gameGetGlobalVar(GVAR_FALLOUT_2) != 3) {
            adjustRep = true;
            movie = MOVIE_ARTIMER3;
        } else if (day >= gMovieTimerArtimer2 && gameGetGlobalVar(GVAR_FALLOUT_2) != 3) {
            adjustRep = true;
            movie = MOVIE_ARTIMER2;
        } else if (day >= gMovieTimerArtimer1 && gameGetGlobalVar(GVAR_FALLOUT_2) != 3) {
            adjustRep = true;
            movie = MOVIE_ARTIMER1;
        }
    }

    if (movie != -1) {
        if (gameMovieIsSeen(movie)) {
            movie = -1;
        } else {
            if (window != -1) {
                windowHide(window);
            }

            gameMoviePlay(movie, movieFlags);

            if (window != -1) {
                windowShow(window);
            }

            if (adjustRep) {
                int rep = gameGetGlobalVar(GVAR_TOWN_REP_ARROYO);
                gameSetGlobalVar(GVAR_TOWN_REP_ARROYO, rep - 15);
            }
        }
    }

    if (endgame) {
        _game_user_wants_to_quit = GAME_QUIT_REQUEST_MAIN_MENU;
    } else {
        tileWindowRefresh();
    }

    if (moviePtr != nullptr) {
        *moviePtr = movie;
    }

    return 0;
}

// 0x4A382C
int mapUpdateEventProcess(Object* obj, void* data)
{
    scriptsExecMapUpdateScripts(SCRIPT_PROC_MAP_UPDATE);

    queueClearByEventType(EVENT_TYPE_MAP_UPDATE_EVENT, nullptr);

    if (gMapHeader.name[0] == '\0') {
        return 0;
    }

    if (queueAddEvent(600, nullptr, nullptr, EVENT_TYPE_MAP_UPDATE_EVENT) != -1) {
        return 0;
    }

    return -1;
}

// new_obj_id
// 0x4A386C
int scriptsNewObjectId()
{
    Object* ptr;

    do {
        gObjectIdCounter++;
        ptr = objectFindFirst();

        while (ptr) {
            if (gObjectIdCounter == ptr->id) {
                break;
            }

            ptr = objectFindNext();
        }
    } while (ptr);

    if (gObjectIdCounter >= 18000) {
        debugPrint("\n    ERROR: new_obj_id() !!!! Picked PLAYER ID!!!!");
    }

    gObjectIdCounter++;

    return gObjectIdCounter;
}

// 0x4A390C
int scriptGetSid(Program* program)
{
    for (int type = 0; type < SCRIPT_TYPE_COUNT; type++) {
        ScriptListExtent* extent = gScriptLists[type].head;
        while (extent != nullptr) {
            for (int index = 0; index < extent->length; index++) {
                Script* script = &(extent->scripts[index]);
                if (script->program == program) {
                    return script->sid;
                }
            }
            extent = extent->next;
        }
    }

    return -1;
}

// 0x4A39AC
Object* scriptGetSelf(Program* program)
{
    int sid = scriptGetSid(program);

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        return nullptr;
    }

    if (script->owner != nullptr) {
        return script->owner;
    }

    if (SID_TYPE(sid) != SCRIPT_TYPE_SPATIAL) {
        return nullptr;
    }

    Object* object;
    int fid = buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, 0);
    objectCreateWithFidPid(&object, fid, -1);
    objectHide(object, nullptr);
    _obj_toggle_flat(object, nullptr);
    object->sid = sid;

    // NOTE: Redundant, we've already obtained script earlier. Probably
    // inlining.
    Script* spatialScript;
    if (scriptGetScript(sid, &spatialScript) == -1) {
        // FIXME: this is clearly an error, but I guess it's never reached since
        // we've already obtained script for given sid earlier.
        return (Object*)-1;
    }

    object->id = scriptsNewObjectId();
    spatialScript->ownerId = object->id;
    spatialScript->owner = object;

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        Script* spatialIter = scriptGetFirstSpatialScript(elevation);
        while (spatialIter != nullptr) {
            if (spatialIter == script) {
                objectSetLocation(object, builtTileGetTile(script->sp.built_tile), elevation, nullptr);
                return object;
            }
            spatialIter = scriptGetNextSpatialScript();
        }
    }

    return object;
}

// 0x4A3B0C
int scriptSetObjects(int sid, Object* source, Object* target)
{
    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        return -1;
    }

    script->source = source;
    script->target = target;

    return 0;
}

// 0x4A3B34
void scriptSetFixedParam(int sid, int value)
{
    Script* script;
    if (scriptGetScript(sid, &script) != -1) {
        script->fixedParam = value;
    }
}

// 0x4A3B54
int scriptSetActionBeingUsed(int sid, int value)
{
    Script* scr;

    if (scriptGetScript(sid, &scr) == -1) {
        return -1;
    }

    scr->actionBeingUsed = value;

    return 0;
}

// 0x4A3B74
static Program* scriptsCreateProgramByName(const char* name)
{
    char path[COMPAT_MAX_PATH];

    strcpy(path, _cd_path_base);
    strcat(path, gScriptsBasePath);
    strcat(path, name);
    strcat(path, ".int");

    return programCreateByPath(path);
}

// 0x4A3C2C
static void _doBkProcesses()
{
    if (!gBackgroundProcessTimeInitialized) {
        gLastBackgroundProcessTime = _get_bk_time();
        gBackgroundProcessTimeInitialized = 1;
    }

    int currentTime = _get_bk_time();
    if (gScriptsEnabled) {
        gLastBackgroundProcessTime = currentTime;

        // NOTE: There is a loop at 0x4A3C64, consisting of one iteration, going
        // downwards from 1.
        for (int index = 0; index < 1; index++) {
            _updatePrograms();
        }
    }

    scriptWindowUpdateAll();

    if (gScriptsEnabled && gCritterProcessingEnabled) {
        // SFALL: Fix to prevent the execution of critter_p_proc and game events
        // when playing movies.
        if (!_gdialogActive() && !gameMovieIsPlaying()) {
            _script_chk_critters();
            _script_chk_timed_events();
        }
    }
}

// 0x4A3CA0
static void _script_chk_critters()
{
    if (!_gdialogActive() && !isInCombat()) {
        ScriptList* scriptList;
        ScriptListExtent* scriptListExtent;

        int scriptsCount = 0;

        scriptList = &(gScriptLists[SCRIPT_TYPE_CRITTER]);
        scriptListExtent = scriptList->head;
        while (scriptListExtent != nullptr) {
            scriptsCount += scriptListExtent->length;
            scriptListExtent = scriptListExtent->next;
        }

        gCritterProcessingIndex += 1;
        if (gCritterProcessingIndex >= scriptsCount) {
            gCritterProcessingIndex = 0;
        }

        if (gCritterProcessingIndex < scriptsCount) {
            int proc = isInCombat() ? SCRIPT_PROC_COMBAT : SCRIPT_PROC_CRITTER;
            int extentIndex = gCritterProcessingIndex / SCRIPT_LIST_EXTENT_SIZE;
            int scriptIndex = gCritterProcessingIndex % SCRIPT_LIST_EXTENT_SIZE;

            scriptList = &(gScriptLists[SCRIPT_TYPE_CRITTER]);
            scriptListExtent = scriptList->head;
            while (scriptListExtent != nullptr && extentIndex != 0) {
                extentIndex -= 1;
                scriptListExtent = scriptListExtent->next;
            }

            if (scriptListExtent != nullptr) {
                Script* script = &(scriptListExtent->scripts[scriptIndex]);
                scriptExecProc(script->sid, proc);
            }
        }
    }
}

// TODO: Check.
//
// 0x4A3D84
static void _script_chk_timed_events()
{
    int currentTime = _get_bk_time();

    bool shouldProcessQueue = false;
    if (!isInCombat()) {
        shouldProcessQueue = true;
    }

    if (gameGetState() != GAME_STATE_4) {
        if (getTicksBetween(currentTime, gLastMapUpdateTime) >= 30000) {
            gLastMapUpdateTime = currentTime;
            scriptsExecMapUpdateScripts(SCRIPT_PROC_MAP_UPDATE);
        }
    } else {
        shouldProcessQueue = false;
    }

    if (getTicksBetween(currentTime, gLastQueueProcessingTime) >= 100) {
        gLastQueueProcessingTime = currentTime;
        if (!isInCombat()) {
            gGameTime += 1;
        }
        shouldProcessQueue = true;
    }

    if (shouldProcessQueue) {
        while (!queueIsEmpty()) {
            if (gameTimeGetTime() < queueGetNextEventTime()) {
                break;
            }

            queueProcessEvents();
        }
    }
}

// 0x4A3E30
void _scrSetQueueTestVals(Object* obj, int value)
{
    _scrQueueTestObj = obj;
    _scrQueueTestValue = value;
}

// 0x4A3E3C
int _scrQueueRemoveFixed(Object* obj, void* data)
{
    ScriptEvent* scriptEvent = (ScriptEvent*)data;
    return obj == _scrQueueTestObj && scriptEvent->fixedParam == _scrQueueTestValue;
}

// 0x4A3E60
int scriptAddTimerEvent(int sid, int delay, int param)
{
    ScriptEvent* scriptEvent = (ScriptEvent*)internal_malloc(sizeof(*scriptEvent));
    if (scriptEvent == nullptr) {
        return -1;
    }

    scriptEvent->sid = sid;
    scriptEvent->fixedParam = param;

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        internal_free(scriptEvent);
        return -1;
    }

    if (queueAddEvent(delay, script->owner, scriptEvent, EVENT_TYPE_SCRIPT) == -1) {
        internal_free(scriptEvent);
        return -1;
    }

    return 0;
}

// 0x4A3EDC
int scriptEventWrite(File* stream, void* data)
{
    ScriptEvent* scriptEvent = (ScriptEvent*)data;

    if (fileWriteInt32(stream, scriptEvent->sid) == -1) return -1;
    if (fileWriteInt32(stream, scriptEvent->fixedParam) == -1) return -1;

    return 0;
}

// 0x4A3F04
int scriptEventRead(File* stream, void** dataPtr)
{
    ScriptEvent* scriptEvent = (ScriptEvent*)internal_malloc(sizeof(*scriptEvent));
    if (scriptEvent == nullptr) {
        return -1;
    }

    if (fileReadInt32(stream, &(scriptEvent->sid)) == -1) goto err;
    if (fileReadInt32(stream, &(scriptEvent->fixedParam)) == -1) goto err;

    *dataPtr = scriptEvent;

    return 0;

err:

    // there is a memory leak in original code, free is not called
    internal_free(scriptEvent);

    return -1;
}

// 0x4A3F4C
int scriptEventProcess(Object* obj, void* data)
{
    ScriptEvent* scriptEvent = (ScriptEvent*)data;

    Script* script;
    if (scriptGetScript(scriptEvent->sid, &script) == -1) {
        return 0;
    }

    script->fixedParam = scriptEvent->fixedParam;

    scriptExecProc(scriptEvent->sid, SCRIPT_PROC_TIMED);

    return 0;
}

// 0x4A3F80
static int scriptsClearPendingRequests()
{
    gScriptsRequests = 0;
    return 0;
}

// NOTE: Inlined.
//
// 0x4A3F90
int _scripts_clear_combat_requests(Script* script)
{
    if ((gScriptsRequests & SCRIPT_REQUEST_COMBAT) != 0 && gScriptsRequestedCSD.attacker == script->owner) {
        gScriptsRequests &= ~(SCRIPT_REQUEST_0x0400 | SCRIPT_REQUEST_COMBAT);
    }
    return 0;
}

static void scriptsCloseNearbyElevatorDoors()
{
    Object* elevatorDoors = objectFindFirstAtElevation(gDude->elevation);
    while (elevatorDoors != nullptr) {
        int pid = elevatorDoors->pid;
        if (PID_TYPE(pid) == OBJ_TYPE_SCENERY
            && (pid == PROTO_ID_0x2000099 || pid == PROTO_ID_0x20001A5 || pid == PROTO_ID_0x20001D6)
            && tileDistanceBetween(elevatorDoors->tile, gDude->tile) <= 4) {
            break;
        }

        elevatorDoors = objectFindNextAtElevation();
    }

    if (elevatorDoors != nullptr) {
        objectSetFrame(elevatorDoors, 0, nullptr);
        objectSetLocation(elevatorDoors, elevatorDoors->tile, elevatorDoors->elevation, nullptr);
        elevatorDoors->flags &= ~OBJECT_OPEN_DOOR;
        elevatorDoors->data.scenery.door.openFlags &= ~0x01;
        _obj_rebuild_all_light();
    } else {
        debugPrint("\nWarning: Elevator: Couldn't find old elevator doors!");
    }
}

static int scriptsHandleElevatorRequest(bool closeDoorsBeforeMapTransition)
{
    int map = gMapHeader.index;
    int elevation = gScriptsRequestedElevatorLevel;
    int tile = -1;

    if (elevatorSelectLevel(gScriptsRequestedElevatorType, &map, &elevation, &tile) == -1) {
        return -1;
    }

    automapSaveCurrent();

    if (map == gMapHeader.index) {
        if (elevation != gElevation) {
            scriptsCloseNearbyElevatorDoors();
        }

        reg_anim_clear(gDude);
        objectSetRotation(gDude, ROTATION_SE, nullptr);
        objectAttemptPlacement(gDude, tile, elevation, 0);

        return 0;
    }

    if (closeDoorsBeforeMapTransition) {
        scriptsCloseNearbyElevatorDoors();
    }

    MapTransition transition;
    memset(&transition, 0, sizeof(transition));

    transition.map = map;
    transition.elevation = elevation;
    transition.tile = tile;
    transition.rotation = ROTATION_SE;

    mapSetTransition(&transition);

    return 0;
}

// 0x4A3FB4
int scriptsHandleRequests()
{
    if (gScriptsRequests == 0) {
        return 0;
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_COMBAT) != 0) {
        if (!_action_explode_running()) {
            // entering combat
            gScriptsRequests &= ~(SCRIPT_REQUEST_0x0400 | SCRIPT_REQUEST_COMBAT);
            memcpy(&gScriptsCSD, &gScriptsRequestedCSD, sizeof(gScriptsCSD));

            if ((gScriptsRequests & SCRIPT_REQUEST_0x40) != 0) {
                gScriptsRequests &= ~SCRIPT_REQUEST_0x40;
                _combat(nullptr);
            } else {
                _combat(&gScriptsCSD);
                memset(&gScriptsCSD, 0, sizeof(gScriptsCSD));
            }
        }
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_TOWN_MAP) != 0) {
        gScriptsRequests &= ~SCRIPT_REQUEST_TOWN_MAP;
        wmTownMap();
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_WORLD_MAP) != 0) {
        gScriptsRequests &= ~SCRIPT_REQUEST_WORLD_MAP;
        wmWorldMap();
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_ELEVATOR) != 0) {
        gScriptsRequests &= ~SCRIPT_REQUEST_ELEVATOR;
        scriptsHandleElevatorRequest(true);
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_EXPLOSION) != 0) {
        gScriptsRequests &= ~SCRIPT_REQUEST_EXPLOSION;
        actionExplode(gScriptsRequestedExplosionTile, gScriptsRequestedExplosionElevation, gScriptsRequestedExplosionMinDamage, gScriptsRequestedExplosionMaxDamage, nullptr, 1);
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_DIALOG) != 0) {
        gScriptsRequests &= ~SCRIPT_REQUEST_DIALOG;
        gameDialogEnter(gScriptsRequestedDialogWith, 0);
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_ENDGAME) != 0) {
        gScriptsRequests &= ~SCRIPT_REQUEST_ENDGAME;
        endgamePlaySlideshow();
        endgamePlayMovie();
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_LOOTING) != 0) {
        gScriptsRequests &= ~SCRIPT_REQUEST_LOOTING;
        inventoryOpenLooting(gScriptsRequestedLootingBy, gScriptsRequestedLootingFrom);
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_STEALING) != 0) {
        gScriptsRequests &= ~SCRIPT_REQUEST_STEALING;
        inventoryOpenStealing(gScriptsRequestedStealingBy, gScriptsRequestedStealingFrom);
    }

    DeleteAllTempArrays();

    return 0;
}

// 0x4A43A0
int _scripts_check_state_in_combat()
{
    if ((gScriptsRequests & SCRIPT_REQUEST_ELEVATOR) != 0) {
        // do not close elevator doors before map transition
        scriptsHandleElevatorRequest(false);
    }

    if ((gScriptsRequests & SCRIPT_REQUEST_LOOTING) != 0) {
        inventoryOpenLooting(gScriptsRequestedLootingBy, gScriptsRequestedLootingFrom);
    }

    // NOTE: Uninline.
    scriptsClearPendingRequests();

    return 0;
}

// 0x4A457C
int scriptsRequestCombat(CombatStartData* combat)
{
    if ((gScriptsRequests & SCRIPT_REQUEST_0x0400) != 0) {
        return -1;
    }

    if (combat) {
        memcpy(&gScriptsRequestedCSD, combat, sizeof(gScriptsRequestedCSD));
    } else {
        gScriptsRequests |= SCRIPT_REQUEST_0x40;
    }

    gScriptsRequests |= SCRIPT_REQUEST_COMBAT;

    return 0;
}

// Likely related to random encounter, ala scriptsRequestRandomEncounter RELEASE
//
// 0x4A45D4
void _scripts_request_combat_locked(CombatStartData* combat)
{
    if (combat != nullptr) {
        memcpy(&gScriptsRequestedCSD, combat, sizeof(gScriptsRequestedCSD));
    } else {
        gScriptsRequests |= SCRIPT_REQUEST_0x40;
    }

    gScriptsRequests |= (SCRIPT_REQUEST_0x0400 | SCRIPT_REQUEST_COMBAT);
}

// 0x4A461C
void scripts_request_townmap()
{
    if (isInCombat()) {
        _game_user_wants_to_quit = GAME_QUIT_REQUEST_END_COMBAT;
    }

    gScriptsRequests |= SCRIPT_REQUEST_TOWN_MAP;
}

// request_world_map()
// 0x4A4644
void scriptsRequestWorldMap()
{
    if (isInCombat()) {
        _game_user_wants_to_quit = GAME_QUIT_REQUEST_END_COMBAT;
    }

    gScriptsRequests |= SCRIPT_REQUEST_WORLD_MAP;
}

// scripts_request_elevator
// 0x4A466C
int scriptsRequestElevator(Object* obj, int elevatorType)
{
    int elevatorLevel = gElevation;

    int tile = obj->tile;
    if (tile == -1) {
        debugPrint("\nError: scripts_request_elevator! Bad tile num");
        return -1;
    }

    // In the following code we are looking for an elevator. 5 tiles in each direction
    tile = tile - (HEX_GRID_WIDTH * 5) - 5; // left upper corner

    Object* elevator;
    for (int y = -5; y < 5; y++) {
        for (int x = -5; x < 5; x++) {
            elevator = objectFindFirstAtElevation(obj->elevation);
            while (elevator != nullptr) {
                if (tile == elevator->tile && elevator->pid == PROTO_ID_0x200050D) {
                    break;
                }

                elevator = objectFindNextAtElevation();
            }

            if (elevator != nullptr) {
                break;
            }

            tile += 1;
        }

        if (elevator != nullptr) {
            break;
        }

        tile += HEX_GRID_WIDTH - 10;
    }

    if (elevator != nullptr) {
        elevatorType = elevator->data.scenery.elevator.type;
        elevatorLevel = elevator->data.scenery.elevator.level;
    }

    if (elevatorType == -1) {
        return -1;
    }

    gScriptsRequests |= SCRIPT_REQUEST_ELEVATOR;
    gScriptsRequestedElevatorType = elevatorType;
    gScriptsRequestedElevatorLevel = elevatorLevel;

    return 0;
}

// 0x4A4730
int scriptsRequestExplosion(int tile, int elevation, int minDamage, int maxDamage)
{
    gScriptsRequests |= SCRIPT_REQUEST_EXPLOSION;
    gScriptsRequestedExplosionTile = tile;
    gScriptsRequestedExplosionElevation = elevation;
    gScriptsRequestedExplosionMinDamage = minDamage;
    gScriptsRequestedExplosionMaxDamage = maxDamage;
    return 0;
}

// 0x4A4754
void scriptsRequestDialog(Object* obj)
{
    gScriptsRequestedDialogWith = obj;
    gScriptsRequests |= SCRIPT_REQUEST_DIALOG;
}

// 0x4A4770
void scriptsRequestEndgame()
{
    gScriptsRequests |= SCRIPT_REQUEST_ENDGAME;
}

// 0x4A477C
int scriptsRequestLooting(Object* looter, Object* container)
{
    gScriptsRequestedLootingBy = looter;
    gScriptsRequestedLootingFrom = container;
    gScriptsRequests |= SCRIPT_REQUEST_LOOTING;
    return 0;
}

// 0x4A479C
int scriptsRequestStealing(Object* thief, Object* target)
{
    gScriptsRequestedStealingBy = thief;
    gScriptsRequestedStealingFrom = target;
    gScriptsRequests |= SCRIPT_REQUEST_STEALING;
    return 0;
}

// NOTE: Inlined.
void _script_make_path(char* path)
{
    strcpy(path, _cd_path_base);
    strcat(path, gScriptsBasePath);
}

// exec_script_proc
// 0x4A4810
int scriptExecProc(int sid, int proc)
{
    assert(proc >= 0 && proc < SCRIPT_PROC_COUNT);

    if (!gScriptsEnabled) {
        return -1;
    }

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        return -1;
    }

    script->scriptOverrides = 0;

    bool programLoaded = false;
    if ((script->flags & SCRIPT_FLAG_LOADED) == 0) {
        clock();

        char name[16];
        if (scriptsGetFileName(script->index & 0xFFFFFF, name, sizeof(name)) == -1) {
            return -1;
        }

        char* pch = strchr(name, '.');
        if (pch != nullptr) {
            *pch = '\0';
        }

        script->program = scriptsCreateProgramByName(name);
        if (script->program == nullptr) {
            debugPrint("\nError: exec_script_proc: script load failed!");
            return -1;
        }

        programLoaded = true;
        script->flags |= SCRIPT_FLAG_LOADED;
    }

    Program* program = script->program;
    if (program == nullptr) {
        return -1;
    }

    if ((program->flags & (PROGRAM_FLAG_FATAL_ERROR | PROGRAM_FLAG_CHILD_CALL | PROGRAM_FLAG_CHILD_SPAWN)) != 0) {
        return 0;
    }

    if (script->target == nullptr) {
        script->target = script->owner;
    }

    script->flags |= SCRIPT_FLAG_EXECUTED;

    if (programLoaded) {
        scriptLocateProcs(script);

        script->action = 0;
        // NOTE: Uninline.
        runProgram(program);
        programInterpret(program, -1);
    }

    // CE: Fix for the start procedure not being called correctly if the required standard script procedure is missing.
    int procedureIndex = script->procs[proc];
    if (procedureIndex == 0) {
        procedureIndex = script->procs[SCRIPT_PROC_START];
        if (procedureIndex == 0) {
            procedureIndex = -1;
        }
    }

    if (procedureIndex == -1) {
        return -1;
    }

    script->action = proc;

    programExecuteProcedure(program, procedureIndex);

    script->source = nullptr;

    return 0;
}

// Locate built-in procs for given script.
//
// 0x4A49D0
static int scriptLocateProcs(Script* script)
{
    for (int proc = 0; proc < SCRIPT_PROC_COUNT; proc++) {
        int index = programFindProcedure(script->program, gScriptProcNames[proc]);
        if (index == -1) {
            index = SCRIPT_PROC_NO_PROC;
        }
        script->procs[proc] = index;
    }

    return 0;
}

// 0x4A4A08
bool scriptHasProc(int sid, int proc)
{
    Script* scr;

    if (scriptGetScript(sid, &scr) == -1) {
        return 0;
    }

    return scr->procs[proc] != SCRIPT_PROC_NO_PROC;
}

// 0x4A4D50
static int scriptsLoadScriptsList()
{
    char path[COMPAT_MAX_PATH];
    _script_make_path(path);
    strcat(path, "scripts.lst");

    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    char string[260];
    while (fileReadString(string, 260, stream)) {
        gScriptsListEntriesLength++;

        ScriptsListEntry* entries = (ScriptsListEntry*)internal_realloc(gScriptsListEntries, sizeof(*entries) * gScriptsListEntriesLength);
        if (entries == nullptr) {
            return -1;
        }

        gScriptsListEntries = entries;

        ScriptsListEntry* entry = &(entries[gScriptsListEntriesLength - 1]);
        entry->local_vars_num = 0;

        char* substr = strstr(string, ".int");
        if (substr != nullptr) {
            int length = substr - string;
            if (length > 13) {
                return -1;
            }

            strncpy(entry->name, string, 13);
            entry->name[length] = '\0';
        }

        if (strstr(string, "#") != nullptr) {
            substr = strstr(string, "local_vars=");
            if (substr != nullptr) {
                entry->local_vars_num = atoi(substr + 11);
            }
        }
    }

    fileClose(stream);

    return 0;
}

// NOTE: Inlined.
//
// 0x4A4EFC
static int scriptsFreeScriptsList()
{
    if (gScriptsListEntries != nullptr) {
        internal_free(gScriptsListEntries);
        gScriptsListEntries = nullptr;
    }

    gScriptsListEntriesLength = 0;

    return 0;
}

// 0x4A4F28
int _scr_find_str_run_info(int scriptIndex, int* /*unused*/, int sid)
{
    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        return -1;
    }

    script->localVarsCount = gScriptsListEntries[scriptIndex].local_vars_num;

    return 0;
}

// 0x4A4F68
static int scriptsGetFileName(int scriptIndex, char* name, size_t size)
{
    snprintf(name, size, "%s.int", gScriptsListEntries[scriptIndex].name);
    return 0;
}

// scr_set_dude_script
// 0x4A4F90
int scriptsSetDudeScript()
{
    if (scriptsClearDudeScript() == -1) {
        return -1;
    }

    if (gDude == nullptr) {
        debugPrint("Error in scr_set_dude_script: obj_dude uninitialized!");
        return -1;
    }

    Proto* proto;
    if (protoGetProto(0x1000000, &proto) == -1) {
        debugPrint("Error in scr_set_dude_script: can't find obj_dude proto!");
        return -1;
    }

    proto->critter.sid = 0x4000000;

    objectSetScriptFromProto(gDude, &(gDude->sid));

    Script* script;
    if (scriptGetScript(gDude->sid, &script) == -1) {
        debugPrint("Error in scr_set_dude_script: can't find obj_dude script!");
        return -1;
    }

    script->flags |= (SCRIPT_FLAG_NO_SAVE | SCRIPT_FLAG_NO_REMOVE);

    return 0;
}

// scr_clear_dude_script
// 0x4A5044
int scriptsClearDudeScript()
{
    if (gDude == nullptr) {
        debugPrint("\nError in scr_clear_dude_script: obj_dude uninitialized!");
        return -1;
    }

    if (gDude->sid != -1) {
        Script* script;
        if (scriptGetScript(gDude->sid, &script) != -1) {
            script->flags &= ~(SCRIPT_FLAG_NO_SAVE | SCRIPT_FLAG_NO_REMOVE);
        }

        scriptRemove(gDude->sid);

        gDude->sid = -1;
    }

    return 0;
}

// scr_init
// 0x4A50A8
int scriptsInit()
{
    if (!messageListInit(&gScrMessageList)) {
        return -1;
    }

    for (int index = 0; index < SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY; index++) {
        if (!messageListInit(&(gScriptDialogMessageLists[index]))) {
            return -1;
        }
    }

    _scr_remove_all();
    _interpretOutputFunc(_win_debug);
    interpreterRegisterOpcodeHandlers();
    _scr_header_load();

    // NOTE: Uninline.
    scriptsClearPendingRequests();

    _partyMemberClear();

    if (scriptsLoadScriptsList() == -1) {
        return -1;
    }

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_SCRIPT, &gScrMessageList);

    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_START_YEAR, &gStartYear);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_START_MONTH, &gStartMonth);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_START_DAY, &gStartDay);

    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MOVIE_TIMER_ARTIMER1, &gMovieTimerArtimer1);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MOVIE_TIMER_ARTIMER2, &gMovieTimerArtimer2);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MOVIE_TIMER_ARTIMER3, &gMovieTimerArtimer3);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MOVIE_TIMER_ARTIMER4, &gMovieTimerArtimer4);

    checkScriptsOpcodes();

    return 0;
}

// 0x4A5120
int _scr_reset()
{
    _scr_remove_all();

    // NOTE: Uninline.
    scriptsClearPendingRequests();

    _partyMemberClear();

    return 0;
}

// 0x4A5138
int _scr_game_init()
{
    int i;
    char path[COMPAT_MAX_PATH];

    if (!messageListInit(&gScrMessageList)) {
        debugPrint("\nError initing script message file!");
        return -1;
    }

    for (i = 0; i < SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY; i++) {
        if (!messageListInit(&(gScriptDialogMessageLists[i]))) {
            debugPrint("\nERROR IN SCRIPT_DIALOG_MSGS!");
            return -1;
        }
    }

    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "script.msg");
    if (!messageListLoad(&gScrMessageList, path)) {
        debugPrint("\nError loading script message file!");
        return -1;
    }

    gScriptsEnabled = true;
    gGameModeEnabled = 1;
    gGameTime = 1;
    gameTimeSetTime(302400);
    tickersAdd(_doBkProcesses);

    if (scriptsSetDudeScript() == -1) {
        return -1;
    }

    gSpatialsEnabled = true;

    // NOTE: Uninline.
    scriptsClearPendingRequests();

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_SCRIPT, &gScrMessageList);

    return 0;
}

// 0x4A5240
int scriptsReset()
{
    debugPrint("\nScripts: [Game Reset]");
    _scr_game_exit();
    _scr_game_init();
    _partyMemberClear();
    _scr_remove_all_force();
    return scriptsSetDudeScript();
}

// 0x4A5274
int scriptsExit()
{
    gScriptsEnabled = false;
    gCritterProcessingEnabled = 0;

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_SCRIPT, nullptr);
    if (!messageListFree(&gScrMessageList)) {
        debugPrint("\nError exiting script message file!");
        return -1;
    }

    _scr_remove_all();
    _scr_remove_all_force();
    _interpretClose();
    programListFree();

    // NOTE: Uninline.
    scriptsClearPendingRequests();

    // NOTE: Uninline.
    scriptsFreeScriptsList();

    return 0;
}

// scr_message_free
// 0x4A52F4
int _scr_message_free()
{
    for (int index = 0; index < SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY; index++) {
        MessageList* messageList = &(gScriptDialogMessageLists[index]);
        if (messageList->entries_num != 0) {
            if (!messageListFree(messageList)) {
                debugPrint("\nERROR in scr_message_free!");
                return -1;
            }

            if (!messageListInit(messageList)) {
                debugPrint("\nERROR in scr_message_free!");
                return -1;
            }
        }
    }

    return 0;
}

// 0x4A535C
int _scr_game_exit()
{
    gGameModeEnabled = 0;
    gScriptsEnabled = false;
    gCritterProcessingEnabled = 0;
    _scr_message_free();
    _scr_remove_all();
    programListFree();
    tickersRemove(_doBkProcesses);
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_SCRIPT, nullptr);
    messageListFree(&gScrMessageList);
    if (scriptsClearDudeScript() == -1) {
        return -1;
    }

    // NOTE: Uninline.
    scriptsClearPendingRequests();

    return 0;
}

// scr_enable
// 0x4A53A8
int scriptsEnable()
{
    if (!gGameModeEnabled) {
        return -1;
    }

    gCritterProcessingEnabled = 1;
    gScriptsEnabled = true;
    return 0;
}

// scr_disable
// 0x4A53D0
int scriptsDisable()
{
    gScriptsEnabled = false;
    return 0;
}

// 0x4A53E0
void _scr_enable_critters()
{
    gCritterProcessingEnabled = 1;
}

// 0x4A53F0
void _scr_disable_critters()
{
    gCritterProcessingEnabled = 0;
}

// 0x4A5400
int scriptsSaveGameGlobalVars(File* stream)
{
    return fileWriteInt32List(stream, gGameGlobalVars, gGameGlobalVarsLength);
}

// 0x4A5424
int scriptsLoadGameGlobalVars(File* stream)
{
    return fileReadInt32List(stream, gGameGlobalVars, gGameGlobalVarsLength);
}

// NOTE: For unknown reason save game files contains two identical sets of game
// global variables (saved with [scriptsSaveGameGlobalVars]). The first set is
// read with [scriptsLoadGameGlobalVars], the second set is simply thrown away
// using this function.
//
// 0x4A5448
int scriptsSkipGameGlobalVars(File* stream)
{
    int* vars = (int*)internal_malloc(sizeof(*vars) * gGameGlobalVarsLength);
    if (vars == nullptr) {
        return -1;
    }

    if (fileReadInt32List(stream, vars, gGameGlobalVarsLength) == -1) {
        // FIXME: Leaks vars.
        return -1;
    }

    internal_free(vars);

    return 0;
}

// 0x4A5490
static int _scr_header_load()
{
    _num_script_indexes = 0;

    char path[COMPAT_MAX_PATH];
    _script_make_path(path);
    strcat(path, "scripts.lst");

    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    while (1) {
        int ch = fileReadChar(stream);
        if (ch == -1) {
            break;
        }

        if (ch == '\n') {
            _num_script_indexes++;
        }
    }

    _num_script_indexes++;

    fileClose(stream);

    for (int scriptType = 0; scriptType < SCRIPT_TYPE_COUNT; scriptType++) {
        ScriptList* scriptList = &(gScriptLists[scriptType]);
        scriptList->head = nullptr;
        scriptList->tail = nullptr;
        scriptList->length = 0;
        scriptList->nextScriptId = 0;
    }

    return 0;
}

// 0x4A5590
static int scriptWrite(Script* scr, File* stream)
{
    if (fileWriteInt32(stream, scr->sid) == -1) return -1;
    if (fileWriteInt32(stream, scr->field_4) == -1) return -1;

    switch (SID_TYPE(scr->sid)) {
    case SCRIPT_TYPE_SPATIAL:
        if (fileWriteInt32(stream, scr->sp.built_tile) == -1) return -1;
        if (fileWriteInt32(stream, scr->sp.radius) == -1) return -1;
        break;
    case SCRIPT_TYPE_TIMED:
        if (fileWriteInt32(stream, scr->tm.time) == -1) return -1;
        break;
    }

    if (fileWriteInt32(stream, scr->flags) == -1) return -1;
    if (fileWriteInt32(stream, scr->index) == -1) return -1;
    // NOTE: Original code writes `scr->program` pointer which is meaningless.
    if (fileWriteInt32(stream, 0) == -1) return -1;
    if (fileWriteInt32(stream, scr->ownerId) == -1) return -1;
    if (fileWriteInt32(stream, scr->localVarsOffset) == -1) return -1;
    if (fileWriteInt32(stream, scr->localVarsCount) == -1) return -1;
    if (fileWriteInt32(stream, scr->returnValue) == -1) return -1;
    if (fileWriteInt32(stream, scr->action) == -1) return -1;
    if (fileWriteInt32(stream, scr->fixedParam) == -1) return -1;
    if (fileWriteInt32(stream, scr->actionBeingUsed) == -1) return -1;
    if (fileWriteInt32(stream, scr->scriptOverrides) == -1) return -1;
    if (fileWriteInt32(stream, scr->field_48) == -1) return -1;
    if (fileWriteInt32(stream, scr->howMuch) == -1) return -1;
    if (fileWriteInt32(stream, scr->field_50) == -1) return -1;

    return 0;
}

// 0x4A5704
static int scriptListExtentWrite(ScriptListExtent* scriptExtent, File* stream)
{
    for (int index = 0; index < SCRIPT_LIST_EXTENT_SIZE; index++) {
        Script* script = &(scriptExtent->scripts[index]);
        if (scriptWrite(script, stream) != 0) {
            return -1;
        }
    }

    if (fileWriteInt32(stream, scriptExtent->length) != 0) {
        return -1;
    }

    // NOTE: Original code writes `scriptExtent->next` pointer which is meaningless.
    if (fileWriteInt32(stream, 0) != 0) {
        return -1;
    }

    return 0;
}

// 0x4A5768
int scriptSaveAll(File* stream)
{
    for (int scriptType = 0; scriptType < SCRIPT_TYPE_COUNT; scriptType++) {
        ScriptList* scriptList = &(gScriptLists[scriptType]);

        int scriptCount = scriptList->length * SCRIPT_LIST_EXTENT_SIZE;
        if (scriptList->tail != nullptr) {
            scriptCount += scriptList->tail->length - SCRIPT_LIST_EXTENT_SIZE;
        }

        ScriptListExtent* scriptExtent = scriptList->head;
        ScriptListExtent* lastScriptExtent = nullptr;
        while (scriptExtent != nullptr) {
            for (int index = 0; index < scriptExtent->length; index++) {
                Script* script = &(scriptExtent->scripts[index]);

                lastScriptExtent = scriptList->tail;
                if ((script->flags & SCRIPT_FLAG_NO_SAVE) != 0) {
                    scriptCount--;

                    int backwardsIndex = lastScriptExtent->length - 1;
                    if (lastScriptExtent == scriptExtent && backwardsIndex <= index) {
                        break;
                    }

                    while (lastScriptExtent != scriptExtent || backwardsIndex > index) {
                        Script* backwardsScript = &(lastScriptExtent->scripts[backwardsIndex]);
                        if ((backwardsScript->flags & SCRIPT_FLAG_NO_SAVE) == 0) {
                            break;
                        }

                        backwardsIndex--;

                        if (backwardsIndex < 0) {
                            ScriptListExtent* previousScriptExtent = scriptList->head;
                            while (previousScriptExtent->next != lastScriptExtent) {
                                previousScriptExtent = previousScriptExtent->next;
                            }

                            lastScriptExtent = previousScriptExtent;
                            backwardsIndex = lastScriptExtent->length - 1;
                        }
                    }

                    if (lastScriptExtent != scriptExtent || backwardsIndex > index) {
                        Script temp;
                        memcpy(&temp, script, sizeof(Script));
                        memcpy(script, &(lastScriptExtent->scripts[backwardsIndex]), sizeof(Script));
                        memcpy(&(lastScriptExtent->scripts[backwardsIndex]), &temp, sizeof(Script));

                        scriptCount++;
                    }
                }
            }
            scriptExtent = scriptExtent->next;
        }

        if (fileWriteInt32(stream, scriptCount) == -1) {
            return -1;
        }

        if (scriptCount > 0) {
            ScriptListExtent* scriptExtent = scriptList->head;
            while (scriptExtent != lastScriptExtent) {
                if (scriptListExtentWrite(scriptExtent, stream) == -1) {
                    return -1;
                }
                scriptExtent = scriptExtent->next;
            }

            if (lastScriptExtent != nullptr) {
                int index;
                for (index = 0; index < lastScriptExtent->length; index++) {
                    Script* script = &(lastScriptExtent->scripts[index]);
                    if ((script->flags & SCRIPT_FLAG_NO_SAVE) != 0) {
                        break;
                    }
                }

                if (index > 0) {
                    int length = lastScriptExtent->length;
                    lastScriptExtent->length = index;
                    if (scriptListExtentWrite(lastScriptExtent, stream) == -1) {
                        return -1;
                    }
                    lastScriptExtent->length = length;
                }
            }
        }
    }

    return 0;
}

// 0x4A5A1C
static int scriptRead(Script* scr, File* stream)
{
    int prg;

    if (fileReadInt32(stream, &(scr->sid)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->field_4)) == -1) return -1;

    switch (SID_TYPE(scr->sid)) {
    case SCRIPT_TYPE_SPATIAL:
        if (fileReadInt32(stream, &(scr->sp.built_tile)) == -1) return -1;
        if (fileReadInt32(stream, &(scr->sp.radius)) == -1) return -1;
        break;
    case SCRIPT_TYPE_TIMED:
        if (fileReadInt32(stream, &(scr->tm.time)) == -1) return -1;
        break;
    }

    if (fileReadInt32(stream, &(scr->flags)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->index)) == -1) return -1;
    if (fileReadInt32(stream, &(prg)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->ownerId)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->localVarsOffset)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->localVarsCount)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->returnValue)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->action)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->fixedParam)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->actionBeingUsed)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->scriptOverrides)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->field_48)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->howMuch)) == -1) return -1;
    if (fileReadInt32(stream, &(scr->field_50)) == -1) return -1;

    scr->program = nullptr;
    scr->owner = nullptr;
    scr->source = nullptr;
    scr->target = nullptr;

    for (int index = 0; index < SCRIPT_PROC_COUNT; index++) {
        scr->procs[index] = 0;
    }

    if (!(gMapHeader.flags & 1)) {
        scr->localVarsCount = 0;
    }

    scr->overriddenSelf = nullptr;

    return 0;
}

// 0x4A5BE8
static int scriptListExtentRead(ScriptListExtent* scriptExtent, File* stream)
{
    for (int index = 0; index < SCRIPT_LIST_EXTENT_SIZE; index++) {
        Script* scr = &(scriptExtent->scripts[index]);
        if (scriptRead(scr, stream) != 0) {
            return -1;
        }
    }

    if (fileReadInt32(stream, &(scriptExtent->length)) != 0) {
        return -1;
    }

    int next;
    if (fileReadInt32(stream, &(next)) != 0) {
        return -1;
    }

    return 0;
}

static void scriptListExtentClearRuntimeState(ScriptListExtent* scriptExtent)
{
    for (int scriptIndex = 0; scriptIndex < scriptExtent->length; scriptIndex++) {
        Script* script = &(scriptExtent->scripts[scriptIndex]);
        script->owner = nullptr;
        script->source = nullptr;
        script->target = nullptr;
        script->program = nullptr;
        script->flags &= ~SCRIPT_FLAG_LOADED;
    }
}

// 0x4A5C50
int scriptLoadAll(File* stream)
{
    for (int index = 0; index < SCRIPT_TYPE_COUNT; index++) {
        ScriptList* scriptList = &(gScriptLists[index]);

        int scriptsCount = 0;
        if (fileReadInt32(stream, &scriptsCount) == -1) {
            return -1;
        }

        if (scriptsCount != 0) {
            scriptList->length = scriptsCount / SCRIPT_LIST_EXTENT_SIZE;

            if (scriptsCount % SCRIPT_LIST_EXTENT_SIZE != 0) {
                scriptList->length++;
            }

            ScriptListExtent* extent = (ScriptListExtent*)internal_malloc(sizeof(*extent));
            scriptList->head = extent;
            scriptList->tail = extent;
            if (extent == nullptr) {
                return -1;
            }

            if (scriptListExtentRead(extent, stream) != 0) {
                return -1;
            }

            scriptListExtentClearRuntimeState(extent);

            extent->next = nullptr;

            ScriptListExtent* prevExtent = extent;
            for (int extentIndex = 1; extentIndex < scriptList->length; extentIndex++) {
                ScriptListExtent* extent = (ScriptListExtent*)internal_malloc(sizeof(*extent));
                if (extent == nullptr) {
                    return -1;
                }

                if (scriptListExtentRead(extent, stream) != 0) {
                    return -1;
                }

                scriptListExtentClearRuntimeState(extent);

                prevExtent->next = extent;
                extent->next = nullptr;
                prevExtent = extent;
            }

            scriptList->tail = prevExtent;
        } else {
            scriptList->head = nullptr;
            scriptList->tail = nullptr;
            scriptList->length = 0;
        }
    }

    return 0;
}

// scr_ptr
// 0x4A5E34
int scriptGetScript(int sid, Script** scriptPtr)
{
    *scriptPtr = nullptr;

    if (sid == -1) {
        return -1;
    }

    if (sid == 0xCCCCCCCC) {
        debugPrint("\nERROR: scr_ptr called with UN-SET id #!!!!");
        return -1;
    }

    ScriptList* scriptList = &(gScriptLists[SID_TYPE(sid)]);
    ScriptListExtent* scriptListExtent = scriptList->head;

    while (scriptListExtent != nullptr) {
        for (int index = 0; index < scriptListExtent->length; index++) {
            Script* script = &(scriptListExtent->scripts[index]);
            if (script->sid == sid) {
                *scriptPtr = script;
                return 0;
            }
        }
        scriptListExtent = scriptListExtent->next;
    }

    return -1;
}

// 0x4A5ED8
static int scriptGetNewId(int scriptType)
{
    int scriptId = gScriptLists[scriptType].nextScriptId++;
    int sidPrefix = scriptType << 24;

    while (scriptId < 32000) {
        Script* script;
        if (scriptGetScript(sidPrefix | scriptId, &script) == -1) {
            break;
        }
        scriptId++;
    }

    return scriptId;
}

// 0x4A5F28
int scriptAdd(int* sidPtr, int scriptType)
{
    ScriptList* scriptList = &(gScriptLists[scriptType]);
    ScriptListExtent* scriptListExtent = scriptList->tail;
    if (scriptList->head != nullptr) {
        // There is at least one extent available, which means tail is also set.
        if (scriptListExtent->length == SCRIPT_LIST_EXTENT_SIZE) {
            ScriptListExtent* newExtent = scriptListExtent->next = (ScriptListExtent*)internal_malloc(sizeof(*newExtent));
            if (newExtent == nullptr) {
                return -1;
            }

            newExtent->length = 0;
            newExtent->next = nullptr;

            scriptList->tail = newExtent;
            scriptList->length++;

            scriptListExtent = newExtent;
        }
    } else {
        // Script head
        scriptListExtent = (ScriptListExtent*)internal_malloc(sizeof(ScriptListExtent));
        if (scriptListExtent == nullptr) {
            return -1;
        }

        scriptListExtent->length = 0;
        scriptListExtent->next = nullptr;

        scriptList->head = scriptListExtent;
        scriptList->tail = scriptListExtent;
        scriptList->length = 1;
    }

    int sid = scriptGetNewId(scriptType) | (scriptType << 24);

    *sidPtr = sid;

    Script* scr = &(scriptListExtent->scripts[scriptListExtent->length]);
    scr->sid = sid;
    scr->sp.built_tile = -1;
    scr->sp.radius = -1;
    scr->flags = 0;
    scr->index = -1;
    scr->program = nullptr;
    scr->localVarsOffset = -1;
    scr->localVarsCount = 0;
    scr->returnValue = 0;
    scr->action = 0;
    scr->fixedParam = 0;
    scr->owner = nullptr;
    scr->source = nullptr;
    scr->target = nullptr;
    scr->actionBeingUsed = -1;
    scr->scriptOverrides = 0;
    scr->field_48 = 0;
    scr->howMuch = 0;
    scr->field_50 = 0;

    for (int index = 0; index < SCRIPT_PROC_COUNT; index++) {
        scr->procs[index] = SCRIPT_PROC_NO_PROC;
    }

    scr->overriddenSelf = nullptr;

    scriptListExtent->length++;

    return 0;
}

// scr_remove_local_vars
// 0x4A60D4
static int scriptsRemoveLocalVars(Script* script)
{
    if (script == nullptr) {
        return -1;
    }

    if (script->localVarsCount != 0) {
        int oldMapLocalVarsCount = gMapLocalVarsLength;
        if (oldMapLocalVarsCount > 0 && script->localVarsOffset >= 0) {
            gMapLocalVarsLength -= script->localVarsCount;

            if (oldMapLocalVarsCount - script->localVarsCount != script->localVarsOffset && script->localVarsOffset != -1) {
                memmove(gMapLocalVars + script->localVarsOffset,
                    gMapLocalVars + (script->localVarsOffset + script->localVarsCount),
                    sizeof(*gMapLocalVars) * (oldMapLocalVarsCount - script->localVarsCount - script->localVarsOffset));

                gMapLocalVars = (int*)internal_realloc(gMapLocalVars, sizeof(*gMapLocalVars) * gMapLocalVarsLength);
                if (gMapLocalVars == nullptr) {
                    debugPrint("\nError in mem_realloc in scr_remove_local_vars!\n");
                }

                for (int index = 0; index < SCRIPT_TYPE_COUNT; index++) {
                    ScriptList* scriptList = &(gScriptLists[index]);
                    ScriptListExtent* extent = scriptList->head;
                    while (extent != nullptr) {
                        for (int index = 0; index < extent->length; index++) {
                            Script* other = &(extent->scripts[index]);
                            if (other->localVarsOffset > script->localVarsOffset) {
                                other->localVarsOffset -= script->localVarsCount;
                            }
                        }
                        extent = extent->next;
                    }
                }
            }
        }
    }

    return 0;
}

// scr_remove
// 0x4A61D4
int scriptRemove(int sid)
{
    if (sid == -1) {
        return -1;
    }

    ScriptList* scriptList = &(gScriptLists[SID_TYPE(sid)]);

    ScriptListExtent* scriptListExtent = scriptList->head;
    int index;
    while (scriptListExtent != nullptr) {
        for (index = 0; index < scriptListExtent->length; index++) {
            Script* script = &(scriptListExtent->scripts[index]);
            if (script->sid == sid) {
                break;
            }
        }

        if (index < scriptListExtent->length) {
            break;
        }

        scriptListExtent = scriptListExtent->next;
    }

    if (scriptListExtent == nullptr) {
        return -1;
    }

    Script* script = &(scriptListExtent->scripts[index]);
    if ((script->flags & SCRIPT_FLAG_NO_SPATIAL) != 0) {
        if (script->program != nullptr) {
            script->program = nullptr;
        }
    }

    if ((script->flags & SCRIPT_FLAG_NO_REMOVE) == 0) {
        // NOTE: Uninline.
        _scripts_clear_combat_requests(script);

        if (scriptsRemoveLocalVars(script) == -1) {
            debugPrint("\nERROR Removing local vars on scr_remove!!\n");
        }

        if (queueRemoveEventsByType(script->owner, EVENT_TYPE_SCRIPT) == -1) {
            debugPrint("\nERROR Removing Timed Events on scr_remove!!\n");
        }

        if (scriptListExtent == scriptList->tail && index + 1 == scriptListExtent->length) {
            // Removing last script in tail extent
            scriptListExtent->length -= 1;

            if (scriptListExtent->length == 0) {
                scriptList->length--;
                internal_free(scriptListExtent);

                if (scriptList->length != 0) {
                    ScriptListExtent* previousTailExtent = scriptList->head;
                    while (scriptList->tail != previousTailExtent->next) {
                        previousTailExtent = previousTailExtent->next;
                    }
                    previousTailExtent->next = nullptr;
                    scriptList->tail = previousTailExtent;
                } else {
                    scriptList->head = nullptr;
                    scriptList->tail = nullptr;
                }
            }
        } else {
            // Relocate last script from tail extent into this script's slot.
            memcpy(&(scriptListExtent->scripts[index]), &(scriptList->tail->scripts[scriptList->tail->length - 1]), sizeof(Script));

            // Decrement number of scripts in tail extent.
            scriptList->tail->length -= 1;

            // Check to see if this extent became empty.
            if (scriptList->tail->length == 0) {
                scriptList->length -= 1;

                // Find previous extent that is about to become a new tail for
                // this script list.
                ScriptListExtent* prev = scriptList->head;
                while (prev->next != scriptList->tail) {
                    prev = prev->next;
                }
                prev->next = nullptr;

                internal_free(scriptList->tail);
                scriptList->tail = prev;
            }
        }
    }

    return 0;
}

// 0x4A63E0
int _scr_remove_all()
{
    queueClearByEventType(EVENT_TYPE_SCRIPT, nullptr);
    _scr_message_free();

    for (int scriptType = 0; scriptType < SCRIPT_TYPE_COUNT; scriptType++) {
        ScriptList* scriptList = &(gScriptLists[scriptType]);

        ScriptListExtent* scriptListExtent = scriptList->head;
        while (scriptListExtent != nullptr) {
            int index = 0;
            while (scriptListExtent != nullptr && index < scriptListExtent->length) {
                Script* script = &(scriptListExtent->scripts[index]);

                if ((script->flags & SCRIPT_FLAG_NO_REMOVE) != 0) {
                    index++;
                } else {
                    if (index == 0 && scriptListExtent->length == 1) {
                        scriptListExtent = scriptListExtent->next;
                        scriptRemove(script->sid);
                    } else {
                        scriptRemove(script->sid);
                    }
                }
            }

            if (scriptListExtent != nullptr) {
                scriptListExtent = scriptListExtent->next;
            }
        }
    }

    gScriptsEnumerationScriptIndex = 0;
    gScriptsEnumerationScriptListExtent = nullptr;
    gScriptsEnumerationElevation = 0;
    gMapSid = -1;

    programListFree();
    _exportClearAllVariables();

    return 0;
}

// 0x4A64A8
int _scr_remove_all_force()
{
    queueClearByEventType(EVENT_TYPE_SCRIPT, nullptr);
    _scr_message_free();

    for (int type = 0; type < SCRIPT_TYPE_COUNT; type++) {
        ScriptList* scriptList = &(gScriptLists[type]);
        ScriptListExtent* extent = scriptList->head;
        while (extent != nullptr) {
            ScriptListExtent* next = extent->next;
            internal_free(extent);
            extent = next;
        }

        scriptList->head = nullptr;
        scriptList->tail = nullptr;
        scriptList->length = 0;
    }

    gScriptsEnumerationScriptIndex = 0;
    gScriptsEnumerationScriptListExtent = nullptr;
    gScriptsEnumerationElevation = 0;
    gMapSid = -1;
    programListFree();
    _exportClearAllVariables();

    return 0;
}

// 0x4A6524
Script* scriptGetFirstSpatialScript(int elevation)
{
    gScriptsEnumerationElevation = elevation;
    gScriptsEnumerationScriptIndex = 0;
    gScriptsEnumerationScriptListExtent = gScriptLists[SCRIPT_TYPE_SPATIAL].head;

    if (gScriptsEnumerationScriptListExtent == nullptr) {
        return nullptr;
    }

    Script* script = &(gScriptsEnumerationScriptListExtent->scripts[0]);
    if ((script->flags & SCRIPT_FLAG_NO_SPATIAL) != 0 || builtTileGetElevation(script->sp.built_tile) != elevation) {
        script = scriptGetNextSpatialScript();
    }

    return script;
}

// 0x4A6564
Script* scriptGetNextSpatialScript()
{
    ScriptListExtent* scriptListExtent = gScriptsEnumerationScriptListExtent;
    int scriptIndex = gScriptsEnumerationScriptIndex;

    if (scriptListExtent == nullptr) {
        return nullptr;
    }

    for (;;) {
        scriptIndex++;

        if (scriptIndex == SCRIPT_LIST_EXTENT_SIZE) {
            scriptListExtent = scriptListExtent->next;
            scriptIndex = 0;
        } else if (scriptIndex >= scriptListExtent->length) {
            scriptListExtent = nullptr;
        }

        if (scriptListExtent == nullptr) {
            break;
        }

        Script* script = &(scriptListExtent->scripts[scriptIndex]);
        if ((script->flags & SCRIPT_FLAG_NO_SPATIAL) == 0 && builtTileGetElevation(script->sp.built_tile) == gScriptsEnumerationElevation) {
            break;
        }
    }

    Script* script;
    if (scriptListExtent != nullptr) {
        script = &(scriptListExtent->scripts[scriptIndex]);
    } else {
        script = nullptr;
    }

    gScriptsEnumerationScriptIndex = scriptIndex;
    gScriptsEnumerationScriptListExtent = scriptListExtent;

    return script;
}

// 0x4A65F0
void _scr_spatials_enable()
{
    gSpatialsEnabled = true;
}

// 0x4A6600
void _scr_spatials_disable()
{
    gSpatialsEnabled = false;
}

// 0x4A6610
bool scriptsExecSpatialProc(Object* object, int tile, int elevation)
{
    if (object == gGameMouseBouncingCursor) {
        return false;
    }

    if (object == gGameMouseHexCursor) {
        return false;
    }

    if ((object->flags & OBJECT_HIDDEN) != 0 || (object->flags & OBJECT_FLAT) != 0) {
        return false;
    }

    if (tile < 10) {
        return false;
    }

    if (!gSpatialsEnabled) {
        return false;
    }

    gSpatialsEnabled = false;

    int builtTile = builtTileCreate(tile, elevation);

    for (Script* script = scriptGetFirstSpatialScript(elevation); script != nullptr; script = scriptGetNextSpatialScript()) {
        if (builtTile == script->sp.built_tile) {
            // NOTE: Uninline.
            scriptSetObjects(script->sid, object, nullptr);
        } else {
            if (script->sp.radius == 0) {
                continue;
            }

            int distance = tileDistanceBetween(builtTileGetTile(script->sp.built_tile), tile);
            if (distance > script->sp.radius) {
                continue;
            }

            // NOTE: Uninline.
            scriptSetObjects(script->sid, object, nullptr);
        }

        scriptExecProc(script->sid, SCRIPT_PROC_SPATIAL);
    }

    gSpatialsEnabled = true;

    return true;
}

// scr_load_all_scripts
// 0x4A677C
int scriptsExecStartProc()
{
    for (int scriptListIndex = 0; scriptListIndex < SCRIPT_TYPE_COUNT; scriptListIndex++) {
        ScriptList* scriptList = &(gScriptLists[scriptListIndex]);
        ScriptListExtent* extent = scriptList->head;
        while (extent != nullptr) {
            for (int scriptIndex = 0; scriptIndex < extent->length; scriptIndex++) {
                Script* script = &(extent->scripts[scriptIndex]);
                scriptExecProc(script->sid, SCRIPT_PROC_START);
            }
            extent = extent->next;
        }
    }

    return 0;
}

// 0x4A67DC
void scriptsExecMapEnterProc()
{
    scriptsExecMapUpdateScripts(SCRIPT_PROC_MAP_ENTER);
}

// 0x4A67E4
void scriptsExecMapUpdateProc()
{
    scriptsExecMapUpdateScripts(SCRIPT_PROC_MAP_UPDATE);
}

// scr_exec_map_update_scripts
// 0x4A67EC
void scriptsExecMapUpdateScripts(int proc)
{
    // SFALL: Run global scripts.
    sfall_gl_scr_exec_map_update_scripts(proc);

    gSpatialsEnabled = false;

    int fixedParam = 0;
    if (proc == SCRIPT_PROC_MAP_ENTER) {
        fixedParam = (gMapHeader.flags & 1) == 0;
    } else {
        scriptExecProc(gMapSid, proc);
    }

    int sidListCapacity = 0;
    for (int scriptType = 0; scriptType < SCRIPT_TYPE_COUNT; scriptType++) {
        ScriptList* scriptList = &(gScriptLists[scriptType]);
        ScriptListExtent* scriptListExtent = scriptList->head;
        while (scriptListExtent != nullptr) {
            sidListCapacity += scriptListExtent->length;
            scriptListExtent = scriptListExtent->next;
        }
    }

    if (sidListCapacity == 0) {
        return;
    }

    int* sidList = (int*)internal_malloc(sizeof(*sidList) * sidListCapacity);
    if (sidList == nullptr) {
        debugPrint("\nError: scr_exec_map_update_scripts: Out of memory for sidList!");
        return;
    }

    int sidListLength = 0;
    for (int scriptType = 0; scriptType < SCRIPT_TYPE_COUNT; scriptType++) {
        ScriptList* scriptList = &(gScriptLists[scriptType]);
        ScriptListExtent* scriptListExtent = scriptList->head;
        while (scriptListExtent != nullptr) {
            for (int scriptIndex = 0; scriptIndex < scriptListExtent->length; scriptIndex++) {
                Script* script = &(scriptListExtent->scripts[scriptIndex]);
                if (script->sid != gMapSid && script->procs[proc] > 0) {
                    sidList[sidListLength++] = script->sid;
                }
            }
            scriptListExtent = scriptListExtent->next;
        }
    }

    if (proc == SCRIPT_PROC_MAP_ENTER) {
        for (int index = 0; index < sidListLength; index++) {
            scriptSetFixedParam(sidList[index], fixedParam);
            scriptExecProc(sidList[index], proc);
        }
    } else {
        for (int index = 0; index < sidListLength; index++) {
            scriptExecProc(sidList[index], proc);
        }
    }

    internal_free(sidList);

    gSpatialsEnabled = true;
}

// 0x4A69A0
void scriptsExecMapExitProc()
{
    scriptsExecMapUpdateScripts(SCRIPT_PROC_MAP_EXIT);
}

// 0x4A6B64
static int scriptsGetMessageList(int messageListId, MessageList** messageListPtr)
{
    if (messageListId == -1) {
        return -1;
    }

    int messageListIndex = messageListId - 1;
    MessageList* messageList = &(gScriptDialogMessageLists[messageListIndex]);
    if (messageList->entries_num == 0) {
        char scriptName[20];
        scriptName[0] = '\0';
        scriptsGetFileName(messageListIndex & 0xFFFFFF, scriptName, sizeof(scriptName));

        char* pch = strrchr(scriptName, '.');
        if (pch != nullptr) {
            *pch = '\0';
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "dialog\\%s.msg", scriptName);

        if (!messageListLoad(messageList, path)) {
            debugPrint("\nError loading script dialog message file!");
            return -1;
        }

        if (!messageListFilterBadwords(messageList)) {
            debugPrint("\nError filtering script dialog message file!");
            return -1;
        }

        // SFALL: Gender-specific words.
        int gender = critterGetStat(gDude, STAT_GENDER);
        messageListFilterGenderWords(messageList, gender);
    }

    *messageListPtr = messageList;

    return 0;
}

// 0x4A6C50
char* _scr_get_msg_str(int messageListId, int messageId)
{
    return _scr_get_msg_str_speech(messageListId, messageId, 0);
}

// message_str
// 0x4A6C5C
char* _scr_get_msg_str_speech(int messageListId, int messageId, int shouldStartSpeech)
{
    if (messageListId == 0 && messageId == 0) {
        return gEmptyString;
    }

    if (messageListId == -1 && messageId == -1) {
        return gEmptyString;
    }

    if (messageListId == -2 && messageId == -2) {
        MessageListItem messageListItem;
        return getmsg(&gProtoMessageList, &messageListItem, 650);
    }

    MessageList* messageList;
    if (scriptsGetMessageList(messageListId, &messageList) == -1) {
        debugPrint("\nERROR: message_str: can't find message file: List: %d!", messageListId);
        return nullptr;
    }

    if (FID_TYPE(gGameDialogHeadFid) != OBJ_TYPE_HEAD) {
        shouldStartSpeech = 0;
    }

    MessageListItem messageListItem;
    messageListItem.num = messageId;
    if (!messageListGetItem(messageList, &messageListItem)) {
        debugPrint("\nError: can't find message: List: %d, Num: %d!", messageListId, messageId);
        return gErrorString;
    }

    if (shouldStartSpeech) {
        if (_gdialogActive()) {
            if (messageListItem.audio != nullptr && messageListItem.audio[0] != '\0') {
                if (messageListItem.flags & 0x01) {
                    gameDialogStartLips(nullptr);
                } else {
                    gameDialogStartLips(messageListItem.audio);
                }
            } else {
                debugPrint("Missing speech name: %d\n", messageListItem.num);
            }
        }
    }

    return messageListItem.text;
}

// 0x4A6D64
int scriptGetLocalVar(int sid, int variable, ProgramValue& value)
{
    if (SID_TYPE(sid) == SCRIPT_TYPE_SYSTEM) {
        debugPrint("\nError! System scripts/Map scripts not allowed local_vars! ");

        gDebugScriptFileName[0] = '\0';
        scriptsGetFileName(sid & 0xFFFFFF, gDebugScriptFileName, sizeof(gDebugScriptFileName));

        debugPrint(":%s\n", gDebugScriptFileName);

        value.opcode = VALUE_TYPE_INT;
        value.integerValue = -1;
        return -1;
    }

    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        value.opcode = VALUE_TYPE_INT;
        value.integerValue = -1;
        return -1;
    }

    if (script->localVarsCount == 0) {
        // NOTE: Uninline.
        _scr_find_str_run_info(script->index, &(script->field_50), sid);
    }

    if (script->localVarsCount > 0) {
        if (script->localVarsOffset == -1) {
            script->localVarsOffset = mapAllocLocalVars(script->localVarsCount);
        }

        if (mapGetLocalVar(script->localVarsOffset + variable, value) == -1) {
            value.opcode = VALUE_TYPE_INT;
            value.integerValue = -1;
            return -1;
        }
    }

    return 0;
}

// 0x4A6E58
int scriptSetLocalVar(int sid, int variable, ProgramValue& value)
{
    Script* script;
    if (scriptGetScript(sid, &script) == -1) {
        return -1;
    }

    if (script->localVarsCount == 0) {
        // NOTE: Uninline.
        _scr_find_str_run_info(script->index, &(script->field_50), sid);
    }

    if (script->localVarsCount <= 0) {
        return -1;
    }

    if (script->localVarsOffset == -1) {
        script->localVarsOffset = mapAllocLocalVars(script->localVarsCount);
    }

    mapSetLocalVar(script->localVarsOffset + variable, value);

    return 0;
}

// Performs combat script and returns true if default action has been overriden
// by script.
//
// 0x4A6EFC
bool _scr_end_combat()
{
    if (gMapSid == 0 || gMapSid == -1) {
        return false;
    }

    int team = _combat_player_knocked_out_by();
    if (team == -1) {
        return false;
    }

    scriptSetFixedParam(gMapSid, team);

    scriptExecProc(gMapSid, SCRIPT_PROC_COMBAT);

    bool success = false;

    Script* after;
    if (scriptGetScript(gMapSid, &after) != -1) {
        if (after->scriptOverrides != 0) {
            success = true;
        }
    }

    return success;
}

// 0x4A6F70
int _scr_explode_scenery(Object* explosionSource, int tile, int radius, int elevation)
{
    int scriptExtentsCount = gScriptLists[SCRIPT_TYPE_SPATIAL].length + gScriptLists[SCRIPT_TYPE_ITEM].length;
    if (scriptExtentsCount == 0) {
        return 0;
    }

    int* scriptIds = (int*)internal_malloc(sizeof(*scriptIds) * scriptExtentsCount * SCRIPT_LIST_EXTENT_SIZE);
    if (scriptIds == nullptr) {
        return -1;
    }

    ScriptListExtent* extent;
    int scriptsCount = 0;

    gSpatialsEnabled = false;

    extent = gScriptLists[SCRIPT_TYPE_ITEM].head;
    while (extent != nullptr) {
        for (int index = 0; index < extent->length; index++) {
            Script* script = &(extent->scripts[index]);
            if (script->procs[SCRIPT_PROC_DAMAGE] <= 0 && script->program == nullptr) {
                scriptExecProc(script->sid, SCRIPT_PROC_START);
            }

            if (script->procs[SCRIPT_PROC_DAMAGE] > 0) {
                Object* self = script->owner;
                if (self != nullptr) {
                    if (self->elevation == elevation && tileDistanceBetween(self->tile, tile) <= radius) {
                        scriptIds[scriptsCount] = script->sid;
                        scriptsCount += 1;
                    }
                }
            }
        }
        extent = extent->next;
    }

    extent = gScriptLists[SCRIPT_TYPE_SPATIAL].head;
    while (extent != nullptr) {
        for (int index = 0; index < extent->length; index++) {
            Script* script = &(extent->scripts[index]);
            if (script->procs[SCRIPT_PROC_DAMAGE] <= 0 && script->program == nullptr) {
                scriptExecProc(script->sid, SCRIPT_PROC_START);
            }

            if (script->procs[SCRIPT_PROC_DAMAGE] > 0
                && builtTileGetElevation(script->sp.built_tile) == elevation
                && tileDistanceBetween(builtTileGetTile(script->sp.built_tile), tile) <= radius) {
                scriptIds[scriptsCount] = script->sid;
                scriptsCount += 1;
            }
        }
        extent = extent->next;
    }

    for (int index = 0; index < scriptsCount; index++) {
        int sid = scriptIds[index];

        scriptSetFixedParam(sid, 20);
        scriptSetObjects(sid, nullptr, explosionSource);
        scriptExecProc(sid, SCRIPT_PROC_DAMAGE);
    }

    // TODO: Redundant, we already know `scriptIds` is not NULL.
    if (scriptIds != nullptr) {
        internal_free(scriptIds);
    }

    gSpatialsEnabled = true;

    return 0;
}

} // namespace fallout
