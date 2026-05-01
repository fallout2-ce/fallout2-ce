#include "win32.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "main.h"
#include "svga.h"
#include "window_manager.h"

#include "scan_unimplemented.h"

#if __APPLE__ && TARGET_OS_IOS
#include "platform/ios/paths.h"
#endif

namespace fallout {

// 0x51E444
bool gProgramIsActive = false;

#if __APPLE__
static char gMacOsBundleResourcesPath[PATH_MAX];
#endif

#ifdef _WIN32
HANDLE GNW95_mutex = nullptr;
#endif

#if __APPLE__
const char* getMacOsBundleResourcesPath()
{
    return gMacOsBundleResourcesPath[0] != '\0' ? gMacOsBundleResourcesPath : nullptr;
}
#endif

int main(int argc, char* argv[])
{
    scanUnimplementdParseCommandLineArguments(argc, argv);

    int rc;

#if _WIN32
    GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() != ERROR_SUCCESS) {
        return 0;
    }
#ifdef SDL_HINT_WINDOWS_DPI_AWARENESS
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
#endif

#if __APPLE__ && TARGET_OS_IOS
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(iOSGetDocumentsPath());
#endif

#if __APPLE__ && TARGET_OS_OSX
    char executablePath[PATH_MAX];
    if (realpath(argv[0], executablePath) != nullptr) {
        char* sep = strrchr(executablePath, '/');
        if (sep != nullptr) {
            *sep = '\0';
            snprintf(gMacOsBundleResourcesPath, sizeof(gMacOsBundleResourcesPath), "%s/../Resources", executablePath);
        }
    }
#endif

#if __ANDROID__
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(SDL_AndroidGetExternalStoragePath());
#endif

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        return EXIT_FAILURE;
    }

    atexit(SDL_Quit);

    SDL_ShowCursor(SDL_DISABLE);

    gProgramIsActive = true;
    rc = falloutMain(argc, argv);

#if _WIN32
    CloseHandle(GNW95_mutex);
#endif

    return rc;
}

} // namespace fallout

int main(int argc, char* argv[])
{
    return fallout::main(argc, argv);
}
