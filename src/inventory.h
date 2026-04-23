#ifndef INVENTORY_H
#define INVENTORY_H

#include "obj_types.h"

namespace fallout {

// Extra slots per scroller added by the expanded barter/trade window.
constexpr int kExpandedBarterExtraSlots = 1;

typedef enum Hand {
    // Item1 (Punch)
    HAND_LEFT,
    // Item2 (Kick)
    HAND_RIGHT,
    HAND_COUNT,
} Hand;

typedef void InventoryPrintItemDescriptionHandler(const char* string);

void inventoryResetDude();
void inventoryOpen();
void adjustCritterStatsOnArmorChange(Object* critter, Object* oldArmor, Object* newArmor);
int inventoryComputeCritterFid(Object* critter, int basePid, Object* rightHandItem, Object* leftHandItem, Object* armor, int activeHand, int anim, int rotation);
void inventoryOpenUseItemOn(Object* targetObj);
Object* critterGetItem2(Object* critter);
Object* critterGetItem1(Object* critter);
Object* critterGetArmor(Object* critter);
Object* objectGetCarriedObjectByPid(Object* obj, int pid);
int objectGetCarriedQuantityByPid(Object* obj, int pid);
Object* inventoryFindByType(Object* obj, int itemType, int* indexPtr);
Object* inventoryFindById(Object* obj, int id);
Object* inventoryItemByIndex(Object* obj, int index);
// Makes critter equip a given item in a given hand slot with an animation.
// 0 - left hand, 1 - right hand. If item is armor, hand value is ignored.
int inventoryEquip(Object* critter, Object* item, int hand);
// Same as inven_wield but allows to wield item without animation.
int inventoryEquipFunc(Object* critter, Object* item, int hand, bool animate);
// Makes critter unequip an item in a given hand slot with an animation.
int inventoryUnequip(Object* critter, int hand);
// Same as inven_unwield but allows to unwield item without animation.
int inventoryUnequipFunc(Object* critter, int hand, bool animate);
int inventoryOpenLooting(Object* looter, Object* target);
int inventoryOpenStealing(Object* thief, Object* target);
void barterProcessUI(int win, Object* barterer, Object* playerTable, Object* bartererTable, int barterMod);
int inventorySetTimer(Object* item);
Object* inventoryGetTargetObject();

} // namespace fallout

#endif /* INVENTORY_H */
