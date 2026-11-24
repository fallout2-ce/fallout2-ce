#ifndef DINPUT_H
#define DINPUT_H

#include <SDL.h>

namespace fallout {

typedef struct MouseData {
    int x;
    int y;
    unsigned char buttons[2];
    int wheelX;
    int wheelY;
} MouseData;

typedef struct KeyboardData {
    int key;
    char down;
} KeyboardData;

bool directInputInit();
void directInputFree();
bool mouseDeviceUsesRelativeMode();
bool mouseDeviceInitMode();
unsigned int mouseDeviceMotionGeneration();
void mouseDeviceRefreshWindowMapping();
void mouseDeviceSetLogicalPosition(int x, int y);
bool mouseDeviceAcquire();
bool mouseDeviceUnacquire();
bool mouseDeviceGetData(MouseData* mouseData);
void cancelMouseDeviceInput();
SDL_Point mouseDeviceSamplePosition();
bool keyboardDeviceAcquire();
bool keyboardDeviceUnacquire();
bool keyboardDeviceGetData(KeyboardData* keyboardData);
bool mouseDeviceInit();
void mouseDeviceFree();
bool keyboardDeviceInit();
void keyboardDeviceFree();

void handleMouseEvent(SDL_Event* event);
void handleTouchEvent(SDL_Event* event);

} // namespace fallout

#endif /* DINPUT_H */
