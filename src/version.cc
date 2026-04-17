#include "version.h"
#include "content_config.h"

#include <stdio.h>

namespace fallout {

// 0x4B4580
void versionGetVersion(char* dest, size_t size)
{
    // SFALL: custom version string.
    char* versionString = nullptr;
    configGetString(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "version_string", &versionString, "");
    if (!*versionString) {
        versionString = nullptr;
    }
    snprintf(dest, size, (versionString != nullptr ? versionString : "FALLOUT II %d.%02d"), VERSION_MAJOR, VERSION_MINOR);
}

} // namespace fallout
