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

ScriptHookCall::ScriptHookCall(HookType hookType, int maxReturnValues, std::initializer_list<ProgramValue> args)
    : _hookType(hookType)
    , _maxRetVals(maxReturnValues)
{
    assert(hookType >= 0 && hookType < HOOK_COUNT && maxReturnValues >= 0 && maxReturnValues <= HOOKS_MAX_RETURN_VALUES && args.size() <= HOOKS_MAX_ARGUMENTS);
    for (auto arg : args) {
        _args[_numArgs++] = arg;
    }
}

void ScriptHookCall::setArgAt(int idx, ProgramValue value)
{
    assert(idx >= 0 && idx < _numArgs);
    _args[idx] = value;
}

void ScriptHookCall::addReturnValueFromScript(ProgramValue value)
{
    assert(_scriptRetVals < HOOKS_MAX_RETURN_VALUES);
    if (_scriptRetVals >= _maxRetVals)
        return;

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

/*
Runs before/after Fallout executes a standard procedure (handler) in any script
of any object. This hook will not be executed for `start`, `critter_p_proc`,
`timed_event_p_proc`, or `map_update_p_proc`.

int     arg0 - the number of the standard script handler (see *_proc in define.h)
Obj     arg1 - the object that owns this handler (self_obj)
Obj     arg2 - the object that called this handler (source_obj, can be 0)
int     arg3 - always 0 for HOOK_STDPROCEDURE, always 1 for HOOK_STDPROCEDURE_END
Obj     arg4 - the object that is acted upon by this handler (target_obj, can be 0)
int     arg5 - the parameter of this call (fixed_param), useful for combat_proc

int     ret0 - pass -1 to cancel the execution of the handler
*/
bool scriptHooks_StdProcedure(int procedureNumber, Object* self, Object* source, Object* target, int fixedParam, bool after)
{
    if (procedureNumber == SCRIPT_PROC_START
        || procedureNumber == SCRIPT_PROC_CRITTER
        || procedureNumber == SCRIPT_PROC_TIMED
        || procedureNumber == SCRIPT_PROC_MAP_UPDATE) {
        return false;
    }

    ScriptHookCall hook(after ? HOOK_STDPROCEDURE_END : HOOK_STDPROCEDURE, after ? 0 : 1,
        { procedureNumber, self, source, after ? 1 : 0, target, fixedParam });
    hook.call();

    if (after || hook.numReturnValues() <= 0) {
        return false;
    }

    return hook.getReturnValueAt(0).asInt() == -1;
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
    ScriptHookCall(HOOK_GAMEMODECHANGE, 0, { exit, previousGameMode }).call();
}

/*
Runs immediately after a critter dies for any reason.

Critter arg0 - The critter that just died
*/
void scriptHooks_OnDeath(Object* critter)
{
    ScriptHookCall(HOOK_ONDEATH, 0, { critter }).call();
}

/*
Runs before and after each turn in combat (for both PC and NPC).

int     arg0 - event type:
               1 - start of turn
               0 - normal end of turn
              -1 - combat ends abruptly (by script or by pressing Enter during PC turn)
              -2 - combat ends normally (hook always runs at the end of combat)
Critter arg1 - critter doing the turn
int     arg2 - 1 at the start/end of the player's turn after loading a game saved in combat mode, 0 otherwise

int     ret0 - pass 1 at the start of turn to skip the turn, pass -1 at the end of turn to force end of combat
*/
// returns true if turn should be skipped
bool scriptHooks_CombatTurnStart(Object* critter, bool reloadedDuringCombat)
{
    ScriptHookCall hook(HOOK_COMBATTURN, 1, { 1, critter, reloadedDuringCombat ? 1 : 0 });
    hook.call();

    return hook.numReturnValues() > 0 && hook.getReturnValueAt(0).asInt() == 1;
}

// returns true if combat should end immediately
bool scriptHooks_CombatTurnEnd(Object* critter, int turnResult, bool reloadedDuringCombat)
{
    ScriptHookCall hook(HOOK_COMBATTURN, 1, { turnResult, critter, reloadedDuringCombat ? 1 : 0 });
    hook.call();

    return hook.numReturnValues() > 0 && hook.getReturnValueAt(0).asInt() == -1;
}

void scriptHooks_CombatTurnCombatEnd(Object* critter)
{
    ScriptHookCall(HOOK_COMBATTURN, 0, { -2, critter, 0 }).call();
}

/*
Runs when a critter checks if another object is within perception range.

Critter arg0 - The watcher
Obj     arg1 - The target object
int     arg2 - The original engine result
int     arg3 - Call type:
               1 - obj_can_see_obj
               2 - obj_can_hear_obj
               3 - AI target selection
               0 - other engine checks

int     ret0 - Override the engine result:
               0 - not within perception
               1 - within perception
               2 - force detection for obj_can_see_obj
*/
PerceptionResult scriptHooks_WithinPerception(Object* watcher, Object* target, PerceptionType type, PerceptionResult result)
{
    ScriptHookCall hook(HOOK_WITHINPERCEPTION, 1, { watcher, target, result, type });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return result;
    }

    int overrideResult = hook.getReturnValueAt(0).asInt();
    switch (overrideResult) {
    case PERCEPTION_OUT_OF_RANGE:
    case PERCEPTION_IN_RANGE:
    case PERCEPTION_FORCE:
        return static_cast<PerceptionResult>(overrideResult);
    default:
        return result;
    }
}

/*
Runs whenever Fallout calculates the AP cost of using an active item in hand
(or unarmed attack). Doesn't run for moving.

Critter arg0 - The critter performing the action
int     arg1 - Attack Type / hitmode (see ATKTYPE_* constants)
int     arg2 - Is aimed attack (1 or 0)
int     arg3 - The default AP cost
Item    arg4 - The weapon for which the cost is calculated. If it is 0, the
               pointer to the weapon can still be obtained by checking item
               slot based on attack type and then calling critter_inven_obj

int     ret0 - The new AP cost
*/
int scriptHooks_CalcApCost(Object* critter, int hitMode, bool aiming, int actionPoints, Object* weapon)
{
    ScriptHookCall hook(HOOK_CALCAPCOST, 1, { critter, hitMode, aiming ? 1 : 0, actionPoints, weapon });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return actionPoints;
    }

    return hook.getReturnValueAt(0).asInt();
}

/*
Runs before moving items between inventory slots in dude interface. You can override the action.

int     arg0 - Target slot:
               0 - main backpack
               1 - left hand
               2 - right hand
               3 - armor slot
               4 - weapon, when reloading it by dropping ammo
               5 - container, like bag/backpack
               6 - dropping on the ground
               7 - picking up item
               8 - dropping item on the character portrait
Item    arg1 - Item being moved
Item    arg2 - Item being replaced, weapon being reloaded, or container being filled (can be 0)

int     ret0 - Override setting (-1 - use engine handler, any other value - prevent relocation)
*/
bool scriptHooks_InventoryMove(HookInventoryMoveType actionType, Object* item, Object* targetItem)
{
    ScriptHookCall hook(HOOK_INVENTORYMOVE, 1, { actionType, item, targetItem });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return true;
    }

    return hook.getReturnValueAt(0).asInt() == -1;
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
    ScriptHookCall hook(HOOK_TOHIT, 1,
        { hitChance,
            attacker,
            defender,
            hitLocation,
            tile,
            hitMode,
            useDistance,
            hitChanceUncapped });

    hook.call();

    if (hook.numReturnValues() <= 0) return hitChance;

    hitChance = hook.getReturnValueAt(0).asInt();
    return std::clamp(hitChance, -99, 999);
}

/*
Runs after Fallout has calculated the death animation. Lets you set your own death animation id. Performs no validation, so `art_exists` checks are advised.
When using `critter_dmg` function, this script will also run. In that case weapon pid will be -1 and attacker will point to an object with `obj_art_fid == 0x20001F5`.

Does not run for critters in the knockdown/out state.

int     arg0 - The pid of the weapon performing the attack. (-1 if the attack is unarmed)
Critter arg1 - The attacker
Critter arg2 - The target
int     arg3 - The amount of damage
int     arg4 - The death anim id calculated by Fallout

int     ret0 - The death anim id to override with
*/
void scriptHooks_DeathAnim(Object* attacker, Object* defender, Object* weapon, int damage, int* anim)
{
    ScriptHookCall hook(HOOK_DEATHANIM2, 1,
        { weapon != nullptr ? weapon->pid : -1,
            attacker,
            defender,
            damage,
            *anim });

    hook.call();

    if (hook.numReturnValues() > 0) {
        *anim = hook.getReturnValueAt(0).asInt();
    }
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
    ScriptHookCall hook(HOOK_USEOBJ, 1, { user, objUsed });
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
    ScriptHookCall hook(HOOK_USEOBJON, 1, { target, user, objUsed });
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
    ScriptHookCall hook(HOOK_COMBATDAMAGE, 5,
        {
            attack->defender,
            attack->attacker,
            attack->defenderDamage,
            attack->attackerDamage,
            attack->defenderFlags,
            attack->attackerFlags,
            attack->weapon,
            attack->defenderHitLocation,
            baseDmgMult,
            numRounds,
            attack->defenderKnockback,
            attack->hitMode,
            attack // this is how sfall did it.. TODO: make sure get/set_object_data handler is safe!
        });

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

    Obj     arg0 - the critter doing the bartering (either dude_obj or inven_dude)
    Critter arg1 - the critter being bartered with
    int     arg2 - the default value of the goods
    Obj     arg3 - table of requested goods (being bought from NPC)
    int     arg4 - the number of actual caps in the barter stack (as opposed to goods)
    int     arg5 - the value of all goods being traded before skill modifications
    Obj     arg6 - table of offered goods (being sold to NPC)
    int     arg7 - the total cost of the goods offered by the player
    int     arg8 - 1 if the "offers" button was pressed (not for a party member), 0 otherwise
    int     arg9 - 1 if trading with a party member, 0 otherwise

    int     ret0 - the modified value of all of the goods (pass -1 if you just want to modify offered goods)
    int     ret1 - the modified value of all offered goods
*/
void scriptHooks_BarterPrice(BarterPriceContext* ctx)
{
    assert(ctx != nullptr);

    ScriptHookCall hook(HOOK_BARTERPRICE, 2,
        { ctx->dude,
            ctx->npc,
            ctx->value,
            ctx->requestTable,
            ctx->caps,
            ctx->rawValue,
            ctx->offerTable,
            ctx->offerValue,
            ctx->offerButton,
            ctx->partyMember });

    hook.call();

    int* fields[] = {
        &ctx->value,
        &ctx->offerValue
    };

    const int numRets = hook.numReturnValues();
    for (int i = 0; i < numRets; i++) {
        const int valueFromScript = hook.getReturnValueAt(i).asInt();
        if (valueFromScript < 0) continue;
        *fields[i] = valueFromScript;
    }
}

/*
    HOOK_INVENWIELD

    Runs when an item is equipped or unequipped — for any critter, including
    NPC auto-equip via "Use Best Armor". This is the npc_armor mod's primary
    trigger for swapping an NPC's display sprite.

    Critter arg0 - the critter wielding/unwielding
    Item    arg1 - the item being moved
    int     arg2 - INVEN_TYPE_WORN (0), INVEN_TYPE_RIGHT_HAND (1), or
                   INVEN_TYPE_LEFT_HAND (2)
    int     arg3 - 1 on wield, 0 on unwield

    int     ret0 - non-zero allows the action; 0 vetoes it
*/
bool scriptHooks_InvenWield(Object* critter, Object* item, int slot, int isWield)
{
    ScriptHookCall hook(HOOK_INVENWIELD, 1, { critter, item, slot, isWield });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return true;
    }
    return hook.getReturnValueAt(0).asInt() != 0;
}

/*
    HOOK_CANUSEWEAPON

    Runs while the engine's AI is evaluating whether a critter can use a
    weapon. The npc_armor mod uses this to block weapons whose anim code
    isn't supported by the swapped-armor sprite set.

    Critter arg0 - the critter being evaluated
    Item    arg1 - the candidate weapon
    int     arg2 - hand slot (1 right, 2 left)

    int     ret0 - non-zero permits use; 0 forbids it
*/
bool scriptHooks_CanUseWeapon(Object* critter, Object* weapon, int slot)
{
    ScriptHookCall hook(HOOK_CANUSEWEAPON, 1, { critter, weapon, slot });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return true;
    }
    return hook.getReturnValueAt(0).asInt() != 0;
}

} // namespace fallout
