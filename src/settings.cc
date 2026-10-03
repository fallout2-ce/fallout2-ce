#include "settings.h"

#include "audio_channels.h"
#include "debug.h"
#include "game_config.h"
#include "platform_compat.h"
#include "sound.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace fallout {

bool SystemSettings::executableIsMapper() const { return compat_stricmp(executable.c_str(), "mapper") == 0; }

struct SettingRegistryEntry {
    SettingDescriptor descriptor;
    std::function<void()> read;
    std::function<void(bool onlyAdd)> write;
    std::function<SettingValue()> getValue;
    std::function<void(const SettingValue&)> setValue;
    std::function<bool(const SettingValue&)> validateValue;
    std::optional<SettingValue> restartValue;
};

static std::vector<SettingRegistryEntry> settingsRegistry;
static std::vector<SettingDescriptor> settingDescriptors;

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

template <typename T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
static void settingsRead(const char* section, const char* key, T& value)
{
    int v;
    if (configGetInt(&gGameConfig, section, key, &v)) {
        value = static_cast<T>(v);
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

template <typename T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
static void settingsWrite(const char* section, const char* key, T value)
{
    configSetInt(&gGameConfig, section, key, static_cast<int>(value));
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
class Clamp {
public:
    Clamp(T min, T max)
        : min_(min)
        , max_(max)
    {
    }

    explicit operator bool() const { return true; }

    void operator()(T& value, const char* section, const char* key) const
    {
        const T origValue = value;
        value = std::clamp(value, min_, max_);
        if (value != origValue) {
            debugPrint("config value %s.%s was clamped.\n", section, key);
        }
    }

private:
    T min_;
    T max_;
};

template <typename T>
static Clamp<T> clamp(T min, T max)
{
    return Clamp<T>(min, max);
}

template <typename T, typename P>
static bool validatePostProcess(const P&, const T&, const char*, const char*)
{
    return true;
}

template <typename T>
static bool validatePostProcess(const Clamp<T>& constraint, const T& value, const char* section, const char* key)
{
    T processed = value;
    constraint(processed, section, key);
    return processed == value;
}

template <typename T>
static SettingValueType settingValueType()
{
    if constexpr (std::is_same_v<T, bool>) {
        return SettingValueType::Boolean;
    } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
        return SettingValueType::Integer;
    } else if constexpr (std::is_floating_point_v<T>) {
        return SettingValueType::Real;
    } else {
        return SettingValueType::Text;
    }
}

template <typename T>
static SettingValue makeSettingValue(const T& value)
{
    if constexpr (std::is_enum_v<T>) {
        return static_cast<int>(value);
    } else {
        return value;
    }
}

template <typename T>
static void assignSettingValue(T& target, const SettingValue& value)
{
    if constexpr (std::is_enum_v<T>) {
        const auto* typedValue = std::get_if<int>(&value);
        assert(typedValue != nullptr);
        target = static_cast<T>(*typedValue);
    } else {
        const auto* typedValue = std::get_if<T>(&value);
        assert(typedValue != nullptr);
        target = *typedValue;
    }
}

template <typename T, typename P = std::function<void(T&, const char*, const char*)>>
void registerSetting(const char* section,
    const char* key,
    T& variable,
    P postProcess = {})
{
    SettingDescriptor descriptor;
    descriptor.id = std::string(section) + "." + key;
    descriptor.source = "fallout2.cfg";
    descriptor.section = section;
    descriptor.key = key;
    descriptor.valueType = settingValueType<T>();
    descriptor.defaultValue = makeSettingValue(variable);
    descriptor.categoryOrder = static_cast<int>(settingDescriptors.size());

    settingsRegistry.push_back(
        { descriptor,
            [&, section, key, postProcess]() {
                settingsRead(section, key, variable);
                if (postProcess) postProcess(variable, section, key);
            },
            [&, section, key](bool onlyAdd) {
                if (onlyAdd && settingsKeyExists(section, key)) return;
                settingsWrite(section, key, variable);
            },
            [&variable]() { return makeSettingValue(variable); },
            [&variable, section, key, postProcess](const SettingValue& value) {
                assignSettingValue(variable, value);
                if (postProcess) postProcess(variable, section, key);
            },
            [section, key, postProcess](const SettingValue& value) {
                T candidate;
                assignSettingValue(candidate, value);
                return validatePostProcess(postProcess, candidate, section, key);
            } });

    settingDescriptors.push_back(std::move(descriptor));
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
    SETTING_PATH(f2_res_dat);
    SETTING_PATH(critter_dat);
    SETTING_PATH(critter_patches);
    SETTING(language);
    SETTING(scroll_lock);
    SETTING(interrupt_walk);
    SETTING_P(art_cache_size, clamp(32, 512));
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
    SETTING_P(windowed, clamp(WindowMode::Fullscreen, WindowMode::WindowedFullscreen));
    SETTING(mouse_lock);
    SETTING_P(scale, clamp(1, 4));
#undef SECT

#define SECT ui
    SETTING_P(main_menu_scale_mode, clamp(0, 2));
    SETTING(in_game_menu_help);
    SETTING(main_menu_overlay_subscreens);
    SETTING(iface_bar_mode);
    SETTING(perks_progress_bar);
    SETTING_P(iface_bar_width, clamp(640, 4320));
    SETTING_P(iface_bar_side_art, clamp(0, 999));
    SETTING(iface_bar_sides_ori);
    SETTING_P(alternate_ammo_meter, clamp(0, 2));
    SETTING_P(splash_screen_size, clamp(0, 2));
    SETTING_P(death_screen_size, clamp(0, 1));
    SETTING_P(end_slide_size, clamp(0, 1));
    SETTING(movie_aspect_fit);
    SETTING(edg_support);
    SETTING(ignore_scroll_limit);
    SETTING(ignore_map_edges);
    SETTING(quick_toolbar_visible);
    SETTING_P(anim_speed, clamp(0.1, 100.0));
    SETTING_P(skip_opening_movies, clamp(0, 2));
    SETTING(display_karma_changes);
    SETTING(display_bonus_damage);
    SETTING(numbers_in_dialogue);
    SETTING(party_member_extra_info);
    SETTING(dialog_border);
    SETTING_P(auto_quick_save, clamp(0, 10));
    SETTING(enable_high_resolution_stencil);
    SETTING(extend_ap_bar);
    SETTING(expand_barter_window);
    SETTING_P(inventory_columns, clamp(1, 2));
    SETTING_P(loot_weight_indicator, clamp(0, 3));
    SETTING_P(loot_container_size_indicator_threshold, clamp(0, 100));
#undef SECT

#define SECT gameplay
    SETTING_P(perk_carryover, clamp(PERK_CARRY_OVER_MODE_OFF, PERK_CARRY_OVER_MODE_SFALL));
#undef SECT

#define SECT preferences
    // Clamping for these values is handled by the legacy preferences screen.
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
    SETTING(float_speech);
    SETTING(pipboy_speech);
    SETTING(master_volume);
    SETTING(music_volume);
    SETTING(sndfx_volume);
    SETTING(speech_volume);
    // SETTING_P(float_volume, clamp(VOLUME_MIN, VOLUME_MAX));
    // SETTING_P(pipboy_volume, clamp(VOLUME_MIN, VOLUME_MAX));
    SETTING(cache_size);
    SETTING_P(music_path1, normalizePath);
    SETTING_P(music_path2, normalizePath);
    SETTING(gapless_music);
#define CHANNELS(f, type) SETTING_P(f, clamp(kAudioChannelLimits[type].min, kAudioChannelLimits[type].max))
    CHANNELS(music_channels, AUDIO_CHANNEL_MUSIC);
    CHANNELS(speech_channels, AUDIO_CHANNEL_SPEECH);
    CHANNELS(sfx_channels, AUDIO_CHANNEL_SFX);
    CHANNELS(movie_channels, AUDIO_CHANNEL_MOVIE);
    CHANNELS(script_channels, AUDIO_CHANNEL_SCRIPT);
    CHANNELS(float_channels, AUDIO_CHANNEL_FLOAT);
    CHANNELS(pipboy_channels, AUDIO_CHANNEL_PIPBOY);
#undef CHANNELS
#undef SECT

#define SECT debug
    SETTING(mode);
    SETTING(show_fps);
    SETTING(show_tile_num);
    SETTING(show_script_messages);
    SETTING(show_load_info);
    SETTING(output_map_data_info);
    SETTING_P(window_width, clamp(200, 1920));
    SETTING_P(window_height, clamp(100, 1080));
    SETTING(console_output_path);
#undef SECT

#define SECT combat_ai
    SETTING(npcs_try_to_spend_extra_ap);
#undef SECT

#define SECT qol
    SETTING_P(use_walk_distance, clamp(0, 100));
    SETTING(walk_when_sneaking);
    SETTING(auto_open_doors);
    SETTING(party_trade_from_menu);
    SETTING(party_loot_and_barter);
    SETTING(fast_ammo_load);
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
        SETTING(map);
        SETTING(dev_path);
        SETTING(use_grid_item_picker);
#undef SECT
    }

    for (size_t index = 0; index < settingDescriptors.size(); index++) {
        const SettingDescriptor& descriptor = settingDescriptors[index];
        assert(!descriptor.id.empty());
        assert(settingsValidateValue(descriptor, descriptor.defaultValue));
        for (size_t otherIndex = index + 1; otherIndex < settingDescriptors.size(); otherIndex++) {
            assert(descriptor.id != settingDescriptors[otherIndex].id);
        }
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
    for (size_t index = 0; index < settingsRegistry.size(); index++) {
        settingsRegistry[index].restartValue.reset();
        settingsRegistry[index].descriptor.commandLineOverride = false;
        settingDescriptors[index].commandLineOverride = false;
    }
    if (!gameConfigInit(isMapper, argc, argv)) {
        return false;
    }

    for (const auto& entry : settingsRegistry) {
        entry.read();
    }

    return true;
}

void settingsMarkCommandLineOverride(const char* section, const char* key)
{
    for (size_t index = 0; index < settingsRegistry.size(); index++) {
        SettingDescriptor& descriptor = settingDescriptors[index];
        if (compat_stricmp(descriptor.section.c_str(), section) == 0
            && compat_stricmp(descriptor.key.c_str(), key) == 0) {
            descriptor.commandLineOverride = true;
            settingsRegistry[index].descriptor.commandLineOverride = true;
            return;
        }
    }
}

void settingsWriteToConfig(bool onlyAdd)
{
    for (const auto& entry : settingsRegistry) {
        if (entry.restartValue.has_value()) {
            const auto& descriptor = entry.descriptor;
            if (onlyAdd && settingsKeyExists(descriptor.section.c_str(), descriptor.key.c_str())) continue;
            // Non-throwing access also supports iOS deployment targets below 12.
            // TODO replace with std::visit
            const auto& value = *entry.restartValue;
            const char* section = descriptor.section.c_str();
            const char* key = descriptor.key.c_str();
            if (const auto* boolean = std::get_if<bool>(&value)) {
                settingsWrite(section, key, *boolean);
            } else if (const auto* integer = std::get_if<int>(&value)) {
                settingsWrite(section, key, *integer);
            } else if (const auto* real = std::get_if<double>(&value)) {
                settingsWrite(section, key, *real);
            } else {
                const auto* text = std::get_if<std::string>(&value);
                assert(text != nullptr);
                settingsWrite(section, key, *text);
            }
        } else {
            entry.write(onlyAdd);
        }
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

    bool result = gameConfigExit(shouldSave);
    for (auto& entry : settingsRegistry) {
        entry.restartValue.reset();
    }
    return result;
}

const std::vector<SettingDescriptor>& settingsGetDescriptors()
{
    return settingDescriptors;
}

static SettingRegistryEntry* settingsFindEntry(const SettingDescriptor& descriptor)
{
    auto it = std::find_if(settingsRegistry.begin(), settingsRegistry.end(), [&descriptor](const SettingRegistryEntry& entry) {
        return entry.descriptor.id == descriptor.id;
    });
    return it != settingsRegistry.end() ? &*it : nullptr;
}

SettingValue settingsGetValue(const SettingDescriptor& descriptor)
{
    const SettingRegistryEntry* entry = settingsFindEntry(descriptor);
    assert(entry != nullptr);
    return entry != nullptr ? entry->getValue() : descriptor.defaultValue;
}

SettingValue settingsGetConfiguredValue(const SettingDescriptor& descriptor)
{
    const SettingRegistryEntry* entry = settingsFindEntry(descriptor);
    assert(entry != nullptr);
    return entry != nullptr && entry->restartValue.has_value()
        ? *entry->restartValue
        : settingsGetValue(descriptor);
}

bool settingsValidateValue(const SettingDescriptor& descriptor, const SettingValue& value, std::string* error)
{
    const SettingRegistryEntry* entry = settingsFindEntry(descriptor);
    if (entry == nullptr) {
        if (error != nullptr) *error = "Setting is not registered.";
        return false;
    }

    const SettingDescriptor& registeredDescriptor = entry->descriptor;
    if (descriptor.valueType != registeredDescriptor.valueType) {
        if (error != nullptr) *error = "Setting type does not match the registered type.";
        return false;
    }

    bool typeMatches = (registeredDescriptor.valueType == SettingValueType::Boolean && std::holds_alternative<bool>(value))
        || ((registeredDescriptor.valueType == SettingValueType::Integer
                || registeredDescriptor.valueType == SettingValueType::Choice
                || registeredDescriptor.valueType == SettingValueType::KeyBinding)
            && std::holds_alternative<int>(value))
        || (registeredDescriptor.valueType == SettingValueType::Real && std::holds_alternative<double>(value))
        || (registeredDescriptor.valueType == SettingValueType::Text && std::holds_alternative<std::string>(value));
    if (!typeMatches) {
        if (error != nullptr) *error = "Value has the wrong type.";
        return false;
    }

    if (const auto* text = std::get_if<std::string>(&value)) {
        if (text->find_first_of("#;\r\n") != std::string::npos
            || text->find('\0') != std::string::npos) {
            if (error != nullptr) *error = "Text cannot contain #, ;, line breaks, or null characters.";
            return false;
        }
    }

    if (!registeredDescriptor.choices.empty() && std::holds_alternative<int>(value)) {
        int selectedValue = *std::get_if<int>(&value);
        bool validChoice = std::any_of(registeredDescriptor.choices.begin(), registeredDescriptor.choices.end(), [selectedValue](const SettingChoice& choice) {
            return choice.value == selectedValue;
        });
        if (!validChoice) {
            if (error != nullptr) *error = "Value is not an available choice.";
            return false;
        }
    }

    if (!entry->validateValue(value)) {
        if (error != nullptr) *error = "Value is outside the allowed range.";
        return false;
    }
    return true;
}

bool settingsSetValue(const SettingDescriptor& descriptor, const SettingValue& value, std::string* error)
{
    SettingRegistryEntry* entry = settingsFindEntry(descriptor);
    if (entry == nullptr) {
        if (error != nullptr) *error = "Setting is not registered.";
        return false;
    }
    if (entry->descriptor.readOnly) {
        if (error != nullptr) *error = "Setting is read-only.";
        return false;
    }
    if (entry->descriptor.commandLineOverride) {
        if (error != nullptr) *error = "Setting is overridden by the command line.";
        return false;
    }
    if (!settingsValidateValue(entry->descriptor, value, error)) {
        return false;
    }

    switch (entry->descriptor.applyPolicy) {
    case SettingApplyPolicy::OnClose:
        entry->setValue(value);
        break;
    case SettingApplyPolicy::Restart:
        if (value == entry->getValue()) {
            entry->restartValue.reset();
        } else {
            entry->restartValue = value;
        }
        break;
    case SettingApplyPolicy::NextGame:
        if (error != nullptr) *error = "Applying settings on the next game is not supported yet.";
        return false;
    }
    return true;
}

} // namespace fallout
