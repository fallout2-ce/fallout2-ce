#include "sfall_global_scripts.h"

#include <algorithm>
#include <cstring>
#include <list>
#include <string>
#include <vector>

#include "animation.h"
#include "db.h"
#include "input.h"
#include "platform_compat.h"
#include "scripts.h"
#include "sfall_config.h"

namespace fallout {

static constexpr int kGlobalScriptBusyFlags = PROGRAM_FLAG_FATAL_ERROR
    | PROGRAM_FLAG_CHILD_CALL
    | PROGRAM_FLAG_CHILD_SPAWN;

// sfall runs global script procs directly. CE keeps globals outside the normal
// program list, so pending callbacks are resumed here; use a large bounded burst
// to avoid stretching UI callbacks over multiple frames.
static constexpr int kGlobalScriptContinuationBurstSize = 100;

struct GlobalScript {
    Program* program = nullptr;
    int procs[SCRIPT_PROC_COUNT] = { 0 };
    int repeat = 0;
    int count = 0;
    int mode = 0;
    bool once = true;
    struct Timer {
        int id = 0;
        unsigned int dueTime = 0;
        int fixedParam = 0;
        bool isActive = true;
        bool isExecuting = false;
    };
    std::list<Timer> timers;
    int nextTimerId = 1;
    int timerProcessingDepth = 0;
};

struct GlobalScriptsState {
    std::vector<std::string> paths;
    std::vector<GlobalScript> globalScripts;
};

static GlobalScriptsState* state = nullptr;

static bool sfall_gl_scr_is_game_script(const char* fileName)
{
    for (int index = 0; index < scriptsGetListLength(); index++) {
        char gameScriptFileName[100];
        if (scriptsGetFileName(index, gameScriptFileName, sizeof(gameScriptFileName)) == -1) {
            continue;
        }

        if (compat_stricmp(fileName, gameScriptFileName) == 0) {
            return true;
        }
    }

    return false;
}

bool sfall_gl_scr_init()
{
    state = new (std::nothrow) GlobalScriptsState();
    if (state == nullptr) {
        return false;
    }

    // CE: always use "scripts\gl*.int" as global script path
    const char* scriptPath = "scripts\\gl*.int";
    const char* dir = "scripts";
    char** files;
    int filesLength = fileNameListInit(scriptPath, &files);
    if (filesLength != 0) {
        for (int index = 0; index < filesLength; index++) {
            if (sfall_gl_scr_is_game_script(files[index])) {
                continue;
            }

            char path[COMPAT_MAX_PATH];
            snprintf(path, sizeof(path), "%s\\%s", dir, files[index]);
            state->paths.push_back(std::string { path });
        }

        fileNameListFree(&files, 0);
    }

    std::sort(state->paths.begin(), state->paths.end());

    return true;
}

void sfall_gl_scr_reset()
{
    if (state != nullptr) {
        sfall_gl_scr_remove_all();
    }
}

void sfall_gl_scr_exit()
{
    if (state != nullptr) {
        sfall_gl_scr_remove_all();

        delete state;
        state = nullptr;
    }
}

void sfall_gl_scr_exec_start_proc()
{
    for (auto& path : state->paths) {
        Program* program = programCreateByPath(path.c_str());
        if (program != nullptr) {
            scriptDetachedContextRegister(program, DetachedScriptOwnerKind::GlobalScript);

            GlobalScript scr;
            scr.program = program;

            for (int action = 0; action < SCRIPT_PROC_COUNT; action++) {
                scr.procs[action] = programFindProcedure(program, gScriptProcNames[action]);
            }

            state->globalScripts.push_back(std::move(scr));

            programInterpret(program, -1);
        }
    }

    tickersAdd(sfall_gl_scr_process_input);
}

void sfall_gl_scr_remove_all()
{
    tickersRemove(sfall_gl_scr_process_input);

    for (auto& scr : state->globalScripts) {
        scriptDetachedContextUnregister(scr.program);
        programFree(scr.program);
    }

    state->globalScripts.clear();
}

static bool sfall_gl_scr_can_execute_proc(Program* program, int proc)
{
    // matches check in scriptExecProc()
    return proc != -1 && (program->flags & kGlobalScriptBusyFlags) == 0;
}

// Execute proc if it is found and not "busy".  Returns true if proc was executed
static bool sfall_gl_scr_execute_proc_if_ready(Program* program, int proc)
{
    if (!sfall_gl_scr_can_execute_proc(program, proc)) {
        return false;
    }

    programExecuteProcedure(program, proc);
    return true;
}

static void sfall_gl_scr_cleanup_timers(GlobalScript& scr)
{
    if (scr.timerProcessingDepth != 0) {
        return;
    }

    scr.timers.remove_if([](const GlobalScript::Timer& timer) {
        return !timer.isActive && !timer.isExecuting;
    });
}

static GlobalScript::Timer* sfall_gl_scr_find_timer(GlobalScript& scr, int timerId)
{
    for (auto& timer : scr.timers) {
        if (timer.id == timerId) {
            return &timer;
        }
    }

    return nullptr;
}

static GlobalScript::Timer* sfall_gl_scr_find_due_timer(GlobalScript& scr, unsigned int now)
{
    for (auto& timer : scr.timers) {
        if (!timer.isActive) {
            continue;
        }

        if (now < timer.dueTime) {
            return nullptr;
        }

        return &timer;
    }

    return nullptr;
}

static void sfall_gl_scr_execute_due_timers(GlobalScript& scr)
{
    unsigned int now = gameTimeGetTime();
    scr.timerProcessingDepth++;

    while (GlobalScript::Timer* timer = sfall_gl_scr_find_due_timer(scr, now)) {
        if (!sfall_gl_scr_can_execute_proc(scr.program, scr.procs[SCRIPT_PROC_TIMED])) {
            break;
        }

        int timerId = timer->id;
        int fixedParam = timer->fixedParam;
        timer->isActive = false;
        timer->isExecuting = true;

        scriptDetachedContextSetFixedParam(scr.program, fixedParam);
        programExecuteProcedure(scr.program, scr.procs[SCRIPT_PROC_TIMED]);
        scriptDetachedContextSetFixedParam(scr.program, 0);

        timer = sfall_gl_scr_find_timer(scr, timerId);
        if (timer != nullptr) {
            timer->isExecuting = false;
        }
    }

    scr.timerProcessingDepth--;
    sfall_gl_scr_cleanup_timers(scr);
}

void sfall_gl_scr_exec_map_update_scripts(int action)
{
    for (auto& scr : state->globalScripts) {
        if (scr.mode == 0 || scr.mode == 3) {
            sfall_gl_scr_execute_proc_if_ready(scr.program, scr.procs[action]);
        }
    }
}

static void sfall_gl_scr_process_simple(int mode1, int mode2)
{
    for (auto& scr : state->globalScripts) {
        sfall_gl_scr_execute_due_timers(scr);

        if (scr.repeat != 0 && (scr.mode == mode1 || scr.mode == mode2)) {
            scr.count++;
            if (scr.count >= scr.repeat) {
                if (sfall_gl_scr_execute_proc_if_ready(scr.program, scr.procs[SCRIPT_PROC_START])) {
                    scr.count = 0;
                } else {
                    scr.count = scr.repeat;
                }
            }
        }
    }

    // reg_anim_combat_check flips the global gRegAnimCombatCheck flag in
    // animation.cc. Resetting it only once after the whole loop means one
    // global script can disable the combat check and leak that setting into
    // later global scripts executed in the same tick. Might be better to save/restore.
    animationResetCombatCheck();
}

void sfall_gl_scr_process_main()
{
    sfall_gl_scr_process_simple(0, 3);
}

void sfall_gl_scr_process_input()
{
    sfall_gl_scr_process_simple(1, 1);
}

void sfall_gl_scr_process_worldmap()
{
    sfall_gl_scr_process_simple(2, 3);
}

static GlobalScript* sfall_gl_scr_map_program_to_scr(Program* program)
{
    auto it = std::find_if(state->globalScripts.begin(),
        state->globalScripts.end(),
        [&program](const GlobalScript& scr) {
            return scr.program == program;
        });
    return it != state->globalScripts.end() ? &(*it) : nullptr;
}

bool sfall_gl_scr_add_timer_event(Program* program, int delay, int fixedParam)
{
    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr == nullptr) {
        return false;
    }

    GlobalScript::Timer timer;
    timer.id = scr->nextTimerId++;
    if (scr->nextTimerId == 0) {
        scr->nextTimerId = 1;
    }
    timer.dueTime = gameTimeGetTime() + static_cast<unsigned int>(delay);
    timer.fixedParam = fixedParam;
    scr->timers.push_back(timer);
    scr->timers.sort([](const GlobalScript::Timer& a, const GlobalScript::Timer& b) {
        return a.dueTime < b.dueTime;
    });
    return true;
}

bool sfall_gl_scr_remove_timer_events(Program* program, int fixedParam)
{
    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr == nullptr) {
        return false;
    }

    for (auto& timer : scr->timers) {
        if (timer.fixedParam == fixedParam) {
            timer.isActive = false;
        }
    }

    sfall_gl_scr_cleanup_timers(*scr);
    return true;
}

bool sfall_gl_scr_remove_all_timer_events(Program* program)
{
    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr == nullptr) {
        return false;
    }

    for (auto& timer : scr->timers) {
        timer.isActive = false;
    }

    sfall_gl_scr_cleanup_timers(*scr);
    return true;
}

void sfall_gl_scr_set_repeat(Program* program, int frames)
{
    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        scr->repeat = frames;
    }
}

void sfall_gl_scr_set_type(Program* program, int type)
{
    if (type < 0 || type > 3) {
        return;
    }

    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        scr->mode = type;
    }
}

bool sfall_gl_scr_is_loaded(Program* program)
{
    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        if (scr->once) {
            scr->once = false;
            return true;
        }

        return false;
    }

    // Not a global script.
    return true;
}

void sfall_gl_scr_update(int burstSize)
{
    int globalScriptBurstSize = std::max(burstSize, kGlobalScriptContinuationBurstSize);
    for (auto& scr : state->globalScripts) {
        programInterpret(scr.program, globalScriptBurstSize);
        programProcessProcedureEvents(scr.program);
    }
}

} // namespace fallout
