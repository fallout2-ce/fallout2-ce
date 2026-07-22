#include "mapper/mp_utils.h"

#include "color.h"
#include "text_font.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

void mapperShowTimedMsg(const char* msg)
{
    win_timed_msg(msg, COLOR_RED | DRAW_TEXT_FLAG_SHADOWED);
}

bool mapperYesNoDialog(const char* msg)
{
    return win_yes_no(msg, 80, 80, 0x104 | DRAW_TEXT_FLAG_SHADOWED) > 0;
}

void mapperShowMessage(const char* msg)
{
    _win_msg(msg, 80, 80, COLOR_RED | DRAW_TEXT_FLAG_SHADOWED);
}

} // namespace fallout
