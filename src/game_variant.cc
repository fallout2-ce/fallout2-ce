#include "game_variant.h"

#include <string.h>

#include "debug.h"
#include "platform_compat.h"

namespace fallout {

namespace {

struct GameVariantInfo {
    GameVariant variant;
    const char* id;
    const char* windowTitle;
    const char* gameConfigFileName;
    const char* hiResConfigFileName;
    const char* hiResDatFileName;
    const char* startingMap;
    const char* globalVarsFileName;
    bool playIntroCredits;
    bool playNewGameMovie;
};

constexpr GameVariantInfo kFallout1VariantInfo = {
    GameVariant::Fallout1,
    "fo1",
    "FALLOUT",
    "fallout2.cfg",
    "f2_res.ini",
    nullptr,
    "V13Ent.map",
    "data\\vault13.gam",
    false,
    false,
};

constexpr GameVariantInfo kFallout2VariantInfo = {
    GameVariant::Fallout2,
    "fo2",
    "FALLOUT II",
    "fallout2.cfg",
    "f2_res.ini",
    "f2_res.dat",
    "artemple.map",
    "data\\vault13.gam",
    true,
    true,
};

const GameVariantInfo* gGameVariantInfo = &kFallout2VariantInfo;
bool gGameVariantInitialized = false;

const GameVariantInfo* gameVariantInfoById(const char* id)
{
    if (id == nullptr) {
        return nullptr;
    }

    if (compat_stricmp(id, "fo1") == 0 || compat_stricmp(id, "fallout1") == 0) {
        return &kFallout1VariantInfo;
    }

    if (compat_stricmp(id, "fo2") == 0 || compat_stricmp(id, "fallout2") == 0) {
        return &kFallout2VariantInfo;
    }

    return nullptr;
}

void getExecutableDirectory(int argc, char** argv, char* path, size_t size)
{
    path[0] = '\0';

    if (argc <= 0 || argv == nullptr || argv[0] == nullptr) {
        return;
    }

    strncpy(path, argv[0], size - 1);
    path[size - 1] = '\0';

    char* separator = strrchr(path, '\\');
    if (separator == nullptr) {
        separator = strrchr(path, '/');
    }

    if (separator != nullptr) {
        *separator = '\0';
    } else {
        path[0] = '\0';
    }
}

void trimMacosBundleSubpath(char* path)
{
    char* bundleMarker = strstr(path, ".app/Contents/MacOS");
    if (bundleMarker == nullptr) {
        return;
    }

    char* bundleStart = bundleMarker;
    while (bundleStart > path && bundleStart[-1] != '/' && bundleStart[-1] != '\\') {
        bundleStart--;
    }

    if (bundleStart > path) {
        bundleStart[-1] = '\0';
    }
}

bool hasFo1ShimModAtPath(const char* basePath)
{
    if (basePath == nullptr || basePath[0] == '\0') {
        return false;
    }

    char candidate[COMPAT_MAX_PATH];
    snprintf(candidate, sizeof(candidate), "%s/mods/fo1_shims/data/worldmap.txt", basePath);
    return compat_access(candidate, 0) == 0;
}

} // namespace

void gameVariantInit(int argc, char** argv)
{
    if (gGameVariantInitialized) {
        return;
    }

    gGameVariantInfo = &kFallout2VariantInfo;

    bool variantOverridden = false;
    for (int index = 1; index < argc; index++) {
        const char* arg = argv[index];
        if (strncmp(arg, "--game=", 7) == 0) {
            const GameVariantInfo* variantInfo = gameVariantInfoById(arg + 7);
            if (variantInfo != nullptr) {
                gGameVariantInfo = variantInfo;
                variantOverridden = true;
            }
            break;
        }
    }

    char executablePath[COMPAT_MAX_PATH];
    getExecutableDirectory(argc, argv, executablePath, sizeof(executablePath));
    trimMacosBundleSubpath(executablePath);

    bool hasFo1ShimAtExecutablePath = hasFo1ShimModAtPath(executablePath);
    bool hasFo1ShimAtCwd = compat_access("mods/fo1_shims/data/worldmap.txt", 0) == 0;

    debugPrint("gameVariantInit: argv0=%s executablePath=%s fo1ShimExe=%d fo1ShimCwd=%d\n",
        argc > 0 && argv != nullptr && argv[0] != nullptr ? argv[0] : "<null>",
        executablePath,
        hasFo1ShimAtExecutablePath,
        hasFo1ShimAtCwd);

    if (!variantOverridden && (hasFo1ShimAtExecutablePath || hasFo1ShimAtCwd)) {
        gGameVariantInfo = &kFallout1VariantInfo;
        debugPrint("Auto-detected Fallout 1 mode from mods/fo1_shims.\n");
    }

    gGameVariantInitialized = true;
}

void gameVariantResolveInstallPath(int argc, char** argv, char* path, size_t size)
{
    getExecutableDirectory(argc, argv, path, size);
    trimMacosBundleSubpath(path);
}

void gameVariantForceFallout1()
{
    gGameVariantInfo = &kFallout1VariantInfo;
}

GameVariant gameVariantGet()
{
    return gGameVariantInfo->variant;
}

bool gameVariantIsFallout1()
{
    return gameVariantGet() == GameVariant::Fallout1;
}

const char* gameVariantGetId()
{
    return gGameVariantInfo->id;
}

const char* gameVariantGetWindowTitle()
{
    return gGameVariantInfo->windowTitle;
}

const char* gameVariantGetDefaultGameConfigFileName()
{
    return gGameVariantInfo->gameConfigFileName;
}

const char* gameVariantGetDefaultHiResConfigFileName()
{
    return gGameVariantInfo->hiResConfigFileName;
}

const char* gameVariantGetHiResDatFileName()
{
    return gGameVariantInfo->hiResDatFileName;
}

const char* gameVariantGetStartingMap()
{
    return gGameVariantInfo->startingMap;
}

const char* gameVariantGetGlobalVarsFileName()
{
    return gGameVariantInfo->globalVarsFileName;
}

bool gameVariantShouldPlayIntroCredits()
{
    return gGameVariantInfo->playIntroCredits;
}

bool gameVariantShouldPlayNewGameMovie()
{
    return gGameVariantInfo->playNewGameMovie;
}

} // namespace fallout
