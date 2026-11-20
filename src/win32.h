#ifndef WIN32_H
#define WIN32_H

#include "platform_compat.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fallout {

#ifdef _WIN32
extern bool gProgramIsActive;
extern HANDLE GNW95_mutex;
#else
extern bool gProgramIsActive;
#endif

#if __APPLE__
const char* getMacOsBundleResourcesPath();
#endif

} // namespace fallout

#endif /* WIN32_H */
