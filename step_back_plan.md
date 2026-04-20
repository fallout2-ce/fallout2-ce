# Plan: AI "Step Back and Shoot" Against Melee Enemies

## Problem Statement

Currently, ranged AI characters do not proactively retreat when facing melee enemies. When a melee enemy gets adjacent (distance 1), the AI will either:
1. Get punched without shooting (if using `_ai_move_closer` / DISTANCE_CHARGE)
2. Stand in place and get punched (if DISTANCE_STAY)
3. Only react after being hit

The desired behavior: When a ranged AI character detects that an enemy is in melee range and the enemy is a melee-type threat, the AI should step back 1-2 hexes before shooting.

## Key Files to Modify

- `src/combat_ai.cc` - Main AI combat logic
- `src/combat_ai.h` - Header with declarations (if new functions needed)

## Key Existing Functions

| Function | Line | Role |
|----------|------|------|
| `_combat_ai()` | 3068 | Main AI combat entry point |
| `_ai_try_attack()` | 2693 | Attack decision + movement logic |
| `_ai_move_away()` | 1222 | Moves away from target (up to N hexes) |
| `_ai_move_steps_closer()` | 2355 | Moves toward target |
| `_ai_danger_source()` | 1530 | Finds target |
| `_cai_perform_distance_prefs()` | 2985 | Applies DISTANCE mode |
| `objectGetDistanceBetween()` | `object.cc:2616` | Hex distance between objects |
| `weaponGetRange()` | `item.cc:1602` | Weapon range (melee = 1) |
| `_ai_best_weapon()` | 1818 | Weapon selection logic |

## Existing Distance Modes (from `src/combat_ai_defs.h`)

```cpp
typedef enum DistanceMode {
    DISTANCE_STAY_CLOSE,    // 0 - Stay within 5 hexes of party
    DISTANCE_CHARGE,        // 1 - Rush to melee range
    DISTANCE_SNIPE,         // 2 - Keep distance, already has partial kiting logic
    DISTANCE_ON_YOUR_OWN,   // 3 - Ignore distance preferences
    DISTANCE_STAY,          // 4 - Hold position
    DISTANCE_COUNT,
} DistanceMode;
```

## Existing "Kiting" Precedent

`DISTANCE_SNIPE` mode already has retreat logic (line 3014-3033):
```cpp
case DISTANCE_SNIPE:
    if (distance < 10) {
        // ... retreat if would end up too close
        _ai_move_away(a1, a2, 10);
    }
```

## Implementation Plan

### Step 1: Add helper function to detect melee threats

Create a new static helper function:

```cpp
// Returns true if the target is primarily a melee threat.
// A target is melee if their best weapon is melee/unarmed type,
// or if they have no ranged weapon and are in melee range.
static bool _aiIsMeleeThreat(Object* attacker, Object* defender);
```

Logic:
- Get the defender's best weapon via `_ai_best_weapon` or `critterGetItem2`
- Check `weaponGetAttackTypeForHitMode` - if ATTACK_TYPE_MELEE or ATTACK_TYPE_UNARMED, it's a melee threat
- Also check: if defender is within melee range (distance <= 1 or 2) and has melee-capable animation, treat as melee threat
- Return `true` if defender should be considered a melee enemy

### Step 2: Add new AI behavior in `_ai_try_attack`

In `_ai_try_attack()` (around line 2693), **before** the main attack loop:

Add a check: if the attacker has a ranged weapon available AND the defender is a melee threat AND they are in melee range (distance <= 1 or 2):

1. Step back 1-2 hexes using `_ai_move_away(attacker, defender, 2)` or similar
2. Only proceed to attack if the retreat succeeded and AP remains
3. If retreat fails (no path, blocked), still attempt attack (don't skip entirely)

Specifically, insert this logic after `_ai_switch_weapons` is called and before the attack loop:

```cpp
// After weapon selection, check if we need to step back from melee
Object* weapon = critterGetItem2(attacker);
if (weapon != nullptr && itemGetType(weapon) == ITEM_TYPE_WEAPON) {
    int attackType = weaponGetAttackTypeForHitMode(weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY);
    if (attackType == ATTACK_TYPE_RANGED && _aiIsMeleeThreat(attacker, defender)) {
        int distance = objectGetDistanceBetween(attacker, defender);
        if (distance <= 2) {
            // Step back before shooting at melee enemy
            if (_ai_move_away(attacker, defender, 2) == -1) {
                // Can't move, but still try to attack
            }
        }
    }
}
```

### Step 3: Also handle in `_combat_ai` distance preferences

In `_combat_ai()` (line 3068), in the section that handles `a2 != nullptr` and calls `_ai_try_attack`:

Before calling `_ai_try_attack`, check for melee threat and step back if needed. This handles the case where the AI hasn't selected a weapon yet.

### Step 4: Handle DISTANCE_CHARGE mode specially

For `DISTANCE_CHARGE` mode, the AI is supposed to rush to melee. However, if the AI has a ranged weapon and the enemy is melee, it should still consider stepping back. The logic should respect the AI packet's `best_weapon` preference:
- If `best_weapon` is `BEST_WEAPON_MELEE` or `BEST_WEAPON_MELEE_OVER_RANGED`, do NOT step back (they want to melee)
- If `best_weapon` is `BEST_WEAPON_RANGED`, `BEST_WEAPON_RANGED_OVER_MELEE`, or `BEST_WEAPON_NO_PREF`, consider stepping back

### Step 5: Handle action point constraints

When stepping back:
- Check that the AI has enough AP left after the retreat animation to perform an attack
- If not enough AP remains, don't retreat (just attack or disengage)
- The `_ai_move_away` function already handles AP internally, but we need to verify AP remains after

### Step 6: Edge cases to handle

1. **Multi-hex enemies**: Use `objectGetDistanceBetween` which already handles `OBJECT_MULTIHEX`
2. **Blocked paths**: If `_ai_move_away` returns -1 (can't move), still attempt attack
3. **No weapon equipped**: Only step back if ranged weapon is available or can be equipped
4. **Knocked out / dead**: Skip all logic (already handled by early returns in `_combat_ai`)
5. **Single AP remaining**: Don't waste last AP on movement if it prevents attacking

## Detailed Logic Flow for `_aiIsMeleeThreat`

```cpp
static bool _aiIsMeleeThreat(Object* attacker, Object* defender) {
    if (defender == nullptr) return false;
    
    // Get defender's equipped weapon
    Object* weapon = critterGetItem2(defender);
    
    if (weapon != nullptr && itemGetType(weapon) == ITEM_TYPE_WEAPON) {
        int attackType = weaponGetAttackTypeForHitMode(weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY);
        // Defender has a ranged/throw weapon - not a pure melee threat
        if (attackType == ATTACK_TYPE_RANGED || attackType == ATTACK_TYPE_THROW) {
            return false;
        }
        // Defender has melee/unarmed weapon - is a melee threat
        if (attackType == ATTACK_TYPE_MELEE || attackType == ATTACK_TYPE_UNARMED) {
            return true;
        }
    }
    
    // No weapon equipped - check if defender has any melee capability
    // by checking animation frames or body type
    // For simplicity, treat unarmed as melee threat
    return true;
}
```

## Integration Point in `_ai_try_attack`

Current flow in `_ai_try_attack`:
1. Line 2695: `critterSetWhoHitMe`
2. Line 2700-2703: Get weapon
3. Line 2705: `_ai_pick_hit_mode`
4. Line 2717-2721: `_combat_safety_invalidate_weapon` / `_ai_switch_weapons`
5. Line 2727+: Main attack loop

Insert melee step-back logic **after** weapon selection (after line 2721) but **before** the attack loop (before line 2727).

## Testing Considerations

1. Ranged NPC vs melee enemy (radscorpion, super mutant melee) - should step back
2. Ranged NPC vs ranged enemy - should NOT step back unnecessarily
3. Ranged NPC vs mixed enemy (some melee, some ranged) - step back only when melee enemy is target
4. DISTANCE_CHARGE mode with melee preference - should charge, not step back
5. Blocked path - should still attempt attack
6. Low AP - should not waste last AP on movement

## Alternative Approach Considered (Rejected)

Instead of modifying `_ai_try_attack`, we could modify `_cai_perform_distance_prefs`. However, this function handles distance preferences generically and the step-back-when-melee logic is attack-specific. The attack entry point is the cleaner integration point.

## Sfall Compatibility

This change should be compatible with Sfall. Sfall has `set_npc_action` and similar scripting hooks that could override this behavior if needed. The logic is purely in C++ combat AI and doesn't conflict with Sfall's script-based AI modifications.
