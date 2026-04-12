#ifndef CHARACTER_EDITOR_H
#define CHARACTER_EDITOR_H

#include "db.h"

namespace fallout {

extern int gCharacterEditorRemainingCharacterPoints;

enum PerkCarryOverMode {
    // Unspent perks are lost on level up (vanilla)
    PERK_CARRY_OVER_MODE_OFF = 0,
    // Unspent perks are preserved on level up, but you still spend unspent perks at the appropriate levels.
    // E.g. if you reach level 9 and have never spent any perks, you first choose from the level 3 perks, then 6, then 9. (CE)
    PERK_CARRY_OVER_MODE_ON = 1,
    // Unspent perks are preserved on level up, and you can choose to spend them on perks for the current level.
    PERK_CARRY_OVER_MODE_SFALL = 2,
};

int characterEditorShow(bool isCreationMode);
void characterEditorInit();
bool _isdoschar(int ch);
char* _strmfe(char* dest, const char* name, const char* ext);
int characterEditorSave(File* stream);
int characterEditorLoad(File* stream);
void characterEditorReset();
int characterEditorGetPerkOwed();
void characterEditorSetPerkOwed(int value);
void characterEditorHandleLevelUp(int level);
int characterEditorGetPerkSelectionLevel();

} // namespace fallout

#endif /* CHARACTER_EDITOR_H */
