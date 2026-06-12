#include "mapper/mp_utils.h"

#include <string.h>

#include "art.h"
#include "color.h"
#include "text_font.h"
#include "window_manager_private.h"

namespace fallout {

void mapperShowTimedMsg(const char* msg)
{
    win_timed_msg(msg, _colorTable[31744] | FONT_SHADOW);
}

bool mapperYesNoDialog(const char* msg)
{
    return win_yes_no(msg, 80, 80, 0x104 | FONT_SHADOW) > 0;
}

void mapperShowMessage(const char* msg)
{
    _win_msg(msg, 80, 80, _colorTable[31744] | FONT_SHADOW);
}

void art_name_no_ext(int fid, char* buf)
{
    if (art_list_str(fid, buf) == -1) {
        strcpy(buf, "None");
    } else {
        char* pch = strchr(buf, '.');
        if (pch != nullptr) {
            *pch = '\0';
        }
    }
}

} // namespace fallout
