#include "gamepad_combat_assist.h"

#include <algorithm>
#include <climits>
#include <vector>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "combat.h"
#include "critter.h"
#include "display_monitor.h"
#include "game.h"
#include "interface.h"
#include "item.h"
#include "object.h"
#include "party_member.h"
#include "proto.h"
#include "proto_instance.h"
#include "stat.h"
#include "svga.h"
#include "tile.h"

namespace fallout {

int combatGamepadListTargets(Object** out, int maxCount, bool wholeMap)
{
    if (out == nullptr || maxCount <= 0 || gDude == nullptr) {
        return 0;
    }

    bool inCombat = isInCombat();

    // Engine-derived reach. In combat this is the equipped weapon's range for the
    // current attack mode; out of combat, the perception reach (same value the
    // engine uses for line-of-sight awareness).
    int maxRange;
    if (inCombat) {
        HitMode hitMode;
        bool aiming;
        if (interfaceGetCurrentHitMode(&hitMode, &aiming) == -1) {
            maxRange = 1;
        } else {
            maxRange = weaponGetRange(gDude, hitMode);
            // Melee / unarmed (reach 1-2): you close the distance on foot, so the
            // weapon's reach must NOT gate the target list — otherwise no enemy is
            // ever "in range" and melee aim finds nothing. Use the perception reach
            // so any enemy you could walk up to is targetable.
            if (maxRange <= 2) {
                maxRange = critterGetStat(gDude, STAT_PERCEPTION) * 5;
            }
        }
    } else {
        maxRange = critterGetStat(gDude, STAT_PERCEPTION) * 5;
    }
    if (maxRange < 1) {
        maxRange = 1;
    }

    // Candidate source. In combat _combat_list already holds every critter on the
    // elevation; out of combat build a fresh critter list.
    Object** list = nullptr;
    int listLen = 0;
    bool freeList = false;
    if (inCombat) {
        int total = 0;
        list = combatGetCombatList(&total, nullptr);
        listLen = total;
        if (list == nullptr) {
            return 0;
        }
    } else {
        listLen = objectListCreate(-1, gElevation, OBJ_TYPE_CRITTER, &list);
        freeList = true;
    }

    int logicalW = screenGetWidth();
    int visibleH = screenGetVisibleHeight();

    std::vector<Object*> candidates;
    candidates.reserve(64);

    for (int i = 0; i < listLen; i++) {
        Object* c = list[i];
        if (c == nullptr || c == gDude) {
            continue;
        }
        if (ProtoId(c->pid).objectType() != OBJ_TYPE_CRITTER) {
            continue;
        }
        if ((c->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            continue;
        }
        if (objectIsPartyMember(c)) {
            continue; // companion/follower — not a target
        }
        // Reachable within range (engine range vs tile distance).
        if (objectGetDistanceBetween(gDude, c) > maxRange) {
            continue;
        }
        // Clear line of sight (same test as the hostile outline / shot check).
        if (_combat_is_shot_blocked(gDude, gDude->tile, c->tile, c, nullptr)) {
            continue;
        }
        // Viewport filter: unless searching the whole map, the target's tile must
        // be on screen in the play area.
        if (!wholeMap) {
            int sx, sy;
            if (tileToScreenXY(c->tile, &sx, &sy) != 0) {
                continue;
            }
            if (sx < 0 || sx >= logicalW || sy < 0 || sy >= visibleH) {
                continue;
            }
        }
        candidates.push_back(c);
    }

    if (freeList && listLen != 0 && list != nullptr) {
        objectListFree(list);
    }

    // Sort all candidates by distance to the player (nearest first).
    std::sort(candidates.begin(), candidates.end(), [](Object* a, Object* b) {
        return objectGetDistanceBetween(gDude, a) < objectGetDistanceBetween(gDude, b);
    });

    int count = static_cast<int>(candidates.size());
    if (count > maxCount) {
        count = maxCount;
    }
    for (int i = 0; i < count; i++) {
        out[i] = candidates[i];
    }

    return count;
}

int gamepadListInteractables(Object** out, int maxCount, bool wholeMap, int maxTiles, bool skipEmpty)
{
    if (out == nullptr || maxCount <= 0 || gDude == nullptr) {
        return 0;
    }
    if (maxTiles < 1) {
        maxTiles = 1;
    }

    int logicalW = screenGetWidth();
    int visibleH = screenGetVisibleHeight();

    std::vector<Object*> candidates;
    candidates.reserve(64);

    static const ObjectType kTypes[] = { OBJ_TYPE_ITEM, OBJ_TYPE_CRITTER, OBJ_TYPE_SCENERY };
    for (int t = 0; t < 3; t++) {
        Object** list = nullptr;
        int listLen = objectListCreate(-1, gElevation, kTypes[t], &list);
        for (int i = 0; i < listLen; i++) {
            Object* o = list[i];
            if (o == nullptr || o == gDude) {
                continue;
            }
            if ((o->flags & OBJECT_HIDDEN) != 0) {
                continue;
            }

            bool keep = false;
            switch (kTypes[t]) {
            case OBJ_TYPE_ITEM:
                if (itemGetType(o) == ITEM_TYPE_CONTAINER) {
                    keep = !(skipEmpty && o->data.inventory.length == 0);
                }
                break;
            case OBJ_TYPE_CRITTER:
                if ((o->data.critter.combat.results & DAM_DEAD) != 0) {
                    keep = !(skipEmpty && o->data.inventory.length == 0);
                }
                break;
            case OBJ_TYPE_SCENERY: {
                Proto* proto;
                if (protoGetProto(ProtoId(o->pid), &proto) != -1 && proto->scenery.type == SCENERY_TYPE_DOOR) {
                    // Only closed doors are worth snapping to (open/close, lockpick);
                    // skip ones already open. openFlags bit 0 is the engine's door-open state.
                    keep = (o->data.scenery.door.openFlags & 0x01) == 0;
                }
                break;
            }
            default:
                break;
            }
            if (!keep) {
                continue;
            }

            if (objectGetDistanceBetween(gDude, o) > maxTiles) {
                continue;
            }
            if (!wholeMap) {
                int sx, sy;
                if (tileToScreenXY(o->tile, &sx, &sy) != 0) {
                    continue;
                }
                if (sx < 0 || sx >= logicalW || sy < 0 || sy >= visibleH) {
                    continue;
                }
            }
            candidates.push_back(o);
        }
        if (listLen != 0 && list != nullptr) {
            objectListFree(list);
        }
    }

    // Sort all candidates by distance to the player (nearest first).
    std::sort(candidates.begin(), candidates.end(), [](Object* a, Object* b) {
        return objectGetDistanceBetween(gDude, a) < objectGetDistanceBetween(gDude, b);
    });

    int count = static_cast<int>(candidates.size());
    if (count > maxCount) {
        count = maxCount;
    }
    for (int i = 0; i < count; i++) {
        out[i] = candidates[i];
    }

    return count;
}

bool combatGamepadCanEndCombat()
{
    int total = 0;
    int combatants = 0;
    Object** combatList = combatGetCombatList(&total, &combatants);
    if (!isInCombat() || combatList == nullptr) {
        return false;
    }

    int dudeTeam = gDude->data.critter.combat.team;
    for (int index = 0; index < combatants; index++) {
        Object* critter = combatList[index];
        if (critter == nullptr || critter == gDude) {
            continue;
        }
        if (ProtoId(critter->pid).objectType() != OBJ_TYPE_CRITTER) {
            continue;
        }
        // Dead / unconscious critters are no threat.
        if ((critter->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            continue;
        }
        // Fleeing or disengaging critters are leaving the fight.
        if ((critter->data.critter.combat.maneuver & (CRITTER_MANUEVER_FLEEING | CRITTER_MANEUVER_DISENGAGING)) != 0) {
            continue;
        }
        // Hostile? Different team, or provoked (was hit by someone on its team,
        // i.e. the player) — same condition combatAttemptEnd uses.
        int critterTeam = critter->data.critter.combat.team;
        Object* critterWhoHitMe = critter->data.critter.combat.whoHitMe;

        // whoHitMe is a raw pointer that can dangle after its target is removed
        // from the combat list mid-fight (mirrors the engine's own
        // combatClearWhoHitMeOutsideCombatList). Only dereference it when the
        // target is still an active combatant; otherwise treat it as "not provoked".
        bool whoHitMeIsLive = false;
        if (critterWhoHitMe != nullptr) {
            for (int li = 0; li < total; li++) {
                if (combatList[li] == critterWhoHitMe) {
                    whoHitMeIsLive = true;
                    break;
                }
            }
        }

        bool hostile = critterTeam != dudeTeam
            || (whoHitMeIsLive && critterWhoHitMe->data.critter.combat.team == critterTeam);
        if (hostile) {
            return false; // a living hostile still in the fight -> not safe to end
        }
    }
    return true;
}

void combatGamepadAimedAttack(Object* target)
{
    if (target == nullptr || (gCombatState & COMBAT_STATE_PLAYER_TURN) == 0) {
        return;
    }

    HitMode hitMode;
    bool aiming;
    if (interfaceGetCurrentHitMode(&hitMode, &aiming) == -1) {
        return;
    }

    // Validate as an aimed shot; on any problem fall back to normal path so its message is shown.
    if (_combat_check_bad_shot(gDude, target, hitMode, true) != COMBAT_BAD_SHOT_OK) {
        _combat_attack_this(target);
        return;
    }

    HitLocation hitLocation;
    if (combatSelectCalledShotLocation(target, &hitLocation, hitMode) != -1) {
        combatExecuteAttack(target, hitLocation, hitMode);
    }
}

void combatGamepadApproachAndAttack(Object* target, bool attackAfter)
{
    if (target == nullptr || (gCombatState & COMBAT_STATE_PLAYER_TURN) == 0) {
        return;
    }

    if (objectGetDistanceBetween(gDude, target) > 1) {
        int actionPoints = gDude->data.critter.combat.ap;
        if (actionPoints <= 0) {
            if (attackAfter) {
                _combat_attack_this(target);
            }
            return;
        }

        reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        if (actionPoints >= critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS) / 2
            && artCritterFrmIdShouldRun(FrmId(gDude->fid))) {
            animationRegisterRunToObject(gDude, target, actionPoints, 0);
        } else {
            animationRegisterMoveToObject(gDude, target, actionPoints, 0);
        }
        if (reg_anim_end() != 0) {
            return;
        }

        combatExecuteTurnRun();
    }

    if (attackAfter) {
        _combat_attack_this(target);
    }
}

} // namespace fallout
