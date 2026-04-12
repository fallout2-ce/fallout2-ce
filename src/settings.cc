#include "settings.h"
#include "game_config.h"
#include "platform_compat.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace fallout {

static void normalizePath(std::string& value)
{
    char* path = value.data();
    compat_windows_path_to_native(path);
    compat_resolve_path(path);
}

template <typename T>
static std::function<void(T&)> clamp(T min, T max)
{
    return [min, max](T& value) { value = std::clamp(value, min, max); };
}

struct SettingDescriptor {
    std::function<void()> read;
    std::function<void()> write;
};

static std::vector<SettingDescriptor> settingsRegistry;

template <typename T, typename P = std::nullptr_t>
void registerSetting(const char* section, const char* key, T& variable, const P& postProcess = nullptr)
{
    settingsRegistry.push_back(
        { [&, section, key, postProcess]() {
             settingsRead(section, key, variable);
             if constexpr (!std::is_same_v<P, std::nullptr_t>) {
                 postProcess(variable);
             }
         },
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

struct SettingEntry {
    std::function<void(const char*)> registerFunc;

    template <typename T>
    SettingEntry(const char* key, T& variable)
        : registerFunc([key, &variable](const char* section) { registerSetting(section, key, variable); })
    {
    }

    template <typename T, typename P>
    SettingEntry(const char* key, T& variable, const P& postProcess)
        : registerFunc([key, &variable, postProcess](const char* section) { registerSetting(section, key, variable, postProcess); })
    {
    }
};

static void addSection(const char* section, const std::initializer_list<SettingEntry> entries)
{
    for (const auto& entry : entries) {
        entry.registerFunc(section);
    }
}

void initSettingsRegistry(bool isMapper)
{
    if (!settingsRegistry.empty()) return;

    addSection(GAME_CONFIG_SYSTEM_KEY,
        {
            { "executable", settings.system.executable },
            { GAME_CONFIG_MASTER_DAT_KEY, settings.system.master_dat_path, normalizePath },
            { GAME_CONFIG_MASTER_PATCHES_KEY, settings.system.master_patches_path, normalizePath },
            { GAME_CONFIG_CRITTER_DAT_KEY, settings.system.critter_dat_path, normalizePath },
            { GAME_CONFIG_CRITTER_PATCHES_KEY, settings.system.critter_patches_path, normalizePath },
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
            { GAME_CONFIG_RESOLUTION_X_KEY, settings.screen.resolution_x, clamp(640, 7680) },
            { GAME_CONFIG_RESOLUTION_Y_KEY, settings.screen.resolution_y, clamp(480, 4320) },
            { GAME_CONFIG_WINDOWED_KEY, settings.screen.windowed },
            { GAME_CONFIG_SCALE_KEY, settings.screen.scale, clamp(1, 4) },
        });

    addSection(GAME_CONFIG_UI_KEY,
        {
            { GAME_CONFIG_IFACE_BAR_MODE_KEY, settings.ui.iface_bar_mode },
            { GAME_CONFIG_IFACE_BAR_WIDTH_KEY, settings.ui.iface_bar_width, clamp(640, 4320) },
            { GAME_CONFIG_IFACE_BAR_SIDE_ART_KEY, settings.ui.iface_bar_side_art, clamp(0, 999) },
            { GAME_CONFIG_IFACE_BAR_SIDES_ORI_KEY, settings.ui.iface_bar_sides_ori },
            { GAME_CONFIG_SPLASH_SCREEN_SIZE_KEY, settings.ui.splash_screen_size, clamp(0, 2) },
            { GAME_CONFIG_IGNORE_MAP_EDGES_KEY, settings.ui.ignore_map_edges },
            { "ui_anim_speed", settings.ui.ui_anim_speed, clamp(0.1, 100.0) },
        });

    addSection("preferences",
        {
            // Clamping for most of these values is handled in preferences.cc
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
            { GAME_CONFIG_MUSIC_PATH1_KEY, settings.sound.music_path1, normalizePath },
            { GAME_CONFIG_MUSIC_PATH2_KEY, settings.sound.music_path2, normalizePath },
        });

    addSection("debug",
        {
            { "mode", settings.debug.mode },
            { "show_tile_num", settings.debug.show_tile_num },
            { "show_script_messages", settings.debug.show_script_messages },
            { "show_load_info", settings.debug.show_load_info },
            { "output_map_data_info", settings.debug.output_map_data_info },
            { "window_width", settings.debug.debug_window_width, clamp(200, 1920) },
            { "window_height", settings.debug.debug_window_height, clamp(100, 1080) },
        });

    if (isMapper) {
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
}

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
