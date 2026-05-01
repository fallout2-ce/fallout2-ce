#include "game_config.h"
#include "debug.h"
#include "game_config_migration.h"
#include "settings.h"
#include "sfall_config.h"

#include <stdio.h>
#include <string.h>

#include "db.h"
#include "main.h"
#include "platform_compat.h"
#include "scan_unimplemented.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace fallout {

constexpr char kDefaultGameConfigFileName[] = "fallout2.cfg";
constexpr char kMapperConfigFileName[] = "mapper2.cfg";

// A flag indicating if [gGameConfig] was initialized.
//
// 0x5186D0
bool gGameConfigInitialized = false;

// fallout2.cfg
//
// 0x58E950
Config gGameConfig;

// NOTE: There are additional 4 bytes following this array at 0x58EA7C, which
// probably means it's size is 264 bytes.
//
// 0x58E978
char gGameConfigFilePath[COMPAT_MAX_PATH];

// Inits main game config.
//
// [isMapper] is a flag indicating whether we're initing config for a main
// game, or a mapper. This value is `false` for the game itself.
//
// [argc] and [argv] are command line arguments. The engine assumes there is
// at least 1 element which is executable path at index 0. There is no
// additional check for [argc], so it will crash if you pass NULL, or an empty
// array into [argv].
//
// The executable path from [argv] is used resolve path to `fallout2.cfg`,
// which should be in the same folder. This function provide defaults if
// `fallout2.cfg` is not present, or cannot be read for any reason.
//
// Finally, this function merges key-value pairs from [argv] if any, see
// [configParseCommandLineArguments] for expected format.
//
// 0x444570
bool gameConfigInit(bool isMapper, int argc, char** argv)
{
    if (gGameConfigInitialized) {
        return false;
    }

    if (!configInit(&gGameConfig)) {
        return false;
    }

    // CE: Detect alternative default music directory.
    char alternativeMusicPath[COMPAT_MAX_PATH];
    strcpy(alternativeMusicPath, R"(data\sound\music\*.acm)");
    compat_windows_path_to_native(alternativeMusicPath);
    compat_resolve_path(alternativeMusicPath);

    char** acms;
    int acmsLength = fileNameListInit(alternativeMusicPath, &acms);
    if (acmsLength != -1) {
        if (acmsLength > 0) {
            // TODO: apply these to settings directly instead of config
            configSetString(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_PATH1_KEY, R"(data\sound\music\)");
            configSetString(&gGameConfig, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_PATH2_KEY, R"(data\sound\music\)");
        }
        fileNameListFree(&acms, 0);
    }

    // SFALL: Custom config file name.
    char* customConfigFileName = nullptr;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_CONFIG_FILE, &customConfigFileName);

    // CE: Derive config file name from executable name.
    char exeDrive[COMPAT_MAX_DRIVE];
    char exeDir[COMPAT_MAX_DIR];
    char exeFname[COMPAT_MAX_FNAME];
    compat_splitpath(argv[0], exeDrive, exeDir, exeFname, nullptr);

    char derivedConfigFileName[COMPAT_MAX_FNAME + 5];
    snprintf(derivedConfigFileName, sizeof(derivedConfigFileName), "%s.cfg", exeFname);

    const char* defaultConfigFileName = isMapper ? kMapperConfigFileName : kDefaultGameConfigFileName;

    const bool hasCustomConfigFileName = customConfigFileName != nullptr && *customConfigFileName != '\0';
    const char* configFileName = hasCustomConfigFileName ? customConfigFileName : derivedConfigFileName;

    compat_makepath(gGameConfigFilePath, exeDrive, exeDir, configFileName, nullptr);

    const bool usingDerivedConfig = !hasCustomConfigFileName
        && compat_stricmp(derivedConfigFileName, defaultConfigFileName) != 0;

    configRead(&gGameConfig, gGameConfigFilePath, false);

    debugPrint("Game config loaded from %s.\n", gGameConfigFilePath);

    if (usingDerivedConfig) {
        char defaultConfigFilePath[COMPAT_MAX_PATH];
        compat_makepath(defaultConfigFilePath, exeDrive, exeDir, defaultConfigFileName, nullptr);
        if (gameConfigMigrateFromDefaultConfig(defaultConfigFilePath, &gGameConfig)) {
            debugPrint("Migrated settings from %s.\n", defaultConfigFileName);
            configWriteEx(&gGameConfig, gGameConfigFilePath, CONFIG_RETAIN_ALL);
        }
    }

    if (!isMapper && gameConfigMigrateFromF2Res(gGameConfigFilePath, &gGameConfig)) {
        debugPrint("Migrated settings from f2_res.ini.\n");
        configWriteEx(&gGameConfig, gGameConfigFilePath, CONFIG_RETAIN_ALL);
    }

    // Add key-values from command line, which overrides both defaults and
    // whatever was loaded from `fallout2.cfg`.
    configParseCommandLineArguments(&gGameConfig, argc, argv);

    // Writes default values to config, skipping keys that were already loaded.
    settingsWriteToConfig(true);

    gGameConfigInitialized = true;

    return true;
}

#if defined(__EMSCRIPTEN__)
// clang-format off
EM_ASYNC_JS(void, do_save_idbfs_gameconfig, (), {
    await new Promise((resolve, reject) => FS.syncfs(err => err ? reject(err) : resolve()))
});
// clang-format on
#endif

// Saves game config into `fallout2.cfg`.
//
// 0x444C14
bool gameConfigSave()
{
    if (!gGameConfigInitialized) {
        return false;
    }

    if (!configWriteEx(&gGameConfig, gGameConfigFilePath, CONFIG_RETAIN_ALL)) {
        return false;
    }

#if defined(__EMSCRIPTEN__)
    do_save_idbfs_gameconfig();
#endif

    return true;
}

// Frees game config, optionally saving it.
//
// 0x444C3C
bool gameConfigExit(bool shouldSave)
{
    if (!gGameConfigInitialized) {
        return false;
    }

    bool result = true;

    if (shouldSave) {
        if (!configWriteEx(&gGameConfig, gGameConfigFilePath, CONFIG_RETAIN_ALL)) {
            result = false;
        }
    }

    configFree(&gGameConfig);

    gGameConfigInitialized = false;

    return result;
}

} // namespace fallout
