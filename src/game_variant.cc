#include "game_variant.h"

#include <string.h>

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
    "fallout.cfg",
    "f1_res.ini",
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

} // namespace

void gameVariantInit(int argc, char** argv)
{
    if (gGameVariantInitialized) {
        return;
    }

    gGameVariantInfo = &kFallout2VariantInfo;

    for (int index = 1; index < argc; index++) {
        const char* arg = argv[index];
        if (strncmp(arg, "--game=", 7) == 0) {
            const GameVariantInfo* variantInfo = gameVariantInfoById(arg + 7);
            if (variantInfo != nullptr) {
                gGameVariantInfo = variantInfo;
            }
            break;
        }
    }

    gGameVariantInitialized = true;
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
