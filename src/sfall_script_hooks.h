#ifndef FALLOUT_SFALL_SCRIPT_HOOKS_H_
#define FALLOUT_SFALL_SCRIPT_HOOKS_H_

#include "interpreter.h"
#include "scripts.h"

namespace fallout {

// Some hooks are implemented in sfall but aren't worth porting over:
// - useless, never deployed in popular mods
// - bad for performance and/or stability
// - awkwardly implemented (need better alternatives)

typedef enum {
    // Any hit chance calculation.
    HOOK_TOHIT = 0,

    // Attack hit roll.
    HOOK_AFTERHITROLL = 1,

    // AP cost of attacks.
    HOOK_CALCAPCOST = 2,

//    HOOK_DEATHANIM1 = 3,

    // Death animation selection.
    HOOK_DEATHANIM2 = 4,

    // Damage calculation.
    HOOK_COMBATDAMAGE = 5,

    // Critter death.
    HOOK_ONDEATH = 6,

//    HOOK_FINDTARGET = 7,

    // Using object on another object, before normal script proc.
    HOOK_USEOBJON = 8,

    // Removing object from inventory.
    HOOK_REMOVEINVENOBJ = 9,

    // Barter price calculation.
    HOOK_BARTERPRICE = 10,

    // Movement AP cost calculation. For AI triggers for every hex they move.
    HOOK_MOVECOST = 11,

// obsolete:
//    HOOK_HEXMOVEBLOCKING = 12,
//    HOOK_HEXAIBLOCKING = 13,
//    HOOK_HEXSHOOTBLOCKING = 14,
//    HOOK_HEXSIGHTBLOCKING = 15,

    // Weapon min/max damage calculation.
    HOOK_ITEMDAMAGE = 16,

    // Weapon ammo cost per attack (or per round for bursts).
    HOOK_AMMOCOST = 17,

    // Using objects.
    HOOK_USEOBJ = 18,

    // Keypress/release. Originally implemented in DirectInput layer, not in the game code. Used e.g. in Party Orders addon.
    HOOK_KEYPRESS = 19,

    // Similar to KEYPRESS but for mouse buttons.
    HOOK_MOUSECLICK = 20,

    // Skill is used on object.
    HOOK_USESKILL = 21,

    // Steal is attempted on a critter.
    HOOK_STEAL = 22,

    // A check if one critter can see another (in and out of combat).
    HOOK_WITHINPERCEPTION = 23,

    // Item is moved around the player's inventory via UI.
    HOOK_INVENTORYMOVE = 24,

    // Critter is equipping/unequiping item in armor/hand slots.
    HOOK_INVENWIELD = 25,

    // Character FID calculation in UI (inventory, barter). Used for npc_armor.mod
    HOOK_ADJUSTFID = 26,

    // Before/after combat turn (global version of combat_p_proc).
    HOOK_COMBATTURN = 27,

    // Car travel on worldmap. Can change car speed and fuel consumption.
    HOOK_CARTRAVEL = 28,

    // Normal global variable is set. Allows to override value.
    HOOK_SETGLOBALVAR = 29,

    // Continuously during rest, allows to interrupt it.
    HOOK_RESTTIMER = 30,

    // Game mode is changed. Used in many mods.
    HOOK_GAMEMODECHANGE = 31,

//    HOOK_USEANIMOBJ = 32,

    // Explosive timer is set. Allows to override the time.
    HOOK_EXPLOSIVETIMER = 33,

    // Object is examined by player. Allows to override text.
    HOOK_DESCRIPTIONOBJ = 34,

    // Runs before using a skill, allows to override a critter using the skill.
    // TODO: maybe combine with USESKILL?
    HOOK_USESKILLON = 35,

//    HOOK_ONEXPLOSION = 36,
//    HOOK_SUBCOMBATDAMAGE = 37,
//    HOOK_SETLIGHTING = 38,

    // A continuous sneak check.
    HOOK_SNEAK = 39,

    // A script procedure is called.
    // Note: those two are basically the same hook with different flag argument value.
    HOOK_STDPROCEDURE = 40,
    HOOK_STDPROCEDURE_END = 41,

//    HOOK_TARGETOBJECT = 42,

    // Random encounter occurs. Override map or cancel the encounter.
    HOOK_ENCOUNTER = 43,

//    HOOK_ADJUSTPOISON = 44,
//    HOOK_ADJUSTRADS = 45,

    // Any random roll. Has various uses for advanced scripts.
    HOOK_ROLLCHECK = 46,

    // AI is comparing two weapons when selecting the best one against a specific target or in general.
    HOOK_BESTWEAPON = 47,

    // Allows to prevent PC or NPC from using a weapon.
    HOOK_CANUSEWEAPON = 48,

    // RESERVED 49..60

    // Weapon SFX name is generated.
    HOOK_BUILDSFXWEAPON = 61,

    HOOK_COUNT,
} HookType;


bool script_hooks_init();
void script_hooks_reset();
void script_hooks_exit();

} // namespace fallout

#endif /* FALLOUT_SFALL_GLOBAL_SCRIPTS_H_ */
