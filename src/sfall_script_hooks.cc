#include "sfall_script_hooks.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "db.h"
#include "scripts.h"

namespace fallout {


struct ScriptHook {
    Program* program = nullptr;
    int procedureIndex;
};

static std::vector<ScriptHook> scriptHooks[HOOK_ADJUSTFID];

bool script_hooks_init()
{
    return true;
}

void script_hooks_reset()
{
    // TODO
}

void script_hooks_exit()
{
    // TODO
}

/*
void sfall_gl_scr_exec_start_proc()
{
    for (auto& path : state->paths) {
        Program* program = programCreateByPath(path.c_str());
        if (program != nullptr) {
            GlobalScript scr;
            scr.program = program;

            for (int action = 0; action < SCRIPT_PROC_COUNT; action++) {
                scr.procs[action] = programFindProcedure(program, gScriptProcNames[action]);
            }

            state->globalScripts.push_back(std::move(scr));

            _interpret(program, -1);
        }
    }

    tickersAdd(sfall_gl_scr_process_input);
}

static void sfall_gl_scr_process_simple(int mode1, int mode2)
{
    for (auto& scr : state->globalScripts) {
        if (scr.repeat != 0 && (scr.mode == mode1 || scr.mode == mode2)) {
            scr.count++;
            if (scr.count >= scr.repeat) {
                _executeProcedure(scr.program, scr.procs[SCRIPT_PROC_START]);
                scr.count = 0;
            }
        }
    }
}
*/

} // namespace fallout
