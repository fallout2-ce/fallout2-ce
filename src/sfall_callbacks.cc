#include "sfall_callbacks.h"

#include "display_monitor.h"
#include "sfall_config.h"
#include "sfall_script_hooks.h"
#include "worldmap.h"

namespace fallout {

void sfallOnBeforeGameInit()
{
    return;
}

void sfallOnGameInit()
{
    return;
}

void sfallOnAfterGameInit()
{
    return;
}

void sfallOnGameExit()
{
    return;
}

void sfallOnGameReset()
{
    return;
}

void sfallOnBeforeGameStart()
{
    return;
}

void sfallOnAfterGameStarted()
{
    // Disable Horrigan Patch
    bool isDisableHorrigan = false;
    configGetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DISABLE_HORRIGAN, &isDisableHorrigan);

    if (isDisableHorrigan) {
        gDidMeetFrankHorrigan = true;
    }
}

void sfallOnAfterNewGame()
{
    return;
}

void sfallOnGameModeChange(int exit, int previousGameMode)
{
    scriptHooks_GameModeChange(exit, previousGameMode);
}

void sfallOnBeforeGameClose()
{
    return;
}

void sfallOnCombatStart()
{
    return;
}

void sfallOnCombatEnd()
{
    return;
}

void sfallOnBeforeMapLoad()
{
    return;
}

} // namespace fallout
