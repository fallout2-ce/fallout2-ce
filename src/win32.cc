#include "win32.h"

#include <stdlib.h>

#include <SDL.h>

#if __APPLE__
#include <TargetConditionals.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

#if __APPLE__ && TARGET_OS_OSX
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef FALLOUT_MAPPER
#include "mapper/mapper.h"
#else
#include "main.h"
#endif
#include "svga.h"
#include "window_manager.h"

#include "scan_unimplemented.h"

#if __APPLE__ && TARGET_OS_IOS
#include "platform/ios/paths.h"
#endif

namespace fallout {

// 0x51E444
bool gProgramIsActive = false;

#if __APPLE__ && TARGET_OS_OSX
static char gMacOsBundleResourcesPath[COMPAT_MAX_PATH];
#endif

#ifdef _WIN32
HANDLE GNW95_mutex = nullptr;
#endif

#if __APPLE__ && TARGET_OS_OSX
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
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle != nullptr) {
        CFURLRef resourcesUrl = CFBundleCopyResourcesDirectoryURL(mainBundle);
        if (resourcesUrl != nullptr) {
            if (CFURLGetFileSystemRepresentation(resourcesUrl,
                    true,
                    reinterpret_cast<UInt8*>(gMacOsBundleResourcesPath),
                    sizeof(gMacOsBundleResourcesPath))) {
                gMacOsBundleResourcesPath[sizeof(gMacOsBundleResourcesPath) - 1] = '\0';
            }
            CFRelease(resourcesUrl);
        }
    }

    char* basePath = SDL_GetBasePath();
    chdir(basePath);
    SDL_free(basePath);
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
#ifdef FALLOUT_MAPPER
    rc = mapper_main(argc, argv);
#else
    rc = falloutMain(argc, argv);
#endif

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
