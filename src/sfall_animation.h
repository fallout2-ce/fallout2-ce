#ifndef FALLOUT_SFALL_ANIMATION_H_
#define FALLOUT_SFALL_ANIMATION_H_

#include "interpreter.h"

namespace fallout {

class MetaruleContext;

void op_reg_anim_combat_check(Program* program);
void op_reg_anim_destroy(Program* program);
void op_reg_anim_animate_and_hide(Program* program);
void op_reg_anim_light(Program* program);
void op_reg_anim_change_fid(Program* program);
void op_reg_anim_take_out(Program* program);
void op_reg_anim_turn_towards(Program* program);
void mf_reg_anim_animate_and_move(MetaruleContext& ctx);

} // namespace fallout

#endif /* FALLOUT_SFALL_ANIMATION_H_ */
