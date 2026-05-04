#ifndef INTERPRETER_EXTRA_H
#define INTERPRETER_EXTRA_H

#include "interpreter.h"

namespace fallout {

enum class InvenSlot {
    Armor = 0, // INVEN_TYPE_WORN
    RightHand = 1, // INVEN_TYPE_RIGHT_HAND
    LeftHand = 2, // INVEN_TYPE_LEFT_HAND
};

constexpr int kInvenSlotInvCount = -2;

void _intExtraClose_();
void _initIntExtra();
void intExtraUpdate();
void intExtraRemoveProgramReferences(Program* program);

} // namespace fallout

#endif /* INTERPRETER_EXTRA_H */
