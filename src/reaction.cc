#include "reaction.h"

#include "debug.h"
#include "scripts.h"

namespace fallout {

static constexpr int kDefaultNeutralReactionThreshold = -10;
static constexpr int kDefaultGoodReactionThreshold = 10;

static int neutralReactionThreshold = kDefaultNeutralReactionThreshold;
static int goodReactionThreshold = kDefaultGoodReactionThreshold;

// 0x4A29D0 reaction_set
int reactionSetValue(Object* critter, int value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_INT;
    programValue.integerValue = value;
    scriptSetLocalVar(critter->sid, 0, programValue);
    return 0;
}

// 0x4A29E8 reaction_to_level
NpcReaction reactionTranslateValue(int value)
{
    NpcReaction reaction;

    // Original had several redundant thresholds that all mapped to "BAD"
    if (value > goodReactionThreshold) {
        reaction = NPC_REACTION_GOOD;
    } else if (value > neutralReactionThreshold) {
        reaction = NPC_REACTION_NEUTRAL;
    } else {
        reaction = NPC_REACTION_BAD;
    }
    return reaction;
}

void reactionSetThresholds(int neutralThreshold, int goodThreshold)
{
    neutralReactionThreshold = neutralThreshold;
    goodReactionThreshold = goodThreshold;

    debugPrint("Reaction: set thresholds neutral=%d good=%d\n", neutralThreshold, goodThreshold);
}

void reactionResetThresholds()
{
    reactionSetThresholds(kDefaultNeutralReactionThreshold, kDefaultGoodReactionThreshold);
}

// 0x4A29F0
int _reaction_influence_()
{
    return 0;
}

// 0x4A2B28 reaction_get
int reactionGetValue(Object* critter)
{
    ProgramValue programValue;

    if (scriptGetLocalVar(critter->sid, 0, programValue) == -1) {
        return -1;
    }

    return programValue.integerValue;
}

} // namespace fallout
