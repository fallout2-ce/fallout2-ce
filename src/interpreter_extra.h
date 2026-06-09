#ifndef INTERPRETER_EXTRA_H
#define INTERPRETER_EXTRA_H

#include "interpreter.h"
#include "obj_types.h"

namespace fallout {

enum class InvenSlot : int {
    Armor = 0, // INVEN_TYPE_WORN
    RightHand = 1, // INVEN_TYPE_RIGHT_HAND
    LeftHand = 2, // INVEN_TYPE_LEFT_HAND
};

constexpr int kInvenSlotInvCount = -2;

int correctFidForRemovedItem(Object* critter, Object* item, int flags);

void _intExtraClose_();
void _initIntExtra();
void intExtraUpdate();
void intExtraRemoveProgramReferences(Program* program);

} // namespace fallout

#endif /* INTERPRETER_EXTRA_H */
