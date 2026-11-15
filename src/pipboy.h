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

int pipboyOpen(PipboyOpenIntent intent);
void pipboyInit();
void pipboyReset();
void pipboySetRestHealTime(int minutes);
int pipboySave(File* stream);
int pipboyLoad(File* stream);
int pipboyGetWindow();
bool pipboyIsResting();
bool pipboyRestOptionMsgsSetBase(int baseMessageId);
bool pipboyRestOptionSet(int restOption, int value);

extern MessageList gPipboyMessageList;
int pipboyMessageListInit();
void pipboyMessageListFree();

} // namespace fallout

#endif /* PIPBOY_H */
