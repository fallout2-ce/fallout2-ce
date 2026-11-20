#ifndef FALLOUT_MAINMENU_H_
#define FALLOUT_MAINMENU_H_

#include "window_manager.h"

namespace fallout {

enum MainMenuOption : int {
    Invalid = -1,
    Intro,
    NewGame,
    LoadGame,
    ScreenSaver,
    Timeout,
    Credits,
    Quotes,
    Exit,
    SelfRun,
    Options,
};

int mainMenuWindowInit();
void mainMenuWindowFree();
void mainMenuWindowHide(bool animate);
void mainMenuWindowUnhide(bool animate);
void mainMenuBeginSubscreen();
void mainMenuCancelSubscreen();
void mainMenuFinishSubscreen();
WindowFlags mainMenuSubscreenWindowFlags(WindowFlags defaultFlags, WindowFlags overlayFlags);
void mainMenuShowSubscreen(bool animate);
void mainMenuFadeOutForMenuReturn(bool animate);
void mainMenuFadeOutForGameStart(bool animate);
int _main_menu_is_enabled();
MainMenuOption mainMenuWindowHandleEvents();
void mainMenuRequestExit();

} // namespace fallout

#endif /* FALLOUT_MAINMENU_H_ */
