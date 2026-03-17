#include "sfall_script_hooks.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "db.h"
#include "scripts.h"

#include <assert.h>

namespace fallout {


struct ScriptHook {
    Program* program = nullptr;
    int procedureIndex = -1;
};

static std::vector<ScriptHook> scriptHooks[HOOK_COUNT];

std::vector<std::unique_ptr<ScriptHookCall>> ScriptHookCall::_callStack;

ScriptHookCall::Scope::Scope(HookType hookType)
{
    _callStack.emplace_back(std::make_unique<ScriptHookCall>(hookType));
    _call = _callStack.back().get();
}

ScriptHookCall::Scope::~Scope()
{
    assert(_callStack.back().get() == _call);

    _call = nullptr;
    _callStack.pop_back();
}

ScriptHookCall::ScriptHookCall(HookType hookType) : _hookType(hookType)
{
    assert(_hookType >= 0 && _hookType < HOOK_COUNT);
}

ScriptHookCall* ScriptHookCall::addArg(ProgramValue value)
{
    assert(_numArgs < MAX_HOOK_ARGS - 1);
    _args[_numArgs++] = value;
    return this;
}

ScriptHookCall* ScriptHookCall::setArgAt(int idx, ProgramValue value)
{
    assert(idx >= 0 && idx < _numArgs);
    _args[idx] = value;
    return this;
}

void ScriptHookCall::addReturn(ProgramValue value)
{
    assert(_numRetVals < MAX_HOOK_RETS - 1);
    _retVals[_numRetVals++] = value;
}

void ScriptHookCall::setReturnAt(int idx, ProgramValue value)
{
    assert(idx >= 0 && idx < _numRetVals);
    _retVals[idx] = value;
}

ProgramValue ScriptHookCall::getArgAt(int idx) const
{
    assert(idx >= 0 && idx < _numArgs);
    return _args[idx];
}

ProgramValue ScriptHookCall::getReturnAt(int idx) const
{
    assert(idx >= 0 && idx < _numRetVals);
    return _retVals[idx];
}

void ScriptHookCall::call()
{
    const auto& hooksOfType = scriptHooks[_hookType];
    for (const auto& hook : hooksOfType) {
        programExecuteProcedure(hook.program, hook.procedureIndex);
    }
}

void scriptHookRegister(Program* program, const HookType hookType, const int procedureIndex)
{
    assert(program != nullptr && hookType >= 0 && hookType < HOOK_COUNT && procedureIndex > 0);

    // TODO: unregister

    scriptHooks[hookType].emplace_back(ScriptHook{program, procedureIndex});
}

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
