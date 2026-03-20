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

int ScriptHookCall::numArgs() const { return _numArgs; }
int ScriptHookCall::maxReturnValues() const { return _maxRetVals; }
int ScriptHookCall::numReturnValues() const { return _numRetVals; }

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

/*
Runs once every time when the game mode was changed, like opening/closing the inventory, character screen, pipboy, etc.

int arg0 - event type: 1 - when the player exits the game, 0 - otherwise
int arg1 - the previous game mode
*/
void scriptHooks_GameModeChange(int exit, int previousGameMode)
{
    ScriptHookCall hook(HOOK_GAMEMODECHANGE, 0);
    hook.addArg(exit).addArg(previousGameMode).call();
}

/*
Runs when Fallout is calculating the chances of an attack striking a target.
Runs after the hit chance is fully calculated normally, including applying the 95% cap.

int     arg0 - The hit chance (capped)
Critter arg1 - The attacker
Critter arg2 - The target of the attack
int     arg3 - The targeted bodypart
int     arg4 - Source tile (may differ from attacker's tile, when AI is considering potential fire position)
int     arg5 - Attack Type (see ATKTYPE_* constants)
int     arg6 - Ranged flag. 1 if the hit chance calculation takes into account the distance to the target. This does not mean the attack is a ranged attack
int     arg7 - The raw hit chance before applying the cap

int     ret0 - The new hit chance. The value is limited to the range of -99 to 999
*/
int scriptHooks_ToHit(Object* attacker, Object* defender, int tile, int hitMode, int hitLocation, int hitChance, int hitChanceUncapped, bool useDistance)
{
    ScriptHookCall hook(HOOK_TOHIT, 1);
    hook
        .addArg(hitChance)
        .addArg(attacker)
        .addArg(defender)
        .addArg(hitLocation)
        .addArg(tile)
        .addArg(hitMode)
        .addArg(useDistance)
        .addArg(hitChanceUncapped);

    hook.call();

    return hook.numReturnValues() > 0 ? hook.getReturnValueAt(0).asInt() : hitChance;
}

/*
Runs when:
- a critter uses an object from inventory which have “Use” action flag set or it’s an active flare or dynamite.
- player uses an object from main interface

This is fired before the object is used, and the relevant use_obj script procedures are run. You can disable default item behavior.

NOTE: You can’t remove and/or destroy this object during the hookscript (game will crash otherwise). To remove it, return 1.

Critter arg0 - The user
Obj     arg1 - The object used

int     ret0 - overrides hard-coded handler and selects what should happen with the item (0 - place it back, 1 - remove it, -1 - use engine handler)
*/
// TODO: there's an inconsistency with the use of rc = 2. It drops items when used from the main interface, but not from inventory context menu. This matches sfall, but should probably be improved.
int scriptHooks_UseObj(Object* user, Object* objUsed)
{
    ScriptHookCall hook(HOOK_USEOBJ, 1);
    hook.addArg(user).addArg(objUsed);
    hook.call();

    if (hook.numReturnValues() <= 0)
        return -1;

    return hook.getReturnValueAt(0).asInt();
}

/*
Runs when:
- a critter uses an object on another critter. (Or themselves)
- a critter uses an object from inventory screen AND this object does not have “Use” action flag set and it’s not active flare or explosive.
- player or AI uses any drug

This is fired before the object is used, and the relevant use_obj_on script procedures are run. You can disable default item behavior.

NOTE: You can’t remove and/or destroy this object during the hookscript (game will crash otherwise). To remove it, return 1.

Critter arg0 - The target
Critter arg1 - The user
int     arg2 - The object used

int     ret0 - overrides hard-coded handler and selects what should happen with the item (0 - place it back, 1 - remove it, -1 - use engine handler)
*/
int scriptHooks_UseObjOn(Object* user, Object* target, Object* objUsed)
{
    ScriptHookCall hook(HOOK_USEOBJON, 1);
    hook.addArg(target).addArg(user).addArg(objUsed);
    hook.call();

    if (hook.numReturnValues() <= 0)
        return -1;

    return hook.getReturnValueAt(0).asInt();
}

} // namespace fallout
