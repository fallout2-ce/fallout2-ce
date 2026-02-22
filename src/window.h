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
void windowResetTextAttributes();
int windowGetTextFlags();
int windowSetTextFlags(int flags);
unsigned char windowGetTextColor();
unsigned char windowGetHighlightColor();
int windowSetTextColor(float r, float g, float b);
int windowSetHighlightColor(float r, float g, float b);
bool _checkRegion(int windowIndex, int mouseX, int mouseY, int mouseEvent);
bool windowCheckRegion(int windowIndex, int mouseX, int mouseY, int mouseEvent);
bool windowRefreshRegions();
bool _checkAllRegions();
void windowAddInputFunc(WindowInputHandler* handler);
void _doRegionRightFunc(Region* region, int mouseEvent);
void _doRegionFunc(Region* region, int mouseEvent);
bool windowActivateRegion(const char* regionName, int mouseEvent);
int _getInput();
void _doButtonOn(int btn, int keyCode);
void sub_4B6F68(int btn, int mouseEvent);
void _doButtonOff(int btn, int keyCode);
void _doButtonPress(int btn, int keyCode);
void _doButtonRelease(int btn, int keyCode);
void _doRightButtonPress(int btn, int keyCode);
void sub_4B704C(int btn, int mouseEvent);
void _doRightButtonRelease(int btn, int keyCode);
void _setButtonGFX(int width, int height, unsigned char* normal);
bool windowHide();
bool windowShow();
int windowWidth();
int windowHeight();
bool windowDraw();
bool _deleteWindow(const char* windowName);
int resizeWindow(const char* windowName, int x, int y, int width, int height);
int scaleWindow(const char* windowName, int x, int y, int width, int height);
int _createWindow(const char* windowName, int x, int y, int width, int height, int a6, int flags);
int windowOutput(char* string);
bool windowGotoXY(int x, int y);
bool _selectWindowID(int index);
int _selectWindow(const char* windowName);
unsigned char* windowGetBuffer();
int _pushWindow(const char* windowName);
int _popWindow();
void windowPrintBuf(int win, char* string, int stringLength, int width, int maxY, int x, int y, int flags, int textAlignment);
char** windowWordWrap(char* string, int maxLength, int indent, int* substringListLengthPtr);
void windowFreeWordList(char** substringList, int substringListLength);
void windowWrapLineWithSpacing(int win, char* string, int width, int height, int x, int y, int flags, int textAlignment, int spacing);
void windowWrapLine(int win, char* string, int width, int height, int x, int y, int flags, int textAlignment);
bool windowPrintRect(char* string, int a2, int textAlignment);
bool windowFormatMessage(char* string, int x, int y, int width, int height, int textAlignment);
bool windowPrint(char* string, int width, int x, int y, int color);
void _displayInWindow(unsigned char* data, int width, int height, int pitch);
void _displayFile(char* fileName);
void _displayFileRaw(char* fileName);
bool windowDisplay(char* fileName, int x, int y, int width, int height);
bool windowDisplayBuf(unsigned char* src, int srcWidth, int srcHeight, int destX, int destY, int destWidth, int destHeight);
int windowGetXres();
int windowGetYres();
void _removeProgramReferences_3(Program* program);
void _initWindow(int resolution, int flags);
void windowClose();
bool windowDeleteButton(const char* buttonName);
bool windowSetButtonFlag(const char* buttonName, int value);
bool windowAddButton(const char* buttonName, int x, int y, int width, int height, int flags);
bool windowAddButtonGfx(const char* buttonName, char* pressedFileName, char* normalFileName, char* hoverFileName);
bool windowAddButtonProc(const char* buttonName, Program* program, int mouseEnterProc, int mouseExitProc, int mouseDownProc, int mouseUpProc);
bool windowAddButtonRightProc(const char* buttonName, Program* program, int rightMouseDownProc, int rightMouseUpProc);
bool windowAddButtonCfunc(const char* buttonName, ManagedButtonMouseEventCallback* callback, void* userData);
bool windowAddButtonRightCfunc(const char* buttonName, ManagedButtonMouseEventCallback* callback, void* userData);
bool windowAddButtonText(const char* buttonName, const char* text);
bool windowAddButtonTextWithOffsets(const char* buttonName, const char* text, int pressedImageOffsetX, int pressedImageOffsetY, int normalImageOffsetX, int normalImageOffsetY);
bool windowFill(float r, float g, float b);
bool windowFillRect(int x, int y, int width, int height, float r, float g, float b);
void windowEndRegion();
bool windowCheckRegionExists(const char* regionName);
bool windowStartRegion(int initialCapacity);
bool windowAddRegionPoint(int x, int y, bool a3);
bool windowAddRegionProc(const char* regionName, Program* program, int a3, int a4, int a5, int a6);
bool windowAddRegionRightProc(const char* regionName, Program* program, int a3, int a4);
bool windowSetRegionFlag(const char* regionName, int value);
bool windowAddRegionName(const char* regionName);
bool windowDeleteRegion(const char* regionName);
void _updateWindows();
int windowMoviePlaying();
bool windowSetMovieFlags(int flags);
bool windowPlayMovie(char* filePath);
bool windowPlayMovieRect(char* filePath, int x, int y, int w, int h);
void windowStopMovie();
void _drawScaled(unsigned char* dest, int destWidth, int destHeight, int destPitch, unsigned char* src, int srcWidth, int srcHeight, int srcPitch);
void _drawScaledBuf(unsigned char* dest, int destWidth, int destHeight, unsigned char* src, int srcWidth, int srcHeight);
void _alphaBltBuf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* alphaWindowBuffer, unsigned char* alphaBuffer, unsigned char* dest, int destPitch);
void _fillBuf3x3(unsigned char* src, int srcWidth, int srcHeight, unsigned char* dest, int destWidth, int destHeight);

bool windowShowNamed(const char* name);

} // namespace fallout

#endif /* WINDOW_H */
