#include "settings.h"
#include "game_config.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace fallout {

struct SettingDescriptor {
    std::function<void()> read;
    std::function<void()> write;
};

static std::vector<SettingDescriptor> settingsRegistry;

template <typename T>
void registerSetting(const char* section, const char* key, T& variable)
{
    settingsRegistry.push_back(
        { [&, section, key]() { settingsRead(section, key, variable); },
            [&, section, key]() { settingsWrite(section, key, variable); } });
}

template <typename T>
void registerSetting(const char* section, const char* key, T& variable, T min, T max)
{
    settingsRegistry.push_back(
        { [&, section, key, min, max]() { settingsRead(section, key, variable, min, max); },
            [&, section, key]() { settingsWrite(section, key, variable); } });
}

Settings settings;

static void settingsRead(const char* section, const char* key, std::string& value)
{
    char* v;
    if (configGetString(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static void settingsRead(const char* section, const char* key, int& value)
{
    int v;
    if (configGetInt(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static void settingsRead(const char* section, const char* key, int& value, int minValue, int maxValue)
{
    settingsRead(section, key, value);
    value = std::clamp(value, minValue, maxValue);
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

static void settingsRead(const char* section, const char* key, double& value, double minValue, double maxValue)
{
    settingsRead(section, key, value);
    value = std::clamp(value, minValue, maxValue);
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

struct SettingEntry {
    std::function<void(const char*)> registerFunc;

    SettingEntry(const char* key, std::string& variable)
        : registerFunc([key, &variable](const char* section) { registerSetting(section, key, variable); })
    {
    }

    SettingEntry(const char* key, int& variable)
        : registerFunc([key, &variable](const char* section) { registerSetting(section, key, variable); })
    {
    }

    SettingEntry(const char* key, int& variable, int min, int max)
        : registerFunc([key, &variable, min, max](const char* section) { registerSetting(section, key, variable, min, max); })
    {
    }

    SettingEntry(const char* key, bool& variable)
        : registerFunc([key, &variable](const char* section) { registerSetting(section, key, variable); })
    {
    }

    SettingEntry(const char* key, double& variable)
        : registerFunc([key, &variable](const char* section) { registerSetting(section, key, variable); })
    {
    }

    SettingEntry(const char* key, double& variable, double min, double max)
        : registerFunc([key, &variable, min, max](const char* section) { registerSetting(section, key, variable, min, max); })
    {
    }
};

void initSettingsRegistry()
{
    if (!settingsRegistry.empty()) return;

    auto addSection = [](const char* section, std::initializer_list<SettingEntry> entries) {
        for (const auto& entry : entries) {
            entry.registerFunc(section);
        }
    };

    addSection(GAME_CONFIG_SYSTEM_KEY,
        {
            { "executable", settings.system.executable },
            { GAME_CONFIG_MASTER_DAT_KEY, settings.system.master_dat_path },
            { GAME_CONFIG_MASTER_PATCHES_KEY, settings.system.master_patches_path },
            { GAME_CONFIG_CRITTER_DAT_KEY, settings.system.critter_dat_path },
            { GAME_CONFIG_CRITTER_PATCHES_KEY, settings.system.critter_patches_path },
            { "language", settings.system.language },
            { "scroll_lock", settings.system.scroll_lock },
            { "interrupt_walk", settings.system.interrupt_walk },
            { "art_cache_size", settings.system.art_cache_size },
            { "color_cycling", settings.system.color_cycling },
            { "cycle_speed_factor", settings.system.cycle_speed_factor },
            { "hashing", settings.system.hashing },
            { "splash", settings.system.splash },
            { "free_space", settings.system.free_space },
        });

    addSection(GAME_CONFIG_SCREEN_KEY,
        {
            { GAME_CONFIG_RESOLUTION_X_KEY, settings.screen.resolution_x, 640, 7680 },
            { GAME_CONFIG_RESOLUTION_Y_KEY, settings.screen.resolution_y, 480, 4320 },
            { GAME_CONFIG_WINDOWED_KEY, settings.screen.windowed },
            { GAME_CONFIG_SCALE_KEY, settings.screen.scale, 1, 4 },
        });

    addSection(GAME_CONFIG_UI_KEY,
        {
            { GAME_CONFIG_IFACE_BAR_MODE_KEY, settings.ui.iface_bar_mode },
            { GAME_CONFIG_IFACE_BAR_WIDTH_KEY, settings.ui.iface_bar_width, 640, 4320 },
            { GAME_CONFIG_IFACE_BAR_SIDE_ART_KEY, settings.ui.iface_bar_side_art, 0, 999 },
            { GAME_CONFIG_IFACE_BAR_SIDES_ORI_KEY, settings.ui.iface_bar_sides_ori },
            { GAME_CONFIG_SPLASH_SCREEN_SIZE_KEY, settings.ui.splash_screen_size, 0, 2 },
            { GAME_CONFIG_IGNORE_MAP_EDGES_KEY, settings.ui.ignore_map_edges },
        });

    addSection("preferences",
        {
            { "game_difficulty", settings.preferences.game_difficulty },
            { "combat_difficulty", settings.preferences.combat_difficulty },
            { "violence_level", settings.preferences.violence_level },
            { "target_highlight", settings.preferences.target_highlight },
            { "item_highlight", settings.preferences.item_highlight },
            { "combat_looks", settings.preferences.combat_looks },
            { "combat_messages", settings.preferences.combat_messages },
            { "combat_taunts", settings.preferences.combat_taunts },
            { "language_filter", settings.preferences.language_filter },
            { "running", settings.preferences.running },
            { "subtitles", settings.preferences.subtitles },
            { "combat_speed", settings.preferences.combat_speed },
            { "player_speedup", settings.preferences.player_speedup },
            { "text_base_delay", settings.preferences.text_base_delay },
            { "text_line_delay", settings.preferences.text_line_delay },
            { "brightness", settings.preferences.brightness },
            { "mouse_sensitivity", settings.preferences.mouse_sensitivity },
            { "running_burning_guy", settings.preferences.running_burning_guy },
            { "ui_anim_speed", settings.preferences.ui_anim_speed, 0.1, 100.0 },
        });

    addSection(GAME_CONFIG_SOUND_KEY,
        {
            { "initialize", settings.sound.initialize },
            { "debug", settings.sound.debug },
            { "debug_sfxc", settings.sound.debug_sfxc },
            { "device", settings.sound.device },
            { "port", settings.sound.port },
            { "irq", settings.sound.irq },
            { "dma", settings.sound.dma },
            { "sounds", settings.sound.sounds },
            { "music", settings.sound.music },
            { "speech", settings.sound.speech },
            { "master_volume", settings.sound.master_volume },
            { "music_volume", settings.sound.music_volume },
            { "sndfx_volume", settings.sound.sndfx_volume },
            { "speech_volume", settings.sound.speech_volume },
            { "cache_size", settings.sound.cache_size },
            { GAME_CONFIG_MUSIC_PATH1_KEY, settings.sound.music_path1 },
            { GAME_CONFIG_MUSIC_PATH2_KEY, settings.sound.music_path2 },
        });

    addSection("debug",
        {
            { "mode", settings.debug.mode },
            { "show_tile_num", settings.debug.show_tile_num },
            { "show_script_messages", settings.debug.show_script_messages },
            { "show_load_info", settings.debug.show_load_info },
            { "output_map_data_info", settings.debug.output_map_data_info },
            { "window_width", settings.debug.debug_window_width, 200, 1920 },
            { "window_height", settings.debug.debug_window_height, 100, 1080 },
        });

    addSection("mapper",
        {
            { "override_librarian", settings.mapper.override_librarian },
            { "librarian", settings.mapper.librarian },
            { "use_art_not_protos", settings.mapper.user_art_not_protos },
            { "rebuild_protos", settings.mapper.rebuild_protos },
            { "fix_map_objects", settings.mapper.fix_map_objects },
            { "fix_map_inventory", settings.mapper.fix_map_inventory },
            { "ignore_rebuild_errors", settings.mapper.ignore_rebuild_errors },
            { "show_pid_numbers", settings.mapper.show_pid_numbers },
            { "save_text_maps", settings.mapper.save_text_maps },
            { "run_mapper_as_game", settings.mapper.run_mapper_as_game },
            { "default_f8_as_game", settings.mapper.default_f8_as_game },
            { "sort_script_list", settings.mapper.sort_script_list },
        });
}

bool settingsInit(bool isMapper, int argc, char** argv)
{
    initSettingsRegistry();
    if (!gameConfigInit(isMapper, argc, argv)) {
        return false;
    }

    for (const auto& descriptor : settingsRegistry) {
        descriptor.read();
    }

    return true;
}

void settingsWriteToConfig()
{
    for (const auto& descriptor : settingsRegistry) {
        descriptor.write();
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
        settingsSave();
    }

    return gameConfigExit(shouldSave);
}

} // namespace fallout
