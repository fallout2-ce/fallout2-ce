#ifndef FALLOUT_PLATFORM_IOS_QUICK_TOOLBAR_H_
#define FALLOUT_PLATFORM_IOS_QUICK_TOOLBAR_H_

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace fallout {

#if defined(__APPLE__) && TARGET_OS_IOS

void quickToolbarInit();
void quickToolbarFree();
void quickToolbarShow();
void quickToolbarHide();
bool quickToolbarIsWindow(int windowId);

// True when the screen point falls within the toolbar window rect. Used by the
// touch dispatcher to route taps to the toolbar without going through the
// mouse-simulation pipeline (which would teleport the cursor).
bool quickToolbarContainsPoint(int x, int y);

// Invokes the action under the screen point. Returns true if a button was hit.
// Does not move the cursor and does not enqueue mouse events.
bool quickToolbarHandleTap(int x, int y);

#else

inline void quickToolbarInit() { }
inline void quickToolbarFree() { }
inline void quickToolbarShow() { }
inline void quickToolbarHide() { }
inline bool quickToolbarIsWindow(int) { return false; }
inline bool quickToolbarContainsPoint(int, int) { return false; }
inline bool quickToolbarHandleTap(int, int) { return false; }

#endif

} // namespace fallout

#endif /* FALLOUT_PLATFORM_IOS_QUICK_TOOLBAR_H_ */
