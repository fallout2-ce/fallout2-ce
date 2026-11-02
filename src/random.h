#ifndef RANDOM_H
#define RANDOM_H

#include "db.h"

namespace fallout {

enum class Roll : int {
    CriticalFailure,
    Failure,
    Success,
    CriticalSuccess,
};

constexpr inline bool rollIsValid(int roll)
{
    return roll >= static_cast<int>(Roll::CriticalFailure) && roll <= static_cast<int>(Roll::CriticalSuccess);
}

constexpr inline int rollIsFailure(Roll roll)
{
    return roll == Roll::Failure || roll == Roll::CriticalFailure;
}

constexpr inline int rollIsSuccess(Roll roll)
{
    return roll == Roll::Success || roll == Roll::CriticalSuccess;
}

void randomInit();
void randomReset();
void randomExit();
int randomSave(File* stream);
int randomLoad(File* stream);
Roll randomRoll(int difficulty, int criticalSuccessModifier, int* howMuchPtr);
int randomBetween(int min, int max);
void randomSeedPrerandom(int seed);

} // namespace fallout

#endif /* RANDOM_H */
