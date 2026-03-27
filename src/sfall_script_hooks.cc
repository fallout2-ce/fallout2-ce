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
    assert(_numArgs < HOOKS_MAX_ARGUMENTS);
    _args[_numArgs++] = value;
    return *this;
}

ScriptHookCall& ScriptHookCall::setArgAt(int idx, ProgramValue value)
{
    assert(idx >= 0 && idx < _numArgs);
    _args[idx] = value;
    return *this;
}

void ScriptHookCall::addReturnValueFromScript(ProgramValue value)
{
    assert(_scriptRetVals < HOOKS_MAX_RETURN_VALUES);
    _retVals[_scriptRetVals++] = value;

    if (_scriptRetVals > _numRetVals) {
        _numRetVals = _scriptRetVals;
    }
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
int ScriptHookCall::numScriptReturnValues() const { return _scriptRetVals; }

void ScriptHookCall::call()
{
    if (_callStack.size() == MAX_HOOK_CALL_DEPTH) {
        debugPrint("! ERROR: Maximum Script Hook call depth reached! Last hook: %d", _hookType);
        return;
    }
    _callStack.push_back(this);

    const auto& hooksOfType = scriptHooks[_hookType];
    // Iterate in reverse order. In case current hook is unregistered inside the call, we can just continue iteration.
    for (int i = hooksOfType.size() - 1; i >= 0; --i) {
        const auto& hook = hooksOfType[i];
        _scriptArgs = 0;
        _scriptRetVals = 0;
        programExecuteProcedure(hook.program, hook.procedureIndex);
    }

    assert(_callStack.back() == this);
    _callStack.pop_back();
}

ProgramValue ScriptHookCall::getNextArgFromScript()
{
    if (_scriptArgs >= _numArgs) {
        return { 0 };
    }
    return _args[_scriptArgs++];
}

bool scriptHooksRegister(Program* program, const HookType hookType, const int procedureIndex)
{
    assert(program != nullptr && hookType >= 0 && hookType < HOOK_COUNT && procedureIndex >= 0 && procedureIndex < program->procedureCount());

    auto& hooksByType = scriptHooks[hookType];
    const bool isUnregisterRequest = procedureIndex == 0;
    // Check for existing registration.
    for (auto it = hooksByType.begin(); it != hooksByType.end(); ++it) {
        if (it->program == program) {
            if (isUnregisterRequest) {
                hooksByType.erase(it);
                return true; // unregister success
            }
            // Skip: no more than 1 procedure in a script for a given hook type.
            return false; // register fail
        }
    }
    if (isUnregisterRequest) {
        return false; // unregister fail
    }

    // Put new hooks to beginning, because we want to iterate them in reverse.
    hooksByType.emplace(hooksByType.begin(), ScriptHook { program, procedureIndex });
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

    if (hook.numReturnValues() <= 0) return hitChance;

    hitChance = hook.getReturnValueAt(0).asInt();
    return std::clamp(hitChance, -99, 999);
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
int scriptHooks_UseItem(Object* user, Object* objUsed)
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
int scriptHooks_UseItemOn(Object* user, Object* target, Object* objUsed)
{
    ScriptHookCall hook(HOOK_USEOBJON, 1);
    hook.addArg(target).addArg(user).addArg(objUsed);
    hook.call();

    if (hook.numReturnValues() <= 0)
        return -1;

    return hook.getReturnValueAt(0).asInt();
}

/*
Runs when:

- Game calculates how much damage each target will get. This includes primary target as well as all extras (explosions and bursts). This happens BEFORE the actual attack animation.
- AI decides whether it is safe to use area attack (burst, grenades), if he might hit friendlies.

Does not run for misses, or non-combat damage like dynamite explosions.

Critter arg0  - The target
Critter arg1  - The attacker
int     arg2  - The amount of damage to the target
int     arg3  - The amount of damage to the attacker
int     arg4  - The special effect flags for the target (use bwand DAM_* to check specific flags)
int     arg5  - The special effect flags for the attacker (use bwand DAM_* to check specific flags)
Item    arg6  - The weapon used in the attack
int     arg7  - The bodypart that was struck
int     arg8  - Damage Multiplier (this is divided by 2, so a value of 3 does 1.5x damage, and 8 does 4x damage. Usually it's 2; for critical hits, the value is taken from the critical table; with Silent Death perk and the corresponding attack conditions, the value will be doubled)
int     arg9 - Number of bullets actually hit the target (1 for melee attacks)
int     arg10 - The amount of knockback to the target
int     arg11 - Attack Type (see ATKTYPE_* constants)
mixed   arg12 - computed attack data (see C_ATTACK_* for offsets and use get/set_object_data functions to get/set the data)

int     ret0 - The damage to the target
int     ret1 - The damage to the attacker
int     ret2 - The special effect flags for the target
int     ret3 - The special effect flags for the attacker
int     ret4 - The amount of knockback to the target
*/
void scriptHooks_ComputeDamage(Attack* attack, int numRounds, int baseDmgMult)
{
    ScriptHookCall hook(HOOK_COMBATDAMAGE, 5);
    hook
        .addArg(attack->defender)
        .addArg(attack->attacker)
        .addArg(attack->defenderDamage)
        .addArg(attack->attackerDamage)
        .addArg(attack->defenderFlags)
        .addArg(attack->attackerFlags)
        .addArg(attack->weapon)
        .addArg(attack->defenderHitLocation)
        .addArg(baseDmgMult)
        .addArg(numRounds)
        .addArg(attack->defenderKnockback)
        .addArg(attack->hitMode)
        .addArg(attack); // this is how sfall did it.. TODO: make sure get/set_object_data handler is safe!

    hook.call();

    int* fields[] = {
        &attack->defenderDamage,
        &attack->attackerDamage,
        &attack->defenderFlags,
        &attack->attackerFlags,
        &attack->defenderKnockback
    };

    int numRets = hook.numReturnValues();
    for (int i = 0; i < numRets; i++) {
        *fields[i] = hook.getReturnValueAt(i).asInt();
    }
}

/**
Runs whenever the value of goods being purchased is calculated.

NOTE: the hook is executed twice when entering the barter screen or after transaction: the first time is for the player and the second time is for NPC.

    Critter arg0 - the critter doing the bartering (either dude_obj or inven_dude)
    Critter arg1 - the critter being bartered with
    int     arg2 - the default value of the goods
    Critter arg3 - table of requested goods (being bought from NPC)
    int     arg4 - the number of actual caps in the barter stack (as opposed to goods)
    int     arg5 - the value of all goods being traded before skill modifications
    Critter arg6 - table of offered goods (being sold to NPC)
    int     arg7 - the total cost of the goods offered by the player
    int     arg8 - 1 if the "offers" button was pressed (not for a party member), 0 otherwise
    int     arg9 - 1 if trading with a party member, 0 otherwise

    int     ret0 - the modified value of all of the goods (pass -1 if you just want to modify offered goods)
    int     ret1 - the modified value of all offered goods
*/
void scriptHooks_BarterPrice(BarterPriceContext& ctx)
{
    ScriptHookCall hook(HOOK_BARTERPRICE, 2);
    hook
        .addArg(ctx.barterer)
        .addArg(ctx.npc)
        .addArg(ctx.value)
        .addArg(ctx.requestedTable)
        .addArg(ctx.caps)
        .addArg(ctx.rawValue)
        .addArg(ctx.offerTable)
        .addArg(ctx.offerValue)
        .addArg(ctx.offerButton)
        .addArg(ctx.partyMember);

    hook.call();

    int* fields[] = {
        &ctx.value,
        &ctx.offerValue
    };

    const int numRets = hook.numReturnValues();
    for (int i = 0; i < numRets; i++) {
        const int valueFromScript = hook.getReturnValueAt(i).asInt();
        if (valueFromScript < 0) continue;
        *fields[i] = valueFromScript;
    }
}

} // namespace fallout
