#ifndef INTERFACE_H
#define INTERFACE_H

#include "combat_defs.h"
#include "db.h"
#include "obj_types.h"

namespace fallout {

#define INDICATOR_BOX_WIDTH 130
#define INDICATOR_BOX_HEIGHT 21

#define INTERFACE_BAR_WIDTH 640
#define INTERFACE_BAR_HEIGHT 100

// Minimum radiation amount to display RADIATED indicator.
#define RADATION_INDICATOR_THRESHOLD 65

// Minimum poison amount to display POISONED indicator.
#define POISON_INDICATOR_THRESHOLD 0

typedef enum InterfaceItemAction {
    INTERFACE_ITEM_ACTION_DEFAULT = -1,
    INTERFACE_ITEM_ACTION_USE,
    INTERFACE_ITEM_ACTION_PRIMARY,
    INTERFACE_ITEM_ACTION_PRIMARY_AIMING,
    INTERFACE_ITEM_ACTION_SECONDARY,
    INTERFACE_ITEM_ACTION_SECONDARY_AIMING,
    INTERFACE_ITEM_ACTION_RELOAD,
    INTERFACE_ITEM_ACTION_COUNT,
} InterfaceItemAction;

extern int gInterfaceBarWindow;
extern bool gInterfaceBarMode;
extern int gInterfaceBarWidth;
extern bool gInterfaceBarIsCustom;
extern int gInterfaceBarContentOffset;

int interfaceInit();
void interfaceReset();
void interfaceFree();
int interfaceLoad(File* stream);
int interfaceSave(File* stream);
void interfaceBarHide();
void interfaceBarShow();
void interfaceBarEnable();
void interfaceBarDisable();
bool interfaceBarEnabled();
void interfaceBarRefresh();
void interfaceRenderHitPoints(bool animate);
void interfaceRenderArmorClass(bool animate);
void interfaceRenderActionPoints(int actionPointsLeft, int bonusActionPoints);
int interfaceGetCurrentHitMode(HitMode* hitMode, bool* aiming);
int interfaceUpdateItems(bool animated, int leftItemAction, int rightItemAction);
int interfaceBarSwapHands(bool animated);
int interfaceGetItemActions(int* leftItemAction, int* rightItemAction);
int interfaceCycleItemAction();
void _intface_use_item();
int interfaceGetCurrentHand();
int interfaceGetActiveItem(Object** itemPtr);
int _intface_update_ammo_lights();
void interfaceBarEndButtonsShow(bool animated);
void interfaceBarEndButtonsHide(bool animated);
void interfaceBarEndButtonsRenderGreenLights();
void interfaceBarEndButtonsRenderRedLights();
int indicatorBarRefresh();
bool indicatorBarShow();
bool indicatorBarHide();
bool interface_get_current_attack_mode(HitMode* hitMode);
int interfaceTagAdd();
int interfaceTagGetMax();
bool interfaceTagShow(int tag);
bool interfaceTagHide(int tag);
bool interfaceTagIsActive(int tag);
void interfaceTagSetText(int tag, const char* text, int color);

unsigned char* customInterfaceBarGetBackgroundImageData();

} // namespace fallout

#endif /* INTERFACE_H */
