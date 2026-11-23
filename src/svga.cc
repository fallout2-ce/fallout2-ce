#include "svga.h"

#include <algorithm>
#include <bitset>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include <SDL.h>

#include "color.h"
#include "config.h"
#include "debug.h"
#include "dinput.h"
#include "draw.h"
#include "game.h"
#include "interface.h"
#include "memory.h"
#include "mouse.h"
#include "movie.h"
#include "scan_unimplemented.h"
#include "settings.h"
#include "text_font.h"
#include "tile.h"
#include "win32.h"
#include "window_manager.h"
#include "window_manager_private.h"

namespace fallout {

static bool createRenderer(int width, int height);
static void destroyRenderer();

// screen rect
Rect _scr_size;

// 0x6ACA18 scr_blit
void (*_scr_blit)(unsigned char* src, int src_pitch, int unused, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y) = _GNW95_ShowRect;

// 0x6ACA1C zero_mem
void (*_zero_mem)() = nullptr;

SDL_Window* gSdlWindow = nullptr;
SDL_Surface* gSdlSurface = nullptr;
SDL_Renderer* gSdlRenderer = nullptr;
SDL_Texture* gSdlTexture = nullptr;
SDL_Surface* gSdlTextureSurface = nullptr;

// The counter is drawn during updates and measures frames actually presented.
static unsigned int framesPresented = 0;
static int displayRefreshRate = 0;

namespace {

    using PaletteIndices = std::bitset<COLOR_COUNT>;

    SDL_Color toSdlPaletteColor(const unsigned char* rgb)
    {
        return {
            static_cast<Uint8>(rgb[0] << 2),
            static_cast<Uint8>(rgb[1] << 2),
            static_cast<Uint8>(rgb[2] << 2),
            255,
        };
    }

    class IndexedColorUsage {
    public:
        void reset(int width, int height)
        {
            _columns = (width + kBlockSize - 1) / kBlockSize;
            _rows = (height + kBlockSize - 1) / kBlockSize;
            _blocks.assign(_columns * _rows, Block {});
        }

        void invalidate(const SDL_Surface& scene, const SDL_Rect& rect)
        {
            const SDL_Rect bounds { 0, 0, scene.w, scene.h };
            SDL_Rect clipped;
            if (!SDL_IntersectRect(&rect, &bounds, &clipped)) {
                return;
            }

            const int lastColumn = (clipped.x + clipped.w - 1) / kBlockSize;
            const int lastRow = (clipped.y + clipped.h - 1) / kBlockSize;
            for (int row = clipped.y / kBlockSize; row <= lastRow; ++row) {
                for (int column = clipped.x / kBlockSize; column <= lastColumn; ++column) {
                    _blocks[row * _columns + column].valid = false;
                }
            }
        }

        SDL_Rect affectedBounds(const SDL_Surface& scene, const PaletteIndices& changed)
        {
            SDL_Rect bounds {};
            const auto* pixels = static_cast<const unsigned char*>(scene.pixels);
            for (int row = 0; row < _rows; ++row) {
                for (int column = 0; column < _columns; ++column) {
                    auto& block = _blocks[row * _columns + column];
                    const int x = column * kBlockSize;
                    const int y = row * kBlockSize;
                    const SDL_Rect rect { x, y, std::min(kBlockSize, scene.w - x), std::min(kBlockSize, scene.h - y) };
                    // Refresh the index mask only when a palette change needs
                    // it and the indexed pixels in this block were rewritten.
                    if (!block.valid) {
                        block.indices.reset();
                        for (int line = 0; line < rect.h; ++line) {
                            const auto* source = pixels + (rect.y + line) * scene.pitch + rect.x;
                            for (int offset = 0; offset < rect.w; ++offset) {
                                block.indices.set(source[offset]);
                            }
                        }
                        block.valid = true;
                    }

                    if ((block.indices & changed).any()) {
                        SDL_UnionRect(&bounds, &rect, &bounds);
                    }
                }
            }
            return bounds;
        }

    private:
        static constexpr int kBlockSize = 64;

        struct Block {
            PaletteIndices indices {};
            bool valid = false;
        };

        int _columns = 0;
        int _rows = 0;
        std::vector<Block> _blocks;
    };

    IndexedColorUsage sceneColors;
    PaletteIndices pendingPaletteChanges {};
    SDL_Rect frameDamage {};
    SDL_Rect uploadedCursorBounds {};
    bool softwareRenderer = false;

    void damageFrame(const SDL_Rect& rect)
    {
        SDL_UnionRect(&frameDamage, &rect, &frameDamage);
    }

    void damageScene(const SDL_Rect& rect)
    {
        sceneColors.invalidate(*gSdlSurface, rect);
        damageFrame(rect);
    }

    void damageWholeFrame()
    {
        if (gSdlSurface != nullptr) {
            frameDamage = { 0, 0, gSdlSurface->w, gSdlSurface->h };
        }
    }

    bool updateFrameTexture(SDL_Point cursorPosition)
    {
        const auto cursorBounds = mouseGetCursorDrawBounds(cursorPosition);
        // Rebuild both footprints from the indexed scene so the old cursor
        // disappears when it moves, changes shape, or is hidden.
        damageFrame(uploadedCursorBounds);
        damageFrame(cursorBounds);

        const SDL_Rect textureBounds { 0, 0, gSdlTextureSurface->w, gSdlTextureSurface->h };
        SDL_Rect upload {};
        SDL_IntersectRect(&frameDamage, &textureBounds, &upload);
        if (pendingPaletteChanges.any() && !SDL_RectEquals(&upload, &textureBounds)) {
            damageFrame(sceneColors.affectedBounds(*gSdlSurface, pendingPaletteChanges));
            SDL_IntersectRect(&frameDamage, &textureBounds, &upload);
        }
        if (!SDL_RectEmpty(&upload)) {
            SDL_Rect dest = upload;
            if (SDL_BlitSurface(gSdlSurface, &upload, gSdlTextureSurface, &dest) != 0) {
                return false;
            }
            mouseDrawCursor(*gSdlTextureSurface, *gSdlSurface->format->palette, cursorPosition);

            const int pitch = gSdlTextureSurface->pitch;
            const auto* pixels = static_cast<const unsigned char*>(gSdlTextureSurface->pixels)
                + upload.y * pitch + upload.x * gSdlTextureSurface->format->BytesPerPixel;
            if (SDL_UpdateTexture(gSdlTexture, &upload, pixels, pitch) != 0) {
                return false;
            }
        }

        uploadedCursorBounds = cursorBounds;
        frameDamage = {};
        pendingPaletteChanges.reset();
        return true;
    }

    bool shouldClearOutput()
    {
        if (!softwareRenderer) {
            return true;
        }

        // At 1:1 size the opaque scene overwrites every output pixel.
        // Check the live size in case SDL has resized before input handles the event.
        int width;
        int height;
        return SDL_GetRendererOutputSize(gSdlRenderer, &width, &height) != 0
            || width != gSdlTextureSurface->w || height != gSdlTextureSurface->h;
    }

} // namespace

// 0x4CAD08 init_mode_320_200
int _init_mode_320_200()
{
    return _GNW95_init_mode_ex(320, 200, 8);
}

// 0x4CAD40 init_mode_320_400
int _init_mode_320_400()
{
    return _GNW95_init_mode_ex(320, 400, 8);
}

// 0x4CAD5C init_mode_640_480_16
int _init_mode_640_480_16()
{
    return -1;
}

// 0x4CAD64 init_mode_640_480
int _init_mode_640_480()
{
    return _init_vesa_mode(640, 480);
}

// 0x4CAD94 init_mode_640_400
int _init_mode_640_400()
{
    return _init_vesa_mode(640, 400);
}

// 0x4CADA8 init_mode_800_600
int _init_mode_800_600()
{
    return _init_vesa_mode(800, 600);
}

// 0x4CADBC init_mode_1024_768
int _init_mode_1024_768()
{
    return _init_vesa_mode(1024, 768);
}

// 0x4CADD0 init_mode_1280_1024
int _init_mode_1280_1024()
{
    return _init_vesa_mode(1280, 1024);
}

// 0x4CADF8
void _get_start_mode_()
{
}

// 0x4CADFC zero_vid_mem
void _zero_vid_mem()
{
    if (_zero_mem) {
        _zero_mem();
    }
}

// 0x4CAE1C GNW95_init_mode_ex
int _GNW95_init_mode_ex(int width, int height, int bpp)
{
    width = settings.screen.resolution_x;
    height = settings.screen.resolution_y;
    int scale = settings.screen.scale;

    // Only allow scaling if resulting game resolution is >= 640x480
    if ((width / scale) < 640 || (height / scale) < 480) {
        scale = 1;
    } else {
        width /= scale;
        height /= scale;
    }

    if (_GNW95_init_window(width, height, settings.screen.windowed, scale) == -1) {
        return -1;
    }

    if (directDrawInit(width, height, bpp) == -1) {
        return -1;
    }

    // macOS seems to require dequeuing NSApp events in order for window to
    // become visible. There is no concrete number of calls required to make
    // it happen. Sadly there is no particular event to watch for because SDL
    // marks window as shown immediately after creation (see
    // `SDL_FinishWindowCreation`).
    for (int i = 0; i < 10; i++) {
        SDL_PumpEvents();
    }

    _scr_size.left = 0;
    _scr_size.top = 0;
    _scr_size.right = width - 1;
    _scr_size.bottom = height - 1;

    _scr_blit = _GNW95_ShowRect;
    _zero_mem = _GNW95_zero_vid_mem;

    return 0;
}

// 0x4CAECC init_vesa_mode
int _init_vesa_mode(int width, int height)
{
    return _GNW95_init_mode_ex(width, height, 8);
}

// 0x4CAEDC GNW95_init_window
int _GNW95_init_window(int width, int height, WindowMode mode, int scale)
{
    if (gSdlWindow == nullptr) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

        if (mode == WindowMode::Fullscreen) {
            windowFlags |= SDL_WINDOW_FULLSCREEN;
        } else if (mode == WindowMode::WindowedFullscreen) {
            windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

            // Prevent SDL from minimizing the window when it loses focus.
            // Without this, SDL_WINDOW_FULLSCREEN_DESKTOP on Linux with
            // NVIDIA multi-monitor setups deactivates secondary displays
            // or triggers X11 BadRRCrtc errors.
            SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
        }

        gSdlWindow = SDL_CreateWindow(gProgramWindowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width * scale, height * scale, windowFlags);

        if (gSdlWindow == nullptr) {
            return -1;
        }

        if (!createRenderer(width, height)) {
            destroyRenderer();

            SDL_DestroyWindow(gSdlWindow);
            gSdlWindow = nullptr;

            return -1;
        }
        handleWindowDisplayChanged();
    }

    return 0;
}

// 0x4CAF9C GNW95_init_DirectDraw
int directDrawInit(int width, int height, int bpp)
{
    if (gSdlSurface != nullptr) {
        unsigned char* palette = directDrawGetPalette();
        directDrawFree();

        if (directDrawInit(width, height, bpp) == -1) {
            return -1;
        }

        directDrawSetPalette(palette);

        return 0;
    }

    gSdlSurface = SDL_CreateRGBSurface(0, width, height, bpp, 0, 0, 0, 0);
    if (gSdlSurface == nullptr || gSdlSurface->format->palette == nullptr) {
        directDrawFree();
        return -1;
    }

    SDL_Color colors[256];
    for (int index = 0; index < 256; index++) {
        colors[index].r = index;
        colors[index].g = index;
        colors[index].b = index;
        colors[index].a = 255;
    }

    SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);
    sceneColors.reset(width, height);
    pendingPaletteChanges.reset();
    damageWholeFrame();

    return 0;
}

// 0x4CB1B0 GNW95_reset_mode
void directDrawFree()
{
    if (gSdlSurface != nullptr) {
        SDL_FreeSurface(gSdlSurface);
        gSdlSurface = nullptr;
        sceneColors.reset(0, 0);
        pendingPaletteChanges.reset();
    }
}

// 0x4CB310 GNW95_SetPaletteEntries
void directDrawSetPaletteInRange(unsigned char* palette, int start, int count)
{
    if (gSdlSurface == nullptr || gSdlSurface->format->palette == nullptr) {
        return;
    }
    auto* current = gSdlSurface->format->palette;
    if (start < 0 || start > current->ncolors || count <= 0 || count > current->ncolors - start) {
        return;
    }

    SDL_Color colors[COLOR_COUNT];
    PaletteIndices changed;
    for (int index = 0; index < count; ++index) {
        auto& color = colors[index];
        color = toSdlPaletteColor(palette + index * COLOR_COMPONENTS_RGB);
        const auto& old = current->colors[start + index];
        if (color.r != old.r || color.g != old.g || color.b != old.b || color.a != old.a) {
            changed.set(start + index);
        }
    }

    if (changed.any() && SDL_SetPaletteColors(current, colors, start, count) == 0) {
        pendingPaletteChanges |= changed;
    }
}

// 0x4CB568 GNW95_SetPalette
void directDrawSetPalette(unsigned char* palette)
{
    if (gSdlSurface != nullptr && gSdlSurface->format->palette != nullptr) {
        SDL_Color colors[COLOR_COUNT];

        for (int index = 0; index < COLOR_COUNT; index++) {
            colors[index] = toSdlPaletteColor(palette + index * COLOR_COMPONENTS_RGB);
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, COLOR_COUNT);
        damageWholeFrame();
    }
}

// 0x4CB68C GNW95_GetPalette
unsigned char* directDrawGetPalette()
{
    // 0x6ACA24
    static unsigned char palette[768];

    if (gSdlSurface != nullptr && gSdlSurface->format->palette != nullptr) {
        SDL_Color* colors = gSdlSurface->format->palette->colors;

        for (int index = 0; index < 256; index++) {
            SDL_Color* color = &(colors[index]);
            palette[index * 3] = color->r >> 2;
            palette[index * 3 + 1] = color->g >> 2;
            palette[index * 3 + 2] = color->b >> 2;
        }
    }

    return palette;
}

// 0x4CB850 GNW95_ShowRect
void _GNW95_ShowRect(unsigned char* src, int srcPitch, int unused, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    (void)unused;

    blitBufferToBuffer(src + srcPitch * srcY + srcX, srcWidth, srcHeight, srcPitch, (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + destX, gSdlSurface->pitch);
    damageScene({ destX, destY, srcWidth, srcHeight });
}

// Clears drawing surface.
//
// 0x4CBBC8 GNW95_zero_vid_mem
void _GNW95_zero_vid_mem()
{
    if (!gProgramIsActive) {
        return;
    }

    unsigned char* surface = (unsigned char*)gSdlSurface->pixels;
    for (int y = 0; y < gSdlSurface->h; y++) {
        memset(surface, 0, gSdlSurface->w);
        surface += gSdlSurface->pitch;
    }
    damageScene({ 0, 0, gSdlSurface->w, gSdlSurface->h });
}

int screenGetWidth()
{
    // TODO: Make it on par with _xres;
    return rectGetWidth(&_scr_size);
}

int screenGetHeight()
{
    // TODO: Make it on par with _yres.
    return rectGetHeight(&_scr_size);
}

int screenGetVisibleHeight()
{
    int windowBottomMargin = 0;

    if (!settings.ui.iface_bar_mode) {
        windowBottomMargin = INTERFACE_BAR_HEIGHT;
    }
    return screenGetHeight() - windowBottomMargin;
}

// returns true if the game is running in fullscreen mode, false otherwise (including windowed fullscreen mode)
bool screenIsExclusiveFullscreen()
{
    Uint32 flags = SDL_GetWindowFlags(gSdlWindow);
    return (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN;
}

static bool createRenderer(int width, int height)
{
    gSdlRenderer = SDL_CreateRenderer(gSdlWindow, -1, 0);
    if (gSdlRenderer == nullptr) {
        return false;
    }

    if (SDL_RenderSetLogicalSize(gSdlRenderer, width, height) != 0) {
        return false;
    }

    gSdlTexture = SDL_CreateTexture(gSdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (gSdlTexture == nullptr) {
        return false;
    }

    Uint32 format;
    if (SDL_QueryTexture(gSdlTexture, &format, nullptr, nullptr, nullptr) != 0) {
        return false;
    }

    gSdlTextureSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, SDL_BITSPERPIXEL(format), format);
    if (gSdlTextureSurface == nullptr) {
        return false;
    }

    SDL_RendererInfo rendererInfo {};
    softwareRenderer = SDL_GetRendererInfo(gSdlRenderer, &rendererInfo) == 0
        && (rendererInfo.flags & SDL_RENDERER_SOFTWARE) != 0;

    uploadedCursorBounds = {};
    frameDamage = { 0, 0, width, height };
    return true;
}

static void destroyRenderer()
{
    if (gSdlTextureSurface != nullptr) {
        SDL_FreeSurface(gSdlTextureSurface);
        gSdlTextureSurface = nullptr;
    }

    if (gSdlTexture != nullptr) {
        SDL_DestroyTexture(gSdlTexture);
        gSdlTexture = nullptr;
    }

    if (gSdlRenderer != nullptr) {
        SDL_DestroyRenderer(gSdlRenderer);
        gSdlRenderer = nullptr;
    }
}

int screenGetRefreshRate()
{
    return displayRefreshRate;
}

void handleWindowDisplayChanged()
{
    if (gSdlWindow == nullptr) {
        return;
    }

    SDL_DisplayMode mode {};
    if (!screenIsExclusiveFullscreen() || SDL_GetWindowDisplayMode(gSdlWindow, &mode) != 0 || mode.refresh_rate <= 0) {
        const int display = std::max(SDL_GetWindowDisplayIndex(gSdlWindow), 0);
        if (SDL_GetCurrentDisplayMode(display, &mode) != 0 || mode.refresh_rate <= 0) {
            if (SDL_GetDesktopDisplayMode(display, &mode) != 0) {
                mode.refresh_rate = 0;
            }
        }
    }
    displayRefreshRate = mode.refresh_rate;
}

void handleRenderTargetsReset()
{
    damageWholeFrame();
}

void handleRenderDeviceReset()
{
    if (gSdlWindow == nullptr || gSdlSurface == nullptr) {
        return;
    }

    movieHandleRendererReset();
    destroyRenderer();
    if (!createRenderer(screenGetWidth(), screenGetHeight())) {
        debugPrint("Failed to restore SDL renderer: %s\n", SDL_GetError());
        destroyRenderer();
    }
}

void handleWindowSizeChanged()
{
    handleRenderDeviceReset();
    if (gSdlRenderer != nullptr) {
        windowRefreshAll(&_scr_size);
    }
    mouseDeviceRefreshWindowMapping();
    handleWindowDisplayChanged();
}

void renderFpsCounter()
{
    if (!settings.debug.show_fps || gSdlSurface == nullptr || gSdlTextureSurface == nullptr) {
        return;
    }

    static unsigned int sampleStartTicks = 0;
    static double fps = 0.0;

    unsigned int now = SDL_GetTicks();
    if (sampleStartTicks == 0) {
        sampleStartTicks = now;
        framesPresented = 0;
    }

    unsigned int elapsed = now - sampleStartTicks;
    if (elapsed >= 500) {
        fps = framesPresented * 1000.0 / elapsed;
        framesPresented = 0;
        sampleStartTicks = now;
    }

    char text[32];
    snprintf(text, sizeof(text), "FPS: %.1f", fps);

    ScopedFont font(101);

    constexpr int kPadding = 2;
    int textWidth = fontGetStringWidth(text);
    int textHeight = fontGetLineHeight();
    int width = textWidth + kPadding * 2;
    int height = textHeight + kPadding * 2;

    if (width > gSdlSurface->w) {
        width = gSdlSurface->w;
    }

    if (height > gSdlSurface->h) {
        height = gSdlSurface->h;
    }

    bufferFill(static_cast<unsigned char*>(gSdlSurface->pixels), width, height, gSdlSurface->pitch, COLOR_BLACK);
    if (width > kPadding * 2 && height > kPadding * 2) {
        fontDrawText(static_cast<unsigned char*>(gSdlSurface->pixels) + gSdlSurface->pitch * kPadding + kPadding, text, width - kPadding * 2, gSdlSurface->pitch, COLOR_LIGHT_GREY);
    }
    damageScene({ 0, 0, width, height });
}

void renderFrame(SDL_Point cursorPosition)
{
    if (gSdlTextureSurface == nullptr || gSdlSurface == nullptr || !updateFrameTexture(cursorPosition)) {
        return;
    }

    if (shouldClearOutput()) {
        SDL_RenderClear(gSdlRenderer);
    }
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, nullptr, nullptr);
    // render movie SDL texture if present
    movieRenderDirectOverlay();
    SDL_RenderPresent(gSdlRenderer);
    ++framesPresented;
}

} // namespace fallout
