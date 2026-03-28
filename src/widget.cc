#include "widget.h"

namespace fallout {

// 0x66E6A0
static int _updateRegions[32];

// 0x4B5A64 showRegion_
static void _showRegion(int region)
{
    // TODO: Incomplete.
}

// 0x4B5C24 update_widgets_
int windowUpdateWidgets()
{
    for (int index = 0; index < 32; index++) {
        if (_updateRegions[index]) {
            _showRegion(_updateRegions[index]);
        }
    }

    return 1;
}

// 0x4B5998 win_delete_widgets_
void windowDeleteWidgets(int win)
{
    // TODO: Incomplete.
}

} // namespace fallout
