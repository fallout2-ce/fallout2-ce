#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include "config.h"

namespace fallout {

#define GAME_CONFIG_DEBUG_KEY "debug"
#define GAME_CONFIG_SYSTEM_KEY "system"
#define GAME_CONFIG_SCREEN_KEY "screen"
#define GAME_CONFIG_UI_KEY "ui"
#define GAME_CONFIG_SOUND_KEY "sound"

#define GAME_CONFIG_MASTER_DAT_KEY "master_dat"
#define GAME_CONFIG_MASTER_PATCHES_KEY "master_patches"
#define GAME_CONFIG_F2_RES_DAT_KEY "f2_res_dat"
#define GAME_CONFIG_CRITTER_DAT_KEY "critter_dat"
#define GAME_CONFIG_CRITTER_PATCHES_KEY "critter_patches"
#define GAME_CONFIG_MUSIC_PATH1_KEY "music_path1"
#define GAME_CONFIG_MUSIC_PATH2_KEY "music_path2"

#define GAME_CONFIG_MODE_KEY "mode"
#define GAME_CONFIG_RESOLUTION_X_KEY "resolution_x"
#define GAME_CONFIG_RESOLUTION_Y_KEY "resolution_y"
#define GAME_CONFIG_WINDOWED_KEY "windowed"
#define GAME_CONFIG_SCALE_KEY "scale"
#define GAME_CONFIG_MAIN_MENU_SCALE_MODE_KEY "main_menu_scale_mode"
#define GAME_CONFIG_IFACE_BAR_MODE_KEY "iface_bar_mode"
#define GAME_CONFIG_IFACE_BAR_WIDTH_KEY "iface_bar_width"
#define GAME_CONFIG_IFACE_BAR_SIDE_ART_KEY "iface_bar_side_art"
#define GAME_CONFIG_IFACE_BAR_SIDES_ORI_KEY "iface_bar_sides_ori"
#define GAME_CONFIG_IGNORE_SCROLL_LIMIT_KEY "ignore_scroll_limit"
#define GAME_CONFIG_ALTERNATE_AMMO_METER_KEY "alternate_ammo_meter"
#define GAME_CONFIG_IGNORE_MAP_EDGES_KEY "ignore_map_edges"
#define GAME_CONFIG_SPLASH_SCREEN_SIZE_KEY "splash_screen_size"
#define GAME_CONFIG_DEATH_SCREEN_SIZE_KEY "death_screen_size"
#define GAME_CONFIG_END_SLIDE_SIZE_KEY "end_slide_size"
#define GAME_CONFIG_MOVIE_ASPECT_FIT_KEY "movie_aspect_fit"

#define ENGLISH "english"
#define FRENCH "french"
#define GERMAN "german"
#define ITALIAN "italian"
#define SPANISH "spanish"

enum GameDifficulty : int {
    GAME_DIFFICULTY_EASY,
    GAME_DIFFICULTY_NORMAL,
    GAME_DIFFICULTY_HARD,
    GAME_DIFFICULTY_MIN = GAME_DIFFICULTY_EASY,
    GAME_DIFFICULTY_MAX = GAME_DIFFICULTY_HARD
};

enum CombatDifficulty : int {
    COMBAT_DIFFICULTY_EASY,
    COMBAT_DIFFICULTY_NORMAL,
    COMBAT_DIFFICULTY_HARD,
    COMBAT_DIFFICULTY_MIN = COMBAT_DIFFICULTY_EASY,
    COMBAT_DIFFICULTY_MAX = COMBAT_DIFFICULTY_HARD
};

enum ViolenceLevel : int {
    VIOLENCE_LEVEL_INVALID = -1,
    VIOLENCE_LEVEL_NONE,
    VIOLENCE_LEVEL_MINIMAL,
    VIOLENCE_LEVEL_NORMAL,
    VIOLENCE_LEVEL_MAXIMUM_BLOOD,
    VIOLENCE_LEVEL_MIN = VIOLENCE_LEVEL_NONE,
    VIOLENCE_LEVEL_MAX = VIOLENCE_LEVEL_MAXIMUM_BLOOD
};

enum TargetHighlight : int {
    TARGET_HIGHLIGHT_OFF,
    TARGET_HIGHLIGHT_ON,
    TARGET_HIGHLIGHT_TARGETING_ONLY,
    TARGET_HIGHLIGHT_MIN = TARGET_HIGHLIGHT_OFF,
    TARGET_HIGHLIGHT_MAX = TARGET_HIGHLIGHT_TARGETING_ONLY
};

extern bool gGameConfigInitialized;
extern Config gGameConfig;
extern char gGameConfigFilePath[];

bool gameConfigInit(bool isMapper, int argc, char** argv);
bool gameConfigSave();
bool gameConfigExit(bool shouldSave);

} // namespace fallout

#endif /* GAME_CONFIG_H */
