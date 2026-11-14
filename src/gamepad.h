#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <stdint.h>

#include "dinput.h"
#include "map_defs.h"

namespace fallout {

struct Object;

enum GamepadActionType {
    GAMEPAD_ACTION_NONE = 0,
    GAMEPAD_ACTION_ATTACK,
    GAMEPAD_ACTION_AIMED_ATTACK,
    GAMEPAD_ACTION_MELEE_APPROACH,
    GAMEPAD_ACTION_LOOT_INTERACT,
    GAMEPAD_ACTION_RELOAD,
};

struct GamepadPendingAction {
    GamepadActionType type;
    Object* target;
    int targetId;
    int targetPid;
    MapElevation targetElevation;
    int param;
};

// Open any connected SDL_GameController. Called from mouseDeviceInit().
void gamepadOpen();

// Close the active controller. Called from mouseDeviceFree().
void gamepadClose();

// True while a controller is open and gamepad support is enabled in settings.
bool gamepadIsOpen();

// True when the gamepad currently owns the cursor position (false when the host
// mouse is the last device that moved the cursor).
bool gamepadControlsCursor();

// Notify the gamepad layer of the current host mouse read so moving or clicking
// the physical mouse smoothly hands cursor control back to the mouse.
void gamepadNotifyHostMouse(int x, int y, bool relativeMode, uint32_t buttonMask);

// Keep the virtual cursor synchronized when the engine warps the cursor.
void gamepadSyncVirtualCursor(int x, int y);

// Per-frame: read the controller and drive the virtual cursor, buttons, camera,
// and aim/loot assist. Accumulates menu mouse-wheel scroll into *wheelX / *wheelY.
void gamepadPoll(int* wheelX, int* wheelY);

// Write the virtual cursor position + button state into mouseState.
void gamepadFillMouse(MouseData* mouseState);

// OR the gamepad's LMB/RMB state into mouseState->buttons when the host mouse
// owns the cursor position.
void gamepadApplyButtons(MouseData* mouseState);

// Drive the player with the left stick (continuous march) when world_left_stick
// is set to "walk". Called once per frame from mainLoop().
void gamepadWorldMove();

// Queue a gamepad action for deferred execution outside of input polling.
bool gamepadQueueAction(GamepadActionType type, Object* target, int param);

// Execute any pending gamepad actions. Called from safe points in mainLoop()
// and _combat_input().
void gamepadProcessPendingActions();

// Clear the pending action queue (e.g. on combat reset/exit or map change).
void gamepadClearPendingActions();

// Suppress button events until all currently held buttons are fully released.
void gamepadSwallowUntilRelease();

} // namespace fallout

#endif /* GAMEPAD_H */
