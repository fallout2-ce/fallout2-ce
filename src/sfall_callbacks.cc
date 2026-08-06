#include "sfall_callbacks.h"

#include "content_config.h"
#include "display_monitor.h"
#include "interface.h"
#include "inventory.h"
#include "reaction.h"
#include "script_sound.h"
#include "sfall_object_name.h"
#include "sfall_script_hooks.h"
#include "stat.h"
#include "worldmap.h"

namespace fallout {

void sfallOnBeforeGameInit()
{
    return;
}

void sfallOnGameInit()
{
    inventoryResetInvenApCost();
    return;
}

void sfallOnAfterGameInit()
{
    return;
}

void sfallOnGameExit()
{
    scriptSoundExit();
    return;
}

void sfallOnGameReset()
{
    inventoryResetInvenApCost();
    reactionResetThresholds();
    scriptSoundReset();
    sfallObjectNameReset();
    statResetUnspentApBonuses();
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
        InterfaceItemAction leftItemAction;
        InterfaceItemAction rightItemAction;
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
    sfallObjectNameReset();
}

} // namespace fallout
