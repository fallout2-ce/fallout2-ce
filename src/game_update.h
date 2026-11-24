#ifndef FALLOUT_GAME_UPDATE_H_
#define FALLOUT_GAME_UPDATE_H_

#include "input_event.h"

namespace fallout {

// Prepare one gameplay, modal, or mapper update. The caller handles the
// command and then lets the scheduler pace the next update and presentation.
InputEvent beginInteractiveUpdate();

// Advance the registered callbacks and sample legacy input once. Used by
// effects and combat steps that need those services between SDL dispatches.
void runBackgroundUpdate();

using UpdateCallback = void();

// Preserve the legacy blocking wait and optionally service work between sleeps.
void waitForGameTime(unsigned int durationMs, UpdateCallback* update = nullptr);

void addUpdateCallback(UpdateCallback* callback);
void removeUpdateCallback(UpdateCallback* callback);
void setUpdateCallbacksEnabled(bool enabled);
void runUpdateCallbacks();
void resetUpdateCallbacks();
// Game time captured at the start of the latest callback pass.
unsigned int lastBackgroundUpdateTime();

} // namespace fallout

#endif /* FALLOUT_GAME_UPDATE_H_ */
