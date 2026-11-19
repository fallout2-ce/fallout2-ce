#include "game_speed.h"

#include <SDL.h>

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "debug.h"
#include "game.h"
#include "kb.h"
#include "settings.h"
#include "sfall_kb_helpers.h"
#include "win32.h"

namespace fallout {

static bool initialized = false;
static bool enabled = true;
static bool toggleHeld = false;
static int selectedPercent = 100;
static int appliedPercent = 100;
static unsigned int lastRealTicks;
static unsigned int ticks;
static unsigned int fraction = 0;
static int pauseDepth = 0;

static bool modifierIsPressed()
{
    switch (settings.speed.modifier_key) {
    case 0:
        return true;
    case -1:
        return sfall_kb_is_key_pressed(0x1D) || sfall_kb_is_key_pressed(0x9D);
    case -2:
        return sfall_kb_is_key_pressed(0x38) || sfall_kb_is_key_pressed(0xB8);
    case -3:
        return sfall_kb_is_key_pressed(0x2A) || sfall_kb_is_key_pressed(0x36);
    default:
        return sfall_kb_is_key_pressed(settings.speed.modifier_key);
    }
}

static void updateControls()
{
    if (!settings.speed.enabled || !gProgramIsActive || keyboardIsDisabled()) {
        toggleHeld = false;
        return;
    }

    int previousPercent = selectedPercent;
    bool previouslyEnabled = enabled;
    bool togglePressed = settings.speed.toggle_key != 0
        && sfall_kb_is_key_pressed(settings.speed.toggle_key);
    if (modifierIsPressed()) {
        if (togglePressed) {
            if (!toggleHeld) enabled = !enabled;
        } else {
            const int keys[] = { settings.speed.key_0, settings.speed.key_1,
                settings.speed.key_2, settings.speed.key_3 };
            const int percentages[] = { settings.speed.preset_0, settings.speed.preset_1,
                settings.speed.preset_2, settings.speed.preset_3 };
            for (int index = 0; index < 4; index++) {
                if (keys[index] != 0 && sfall_kb_is_key_pressed(keys[index])) {
                    selectedPercent = std::clamp(percentages[index], 1, 1000);
                    break;
                }
            }
        }
    }
    toggleHeld = togglePressed;
    if (selectedPercent != previousPercent || enabled != previouslyEnabled) {
        debugPrint("Game speed: %d%% (selected %d%%, toggle %s).\n",
            enabled ? selectedPercent : 100, selectedPercent, enabled ? "on" : "off");
    }
}

static int effectivePercent()
{
    if (!settings.speed.enabled || !enabled || !gGameLoaded || pauseDepth != 0) {
        return 100;
    }

    auto mode = GameMode::getCurrentGameMode();
    if (mode == GameMode::kNone || mode == GameMode::kCombat
        || mode == (GameMode::kCombat | GameMode::kPlayerTurn)
        || (mode & GameMode::kWorldmap) != GameMode::kNone) {
        return selectedPercent;
    }
    return 100;
}

unsigned int gameSpeedGetTicks()
{
    unsigned int now = SDL_GetTicks();
    if (!initialized) {
        initialized = true;
        ticks = lastRealTicks = now;
        selectedPercent = std::clamp(settings.speed.initial_percent, 1, 1000);
    }

    // Unsigned subtraction handles SDL's 32-bit timer wrapping. Retain fractional
    // milliseconds so slow motion works even when the clock is polled frequently.
    uint64_t elapsed = uint64_t(now - lastRealTicks) * appliedPercent + fraction;
    ticks += static_cast<unsigned int>(elapsed / 100);
    fraction = elapsed % 100;
    lastRealTicks = now;

    updateControls();
    appliedPercent = effectivePercent();
    return ticks;
}

int gameSpeedGetPercent()
{
    gameSpeedGetTicks();
    return appliedPercent;
}

void gameSpeedReset()
{
    gameSpeedGetTicks();
    selectedPercent = std::clamp(settings.speed.initial_percent, 1, 1000);
    enabled = true;
    toggleHeld = false;
    fraction = 0;
    // Do not rewind the clock: other engine timers can survive a game reset.
    appliedPercent = effectivePercent();
}

ScopedGameSpeedPause::ScopedGameSpeedPause()
{
    gameSpeedGetTicks();
    pauseDepth++;
    appliedPercent = effectivePercent();
}

ScopedGameSpeedPause::~ScopedGameSpeedPause()
{
    gameSpeedGetTicks();
    assert(pauseDepth > 0);
    pauseDepth--;
    appliedPercent = effectivePercent();
}

} // namespace fallout
