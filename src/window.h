#ifndef WINDOW_H
#define WINDOW_H

#include "geometry.h"
#include "interpreter.h"
#include "region.h"
#include "window_manager.h"

namespace fallout {

typedef void (*WINDOWDRAWINGPROC)(unsigned char* src, int src_pitch, int _, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y);
typedef void WindowDrawingProc2(unsigned char* buf, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, unsigned char a10);
typedef bool(WindowInputHandler)(int key);
typedef void(WindowDeleteCallback)(int windowIndex, const char* windowName);
typedef void(DisplayInWindowCallback)(int windowIndex, const char* windowName, unsigned char* data, int width, int height);
typedef void(ManagedButtonMouseEventCallback)(void* userData, int eventType);
typedef void(ManagedWindowCreateCallback)(int windowIndex, const char* windowName, int* flagsPtr);

typedef enum TextAlignment {
    TEXT_ALIGNMENT_LEFT,
    TEXT_ALIGNMENT_RIGHT,
    TEXT_ALIGNMENT_CENTER,
} TextAlignment;

typedef enum ManagedButtonMouseEvent {
    MANAGED_BUTTON_MOUSE_EVENT_BUTTON_DOWN,
    MANAGED_BUTTON_MOUSE_EVENT_BUTTON_UP,
    MANAGED_BUTTON_MOUSE_EVENT_ENTER,
    MANAGED_BUTTON_MOUSE_EVENT_EXIT,
    MANAGED_BUTTON_MOUSE_EVENT_COUNT,
} ManagedButtonMouseEvent;

typedef enum ManagedButtonRightMouseEvent {
    MANAGED_BUTTON_RIGHT_MOUSE_EVENT_BUTTON_DOWN,
    MANAGED_BUTTON_RIGHT_MOUSE_EVENT_BUTTON_UP,
    MANAGED_BUTTON_RIGHT_MOUSE_EVENT_COUNT,
} ManagedButtonRightMouseEvent;

int windowGetFont();
int windowSetFont(int font);
void scriptWindowResetTextAttributes();
int scriptWindowGetTextFlags();
int scriptWindowSetTextFlags(int flags);
unsigned char scriptWindowGetTextColor();
int scriptWindowSetTextColor(float r, float g, float b);
unsigned char scriptWindowGetHighlightColor();
int scriptWindowSetHighlightColor(float r, float g, float b);
bool _checkRegion(int windowIndex, int mouseX, int mouseY, int mouseEvent);
bool scriptWindowCheckRegion(int windowIndex, int mouseX, int mouseY, int mouseEvent);
bool scriptWindowRefreshRegions();
bool _checkAllRegions();
void scriptWindowAddInputFunc(WindowInputHandler* handler);
void _doRegionRightFunc(Region* region, int mouseEvent);
void _doRegionFunc(Region* region, int mouseEvent);
bool scriptWindowActivateRegion(const char* regionName, int mouseEvent);
int _getInput();
void _doButtonOn(int btn, int keyCode);
void scriptWindowDispatchButtonMouseEvent(int btn, int mouseEvent);
void _doButtonOff(int btn, int keyCode);
void _doButtonPress(int btn, int keyCode);
void _doButtonRelease(int btn, int keyCode);
void _doRightButtonPress(int btn, int keyCode);
void scriptWindowDispatchButtonRightMouseEvent(int btn, int mouseEvent);
void _doRightButtonRelease(int btn, int keyCode);
void _setButtonGFX(int width, int height, unsigned char* normal);
bool scriptWindowHide();
bool scriptWindowShow();
int scriptWindowWidth();
int scriptWindowHeight();
bool scriptWindowDraw();
bool scriptWindowDelete(const char* windowName);
int scriptWindowResize(const char* windowName, int x, int y, int width, int height);
int scriptWindowScale(const char* windowName, int x, int y, int width, int height);
int scriptWindowCreate(const char* windowName, int x, int y, int width, int height, int a6, int flags);
int scriptWindowOutput(char* string);
bool scriptWindowGotoXY(int x, int y);
bool scriptWindowSelectId(int index);
int scriptWindowSelect(const char* windowName);
unsigned char* scriptWindowGetBuffer();
int scriptWindowPush(const char* windowName);
int _popWindow();

void windowPrintBuf(int win, char* string, int stringLength, int width, int maxY, int x, int y, int flags, int textAlignment);
char** windowWordWrap(char* string, int maxLength, int indent, int* substringListLengthPtr);
void windowFreeWordList(char** substringList, int substringListLength);
void windowWrapLineWithSpacing(int win, char* string, int width, int height, int x, int y, int flags, int textAlignment, int spacing);
void windowWrapLine(int win, char* string, int width, int height, int x, int y, int flags, int textAlignment);

bool scriptWindowPrintRect(char* string, int wrapWidth, int textAlignment);
bool scriptWindowFormatMessage(char* string, int x, int y, int width, int height, int textAlignment);
bool scriptWindowPrint(char* string, int width, int x, int y, int color);
void _displayInWindow(unsigned char* data, int width, int height, int pitch);
void _displayFile(char* fileName);
void _displayFileRaw(char* fileName);
bool scriptWindowDisplay(char* fileName, int x, int y, int width, int height);
bool scriptWindowDisplayBuf(unsigned char* src, int srcWidth, int srcHeight, int destX, int destY, int destWidth, int destHeight);
int windowGetXres();
int windowGetYres();
void _removeProgramReferences_3(Program* program);
void scriptWindowInit(int resolution, int flags);
void scriptWindowClose();
bool scriptWindowDeleteButton(const char* buttonName);
bool scriptWindowSetButtonFlag(const char* buttonName, int value);
bool scriptWindowAddButton(const char* buttonName, int x, int y, int width, int height, int flags);
bool scriptWindowAddButtonGfx(const char* buttonName, char* pressedFileName, char* normalFileName, char* hoverFileName);
bool scriptWindowAddButtonProc(const char* buttonName, Program* program, int mouseEnterProc, int mouseExitProc, int mouseDownProc, int mouseUpProc);
bool scriptWindowAddButtonRightProc(const char* buttonName, Program* program, int rightMouseDownProc, int rightMouseUpProc);
bool scriptWindowAddButtonCfunc(const char* buttonName, ManagedButtonMouseEventCallback* callback, void* userData);
bool scriptWindowAddButtonRightCfunc(const char* buttonName, ManagedButtonMouseEventCallback* callback, void* userData);
bool scriptWindowAddButtonText(const char* buttonName, const char* text);
bool scriptWindowAddButtonTextWithOffsets(const char* buttonName, const char* text, int pressedImageOffsetX, int pressedImageOffsetY, int normalImageOffsetX, int normalImageOffsetY);
bool scriptWindowFill(float r, float g, float b);
bool scriptWindowFillRect(int x, int y, int width, int height, float r, float g, float b);
void scriptWindowEndRegion();
bool scriptWindowCheckRegionExists(const char* regionName);
bool scriptWindowStartRegion(int initialCapacity);
bool scriptWindowAddRegionPoint(int x, int y, bool a3);
bool scriptWindowAddRegionProc(const char* regionName, Program* program, int a3, int a4, int a5, int a6);
bool scriptWindowAddRegionRightProc(const char* regionName, Program* program, int a3, int a4);
bool scriptWindowSetRegionFlag(const char* regionName, int value);
bool scriptWindowAddRegionName(const char* regionName);
bool scriptWindowDeleteRegion(const char* regionName);
void scriptWindowUpdateAll();
int scriptWindowMoviePlaying();
bool scriptWindowSetMovieFlags(int flags);
bool scriptWindowPlayMovie(char* filePath);
bool scriptWindowPlayMovieRect(char* filePath, int x, int y, int w, int h);
void scriptWindowStopMovie();
void _drawScaled(unsigned char* dest, int destWidth, int destHeight, int destPitch, unsigned char* src, int srcWidth, int srcHeight, int srcPitch);
void _drawScaledBuf(unsigned char* dest, int destWidth, int destHeight, unsigned char* src, int srcWidth, int srcHeight);
void _alphaBltBuf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* alphaWindowBuffer, unsigned char* alphaBuffer, unsigned char* dest, int destPitch);
void _fillBuf3x3(unsigned char* src, int srcWidth, int srcHeight, unsigned char* dest, int destWidth, int destHeight);

bool scriptWindowShowNamed(const char* name);

} // namespace fallout

#endif /* WINDOW_H */
