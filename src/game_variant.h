#ifndef FALLOUT_GAME_VARIANT_H_
#define FALLOUT_GAME_VARIANT_H_

#include <stddef.h>

namespace fallout {

enum class GameVariant {
    Fallout1,
    Fallout2,
};

void gameVariantInit(int argc, char** argv);
void gameVariantResolveInstallPath(int argc, char** argv, char* path, size_t size);
GameVariant gameVariantGet();
bool gameVariantIsFallout1();
const char* gameVariantGetId();
const char* gameVariantGetWindowTitle();
const char* gameVariantGetDefaultGameConfigFileName();
const char* gameVariantGetDefaultHiResConfigFileName();
const char* gameVariantGetHiResDatFileName();
const char* gameVariantGetStartingMap();
const char* gameVariantGetGlobalVarsFileName();
bool gameVariantShouldPlayIntroCredits();
bool gameVariantShouldPlayNewGameMovie();

} // namespace fallout

#endif /* FALLOUT_GAME_VARIANT_H_ */
