#ifndef CONTENT_CONFIG_H
#define CONTENT_CONFIG_H

#include "config.h"

namespace fallout {

#define CONTENT_CONFIG_START_SECTION "start"
#define CONTENT_CONFIG_KARMA_SECTION "karma"
#define CONTENT_CONFIG_ITEMS_SECTION "items"
#define CONTENT_CONFIG_DIALOG_SECTION "dialog"
#define CONTENT_CONFIG_MAIN_MENU_SECTION "main_menu"
#define CONTENT_CONFIG_MOVIES_SECTION "movies"
#define CONTENT_CONFIG_COMBAT_SECTION "combat"
#define CONTENT_CONFIG_EXPLOSIONS_SECTION "explosions"
#define CONTENT_CONFIG_SKILLDEX_SECTION "skilldex"
#define CONTENT_CONFIG_WORLDMAP_SECTION "worldmap"
#define CONTENT_CONFIG_CHARACTERS_SECTION "characters"
#define CONTENT_CONFIG_TEXT_SECTION "text"

extern Config gContentConfig;

// Initialize content config and try to load it from file.
void contentConfigInit();
void contentConfigExit();

} // namespace fallout

#endif // CONTENT_CONFIG_H
