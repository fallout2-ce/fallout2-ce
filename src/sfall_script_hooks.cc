#include "sfall_script_hooks.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "db.h"
#include "debug.h"
#include "scripts.h"

#include <assert.h>

namespace fallout {

struct ScriptHook {
    Program* program = nullptr;
    int procedureIndex = -1;
};

static std::vector<ScriptHook> scriptHooks[HOOK_COUNT];

constexpr size_t MAX_HOOK_CALL_DEPTH = 8;

std::vector<ScriptHookCall*> ScriptHookCall::_callStack;

ScriptHookCall* ScriptHookCall::current()
{
    return !_callStack.empty() ? _callStack.back() : nullptr;
}

ScriptHookCall::ScriptHookCall(HookType hookType, int maxReturnValues)
    : _hookType(hookType)
    , _maxRetVals(maxReturnValues)
{
    assert(hookType >= 0 && hookType < HOOK_COUNT && maxReturnValues >= 0 && maxReturnValues <= HOOKS_MAX_RETURN_VALUES);
}

ScriptHookCall& ScriptHookCall::addArg(ProgramValue value)
{
    assert(_numArgs < HOOKS_MAX_ARGUMENTS - 1);
    _args[_numArgs++] = value;
    return *this;
}

ScriptHookCall& ScriptHookCall::setArgAt(int idx, ProgramValue value)
{
    assert(idx >= 0 && idx < _numArgs);
    _args[idx] = value;
    return *this;
}

void ScriptHookCall::addReturnValue(ProgramValue value)
{
    assert(_numRetVals < HOOKS_MAX_RETURN_VALUES - 1);
    _retVals[_numRetVals++] = value;
}

void ScriptHookCall::setReturnValueAt(int idx, ProgramValue value)
{
    assert(idx >= 0 && idx < _numRetVals);
    _retVals[idx] = value;
}

ProgramValue ScriptHookCall::getArgAt(int idx) const
{
    assert(idx >= 0 && idx < _numArgs);
    return _args[idx];
}

ProgramValue ScriptHookCall::getReturnValueAt(int idx) const
{
    assert(idx >= 0 && idx < _numRetVals);
    return _retVals[idx];
}

void ScriptHookCall::call()
{
    if (_callStack.size() == MAX_HOOK_CALL_DEPTH) {
        debugPrint("! ERROR: Maximum Script Hook call depth reached! Last hook: %d", _hookType);
        return;
    }
    _callStack.push_back(this);

    const auto& hooksOfType = scriptHooks[_hookType];
    for (const auto& hook : hooksOfType) {
        programExecuteProcedure(hook.program, hook.procedureIndex);
    }

    assert(_callStack.back() == this);
    _callStack.pop_back();
}

ProgramValue ScriptHookCall::getNextArgFromScript()
{
    if (_scriptNextArg >= _numArgs) {
        return { 0 };
    }
    return _args[_scriptNextArg++];
}

bool scriptHooksRegister(Program* program, const HookType hookType, const int procedureIndex)
{
    assert(program != nullptr && hookType >= 0 && hookType < HOOK_COUNT && procedureIndex >= 0 && procedureIndex < program->procedureCount());

    const bool isUnregisterRequest = procedureIndex == 0;
    // Check for existing registration.
    for (auto it = scriptHooks[hookType].begin(); it != scriptHooks[hookType].end(); ++it) {
        if (it->program == program) {
            if (isUnregisterRequest) {
                scriptHooks[hookType].erase(it);
                return true; // unregister success
            }
            // Skip: no more than 1 procedure in a script for a given hook type.
            return false; // register fail
        }
    }
    if (isUnregisterRequest) {
        return false; // unregister fail
    }

    scriptHooks[hookType].emplace_back(ScriptHook { program, procedureIndex });
    return true; // register success
}

static void scriptHooksClear()
{
    for (auto& hooks : scriptHooks) {
        hooks.clear();
    }
}

bool scriptHooksInit()
{
    return true;
}

void scriptHooksReset()
{
    scriptHooksClear();
}

void scriptHooksExit()
{
    scriptHooksClear();
}

} // namespace fallout
