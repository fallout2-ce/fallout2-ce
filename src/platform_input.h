#ifndef FALLOUT_PLATFORM_INPUT_H_
#define FALLOUT_PLATFORM_INPUT_H_

#include <SDL_scancode.h>

namespace fallout {

// Drain SDL events independently of game updates.
void dispatchPlatformEvents();
void waitForPlatformFocus();

void beginTextInput();
void endTextInput();
void enqueueScriptKey(SDL_Scancode key);

// Native setup and queue maintenance used during input lifecycle changes.
void platformInputInit();
void platformFlushKeyboard();
void platformFlushInput(bool keepReleases = false);

} // namespace fallout

#endif /* FALLOUT_PLATFORM_INPUT_H_ */
