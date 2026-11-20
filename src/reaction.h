#ifndef REACTION_H
#define REACTION_H

#include "obj_types.h"

namespace fallout {

enum NpcReaction : int {
    NPC_REACTION_BAD,
    NPC_REACTION_NEUTRAL,
    NPC_REACTION_GOOD,
};

int reactionSetValue(Object* critter, int value);
NpcReaction reactionTranslateValue(int value);
void reactionSetThresholds(int neutralThreshold, int goodThreshold);
void reactionResetThresholds();
int _reaction_influence_();
int reactionGetValue(Object* critter);

} // namespace fallout

#endif /* REACTION_H */
