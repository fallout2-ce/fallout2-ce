#include "settings.h"

#include "debug.h"
#include "game_config.h"
#include "platform_compat.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace fallout {

struct SettingDescriptor {
    std::function<void()> read;
    std::function<void(bool onlyAdd)> write;
};

static std::vector<SettingDescriptor> settingsRegistry;

Settings settings;

static void settingsRead(const char* section, const char* key, std::string& value)
{
    char* v;
    if (configGetString(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static bool settingsKeyExists(const char* section, const char* key)
{
    char* v;
    return configGetString(&gGameConfig, section, key, &v);
}

static void settingsRead(const char* section, const char* key, int& value)
{
    int v;
    if (configGetInt(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static void settingsRead(const char* section, const char* key, bool& value)
{
    bool v;
    if (configGetBool(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static void settingsRead(const char* section, const char* key, double& value)
{
    double v;
    if (configGetDouble(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static void settingsWrite(const char* section, const char* key, const std::string& value)
{
    configSetString(&gGameConfig, section, key, value.c_str());
}

static void settingsWrite(const char* section, const char* key, int value)
{
    configSetInt(&gGameConfig, section, key, value);
}

static void settingsWrite(const char* section, const char* key, bool value)
{
    configSetBool(&gGameConfig, section, key, value);
}

static void settingsWrite(const char* section, const char* key, double value)
{
    configSetDouble(&gGameConfig, section, key, value);
}

static void normalizePath(std::string& value, const char* section, const char* key)
{
    char* path = value.data();
    compat_windows_path_to_native(path);
    compat_resolve_path(path);
}

template <typename T>
static std::function<void(T&, const char*, const char*)> clamp(T min, T max)
{
    return [min, max](T& value, const char* section, const char* key) {
        const T origValue = value;
        value = std::clamp(value, min, max);
        if (value != origValue) {
            debugPrint("config value %s.%s was clamped.\n", section, key);
        }
    };
}

template <typename T, typename P = std::function<void(T&, const char*, const char*)>>
void registerSetting(const char* section, const char* key, T& variable, P postProcess = {})
{
    settingsRegistry.push_back(
        { [&, section, key, postProcess]() {
             settingsRead(section, key, variable);
             if (postProcess) postProcess(variable, section, key);
         },
            [&, section, key](bool onlyAdd) {
                if (onlyAdd && settingsKeyExists(section, key)) return;
                settingsWrite(section, key, variable);
            } });
}

// SECT must be defined to the settings sub-struct name, which equals the config section string.
#define XSTR(x) #x
#define STR(x) XSTR(x)
#define SETTING(f) registerSetting(STR(SECT), #f, settings.SECT.f)
#define SETTING_P(f, proc) registerSetting(STR(SECT), #f, settings.SECT.f, proc)
#define SETTING_PATH(f) registerSetting(STR(SECT), #f, settings.SECT.f##_path, normalizePath)

void initSettingsRegistry(bool isMapper)
{
    if (!settingsRegistry.empty()) return;

#define SECT system
    SETTING(executable);
    SETTING_PATH(master_dat);
    SETTING_PATH(master_patches);
    SETTING_PATH(critter_dat);
    SETTING_PATH(critter_patches);
    SETTING(language);
    SETTING(scroll_lock);
    SETTING(interrupt_walk);
    SETTING(art_cache_size);
    SETTING(color_cycling);
    SETTING(cycle_speed_factor);
    SETTING(hashing);
    SETTING(splash);
    SETTING(free_space);
    SETTING(screenshots_format);
#undef SECT

#define SECT screen
    SETTING_P(resolution_x, clamp(640, 7680));
    SETTING_P(resolution_y, clamp(480, 4320));
    SETTING(windowed);
    SETTING_P(scale, clamp(1, 4));
#undef SECT

#define SECT ui
    SETTING(iface_bar_mode);
    SETTING_P(iface_bar_width, clamp(640, 4320));
    SETTING_P(iface_bar_side_art, clamp(0, 999));
    SETTING(iface_bar_sides_ori);
    SETTING_P(splash_screen_size, clamp(0, 2));
    SETTING(ignore_map_edges);
    SETTING_P(anim_speed, clamp(0.1, 100.0));
    SETTING_P(skip_opening_movies, clamp(0, 2));
    SETTING(display_karma_changes);
    SETTING(display_bonus_damage);
    SETTING(numbers_in_dialogue);
    SETTING_P(auto_quick_save, clamp(0, 10));
    SETTING(enable_high_resolution_stencil);
    SETTING(extend_ap_bar);
    SETTING(expand_barter_window);
    SETTING_P(max_inventory_columns, clamp(1, 4));
#undef SECT

#define SECT preferences
    // Clamping for most of these values is handled in preferences.cc
    SETTING(game_difficulty);
    SETTING(combat_difficulty);
    SETTING(violence_level);
    SETTING(target_highlight);
    SETTING(item_highlight);
    SETTING(combat_looks);
    SETTING(combat_messages);
    SETTING(combat_taunts);
    SETTING(language_filter);
    SETTING(running);
    SETTING(subtitles);
    SETTING(combat_speed);
    SETTING(player_speedup);
    SETTING(text_base_delay);
    SETTING(text_line_delay);
    SETTING(brightness);
    SETTING(mouse_sensitivity);
    SETTING(running_burning_guy);
#undef SECT

#define SECT sound
    SETTING(initialize);
    SETTING(debug);
    SETTING(debug_sfxc);
    SETTING(sounds);
    SETTING(music);
    SETTING(speech);
    SETTING(master_volume);
    SETTING(music_volume);
    SETTING(sndfx_volume);
    SETTING(speech_volume);
    SETTING(cache_size);
    SETTING_P(music_path1, normalizePath);
    SETTING_P(music_path2, normalizePath);
    SETTING(gapless_music);
#undef SECT

#define SECT debug
    SETTING(mode);
    SETTING(show_tile_num);
    SETTING(show_script_messages);
    SETTING(show_load_info);
    SETTING(output_map_data_info);
    SETTING_P(window_width, clamp(200, 1920));
    SETTING_P(window_height, clamp(100, 1080));
    SETTING(console_output_path);
#undef SECT

#define SECT qol
    SETTING_P(use_walk_distance, clamp(0, 100));
    SETTING(auto_open_doors);
#undef SECT

    if (isMapper) {
#define SECT mapper
        SETTING(override_librarian);
        SETTING(librarian);
        SETTING(use_art_not_protos);
        SETTING(rebuild_protos);
        SETTING(fix_map_objects);
        SETTING(fix_map_inventory);
        SETTING(ignore_rebuild_errors);
        SETTING(show_pid_numbers);
        SETTING(save_text_maps);
        SETTING(run_mapper_as_game);
        SETTING(default_f8_as_game);
        SETTING(sort_script_list);
#undef SECT
    }
}

#undef SETTING
#undef SETTING_P
#undef SETTING_PATH
#undef STR
#undef XSTR

bool settingsInit(bool isMapper, int argc, char** argv)
{
    initSettingsRegistry(isMapper);
    if (!gameConfigInit(isMapper, argc, argv)) {
        return false;
    }

    for (const auto& descriptor : settingsRegistry) {
        descriptor.read();
    }

    return true;
}

void settingsWriteToConfig(bool onlyAdd)
{
    for (const auto& descriptor : settingsRegistry) {
        descriptor.write(onlyAdd);
    }
}

bool settingsSave()
{
    settingsWriteToConfig();
    return gameConfigSave();
}

bool settingsExit(bool shouldSave)
{
    if (shouldSave) {
        settingsWriteToConfig();
    }

    return gameConfigExit(shouldSave);
}

} // namespace fallout
