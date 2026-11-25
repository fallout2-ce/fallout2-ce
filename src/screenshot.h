#ifndef FALLOUT_SCREENSHOT_H_
#define FALLOUT_SCREENSHOT_H_

namespace fallout {

using ScreenshotHandler = int(int width, int height, unsigned char* buffer, unsigned char* palette);

void takeScreenshot();
int screenshotHandlerDefaultImpl(int width, int height, unsigned char* data, unsigned char* palette);
int screenshotHandlerPngImpl(int width, int height, unsigned char* data, unsigned char* palette);
void screenshotHandlerConfigure(int keyCode, ScreenshotHandler* handler);
bool handleScreenshotShortcut(int code);

} // namespace fallout

#endif /* FALLOUT_SCREENSHOT_H_ */
