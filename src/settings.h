#ifndef FALLOUT_SETTINGS_H_
#define FALLOUT_SETTINGS_H_

#include <string>

#include "game_config.h"

namespace fallout {

struct SystemSettings {
    std::string executable = "game";
    std::string master_dat_path = "master.dat";
    std::string master_patches_path = "data";
    std::string critter_dat_path = "critter.dat";
    std::string critter_patches_path = "data";
    std::string language = ENGLISH;
    int scroll_lock = 0;
    bool interrupt_walk = true;
    int art_cache_size = 8;
    bool color_cycling = true;
    int cycle_speed_factor = 1;
    bool hashing = true;
    int splash = 0;
    int free_space = 20480;
    int times_run = 0;
    std::string screenshots_format = "png";
};

struct ScreenSettings {
    int resolution_x = 640;
    int resolution_y = 480;
    bool windowed = false;
    int scale = 1;
};

struct UISettings {
    // Should the game window stretch all the way to the bottom or sit at the top of the interface bar (default).
    bool iface_bar_mode = false;

    // This will increase the width of the interface bar expanding the area used to display text.
    int iface_bar_width = 800;

    // 0 - Black, No Iface-bar side art used.
    // 1 - Metal look Iface-bar side art used.
    // 2 - Leather look Iface-bar side art used.
    int iface_bar_side_art = 2;

    // Iface-bar side graphics extend from the Screen edges to the Iface-Bar if true (otherwise from bar to edges).
    bool iface_bar_sides_ori = false;

    // Extends AP bar to 16 dots instead of 10.
    bool extend_ap_bar = false;

    // Expands barter/trade window vertically, adding a 4th item slot per side.
    bool expand_barter_window = false;

    int splash_screen_size = 0;

    bool ignore_map_edges = false;

    // iOS quick-actions toolbar above the interface bar. No-op on other platforms.
    bool quick_toolbar_visible = false;

    // TODO: add to setting window
    // Speed of various UI transition animations. 1.0 represents vanilla speeds.
    double anim_speed = 1.0;

    int skip_opening_movies = 0;
    bool display_karma_changes = false;
    bool display_bonus_damage = false;
    bool numbers_in_dialogue = false;
    bool enable_dialog_border = true;
    int auto_quick_save = 0;
    bool enable_high_resolution_stencil = true;
    int inventory_columns = 1;
};

// These are settings handled by preferences UI and saved in save games.
struct PreferencesSettings {
    int game_difficulty = GAME_DIFFICULTY_NORMAL;
    int combat_difficulty = COMBAT_DIFFICULTY_NORMAL;
    int violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    int target_highlight = TARGET_HIGHLIGHT_TARGETING_ONLY;
    bool item_highlight = true;
    bool combat_looks = false;
    bool combat_messages = true;
    bool combat_taunts = true;
    bool language_filter = false;
    bool running = false;
    bool subtitles = false;
    int combat_speed = 0;
    bool player_speedup = false;
    double text_base_delay = 3.5;
    double text_line_delay = 1.399994;
    double brightness = 1.0;
    double mouse_sensitivity = 1.0;
    bool running_burning_guy = true;
};

struct SoundSettings {
    bool initialize = true;
    bool debug = false;
    bool debug_sfxc = true;
    bool sounds = true;
    bool music = true;
    bool speech = true;
    int master_volume = 22281;
    int music_volume = 22281;
    int sndfx_volume = 22281;
    int speech_volume = 22281;
    int cache_size = 448;
    std::string music_path1 = "sound\\music\\";
    std::string music_path2 = "sound\\music\\";
    int gapless_music = 1;
};

struct DebugSettings {
    std::string mode = "environment";
    bool show_tile_num = false;
    bool show_script_messages = false;
    bool show_load_info = false;
    bool output_map_data_info = false;
    int window_width = 300;
    int window_height = 192;
    std::string console_output_path = "";
};

struct QolSettings {
    int use_walk_distance = 5;
    bool auto_open_doors = false;
    bool party_loot_and_barter = false;
};

struct MapperSettings {
    bool override_librarian = false;
    bool librarian = false;
    bool use_art_not_protos = false;
    bool rebuild_protos = false;
    bool fix_map_objects = false;
    bool fix_map_inventory = false;
    bool ignore_rebuild_errors = false;
    bool show_pid_numbers = false;
    bool save_text_maps = false;
    bool run_mapper_as_game = false;
    bool default_f8_as_game = true;
    bool sort_script_list = false;
};

struct Settings {
    SystemSettings system;
    ScreenSettings screen;
    UISettings ui;
    PreferencesSettings preferences;
    SoundSettings sound;
    DebugSettings debug;
    QolSettings qol;
    MapperSettings mapper;
};

extern Settings settings;

bool settingsInit(bool isMapper, int argc, char** argv);
bool settingsSave();
void settingsWriteToConfig(bool onlyAdd = false);
bool settingsExit(bool shouldSave);

} // namespace fallout

#endif /* FALLOUT_SETTINGS_H_ */
