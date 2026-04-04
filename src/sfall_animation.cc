#include "sfall_animation.h"

#include <algorithm>
#include <stdint.h>

#include "animation.h"
#include "interpreter.h"
#include "opcode_context.h"

namespace fallout {

void op_reg_anim_combat_check(Program* program)
{
    int enable = programStackPopInteger(program);
    animationSetCombatCheck(enable > 0);
}

void op_reg_anim_destroy(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != nullptr && !animationCheckCombatMode()) {
        animationRegisterHideObjectForced(object);
    }
}

void op_reg_anim_animate_and_hide(Program* program)
{
    int delay = programStackPopInteger(program);
    int anim = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != nullptr && !animationCheckCombatMode()) {
        animationRegisterAnimateAndHide(object, anim, delay);
    }
}

void op_reg_anim_light(Program* program)
{
    int delay = programStackPopInteger(program);
    uint32_t light = static_cast<uint32_t>(programStackPopInteger(program));
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != nullptr && !animationCheckCombatMode()) {
        // sfall encodes radius in the low byte/word and optional intensity in
        // the high word, then patches the original engine light handler to
        // decode that packed value during animation playback. CE applies the
        // packed semantics here and calls native light/intensity animation
        // helpers directly instead of reproducing the downstream hook.
        int radius = light & 0xFFFF;
        int intensity = (light >> 16) & 0xFFFF;

        radius = std::clamp(radius, 0, 8);

        if (intensity != 0) {
            animationRegisterSetLightIntensity(object, radius, intensity, delay);
        } else {
            animationRegisterSetLightDistance(object, radius, delay);
        }
    }
}

void op_reg_anim_change_fid(Program* program)
{
    int delay = programStackPopInteger(program);
    int fid = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != nullptr && !animationCheckCombatMode()) {
        animationRegisterSetFid(object, fid, delay);
    }
}

void op_reg_anim_take_out(Program* program)
{
    int delay = programStackPopInteger(program);
    int holdFrame = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != nullptr && !animationCheckCombatMode()) {
        // sfall: claims `delay` is ignored.  It seems to apply, but to the sound effect only
        animationRegisterTakeOutWeapon(object, holdFrame, delay);
    }
}

void op_reg_anim_turn_towards(Program* program)
{
    int delay = programStackPopInteger(program);
    ProgramValue target = programStackPopValue(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    (void)delay;

    if (object != nullptr && !animationCheckCombatMode()) {
        int tile;
        if (target.opcode == VALUE_TYPE_PTR) {
            Object* targetObject = static_cast<Object*>(target.pointerValue);
            tile = targetObject != nullptr ? targetObject->tile : -1;
        } else {
            tile = target.integerValue;
        }

        animationRegisterRotateToTile(object, tile);
    }
}

void mf_reg_anim_animate_and_move(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    int tile = ctx.arg(1).asInt();
    int anim = ctx.arg(2).asInt();
    int delay = ctx.arg(3).asInt();

    if (object != nullptr && !animationCheckCombatMode()) {
        animationRegisterMoveToTileStraight(object, tile, object->elevation, anim, delay);
    }
}

} // namespace fallout
