#ifndef LOAD_SAVE_GAME_H
#define LOAD_SAVE_GAME_H

namespace fallout {

enum class LoadSaveMode : int {
    FromMainMenu, // Special case - loading game from main menu.
    Normal, // Normal (full-screen) save/load screen.
    Quick, // Quick load/save.
};

void _InitLoadSave();
void _ResetLoadSave();
int lsgSaveGame(LoadSaveMode mode);
int lsgLoadGame(LoadSaveMode mode);
void lsgDevSetLoadGameSlot(int slot);
int lsgGetTotalSlotCount();
bool _isLoadingGame();
int mapIdBeingLoaded();
void lsgInit();
int MapDirErase(const char* path, const char* extension);
int _MapDirEraseFile_(const char* relativePath, const char* fileName);

} // namespace fallout

#endif /* LOAD_SAVE_GAME_H */
