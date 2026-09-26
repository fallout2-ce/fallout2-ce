#ifndef GAME_H
#define GAME_H

#include "game_vars.h"
#include "message.h"
#include "skilldex.h"
#include "touch.h"

namespace fallout {

enum class GameState : int {
    Normal,
    NormalPending,
    DialogFinished,
    DialogFinishedPending,
    DialogActive,
    DialogRequested,
};

enum GameQuitRequest : int {
    GAME_QUIT_REQUEST_NONE = 0,
    GAME_QUIT_REQUEST_END_COMBAT = 1,
    GAME_QUIT_REQUEST_MAIN_MENU = 2,
    GAME_QUIT_REQUEST_EXIT = 3,
};

extern int* gGameGlobalVars;
extern int gGameGlobalVarsLength;
extern const char* asc_5186C8;
extern GameQuitRequest _game_user_wants_to_quit;

extern MessageList gMiscMessageList;

extern bool gGameLoaded;

int gameInitWithOptions(const char* windowTitle, bool isMapper, int font, int flags, int argc, char** argv);
void gameReset();
void gameExit();
int gameHandleKey(int eventCode, bool isInCombatMode);
void gameUiDisable(int allowScrolling);
void gameUiEnable();
bool gameUiIsDisabled();
int gameGetGlobalVar(GameGlobalVar var);
int gameSetGlobalVar(GameGlobalVar var, int value);
int globalVarsRead(const char* path, const char* section, int* variablesListLengthPtr, int** variablesListPtr);
GameState gameGetState();
int gameRequestState(GameState newGameState);
void gameUpdateState();
int showQuitConfirmationDialog();

int gameLoadGlobalVars();
int gameShowDeathDialog(const char* message);
void gameHandleSkilldexResult(SkilldexRC rc);
void showHelp();
void* gameGetGlobalPointer(GameGlobalVar var);
int gameSetGlobalPointer(GameGlobalVar var, void* value);

inline bool globalVariableIsValid(int var)
{
    return var >= 0 && var < gGameGlobalVarsLength;
}

class GameMode {
public:
    enum Flags : unsigned int {
        kNone = 0x0,
        kWorldmap = 0x1,
        kDialog = 0x4,
        kOptions = 0x8,
        kSaveGame = 0x10,
        kLoadGame = 0x20,
        kCombat = 0x40,
        kPreferences = 0x80,
        kHelp = 0x100,
        kEditor = 0x200,
        kPipboy = 0x400,
        kPlayerTurn = 0x800,
        kInventory = 0x1000,
        kAutomap = 0x2000,
        kSkilldex = 0x4000,
        kUseOn = 0x8000,
        kLoot = 0x10000,
        kBarter = 0x20000,
        kHero = 0x40000,
        kDialogReview = 0x80000,
        kCounter = 0x100000,
        kSpecial = 0x80000000,
    };

    static void enterGameMode(Flags gameMode);
    static void exitGameMode(Flags gameMode);
    static void exitGameModeQuietly(Flags gameMode);
    static bool isInGameMode(Flags gameMode);
    static Flags getCurrentGameMode() { return currentGameMode; }

private:
    static Flags currentGameMode;
};

constexpr inline GameMode::Flags operator~(GameMode::Flags rhs)
{
    return static_cast<GameMode::Flags>(~static_cast<unsigned int>(rhs));
}

constexpr inline GameMode::Flags operator&(GameMode::Flags lhs, GameMode::Flags rhs)
{
    return static_cast<GameMode::Flags>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline GameMode::Flags operator|(GameMode::Flags lhs, GameMode::Flags rhs)
{
    return static_cast<GameMode::Flags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

class ScopedGameMode {
public:
    ScopedGameMode(GameMode::Flags gameMode);
    ~ScopedGameMode();

private:
    GameMode::Flags gameMode;
};

} // namespace fallout

#endif /* GAME_H */
