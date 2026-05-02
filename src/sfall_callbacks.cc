#include "sfall_callbacks.h"

#include "content_config.h"
#include "display_monitor.h"
#include "interface.h"
#include "sfall_sound.h"
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
    sfallSoundExit();
    return;
}

void sfallOnGameReset()
{
    sfallSoundReset();
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
    configGetBool(&gContentConfig, CONTENT_CONFIG_WORLDMAP_SECTION, "disable_horrigan", &isDisableHorrigan);

    if (isDisableHorrigan) {
        gDidMeetFrankHorrigan = true;
    }

    // Refresh item art after load, which calls the CALCAPCOST hook if present to
    // display the correct AP cost.
    if (gInterfaceBarWindow != -1) {
        int leftItemAction;
        int rightItemAction;
        interfaceGetItemActions(&leftItemAction, &rightItemAction);
        interfaceUpdateItems(false, leftItemAction, rightItemAction);
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
