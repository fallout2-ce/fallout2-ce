#include "widget.h"

namespace fallout {

// 0x66E6A0 updateRegions
static int _updateRegions[32];

// 0x4B5A64 showRegion
static void _showRegion(int region)
{
    // TODO: Incomplete.
}

// 0x4B5C24 update_widgets
int windowUpdateWidgets()
{
    for (int index = 0; index < 32; index++) {
        if (_updateRegions[index]) {
            _showRegion(_updateRegions[index]);
        }
    }

    return 1;
}

// 0x4B5998 win_delete_widgets
void windowDeleteWidgets(int win)
{
    // TODO: Incomplete.
}

} // namespace fallout
