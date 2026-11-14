#include "gamepad.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "actions.h"
#include "animation.h"
#include "combat.h"
#include "critter.h"
#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "game_mouse.h"
#include "gamepad_combat_assist.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "kb.h"
#include "map.h"
#include "mouse.h"
#include "object.h"
#include "platform_compat.h"
#include "settings.h"
#include "sfall_kb_helpers.h"
#include "svga.h"
#include "tile.h"

namespace fallout {

namespace {

    enum GamepadMappedAction {
        GAMEPAD_ACT_NONE = 0,
        GAMEPAD_ACT_LMB,
        GAMEPAD_ACT_RMB,
        GAMEPAD_ACT_CURSORBOOST,
        GAMEPAD_ACT_ESC,
        GAMEPAD_ACT_INVENTORY,
        GAMEPAD_ACT_CHARSHEET,
        GAMEPAD_ACT_SKILLDEX,
        GAMEPAD_ACT_PIPBOY,
        GAMEPAD_ACT_COMBAT,
        GAMEPAD_ACT_ENDCOMBAT,
        GAMEPAD_ACT_RELOAD,
        GAMEPAD_ACT_WEAPONMODE,
        GAMEPAD_ACT_SWAPHANDS,
    };

    enum GamepadStickRole {
        GAMEPAD_STICK_CAMERA = 0,
        GAMEPAD_STICK_CURSOR = 1,
        GAMEPAD_STICK_OFF = 2,
        GAMEPAD_STICK_CENTERED = 3,
        GAMEPAD_STICK_WALK = 4,
    };

    enum GamepadMeleeApproachMode {
        GAMEPAD_MELEE_OFF = 0,
        GAMEPAD_MELEE_APPROACH = 1,
        GAMEPAD_MELEE_ATTACK = 2,
    };

} // namespace

static SDL_GameController* g_gamepad = nullptr;
static float g_virtualMouseX = 320.0f;
static float g_virtualMouseY = 240.0f;
static uint32_t g_lastTicks = 0;
static bool g_prevButtons[SDL_CONTROLLER_BUTTON_MAX] = { false };
static uint32_t g_lastDpadTicks = 0;

static bool s_gamepadCursorActive = false;
static bool s_hostMouseInitialized = false;
static int s_lastHostMouseX = 0;
static int s_lastHostMouseY = 0;

static constexpr int GAMEPAD_ACTION_QUEUE_CAPACITY = 8;
static GamepadPendingAction s_actionQueue[GAMEPAD_ACTION_QUEUE_CAPACITY];
static int s_actionQueueHead = 0;
static int s_actionQueueTail = 0;
static int s_actionQueueCount = 0;
static bool s_isExecutingAction = false;

// After a queued action runs, the aim list can hold a destroyed object (e.g. a
// picked-up item that merged into a stack once its pickup animation finishes).
// Until this tick the aim list is pruned every frame instead of every ~120 ms.
static uint32_t s_aimFastPruneUntil = 0;

// Set by gamepadClearPendingActions(): the frozen aim list may reference a
// target that a queued action just destroyed/moved, so force gamepadPoll to
// drop the frozen set on its next pass.
static bool s_aimForceReset = false;

static bool s_swallowButtonsUntilRelease = false;

// Touchpad-cursor state (for controllers exposing an SDL2 touchpad such as
// DualShock 4 and DualSense).
static bool g_touchpadFingerActive = false;
static int g_touchpadClickFrames = 0;

// Per-frame results of the remappable button processing.
static bool g_padLmb = false;
static bool g_padRmb = false;
static bool g_padBoost = false;

static int gamepadParseAction(const std::string& s)
{
    const char* c = s.c_str();
    if (compat_stricmp(c, "lmb") == 0) return GAMEPAD_ACT_LMB;
    if (compat_stricmp(c, "rmb") == 0) return GAMEPAD_ACT_RMB;
    if (compat_stricmp(c, "boost") == 0) return GAMEPAD_ACT_CURSORBOOST;
    if (compat_stricmp(c, "esc") == 0) return GAMEPAD_ACT_ESC;
    if (compat_stricmp(c, "inventory") == 0) return GAMEPAD_ACT_INVENTORY;
    if (compat_stricmp(c, "charsheet") == 0) return GAMEPAD_ACT_CHARSHEET;
    if (compat_stricmp(c, "skilldex") == 0) return GAMEPAD_ACT_SKILLDEX;
    if (compat_stricmp(c, "pipboy") == 0) return GAMEPAD_ACT_PIPBOY;
    if (compat_stricmp(c, "endcombat") == 0) return GAMEPAD_ACT_ENDCOMBAT;
    if (compat_stricmp(c, "combat") == 0) return GAMEPAD_ACT_COMBAT;
    if (compat_stricmp(c, "reload") == 0) return GAMEPAD_ACT_RELOAD;
    if (compat_stricmp(c, "weaponmode") == 0) return GAMEPAD_ACT_WEAPONMODE;
    if (compat_stricmp(c, "swaphands") == 0) return GAMEPAD_ACT_SWAPHANDS;
    return GAMEPAD_ACT_NONE;
}

static int gamepadParseStickRole(const std::string& s, int fallback)
{
    const char* c = s.c_str();
    if (compat_stricmp(c, "cursor") == 0) return GAMEPAD_STICK_CURSOR;
    if (compat_stricmp(c, "centered") == 0) return GAMEPAD_STICK_CENTERED;
    if (compat_stricmp(c, "camera") == 0) return GAMEPAD_STICK_CAMERA;
    if (compat_stricmp(c, "walk") == 0) return GAMEPAD_STICK_WALK;
    if (compat_stricmp(c, "off") == 0) return GAMEPAD_STICK_OFF;
    return fallback;
}

static void gamepadClampVirtualCursor()
{
    int lw = screenGetWidth();
    int lh = screenGetHeight();
    if (lw <= 0 || lh <= 0) {
        return;
    }
    if (g_virtualMouseX < 0.0f) g_virtualMouseX = 0.0f;
    if (g_virtualMouseX > static_cast<float>(lw - 1)) g_virtualMouseX = static_cast<float>(lw - 1);
    if (g_virtualMouseY < 0.0f) g_virtualMouseY = 0.0f;
    if (g_virtualMouseY > static_cast<float>(lh - 1)) g_virtualMouseY = static_cast<float>(lh - 1);
}

static void gamepadActivateCursor()
{
    if (!s_gamepadCursorActive) {
        int curX = 0;
        int curY = 0;
        mouseGetPosition(&curX, &curY);
        g_virtualMouseX = static_cast<float>(curX);
        g_virtualMouseY = static_cast<float>(curY);
        gamepadClampVirtualCursor();
        s_gamepadCursorActive = true;
    }
}

void gamepadNotifyHostMouse(int x, int y, bool relativeMode, uint32_t buttonMask)
{
    bool hostMoved = false;
    if (relativeMode) {
        hostMoved = (x != 0 || y != 0);
    } else {
        if (!s_hostMouseInitialized) {
            s_lastHostMouseX = x;
            s_lastHostMouseY = y;
            s_hostMouseInitialized = true;
        } else if (x != s_lastHostMouseX || y != s_lastHostMouseY) {
            hostMoved = true;
            s_lastHostMouseX = x;
            s_lastHostMouseY = y;
        }
    }

    if (hostMoved || buttonMask != 0) {
        s_gamepadCursorActive = false;
        if (!relativeMode) {
            g_virtualMouseX = static_cast<float>(x);
            g_virtualMouseY = static_cast<float>(y);
            gamepadClampVirtualCursor();
        }
    }
}

void gamepadSyncVirtualCursor(int x, int y)
{
    g_virtualMouseX = static_cast<float>(x);
    g_virtualMouseY = static_cast<float>(y);
    gamepadClampVirtualCursor();
}

void gamepadClearPendingActions()
{
    s_actionQueueHead = 0;
    s_actionQueueTail = 0;
    s_actionQueueCount = 0;
    s_aimForceReset = true;
}

void gamepadSwallowUntilRelease()
{
    memset(g_prevButtons, 0, sizeof(g_prevButtons));
    s_swallowButtonsUntilRelease = true;
}

bool gamepadQueueAction(GamepadActionType type, Object* target, int param)
{
    if (s_isExecutingAction || combatIsTurnRunning()) {
        return false;
    }

    if (s_actionQueueCount >= GAMEPAD_ACTION_QUEUE_CAPACITY) {
        return false;
    }

    GamepadPendingAction& act = s_actionQueue[s_actionQueueTail];
    act.type = type;
    act.target = target;
    act.param = param;
    if (target != nullptr) {
        act.targetId = target->id;
        act.targetPid = target->pid;
        act.targetElevation = target->elevation;
    } else {
        act.targetId = -1;
        act.targetPid = -1;
        act.targetElevation = ELEVATION_INVALID;
    }

    s_actionQueueTail = (s_actionQueueTail + 1) % GAMEPAD_ACTION_QUEUE_CAPACITY;
    s_actionQueueCount++;
    return true;
}

// True when the left stick should walk the character rather than move the cursor:
// the free-roam exploration view (no UI window, not on the worldmap, not in
// combat). Combat and every menu/dialog fall back to the cursor system untouched.
static bool gamepadInWorldWalkMode()
{
    return gGameLoaded
        && gDude != nullptr
        && GameMode::getCurrentGameMode() == 0
        && !isInCombat()
        && !gameUiIsDisabled();
}

// True when the isometric map view owns the screen — in the world OR in combat,
// but not while a full-screen menu is up (dialog, inventory, pipboy, worldmap,
// save/load, ...). Used to decide the right stick pans the camera rather than
// acting as a menu scroll wheel.
static bool gamepadInMapView()
{
    if (!gGameLoaded || gDude == nullptr || gameUiIsDisabled()) {
        return false;
    }
    int mode = GameMode::getCurrentGameMode();
    mode &= ~(GameMode::kCombat | GameMode::kPlayerTurn);
    return mode == 0;
}

// True if the queued action's target is still on the current elevation's object
// lists. Compares pointer values only and NEVER dereferences act.target, so it is
// safe even if the object was freed after the action was queued (stack merge on
// pickup, script destroy, map change).
static bool gamepadQueuedTargetIsLive(const GamepadPendingAction& act)
{
    if (act.targetElevation != gElevation) {
        return false;
    }

    Object** list = nullptr;
    int len = objectListCreate(-1, gElevation, ProtoId(act.targetPid).objectType(), &list);
    bool found = false;
    for (int i = 0; i < len; i++) {
        if (list[i] == act.target) {
            found = true;
            break;
        }
    }
    if (list != nullptr) {
        objectListFree(list);
    }
    return found;
}

void gamepadProcessPendingActions()
{
    if (!settings.gamepad.enabled || s_isExecutingAction || combatIsTurnRunning()) {
        return;
    }

    while (s_actionQueueCount > 0) {
        if (gDude == nullptr
            || (gDude->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            gamepadClearPendingActions();
            return;
        }

        GamepadPendingAction act = s_actionQueue[s_actionQueueHead];
        s_actionQueueHead = (s_actionQueueHead + 1) % GAMEPAD_ACTION_QUEUE_CAPACITY;
        s_actionQueueCount--;

        if (act.type == GAMEPAD_ACTION_NONE) {
            continue;
        }

        if (act.target != nullptr) {
            if (!gamepadQueuedTargetIsLive(act)) {
                continue;
            }
            if (act.target->id != act.targetId || act.target->pid != act.targetPid) {
                continue;
            }
            if (act.target->elevation != gElevation || (act.target->flags & OBJECT_HIDDEN) != 0) {
                continue;
            }
        }

        bool inCombat = isInCombat();

        switch (act.type) {
        case GAMEPAD_ACTION_ATTACK: {
            if (!inCombat || (gCombatState & COMBAT_STATE_PLAYER_TURN) == 0) {
                continue;
            }
            if (gDude->data.critter.combat.ap <= 0 && _combat_free_move <= 0) {
                continue;
            }
            if (act.target == nullptr || ProtoId(act.target->pid).objectType() != OBJ_TYPE_CRITTER) {
                continue;
            }
            if (critterIsDead(act.target)) {
                continue;
            }

            s_isExecutingAction = true;
            s_aimFastPruneUntil = SDL_GetTicks() + 2000;
            _combat_attack_this(act.target);
            s_isExecutingAction = false;
            break;
        }

        case GAMEPAD_ACTION_AIMED_ATTACK: {
            if (!inCombat || (gCombatState & COMBAT_STATE_PLAYER_TURN) == 0) {
                continue;
            }
            if (gDude->data.critter.combat.ap <= 0 && _combat_free_move <= 0) {
                continue;
            }
            if (act.target == nullptr || ProtoId(act.target->pid).objectType() != OBJ_TYPE_CRITTER) {
                continue;
            }
            if (critterIsDead(act.target)) {
                continue;
            }

            s_isExecutingAction = true;
            s_aimFastPruneUntil = SDL_GetTicks() + 2000;
            combatGamepadAimedAttack(act.target);
            s_isExecutingAction = false;
            break;
        }

        case GAMEPAD_ACTION_MELEE_APPROACH: {
            if (!inCombat || (gCombatState & COMBAT_STATE_PLAYER_TURN) == 0) {
                continue;
            }
            if (gDude->data.critter.combat.ap <= 0 && _combat_free_move <= 0) {
                continue;
            }
            if (act.target == nullptr || ProtoId(act.target->pid).objectType() != OBJ_TYPE_CRITTER) {
                continue;
            }
            if (critterIsDead(act.target)) {
                continue;
            }

            s_isExecutingAction = true;
            s_aimFastPruneUntil = SDL_GetTicks() + 2000;
            combatGamepadApproachAndAttack(act.target, act.param != 0);
            s_isExecutingAction = false;
            break;
        }

        case GAMEPAD_ACTION_LOOT_INTERACT: {
            if (inCombat || !gamepadInWorldWalkMode()) {
                continue;
            }
            if (act.target == nullptr) {
                continue;
            }

            s_isExecutingAction = true;
            s_aimFastPruneUntil = SDL_GetTicks() + 2000;
            switch (ProtoId(act.target->pid).objectType()) {
            case OBJ_TYPE_ITEM:
                actionPickUp(gDude, act.target);
                break;
            case OBJ_TYPE_CRITTER:
                actionLootCritter(gDude, act.target);
                break;
            case OBJ_TYPE_SCENERY:
                _action_use_an_object(gDude, act.target);
                break;
            default:
                break;
            }
            s_isExecutingAction = false;
            break;
        }

        case GAMEPAD_ACTION_RELOAD: {
            if (inCombat) {
                if ((gCombatState & COMBAT_STATE_PLAYER_TURN) == 0) {
                    continue;
                }
                if (gDude->data.critter.combat.ap <= 0 && _combat_free_move <= 0) {
                    continue;
                }
            }
            s_isExecutingAction = true;
            interfaceGamepadReload();
            s_isExecutingAction = false;
            break;
        }

        default:
            break;
        }
    }
}

static int gamepadActionKey(int action)
{
    switch (action) {
    case GAMEPAD_ACT_ESC:
        return KEY_ESCAPE;
    case GAMEPAD_ACT_INVENTORY:
        return 'i';
    case GAMEPAD_ACT_CHARSHEET:
        return 'c';
    case GAMEPAD_ACT_SKILLDEX:
        return 's';
    case GAMEPAD_ACT_PIPBOY:
        return 'p';
    case GAMEPAD_ACT_WEAPONMODE:
        return 'n';
    case GAMEPAD_ACT_SWAPHANDS:
        return 'b';
    default:
        return 0;
    }
}

static void gamepadApplyButton(int action, bool held, bool* prev)
{
    switch (action) {
    case GAMEPAD_ACT_LMB:
        if (held) g_padLmb = true;
        break;
    case GAMEPAD_ACT_RMB:
        if (held) g_padRmb = true;
        break;
    case GAMEPAD_ACT_CURSORBOOST:
        if (held) g_padBoost = true;
        break;
    case GAMEPAD_ACT_NONE:
        break;
    case GAMEPAD_ACT_COMBAT:
        if (held && !*prev) {
            if (isInCombat()) {
                enqueueInputEvent(combatGamepadCanEndCombat() ? KEY_RETURN : KEY_SPACE);
            } else {
                enqueueInputEvent(static_cast<int>('a'));
            }
        }
        break;
    case GAMEPAD_ACT_ENDCOMBAT:
        if (held && !*prev && isInCombat()) {
            enqueueInputEvent(KEY_RETURN);
        }
        break;
    case GAMEPAD_ACT_RELOAD:
        if (held && !*prev) {
            gamepadQueueAction(GAMEPAD_ACTION_RELOAD, nullptr, 0);
        }
        break;
    default: {
        int key = gamepadActionKey(action);
        if (key != 0 && held && !*prev) {
            enqueueInputEvent(key);
        }
        break;
    }
    }
    *prev = held;
}

void gamepadWorldMove()
{
    if (!settings.gamepad.enabled) {
        return;
    }

    gamepadProcessPendingActions();

    static bool s_active = false;
    static uint32_t s_activeSince = 0;

    if (g_gamepad == nullptr || !gamepadInWorldWalkMode()) {
        s_active = false;
        return;
    }

    int leftRole = gamepadParseStickRole(settings.gamepad.world_left_stick, GAMEPAD_STICK_CENTERED);
    if (leftRole != GAMEPAD_STICK_WALK) {
        s_active = false;
        return;
    }

    if (g_touchpadFingerActive) {
        s_active = false;
        return;
    }

    const float AXIS_MAX = 32768.0f;
    const float DEADZONE = static_cast<float>(settings.gamepad.walk_deadzone);
    const float RUN_THRESHOLD = static_cast<float>(settings.gamepad.run_threshold);
    const int PROJECT_TILES = settings.gamepad.walk_project_tiles;
    const uint32_t SETTLE_MS = static_cast<uint32_t>(settings.gamepad.run_settle_ms);

    float lx = SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_LEFTX) / AXIS_MAX;
    float ly = SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_LEFTY) / AXIS_MAX;
    float mag = sqrtf(lx * lx + ly * ly);
    if (mag < DEADZONE) {
        s_active = false;
        return;
    }

    uint32_t now = SDL_GetTicks();
    if (!s_active) {
        s_active = true;
        s_activeSince = now;
    }

    // Smooth camera follow via engine edge-scroll (mapScroll shifts one tile and
    // redraws only the exposed strip).
    {
        int px, py, cx, cy;
        if (tileToScreenXY(gDude->tile, &px, &py) == 0
            && tileToScreenXY(gCenterTile, &cx, &cy) == 0) {
            int offx = px - cx;
            int offy = py - cy;
            const int DEADX = settings.gamepad.camera_deadzone_x;
            const int DEADY = settings.gamepad.camera_deadzone_y;
            int sdx = (offx > DEADX) ? 1 : (offx < -DEADX ? -1 : 0);
            int sdy = (offy > DEADY) ? 1 : (offy < -DEADY ? -1 : 0);
            if (sdx != 0 || sdy != 0) {
                mapScroll(sdx, sdy);
            }
        }
    }

    if (animationIsBusy(gDude)) {
        return;
    }

    if ((now - s_activeSince) < SETTLE_MS) {
        return;
    }

    bool run = mag >= RUN_THRESHOLD;

    const float kPi = 3.14159265358979f;
    float ang = atan2f(-ly, lx) * (180.0f / kPi);
    if (ang < 0.0f) {
        ang += 360.0f;
    }
    int idx = (static_cast<int>(ang / 60.0f + 0.5f)) % 6;
    static const Rotation kIdxToRotation[6] = {
        ROTATION_E, ROTATION_NE, ROTATION_NW, ROTATION_W, ROTATION_SW, ROTATION_SE
    };
    Rotation rotation = kIdxToRotation[idx];

    int destTile = tileGetTileInDirection(gDude->tile, rotation, PROJECT_TILES);

    reg_anim_begin(ANIMATION_REQUEST_RESERVED);
    if (run) {
        animationRegisterRunToTile(gDude, destTile, gDude->elevation, -1, 0);
    } else {
        animationRegisterMoveToTile(gDude, destTile, gDude->elevation, -1, 0);
    }
    reg_anim_end();
}

static void applyRadialStick(int16_t rawX, int16_t rawY, float* outX, float* outY)
{
    const float AXIS_MAX = 32768.0f;
    float x = static_cast<float>(rawX) / AXIS_MAX;
    float y = static_cast<float>(rawY) / AXIS_MAX;
    float mag = sqrtf(x * x + y * y);
    float dz = static_cast<float>(settings.gamepad.stick_deadzone);
    if (mag <= dz) {
        *outX = 0.0f;
        *outY = 0.0f;
        return;
    }
    float normMag = (mag - dz) / (1.0f - dz);
    if (normMag > 1.0f) normMag = 1.0f;
    float curveMag = powf(normMag, static_cast<float>(settings.gamepad.stick_curve));
    *outX = (x / mag) * curveMag;
    *outY = (y / mag) * curveMag;
}

bool gamepadIsOpen()
{
    return settings.gamepad.enabled && g_gamepad != nullptr;
}

bool gamepadControlsCursor()
{
    return gamepadIsOpen() && s_gamepadCursorActive;
}

void gamepadPoll(int* wheelX, int* wheelY)
{
    if (!settings.gamepad.enabled) {
        if (g_gamepad != nullptr) {
            gamepadClose();
        }
        return;
    }

    if (g_gamepad != nullptr && !SDL_GameControllerGetAttached(g_gamepad)) {
        debugPrint("Gamepad disconnected.\n");
        gamepadClose();
    }
    if (g_gamepad == nullptr) {
        static uint32_t s_lastReconnectCheck = 0;
        uint32_t now = SDL_GetTicks();
        if (now - s_lastReconnectCheck >= 500) {
            s_lastReconnectCheck = now;
            gamepadOpen();
        }
        if (g_gamepad == nullptr) {
            return;
        }
    }

    uint32_t currentTicks = SDL_GetTicks();
    if (g_lastTicks == 0) {
        g_lastTicks = currentTicks;
    }
    float dt = static_cast<float>(currentTicks - g_lastTicks) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    g_lastTicks = currentTicks;

    bool worldWalk = gamepadInWorldWalkMode();

    int16_t trigThreshold = static_cast<int16_t>(settings.gamepad.trigger_threshold);
    bool ltHeld = SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > trigThreshold;
    bool rtHeld = SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > trigThreshold;
    bool lbHeld = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;

    bool calledShotActive = combatGamepadCalledShotActive();
    bool lootAim = ltHeld && lbHeld && settings.gamepad.loot_assist && !calledShotActive
        && (worldWalk || (isInCombat() && GameMode::isInGameMode(GameMode::kPlayerTurn)));
    bool combatAim = ltHeld && settings.gamepad.combat_auto_aim && isInCombat()
        && GameMode::isInGameMode(GameMode::kPlayerTurn)
        && !calledShotActive && !lootAim;
    bool worldAim = ltHeld && settings.gamepad.world_auto_aim && worldWalk && !isInCombat() && !lootAim;
    bool aimActive = combatAim || worldAim || lootAim;

    bool actionMenu = gGameMouseActionMenuActive;
    bool lootSkillMode = lootAim && gameMouseGetMode() >= FIRST_GAME_MOUSE_MODE_SKILL;

    int currentMode = GameMode::getCurrentGameMode();
    bool menuOpen = gGameLoaded
        && (currentMode & ~(GameMode::kCombat | GameMode::kPlayerTurn)) != 0;
    int kCursorCycleModes = GameMode::kInventory | GameMode::kBarter | GameMode::kLoot;
    bool inCursorWindow = (currentMode & kCursorCycleModes) != 0;
    bool bButtonCloses = calledShotActive
        || (menuOpen && !(inCursorWindow && !settings.gamepad.b_close_all));

    g_padLmb = false;
    g_padRmb = false;
    g_padBoost = false;

    if (s_swallowButtonsUntilRelease) {
        bool anyPressed = ltHeld || rtHeld;
        for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
            bool b = (SDL_GameControllerGetButton(g_gamepad, static_cast<SDL_GameControllerButton>(i)) != 0);
            if (b) anyPressed = true;
            g_prevButtons[i] = b;
        }
        if (!anyPressed) {
            s_swallowButtonsUntilRelease = false;
        }
    } else {
        const struct {
            SDL_GameControllerButton button;
            int action;
        } faceButtons[] = {
            { SDL_CONTROLLER_BUTTON_A, gamepadParseAction(settings.gamepad.btn_a) },
            { SDL_CONTROLLER_BUTTON_B, gamepadParseAction(settings.gamepad.btn_b) },
            { SDL_CONTROLLER_BUTTON_X, gamepadParseAction(settings.gamepad.btn_x) },
            { SDL_CONTROLLER_BUTTON_Y, gamepadParseAction(settings.gamepad.btn_y) },
            { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, gamepadParseAction(settings.gamepad.btn_lb) },
            { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, gamepadParseAction(settings.gamepad.btn_rb) },
            { SDL_CONTROLLER_BUTTON_LEFTSTICK, gamepadParseAction(settings.gamepad.btn_ls) },
            { SDL_CONTROLLER_BUTTON_RIGHTSTICK, gamepadParseAction(settings.gamepad.btn_rs) },
            { SDL_CONTROLLER_BUTTON_START, gamepadParseAction(settings.gamepad.btn_start) },
        };

        for (const auto& fb : faceButtons) {
            if ((combatAim || (lootAim && !lootSkillMode)) && !actionMenu
                && fb.button == SDL_CONTROLLER_BUTTON_A) {
                continue;
            }
            if (settings.gamepad.loot_assist && ltHeld && fb.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                g_prevButtons[fb.button] = SDL_GameControllerGetButton(g_gamepad, fb.button) != 0;
                continue;
            }
            bool held = SDL_GameControllerGetButton(g_gamepad, fb.button) != 0;
            bool prev = g_prevButtons[fb.button];
            if (bButtonCloses && fb.button == SDL_CONTROLLER_BUTTON_B) {
                if (held && !prev) {
                    enqueueInputEvent(KEY_ESCAPE);
                }
                g_prevButtons[fb.button] = held;
                continue;
            }
            gamepadApplyButton(fb.action, held, &prev);
            g_prevButtons[fb.button] = prev;
        }

        // Back / View / Share / Select button (plus clickable touchpad on
        // DualShock 4 / DualSense controllers).
        {
            bool held = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_BACK) != 0;
#if SDL_VERSION_ATLEAST(2, 0, 14)
            if (SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_TOUCHPAD) != 0) {
                held = true;
            }
#endif
            bool prev = g_prevButtons[SDL_CONTROLLER_BUTTON_BACK];
            gamepadApplyButton(gamepadParseAction(settings.gamepad.btn_back), held, &prev);
            g_prevButtons[SDL_CONTROLLER_BUTTON_BACK] = prev;
        }

        // Hold-button action menu: D-pad up/down moves the cursor vertically so the
        // modal (which highlights on mouse-Y deltas > 10px) steps prev/next.
        if (actionMenu) {
            bool dpUp = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
            bool dpDown = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
            if (dpUp || dpDown) {
                if (g_lastDpadTicks == 0 || (currentTicks - g_lastDpadTicks) > static_cast<uint32_t>(settings.gamepad.dpad_repeat_ms)) {
                    gamepadActivateCursor();
                    const float step = 14.0f;
                    g_virtualMouseY += dpUp ? -step : step;
                    gamepadClampVirtualCursor();
                    g_lastDpadTicks = currentTicks;
                }
            } else {
                g_lastDpadTicks = 0;
            }
        } else if (!aimActive) {
            bool dpUp = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
            bool dpDown = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
            bool dpLeft = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
            bool dpRight = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
            if (dpUp || dpDown || dpLeft || dpRight) {
                if (g_lastDpadTicks == 0 || (currentTicks - g_lastDpadTicks) > static_cast<uint32_t>(settings.gamepad.dpad_repeat_ms)) {
                    if (dpUp) enqueueInputEvent(KEY_ARROW_UP);
                    if (dpDown) enqueueInputEvent(KEY_ARROW_DOWN);
                    if (dpLeft) enqueueInputEvent(KEY_ARROW_LEFT);
                    if (dpRight) enqueueInputEvent(KEY_ARROW_RIGHT);
                    g_lastDpadTicks = currentTicks;
                }
            } else {
                g_lastDpadTicks = 0;
            }
        }
    }

    // --- Aim assist (hold LT / L2) and Loot assist (hold LT+LB / L2+L1) ---
    static Object* s_aimList[16];
    static int s_aimCount = 0;
    static Object* s_aimSelected = nullptr;
    static bool s_aimEngaged = false;
    static GameMouseMode s_savedMouseMode = GAME_MOUSE_MODE_INVALID;
    static Object* s_apMsgTarget = nullptr;

    static MapElevation s_aimLastElevation = ELEVATION_INVALID;
    static int s_aimLastMapSid = -1;
    if (s_aimForceReset || gElevation != s_aimLastElevation || gMapSid != s_aimLastMapSid) {
        s_aimEngaged = false;
        s_aimSelected = nullptr;
        s_apMsgTarget = nullptr;
        s_aimForceReset = false;
        s_aimLastElevation = gElevation;
        s_aimLastMapSid = gMapSid;
    }

    GameMouseMode desiredMouseMode;
    if (actionMenu) {
        desiredMouseMode = GAME_MOUSE_MODE_INVALID;
    } else if (combatAim) {
        desiredMouseMode = GAME_MOUSE_MODE_CROSSHAIR;
    } else if (lootAim) {
        desiredMouseMode = (gameMouseGetMode() >= FIRST_GAME_MOUSE_MODE_SKILL)
            ? GAME_MOUSE_MODE_INVALID
            : GAME_MOUSE_MODE_ARROW;
    } else if (worldAim) {
        desiredMouseMode = GAME_MOUSE_MODE_ARROW;
    } else {
        desiredMouseMode = GAME_MOUSE_MODE_INVALID;
    }

    if (!actionMenu && desiredMouseMode == GAME_MOUSE_MODE_INVALID && s_savedMouseMode != GAME_MOUSE_MODE_INVALID) {
        gameMouseSetMode(s_savedMouseMode);
        s_savedMouseMode = GAME_MOUSE_MODE_INVALID;
    }
    if (!combatAim) {
        s_apMsgTarget = nullptr;
    }

    static bool s_aimWasLoot = false;
    if (lootAim != s_aimWasLoot) {
        s_aimEngaged = false;
    }
    s_aimWasLoot = lootAim;
    auto buildAimList = [&](Object** buf, int max, bool whole) -> int {
        return lootAim
            ? gamepadListInteractables(buf, max, whole,
                  settings.gamepad.loot_snap_distance, settings.gamepad.loot_skip_empty)
            : combatGamepadListTargets(buf, max, whole);
    };

    if (aimActive && !actionMenu) {
        gamepadActivateCursor();
        if (!s_aimEngaged) {
            s_aimCount = buildAimList(s_aimList, 16, settings.gamepad.auto_aim_whole_map);
            s_aimSelected = (s_aimCount > 0) ? s_aimList[0] : nullptr;
            s_aimEngaged = (s_aimCount > 0);
        } else {
            static uint32_t s_lastPruneTicks = 0;
            bool selectedInvalid = (s_aimSelected == nullptr);
            if (selectedInvalid || currentTicks < s_aimFastPruneUntil || (currentTicks - s_lastPruneTicks >= 120)) {
                s_lastPruneTicks = currentTicks;
                Object* live[64];
                int nLive = buildAimList(live, 64, true);
                int w = 0;
                bool selectedFound = false;
                for (int i = 0; i < s_aimCount; i++) {
                    for (int j = 0; j < nLive; j++) {
                        if (live[j] == s_aimList[i]) {
                            s_aimList[w++] = s_aimList[i];
                            if (s_aimList[i] == s_aimSelected) {
                                selectedFound = true;
                            }
                            break;
                        }
                    }
                }
                s_aimCount = w;
                if (!selectedFound) {
                    s_aimSelected = (s_aimCount > 0) ? s_aimList[0] : nullptr;
                }
                if (s_aimCount == 0) {
                    s_aimEngaged = false;
                }
            }
        }

        if (s_aimEngaged && s_aimCount > 0) {
            int idx = 0;
            for (int i = 0; i < s_aimCount; i++) {
                if (s_aimList[i] == s_aimSelected) {
                    idx = i;
                    break;
                }
            }

            bool dl = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
            bool dr = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
            if (dr && !g_prevButtons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]) {
                idx = (idx + 1) % s_aimCount;
            }
            if (dl && !g_prevButtons[SDL_CONTROLLER_BUTTON_DPAD_LEFT]) {
                idx = (idx + s_aimCount - 1) % s_aimCount;
            }
            g_prevButtons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = dr;
            g_prevButtons[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = dl;
            s_aimSelected = s_aimList[idx];
            Object* target = s_aimSelected;

            if (combatAim && target != s_apMsgTarget) {
                HitMode hitMode;
                bool aiming;
                if (interfaceGetCurrentHitMode(&hitMode, &aiming) != -1) {
                    int attackAP = itemGetActionPointCost(gDude, hitMode, aiming);
                    int range = weaponGetRange(gDude, hitMode);
                    int dist = objectGetDistanceBetween(gDude, target);
                    int haveAP = gDude->data.critter.combat.ap;
                    char buf[96];
                    if (range <= 2 && dist > range) {
                        int approachAP = dist - range;
                        unsigned char rotations[800];
                        int hitChance = _determine_to_hit_no_range(gDude, target,
                            HIT_LOCATION_UNCALLED, hitMode, rotations);
                        snprintf(buf, sizeof(buf),
                            "Reach %d + attack %d = %d AP (have %d), hit ~%d%%",
                            approachAP, attackAP, approachAP + attackAP, haveAP, hitChance);
                    } else {
                        snprintf(buf, sizeof(buf), "Attack: %d AP (have %d)", attackAP, haveAP);
                    }
                    displayMonitorAddMessage(buf);
                }
                s_apMsgTarget = target;
            }

            if (desiredMouseMode != GAME_MOUSE_MODE_INVALID) {
                if (s_savedMouseMode == GAME_MOUSE_MODE_INVALID) {
                    s_savedMouseMode = gameMouseGetMode();
                }
                if (gameMouseGetMode() != desiredMouseMode) {
                    gameMouseSetMode(desiredMouseMode);
                }
            }

            Rect tr;
            objectGetRect(target, &tr);

            if (settings.gamepad.auto_aim_center_camera && (tr.right > tr.left || tr.bottom > tr.top)) {
                int tcx = (tr.left + tr.right) / 2;
                int tcy = (tr.top + tr.bottom) / 2;
                int offx = tcx - screenGetWidth() / 2;
                int offy = tcy - screenGetVisibleHeight() / 2;
                int sdx = (offx > settings.gamepad.camera_deadzone_x) ? 1 : (offx < -settings.gamepad.camera_deadzone_x ? -1 : 0);
                int sdy = (offy > settings.gamepad.camera_deadzone_y) ? 1 : (offy < -settings.gamepad.camera_deadzone_y ? -1 : 0);
                if (sdx != 0 || sdy != 0) {
                    mapScroll(sdx, sdy);
                    objectGetRect(target, &tr);
                }
            }

            if (tr.right > tr.left || tr.bottom > tr.top) {
                g_virtualMouseX = static_cast<float>((tr.left + tr.right) / 2);
                g_virtualMouseY = static_cast<float>((tr.top + tr.bottom) / 2);
                gamepadClampVirtualCursor();
            }

            if (combatAim) {
                bool fire = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_A) != 0;
                if (fire && !g_prevButtons[SDL_CONTROLLER_BUTTON_A]) {
                    if (rtHeld) {
                        gamepadQueueAction(GAMEPAD_ACTION_AIMED_ATTACK, target, 0);
                    } else {
                        HitMode hm;
                        bool aim;
                        int range = (interfaceGetCurrentHitMode(&hm, &aim) != -1)
                            ? weaponGetRange(gDude, hm)
                            : 0;
                        int dist = objectGetDistanceBetween(gDude, target);
                        if (range <= 2 && dist > range
                            && settings.gamepad.melee_approach != GAMEPAD_MELEE_OFF) {
                            gamepadQueueAction(GAMEPAD_ACTION_MELEE_APPROACH, target,
                                settings.gamepad.melee_approach == GAMEPAD_MELEE_ATTACK ? 1 : 0);
                        } else {
                            gamepadQueueAction(GAMEPAD_ACTION_ATTACK, target, 0);
                        }
                    }
                }
                g_prevButtons[SDL_CONTROLLER_BUTTON_A] = fire;
            } else if (lootAim && !lootSkillMode && target != nullptr) {
                bool fire = SDL_GameControllerGetButton(g_gamepad, SDL_CONTROLLER_BUTTON_A) != 0;
                if (fire && !g_prevButtons[SDL_CONTROLLER_BUTTON_A]) {
                    gamepadQueueAction(GAMEPAD_ACTION_LOOT_INTERACT, target, 0);
                }
                g_prevButtons[SDL_CONTROLLER_BUTTON_A] = fire;
            }
        }
    } else {
        s_aimEngaged = false;
    }

    // --- Analog sticks ---
    float lx = 0.0f, ly = 0.0f;
    applyRadialStick(SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_LEFTX),
        SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_LEFTY),
        &lx, &ly);
    float rx = 0.0f, ry = 0.0f;
    applyRadialStick(SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_RIGHTX),
        SDL_GameControllerGetAxis(g_gamepad, SDL_CONTROLLER_AXIS_RIGHTY),
        &rx, &ry);

    auto stickFreeCursor = [&](float ax, float ay) {
        if (ax == 0.0f && ay == 0.0f) return;
        gamepadActivateCursor();
        float speed = static_cast<float>(g_padBoost
                ? settings.gamepad.cursor_speed_boost
                : settings.gamepad.cursor_speed);
        g_virtualMouseX += ax * speed * dt;
        g_virtualMouseY += ay * speed * dt;
        gamepadClampVirtualCursor();
    };

    auto stickCameraPan = [&](float ax, float ay) {
        float pt = static_cast<float>(settings.gamepad.camera_pan_threshold);
        int sdx = (ax > pt) ? 1 : (ax < -pt ? -1 : 0);
        int sdy = (ay > pt) ? 1 : (ay < -pt ? -1 : 0);
        if (sdx != 0 || sdy != 0) {
            mapScroll(sdx, sdy);
        }
    };

    auto stickCenteredCamera = [&](float ax, float ay) {
        float mag = sqrtf(ax * ax + ay * ay);
        if (mag == 0.0f) return;
        gamepadActivateCursor();
        float pt = static_cast<float>(settings.gamepad.camera_pan_threshold);
        int sdx = (ax > pt) ? 1 : (ax < -pt ? -1 : 0);
        int sdy = (ay > pt) ? 1 : (ay < -pt ? -1 : 0);
        int before = gCenterTile;
        int rc = -1;
        if (sdx != 0 || sdy != 0) {
            rc = mapScroll(sdx, sdy);
        }
        bool panned = (gCenterTile != before);
        if (panned) {
            g_virtualMouseX = static_cast<float>(screenGetWidth() / 2);
            g_virtualMouseY = static_cast<float>(screenGetVisibleHeight() / 2);
        } else if (rc != -2) {
            float speed = static_cast<float>(settings.gamepad.cursor_speed);
            g_virtualMouseX += ax * speed * dt;
            g_virtualMouseY += ay * speed * dt;
        }
        if (panned || rc != -2) {
            gamepadClampVirtualCursor();
        }
    };

    auto applyStickRole = [&](int role, float ax, float ay) {
        switch (role) {
        case GAMEPAD_STICK_CURSOR:
            stickFreeCursor(ax, ay);
            break;
        case GAMEPAD_STICK_CAMERA:
            stickCameraPan(ax, ay);
            break;
        case GAMEPAD_STICK_CENTERED:
            stickCenteredCamera(ax, ay);
            break;
        default:
            break;
        }
    };

    bool mapView = gamepadInMapView();
    if (aimActive) {
        // Aim assist owns the cursor; leave both sticks idle this frame.
    } else if (worldWalk) {
        int leftRole = gamepadParseStickRole(settings.gamepad.world_left_stick, GAMEPAD_STICK_CENTERED);
        int rightRole = gamepadParseStickRole(settings.gamepad.world_right_stick, GAMEPAD_STICK_CURSOR);
        applyStickRole(leftRole, lx, ly);
        applyStickRole(rightRole, rx, ry);
    } else if (mapView) {
        stickFreeCursor(lx, ly);
        stickCameraPan(rx, ry);
    } else {
        stickFreeCursor(lx, ly);
        if (rx != 0.0f || ry != 0.0f) {
            static float scrollAccX = 0.0f, scrollAccY = 0.0f;
            scrollAccX += rx * 8.0f * dt;
            scrollAccY += ry * 8.0f * dt;
            int ticksX = static_cast<int>(scrollAccX);
            int ticksY = static_cast<int>(scrollAccY);
            if (ticksX != 0 || ticksY != 0) {
                *wheelX += ticksX;
                *wheelY += ticksY;
                scrollAccX -= static_cast<float>(ticksX);
                scrollAccY -= static_cast<float>(ticksY);
            }
        }
    }

    // --- Controller touchpad cursor (SDL2 touchpad API, e.g. DS4 / DualSense) ---
#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (!aimActive && settings.gamepad.touchpad_enabled
        && SDL_GameControllerGetNumTouchpads(g_gamepad) > 0) {
        static bool s_prevFinger = false;
        static uint16_t s_prevX = 0, s_prevY = 0;
        static uint32_t s_tapStart = 0;
        static int s_tapMoveAccum = 0;
        static float s_smoothDx = 0.0f, s_smoothDy = 0.0f;

        Uint8 fingerState = 0;
        float normX = 0.0f, normY = 0.0f, pressure = 0.0f;
        bool fingerDown = (SDL_GameControllerGetTouchpadFinger(g_gamepad, 0, 0, &fingerState, &normX, &normY, &pressure) == 0
            && fingerState != 0);

        if (fingerDown) {
            uint16_t fx = static_cast<uint16_t>(normX * 1920.0f);
            uint16_t fy = static_cast<uint16_t>(normY * 943.0f);
            if (s_prevFinger) {
                int ddx = static_cast<int>(fx) - static_cast<int>(s_prevX);
                int ddy = static_cast<int>(fy) - static_cast<int>(s_prevY);

                if (static_cast<float>(abs(ddx) + abs(ddy)) < static_cast<float>(settings.gamepad.touch_deadzone)) {
                    ddx = 0;
                    ddy = 0;
                }
                s_tapMoveAccum += abs(ddx) + abs(ddy);

                float sm = static_cast<float>(settings.gamepad.touch_smoothing);
                s_smoothDx = s_smoothDx * sm + static_cast<float>(ddx) * (1.0f - sm);
                s_smoothDy = s_smoothDy * sm + static_cast<float>(ddy) * (1.0f - sm);

                float speed = sqrtf(s_smoothDx * s_smoothDx + s_smoothDy * s_smoothDy);
                float gain = static_cast<float>(settings.gamepad.touch_sensitivity)
                    + static_cast<float>(settings.gamepad.touch_accel) * speed;
                float maxGain = static_cast<float>(settings.gamepad.touch_max_gain);
                if (gain > maxGain) {
                    gain = maxGain;
                }

                if (ddx != 0 || ddy != 0) {
                    gamepadActivateCursor();
                }
                g_virtualMouseX += s_smoothDx * gain;
                g_virtualMouseY += s_smoothDy * gain;
                gamepadClampVirtualCursor();
            } else {
                s_tapStart = currentTicks;
                s_tapMoveAccum = 0;
                s_smoothDx = 0.0f;
                s_smoothDy = 0.0f;
            }
            s_prevX = fx;
            s_prevY = fy;
        }

        if (s_prevFinger && !fingerDown) {
            if ((currentTicks - s_tapStart) < static_cast<uint32_t>(settings.gamepad.touch_tap_ms)
                && s_tapMoveAccum < settings.gamepad.touch_tap_move) {
                g_touchpadClickFrames = 3;
            }
        }

        s_prevFinger = fingerDown;
        g_touchpadFingerActive = fingerDown;
    } else {
        g_touchpadFingerActive = false;
    }
#endif
}

void gamepadFillMouse(MouseData* mouseState)
{
    mouseState->x = static_cast<int>(g_virtualMouseX);
    mouseState->y = static_cast<int>(g_virtualMouseY);
    gamepadApplyButtons(mouseState);
}

void gamepadApplyButtons(MouseData* mouseState)
{
    if (g_padLmb || g_touchpadClickFrames > 0) {
        mouseState->buttons[0] = 1;
    }
    if (g_touchpadClickFrames > 0) {
        g_touchpadClickFrames--;
    }
    if (g_padRmb) {
        mouseState->buttons[1] = 1;
    }
}

void gamepadOpen()
{
    if (!settings.gamepad.enabled) {
        return;
    }

    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
            debugPrint("gamepadOpen: SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s\n", SDL_GetError());
            return;
        }
    }

    int joysticks = SDL_NumJoysticks();
    for (int i = 0; i < joysticks; i++) {
        if (SDL_IsGameController(i)) {
            g_gamepad = SDL_GameControllerOpen(i);
            if (g_gamepad != nullptr) {
                debugPrint("Gamepad connected: %s\n", SDL_GameControllerName(g_gamepad));
                break;
            }
        }
    }

    if (g_gamepad != nullptr) {
        int sw = screenGetWidth();
        int sh = screenGetHeight();
        if (sw > 0 && sh > 0) {
            g_virtualMouseX = static_cast<float>(sw / 2);
            g_virtualMouseY = static_cast<float>(sh / 2);
        }
    }
}

void gamepadClose()
{
    s_gamepadCursorActive = false;
    g_touchpadFingerActive = false;
    if (g_gamepad != nullptr) {
        SDL_GameControllerClose(g_gamepad);
        g_gamepad = nullptr;
    }
}

} // namespace fallout
