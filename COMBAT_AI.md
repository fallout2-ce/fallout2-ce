# Combat AI

Fallout 2 Community Edition includes an improved combat AI that makes combatants
more deliberate about targets, weapons, movement, cover, and unused action
points. It is enabled by default and configured in the `[combatai]` section of
`fallout2.cfg`.

This guide describes the behavior implemented by CE. Most choices still honor
the combatant's normal AI packet, including disposition, preferred distance,
minimum chance to hit, weapon preference, called-shot frequency, and target
preference.

## Quick configuration

Use `1` to enable a Boolean option and `0` to disable it. Restart CE after
editing `fallout2.cfg`.

```ini
[combatai]
smart_behavior=1
debug=1
combat_descriptions=0
try_to_find_targets=1
avoid_premature_flee=1
find_firing_positions=1
hiding_tactics=1
npc_night_penalty=1
ghost_perk_tweak=0
item_pick_up_fix=1
npc_push_on_move_block=1
looting_corpses=1
difficulty_mode=0
npc_run_away_mode=0
re_find_targets=0
npc_attack_who_fix=0
take_better_weapons=0
```

The defaults above are the recommended general-purpose configuration. Options
marked as requiring Smart Behavior below have no effect when
`smart_behavior=0`.

## What Smart Behavior changes

### Targets and attacks

At the beginning of an attack, the AI evaluates whether its current target can
be attacked now or after an affordable move. When `try_to_find_targets=1`, it
can choose a different perceived enemy if the current target is unreachable,
blocked, or has an unacceptable chance to hit. A target that can be attacked
immediately is preferred over one that costs movement AP.

The planning pass estimates attacks using the weapon's primary mode so that it
does not consume combat randomness. When the attack is executed, the normal AI
logic still chooses the actual primary, secondary, burst, or thrown attack
mode.

When `avoid_premature_flee=1`, a bad shot first causes the combatant to look for
another executable target. If none exists, it normally keeps trying the current
target rather than fleeing merely because the present shot is poor. It flees
when that target is focused on it and has more than twice its combat rating.

If an attack kills the target and enough AP remains, Smart Behavior searches
for another target and continues the turn. The search has hard limits and stops
if the attacker is incapacitated, no distinct target exists, or too little AP
remains.

### Weapons and ammunition

Before evaluating targets, the AI checks carried weapons and equips the best
usable choice. It considers weapon preference, expected damage, required skill,
ammunition and AP usability, target safety, and item value as a close-score
tie-breaker.

A biped with at least 3 AP that is not overloaded can also consider weapons on
the map or in permitted corpses:

- With `take_better_weapons=0`, only an adjacent weapon is considered.
- With `take_better_weapons=1`, any reachable candidate within the current
  movement budget can be considered.
- A ranged combatant will not abandon an immediately available shot unless it
  can retrieve the replacement and still afford that shot.
- An unarmed or melee combatant does not divert to a weapon when it can already
  reach and attack its target.

The AI can change to a cheaper attack mode when it cannot afford its current
mode. It avoids switching to an out-of-range melee weapon, but can switch from
an unusable weapon to unarmed combat and then approach normally. If a burst
does not have enough ammunition, it can fall back to primary fire. Reload AP is
deducted using the weapon's actual reload cost.

`item_pick_up_fix=1` prevents an unarmed or melee combatant from beginning an
end-of-turn weapon pickup when movement would leave fewer than the 3 AP needed
to take the item.

### Aimed shots

Aimed shots remain part of the normal AI through the AI packet's `called_freq`
roll. Smart Behavior makes them substantially more likely at close range for
biped and robotic combatants: if the ordinary roll fails and the target is
within 5 hexes, the combatant gets an additional 50% chance to try one.

An aimed shot is used only when:

- the weapon supports aiming;
- the combatant can afford the aimed attack;
- its Intelligence is at least 7 on Easy, 5 on Normal, or 3 on Hard; and
- the randomly selected body part still meets the AI packet's minimum chance
  to hit.

Otherwise, the attack is made against the torso. Animals retain their ordinary
called-shot behavior. CE currently chooses an eligible body part randomly; it
does not score individual body parts to find the tactically best one.

### Clear firing positions

With `find_firing_positions=1`, an AI whose shot is blocked searches nearby
reachable hexes for a clear line of fire. It reserves enough AP for the attack
and rejects positions below the AI packet's minimum chance to hit. If no firing
position is found, ordinary approach behavior is used.

### Moving into cover

With `hiding_tactics=1`, a qualifying combatant can spend its remaining AP on
cover after attacking. It searches behind nearby walls and scenery for a hex
that:

- blocks the current target's shot;
- increases separation from that target;
- is reachable;
- is not a doorway; and
- does not place the combatant in a crowd of three or more critters.

Cover is evaluated against the current target, not every enemy on the map.
Normally the acting combatant must carry a ranged weapon, the target must carry
a ranged or thrown weapon, and the target must currently have a clear shot at
the actor. Cowardly combatants may seek cover without carrying a ranged weapon.
Bipeds other than geckos are eligible; berserk and `stay` combatants are not. A
`charge` combatant only considers cover while its target is within one normal
turn's AP distance.

If the combatant was hurt during the previous turn and is low on health or
badly outmatched, it may begin moving toward useful cover that is farther away
than its remaining AP permits. A non-party combatant receives a temporary
movement-only cover allowance of 2 AP on Easy, 4 on Normal, or 6 on Hard. This
allowance is separate from `difficulty_mode` and cannot be retained after the
cover attempt.

A successful cover move ends the AI turn's movement phase, preventing ordinary
distance-preference movement from immediately walking the combatant back out of
cover.

### Movement and party spacing

With `npc_push_on_move_block=1`, a biped trying to move can ask an active
friendly blocker with AP to spend movement AP and step sideways. The AI can
clear a short chain of blockers, but stops after ten attempts or when no safe
adjacent hex exists.

Smart Behavior also changes how companions without a target follow the player:

| Distance preference | Maximum follow distance |
|---|---:|
| Stay close | 5 hexes |
| Charge | 10 hexes |
| Snipe | 12 hexes |
| On your own | 8 hexes |
| Stay | 5000 hexes |

The `stay_close` preference also enforces a five-hex party leash during combat.
For non-party actors using that preference, it limits pursuit to five hexes.

## Independent and compatibility options

### Darkness and the Ghost perk

`npc_night_penalty` applies the normal darkness accuracy penalty to NPC
attackers, including companions, when their target is standing in a dark hex:

| Value | Behavior |
|---:|---|
| `0` | Do not apply darkness penalties to NPC attacks |
| `1` | Apply them unless the target is the player or a party member |
| `2` | Apply them to all NPC attacks, including attacks against the player and party members |

The target hex is used; darkness around the attacker is irrelevant. Depending
on visible light at the target, the penalty is 10, 25, or 40 percentage points.
A weapon with Night Sight negates this penalty.

With `npc_night_penalty=0` or `1` and `ghost_perk_tweak=1`, an enemy does
receive the darkness penalty when attacking a player or party target with the
Ghost perk. The player's own attacks continue to use the game's normal darkness
rules independently of these options.

### Corpse looting

`looting_corpses` controls whether combatants may search dead critters for
usable weapons, ammunition, drugs, and miscellaneous combat items:

| Value | Behavior |
|---:|---|
| `0` | Never search corpses |
| `1` | Non-party NPCs may search corpses |
| `2` | All NPCs, including party members, may search corpses |

The corpse must be reachable, within Perception + 5 hexes, and not marked
`NoSteal`. Retrieving an item costs 3 AP. When taking ammunition, the AI can
take multiple packs needed for the equipped weapon.

### Flee thresholds

`npc_run_away_mode=1` makes non-party NPC flee thresholds scale with maximum
health. Named run-away preferences range from fleeing very early through
`never`; `never` produces a zero-HP flee threshold. If a critter has no named
run-away preference, its `min_hp` value is treated as the percentage input.

This option defaults off because AI data designed around fixed `min_hp` values
can cause NPCs to flee much earlier than intended when interpreted as
percentages. Party-member flee controls are not changed.

### Target preference compatibility

`re_find_targets=1` forces the normal candidate sorting pass when the AI selects
a target instead of immediately retaining `whoHitMe`. This can make combatants
reconsider targets more frequently, even when `try_to_find_targets=0`.

`npc_attack_who_fix=1` applies an AI packet's `AttackWho` preference to
non-party NPC target sorting. It defaults off because some mods rely on the
original non-party behavior or provide their own target-selection policy.

### Difficulty AP

With both `smart_behavior=1` and `difficulty_mode=1`, non-party NPCs receive 2
temporary AP on Normal difficulty or 4 on Hard. There is no bonus on Easy.
Unused bonus AP is removed at the end of the AI turn and is never saved as
persistent state.

## Option reference

| Option | Default | Requires Smart Behavior | Effect |
|---|---:|:---:|---|
| `smart_behavior` | `1` | — | Enables improved planning and tactics |
| `debug` | `1` | No | Enables tagged Combat AI decision logging |
| `combat_descriptions` | `0` | No | Narrates visible AI decisions: `0` off, `1` concise, `2` verbose |
| `try_to_find_targets` | `1` | Yes | Searches for a more executable target |
| `avoid_premature_flee` | `1` | Yes | Tries alternatives before fleeing from a poor shot |
| `find_firing_positions` | `1` | Yes | Searches reachable hexes for a clear shot |
| `hiding_tactics` | `1` | Yes | Uses remaining AP to seek object-backed cover |
| `npc_night_penalty` | `1` | No | NPC darkness penalties: `0` off, `1` excludes player/party targets, `2` all targets |
| `ghost_perk_tweak` | `0` | No | Lets Ghost protect player/party targets in darkness |
| `item_pick_up_fix` | `1` | Yes | Prevents unaffordable end-of-turn weapon pickups |
| `npc_push_on_move_block` | `1` | No | Lets friendly blockers spend AP to step aside |
| `looting_corpses` | `1` | No | Controls who may search corpse inventories |
| `difficulty_mode` | `0` | Yes | Grants non-party NPCs difficulty-scaled temporary AP |
| `npc_run_away_mode` | `0` | No | Converts non-party flee preferences to health scaling |
| `re_find_targets` | `0` | No | Forces broader target reconsideration each turn |
| `npc_attack_who_fix` | `0` | No | Honors non-party `AttackWho` preferences |
| `take_better_weapons` | `0` | Yes | Allows travel to reachable weapon upgrades |

## Combat descriptions

`combat_descriptions` adds lively explanations of visible AI decisions to the
combat log. It is independent of debug logging and is off by default:

```ini
[combatai]
; 0=off, 1=concise, 2=verbose
combat_descriptions=1
```

Concise mode reports the most important decisions, including target changes,
movement to cover, and continuing onto another target after a kill when enough
AP remains to attack. It normally prints no more than one line per combatant turn;
successful cover movement is always allowed through so it cannot be hidden by
an earlier decision.

Verbose mode can print up to three ordinary descriptions per combatant turn,
with a successful cover move allowed as one additional line. It also covers
movement to firing positions, aimed shots, weapon changes, item retrieval,
low-chance attacks, and allies stepping aside. Each action has several alternate
lines. The choice between alternates does not use the combat random-number
generator, so enabling descriptions does not change combat outcomes.

Descriptions appear only when the acting combatant is on the player's current
elevation and visible on screen. Failed movement and discarded planning
choices are not narrated. The translatable English source is
`text/english/game/combat_ai.msg` in `ce.dat`; another language can supply the
same relative path, with English used as the fallback.

## Combat AI logging

`[combatai]debug=1` allows the improved routines to write tagged messages such
as `[Combat AI/hiding_tactics]` and `[Combat AI/looting_corpses]`. To save those
messages, also enable file logging:

```ini
[combatai]
debug=1

[debug]
mode=log
```

Messages are written to `debug.log` in the game directory. Logging records
decisions only when the corresponding routine is reached. For example, cover
produces no cover-specific message when the actor is ineligible before the
cover search begins; silence does not necessarily mean that the option failed
to load.
