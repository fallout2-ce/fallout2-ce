#ifndef PIPBOY_H
#define PIPBOY_H

#include "db.h"
#include "message.h"

namespace fallout {

enum class PipboyOpenIntent : int {
    Invalid = -1,
    Unspecified = 0,
    Rest = 1,
};

enum PipboyRestDuration : int {
    PIPBOY_REST_DURATION_TEN_MINUTES,
    PIPBOY_REST_DURATION_THIRTY_MINUTES,
    PIPBOY_REST_DURATION_ONE_HOUR,
    PIPBOY_REST_DURATION_TWO_HOURS,
    PIPBOY_REST_DURATION_THREE_HOURS,
    PIPBOY_REST_DURATION_FOUR_HOURS,
    PIPBOY_REST_DURATION_FIVE_HOURS,
    PIPBOY_REST_DURATION_SIX_HOURS,
    PIPBOY_REST_DURATION_UNTIL_MORNING,
    PIPBOY_REST_DURATION_UNTIL_NOON,
    PIPBOY_REST_DURATION_UNTIL_EVENING,
    PIPBOY_REST_DURATION_UNTIL_MIDNIGHT,
    PIPBOY_REST_DURATION_UNTIL_HEALED,
    PIPBOY_REST_DURATION_UNTIL_PARTY_HEALED,
    PIPBOY_REST_DURATION_COUNT,
    PIPBOY_REST_DURATION_COUNT_WITHOUT_PARTY = PIPBOY_REST_DURATION_COUNT - 1,
    PIPBOY_REST_DURATION_DEFAULT = PIPBOY_REST_DURATION_TEN_MINUTES
};

int pipboyOpen(PipboyOpenIntent intent);
void pipboyInit();
void pipboyReset();
void pipboySetRestHealTime(int minutes);
int pipboySave(File* stream);
int pipboyLoad(File* stream);
int pipboyGetWindow();
bool pipboyIsResting();
bool pipboyRestOptionMsgsSetBase(int baseMessageId);
bool pipboyRestOptionSet(PipboyRestDuration restOption, int value);

extern MessageList gPipboyMessageList;
int pipboyMessageListInit();
void pipboyMessageListFree();

} // namespace fallout

#endif /* PIPBOY_H */
