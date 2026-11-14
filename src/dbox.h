#ifndef DBOX_H
#define DBOX_H

#include <string>

#include "color.h"
#include "window_manager.h"

namespace fallout {

typedef enum DialogBoxOptions {
    DIALOG_BOX_LARGE = 0x01, // sfall calls this NORMAL
    DIALOG_BOX_MEDIUM = 0x02, // sfall: SMALL
    DIALOG_BOX_NO_HORIZONTAL_CENTERING = 0x04, // sfall: ALIGN_LEFT
    DIALOG_BOX_NO_VERTICAL_CENTERING = 0x08, // sfall: ALIGN_TOP
    DIALOG_BOX_YES_NO = 0x10,
    DIALOG_BOX_NO_BUTTONS = 0x20, // sfall: CLEAN
} DialogBoxOptions;

struct TextInputOptions {
    int doneKeyCode = -1;
    int cancelKeyCode = -1;
    int cancelReturnCode = 0; // 0 for dbox.cc, -1 for loadsave.cc
    bool playReturnSound = true; // true for dbox.cc, false for loadsave.cc
    bool clearOnFirstBackspace = false; // false for dbox.cc, true for loadsave.cc
};

int runTextInput(int win, std::string& text, int maxLength, int x, int y,
    ColorWithFlags textColor, Color backgroundColor, int flags,
    const TextInputOptions& options);

int showDialogBox(const char* title, const char** body, int bodyLength, int x, int y, ColorWithFlags titleColor, const char* secondaryButtonText, ColorWithFlags bodyColor, int flags);

inline int showDialogBox(const char* title, const char** body, int bodyLength, int x, int y, Color titleColor, const char* secondaryButtonText, Color bodyColor, int flags)
{
    return showDialogBox(title, body, bodyLength, x, y, titleColor | DRAW_TEXT_FLAG_NONE, secondaryButtonText, bodyColor | DRAW_TEXT_FLAG_NONE, flags);
}

const char* showInputDialog(const char* currentInput, int x, int y, const char* doneText, WindowFlags flags);
const char* showInputDialog(const char* currentInput, int x, int y, const char* doneText);

int showLoadFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags);
int showSaveFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags);

} // namespace fallout

#endif /* DBOX_H */
