#ifndef GAMEPAD_COMBAT_ASSIST_H
#define GAMEPAD_COMBAT_ASSIST_H

#include "combat.h"
#include "object.h"

namespace fallout {

// Gamepad combat and targeting assist functions.
int combatGamepadListTargets(Object** out, int maxCount, bool wholeMap);
int gamepadListInteractables(Object** out, int maxCount, bool wholeMap, int maxTiles, bool skipEmpty);
bool combatGamepadCanEndCombat();
void combatGamepadAimedAttack(Object* target);
void combatGamepadApproachAndAttack(Object* target, bool attackAfter);

} // namespace fallout

#endif /* GAMEPAD_COMBAT_ASSIST_H */
