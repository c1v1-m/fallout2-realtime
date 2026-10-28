#include "svga.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#include <algorithm>

#include <SDL.h>
#include <SDL_opengl.h>

#include "config.h"
#include "debug.h"
#include "game_config.h"
#include "rt/game/game_trace.h"
#include "draw.h"
#include "interface.h"
#include "memory.h"
#include "mouse.h"
#include "win32.h"
#include "window_manager.h"
#include "window_manager_private.h"

#include "rt/rt_hooks.h"

namespace fallout {

static bool createRenderer(int width, int height);
static void destroyRenderer();

// screen rect
Rect _scr_size;

// 0x6ACA18
void (*_scr_blit)(unsigned char* src, int src_pitch, int a3, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y) = _GNW95_ShowRect;

// 0x6ACA1C
void (*_zero_mem)() = nullptr;

SDL_Window* gSdlWindow = nullptr;
SDL_Surface* gSdlSurface = nullptr;
SDL_Renderer* gSdlRenderer = nullptr;
SDL_Texture* gSdlTexture = nullptr;
SDL_Surface* gSdlTextureSurface = nullptr;

// TODO: Remove once migration to update-render cycle is completed.
FpsLimiter sharedFpsLimiter;

// --- VSYNC (MODS "VSync", cfg vsync) -- ported from fallout1-ce svga.cc ------------------------------
//
// ⚠️ WITHOUT IT NOTHING ON SCREEN CAN MOVE SMOOTHLY. The renderer had no present-vsync and the loop
// slept to 1000/60 = 16 WHOLE ms -- 62.5 fps handed to the display whenever ready. On a 240 Hz panel a
// frame stayed up for 4 refreshes, sometimes 3, in no fixed rhythm: a camera gliding at constant speed
// moved in uneven jerks.
//
// With it: the swap interval is round(refresh / 60) -- 4 at 240 Hz, 2 at 120/144, 1 at 60 -- so the
// game still runs ~60 fps and everything tuned per frame stays tuned, but every frame is on screen for
// the same number of refreshes. glFinish after the swap keeps the driver from queueing frames ahead,
// and FpsLimiter steps back to a floor.
//
// ⚠️ A DRIVER CAN REFUSE (vsync forced off in the panel, an occluded window). Frames then arrive faster
// than the display shows them; svga_vsync_check sees that and hands pacing back to the sleep.
static bool vsync_wanted = false;
static bool vsync_dirty = true; // apply at the next present, with the renderer's GL context current
static int vsync_interval = 0; // swap interval in force; 0 = not waiting for the display
static int vsync_refresh = 0;
static double vsync_period_ms = 0.0;
static bool vsync_effective = false;
// A map is loading (svga_vsync_suspend). Its reads present a frame every few KB to keep the cursor
// alive, and with each present waiting for the display that turned into seconds of extra loading.
static bool vsync_suspended = false;
static bool vsync_suspend_applied = false;
static Uint64 vsync_last_present = 0;
static double vsync_acc_ms = 0.0;
static int vsync_acc_frames = 0;

typedef void(APIENTRY* SvgaGlFinishProc)(void);
static SvgaGlFinishProc vsync_gl_finish = nullptr;

static void svga_vsync_apply()
{
    vsync_dirty = false;
    vsync_interval = 0;
    vsync_period_ms = 0.0;
    vsync_effective = false;
    vsync_last_present = 0;
    vsync_acc_ms = 0.0;
    vsync_acc_frames = 0;

    if (!vsync_wanted || gSdlWindow == nullptr) {
        SDL_GL_SetSwapInterval(0);
        sharedFpsLimiter.setVsyncPeriod(0.0);
        return;
    }

    int refresh = 60;
    SDL_DisplayMode mode;
    int display = SDL_GetWindowDisplayIndex(gSdlWindow);
    if (SDL_GetCurrentDisplayMode(display >= 0 ? display : 0, &mode) == 0 && mode.refresh_rate > 0) {
        refresh = mode.refresh_rate;
    }

    int interval = (int)floor(refresh / 60.0 + 0.5);
    if (interval < 1) {
        interval = 1;
    }

    if (SDL_GL_SetSwapInterval(interval) != 0) {
        // Some drivers take only 0/1. One refresh per frame on a fast panel is still tear-free.
        if (interval == 1 || SDL_GL_SetSwapInterval(1) != 0) {
            sharedFpsLimiter.setVsyncPeriod(0.0);
            return;
        }
        interval = 1;
    }

    if (vsync_gl_finish == nullptr) {
        vsync_gl_finish = (SvgaGlFinishProc)SDL_GL_GetProcAddress("glFinish");
    }

    vsync_refresh = refresh;
    vsync_interval = interval;
    vsync_period_ms = interval * 1000.0 / refresh;
    vsync_effective = true; // until the check says otherwise
    sharedFpsLimiter.setVsyncPeriod(vsync_period_ms);
}

// Is the display really pacing us? Averaged over two seconds of presents.
static void svga_vsync_check()
{
    Uint64 now = SDL_GetPerformanceCounter();
    if (vsync_last_present != 0) {
        double ms = (double)(now - vsync_last_present) * 1000.0 / (double)SDL_GetPerformanceFrequency();
        if (ms < 250.0) {
            vsync_acc_ms += ms;
            vsync_acc_frames++;
        }
    }
    vsync_last_present = now;

    if (vsync_acc_frames >= 120) {
        double avg = vsync_acc_ms / vsync_acc_frames;
        if (vsync_effective && avg < vsync_period_ms - 1.5) {
            // Faster than the display can show them: the interval is not being honoured.
            vsync_effective = false;
            sharedFpsLimiter.setVsyncPeriod(0.0);
        }
        vsync_acc_ms = 0.0;
        vsync_acc_frames = 0;
    }
}

void svga_vsync_suspend(bool on)
{
    vsync_suspended = on;
}

void svga_set_vsync(bool on)
{
    vsync_wanted = on;
    vsync_dirty = true;
}

bool svga_get_vsync()
{
    return vsync_wanted;
}

void svga_vsync_state(int* interval, int* refresh, bool* effective)
{
    *interval = vsync_interval;
    *refresh = vsync_refresh;
    *effective = vsync_interval > 0 && vsync_effective;
}

// 0x4CAD08
int _init_mode_320_200()
{
    return _GNW95_init_mode_ex(320, 200, 8);
}

// 0x4CAD40
int _init_mode_320_400()
{
    return _GNW95_init_mode_ex(320, 400, 8);
}

// 0x4CAD5C
int _init_mode_640_480_16()
{
    return -1;
}

// 0x4CAD64
int _init_mode_640_480()
{
    return _init_vesa_mode(640, 480);
}

// 0x4CAD94
int _init_mode_640_400()
{
    return _init_vesa_mode(640, 400);
}

// 0x4CADA8
int _init_mode_800_600()
{
    return _init_vesa_mode(800, 600);
}

// 0x4CADBC
int _init_mode_1024_768()
{
    return _init_vesa_mode(1024, 768);
}

// 0x4CADD0
int _init_mode_1280_1024()
{
    return _init_vesa_mode(1280, 1024);
}

// 0x4CADF8
void _get_start_mode_()
{
}

// 0x4CADFC
void _zero_vid_mem()
{
    if (_zero_mem) {
        _zero_mem();
    }
}

// 0x4CAE1C
int _GNW95_init_mode_ex(int width, int height, int bpp)
{
    bool fullscreen = true;
    int scale = 1;

    Config resolutionConfig;
    if (configInit(&resolutionConfig)) {
        // Realtime build (ported from fallout1-ce game.cc, where F1 reads f1_res.ini): NO FILE STILL
        // RUNS THE MATHS BELOW. Every key simply falls back to its default, so a missing f2_res.ini
        // behaves like an empty one instead of leaving a fixed, pillarboxed 640x480.
        configRead(&resolutionConfig, "f2_res.ini", false);
        {
            int screenWidth;
            if (configGetInt(&resolutionConfig, "MAIN", "SCR_WIDTH", &screenWidth)) {
                width = std::max(screenWidth, 640);
            }

            int screenHeight;
            if (configGetInt(&resolutionConfig, "MAIN", "SCR_HEIGHT", &screenHeight)) {
                height = std::max(screenHeight, 480);
            }

            bool windowed;
            if (configGetBool(&resolutionConfig, "MAIN", "WINDOWED", &windowed)) {
                fullscreen = !windowed;
            }

            // ⚠️ WHAT THE GAME ACTUALLY RENDERS AT. SCR_WIDTH/SCR_HEIGHT are the OUTPUT (the screen);
            // the engine draws into output / scale and SDL stretches that to fill the screen, so the
            // INTERNAL size decides how big everything looks. The original shipped at 640x480, so 480
            // internal lines reproduce its element size on any panel; the width only decides how much
            // map you see left and right.
            //
            // SCALE_2X divides by a whole number only -- fine on a 1440p desktop (2560 / 3 = 853x480),
            // useless on a Steam Deck (1280x800: scale 1 = tiny elements, scale 2 = 640x400, under the
            // floor). INTERNAL_WIDTH / INTERNAL_HEIGHT name the internal size outright; either one
            // alone derives the other from the screen's aspect (INTERNAL_HEIGHT=480 = 768x480 on a Deck).
            //
            // ⚠️ THE FLOOR IS 640x480, HARD: the character editor, the Pip-Boy and the world map are
            // fixed-size 640x480 windows and have nowhere to go below it.
            int internalWidth = 0;
            int internalHeight = 0;
            configGetInt(&resolutionConfig, "MAIN", "INTERNAL_WIDTH", &internalWidth);
            configGetInt(&resolutionConfig, "MAIN", "INTERNAL_HEIGHT", &internalHeight);

            // ⚠️ WITH NEITHER KEY, THE DEFAULT IS THE ORIGINAL'S SCALING: 480 lines. Steam and GOG ship
            // the High Resolution Patch's f2_res.ini (SCALE_2X=0, no INTERNAL_*), and SCALE_2X=0 would
            // mean "render at the full screen size" -- a postage-stamp interface on a 1440p desktop.
            // So SCALE_2X counts only when it asks for a divider (1 or more).
            int legacyScale = 0;
            bool haveLegacyScale = configGetInt(&resolutionConfig, "MAIN", "SCALE_2X", &legacyScale) && legacyScale > 0;
            if (internalWidth <= 0 && internalHeight <= 0 && !haveLegacyScale) {
                internalHeight = 480;
            }

            // ⚠️ FULLSCREEN IS THE SCREEN, WHATEVER SCR_* SAYS. Fullscreen is SDL_WINDOW_FULLSCREEN_DESKTOP,
            // so the real output is always the desktop; deriving the internal aspect from SCR_* instead
            // letterboxed a 16:9 picture on the Deck's 16:10 panel. SCR_* only sizes the window.
            if (fullscreen) {
                SDL_DisplayMode desktop;
                if (SDL_GetDesktopDisplayMode(0, &desktop) == 0 && desktop.w >= 640 && desktop.h >= 480) {
                    width = desktop.w;
                    height = desktop.h;
                }
            }

            int outputWidth = width;
            int outputHeight = height;

            if (internalWidth > 0 || internalHeight > 0) {
                if (internalWidth <= 0) {
                    internalWidth = (int)((double)internalHeight * outputWidth / outputHeight + 0.5);
                } else if (internalHeight <= 0) {
                    internalHeight = (int)((double)internalWidth * outputHeight / outputWidth + 0.5);
                }

                width = internalWidth;
                height = internalHeight;

                // Only the WINDOWED window is sized from this (fullscreen ignores it); the renderer's
                // logical size is the internal resolution either way. Biggest whole multiple that fits.
                int fitX = outputWidth / internalWidth;
                int fitY = outputHeight / internalHeight;
                int fit = fitX < fitY ? fitX : fitY;
                scale = fit < 1 ? 1 : fit;
            } else if (haveLegacyScale) {
                scale = legacyScale + 1;
                width /= scale;
                height /= scale;
            }

            // The floor, whichever path produced the numbers -- clamped, and said out loud.
            if (width < 640 || height < 480) {
                debugPrint("f2_res.ini: internal %dx%d is below the 640x480 floor the fixed-size "
                           "interfaces need; clamping. Set INTERNAL_WIDTH/INTERNAL_HEIGHT (or a "
                           "smaller SCALE_2X) for %dx%d output.\n",
                    width, height, outputWidth, outputHeight);
                width = std::max(width, 640);
                height = std::max(height, 480);
                scale = 1;
            }

            configGetBool(&resolutionConfig, "IFACE", "IFACE_BAR_MODE", &gInterfaceBarMode);
            configGetInt(&resolutionConfig, "IFACE", "IFACE_BAR_WIDTH", &gInterfaceBarWidth);
            // Realtime build: the LB / RB quick slots live in the strips left and right of the bar
            // (interface.cc, intface_side_panel_ok: 60 px each). Steam's f2_res.ini asks for the HRP's
            // 800-wide bar, which on our 853-wide internal screen leaves 26 px a side -- LB had nowhere to
            // go and RB fell onto the end-turn box (2026-10-23, "lb chybělo"); on a Deck's 768 the 800 bar
            // does not even fit. So the bar is as wide as asked only while both strips keep 66 px (a 57 px
            // slot and its margins); narrower than that it becomes the widest bar that still leaves them
            // (a multiple of 8: 720 on 853), cut out of the wide art at load (interface.cc,
            // interface_bar_cut -- his "pipboy obrazovka může být širší, když se tam najde místo pro lb a
            // rb"), and vanilla's 640 where not even that is wider.
            if (gInterfaceBarWidth > 640 && width - gInterfaceBarWidth < 2 * 66) {
                int fit = (width - 2 * 66) & ~7;
                int asked = gInterfaceBarWidth;
                if (fit > 640) {
                    gInterfaceBarSourceWidth = asked;
                    gInterfaceBarWidth = fit;
                } else {
                    gInterfaceBarWidth = 640;
                }
                debugPrint("f2_res.ini: IFACE_BAR_WIDTH=%d leaves no room for the quick slots at %d wide; "
                           "using a %d bar.\n",
                    asked, width, gInterfaceBarWidth);
            }
            configGetInt(&resolutionConfig, "IFACE", "IFACE_BAR_SIDE_ART", &gInterfaceSidePanelsImageId);
            configGetBool(&resolutionConfig, "IFACE", "IFACE_BAR_SIDES_ORI", &gInterfaceSidePanelsExtendFromScreenEdge);
        }
        configFree(&resolutionConfig);
    }

    if (_GNW95_init_window(width, height, fullscreen, scale) == -1) {
        return -1;
    }

    if (directDrawInit(width, height, bpp) == -1) {
        return -1;
    }

    _scr_size.left = 0;
    _scr_size.top = 0;
    _scr_size.right = width - 1;
    _scr_size.bottom = height - 1;

    _mouse_blit_trans = nullptr;
    _scr_blit = _GNW95_ShowRect;
    _zero_mem = _GNW95_zero_vid_mem;
    _mouse_blit = _GNW95_ShowRect;

    // Realtime build: MODS "VSync" -- default ON (his setting); 0 = the original sleep-paced present.
    // The game config is already loaded here (settingsInit runs before _initWindow). Applied at the
    // first present, when the renderer's GL context is current.
    int vsync = 1;
    configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VSYNC_KEY, &vsync);
    svga_set_vsync(vsync != 0);

    return 0;
}

// 0x4CAECC
int _init_vesa_mode(int width, int height)
{
    return _GNW95_init_mode_ex(width, height, 8);
}

// 0x4CAEDC
int _GNW95_init_window(int width, int height, bool fullscreen, int scale)
{
    if (gSdlWindow == nullptr) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

        if (fullscreen) {
            // Realtime build: _DESKTOP, not plain SDL_WINDOW_FULLSCREEN. A real fullscreen window
            // switches the display mode, and SDL then minimizes it on ANY focus loss (alt-tab, a
            // notification, Steam) -- the game sits at 0% CPU behind the taskbar looking exactly like
            // a crash. _DESKTOP never minimizes; the output is the desktop size anyway (see above).
            windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }

        // The game playing by itself (rt/game/autotest.cc): a hidden window, never fullscreen.
        if (gnw_headless || gnw_visible_override) {
            windowFlags &= ~(SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_FULLSCREEN);
            if (gnw_headless) {
                windowFlags |= SDL_WINDOW_HIDDEN;
            }
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
    }

    return 0;
}

// 0x4CAF9C
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

    SDL_Color colors[256];
    for (int index = 0; index < 256; index++) {
        colors[index].r = index;
        colors[index].g = index;
        colors[index].b = index;
        colors[index].a = 255;
    }

    SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);

    return 0;
}

// 0x4CB1B0
void directDrawFree()
{
    if (gSdlSurface != nullptr) {
        SDL_FreeSurface(gSdlSurface);
        gSdlSurface = nullptr;
    }
}

// 0x4CB310
void directDrawSetPaletteInRange(unsigned char* palette, int start, int count)
{
    if (gSdlSurface != nullptr && gSdlSurface->format->palette != nullptr) {
        SDL_Color colors[256];

        if (count != 0) {
            for (int index = 0; index < count; index++) {
                colors[index].r = palette[index * 3] << 2;
                colors[index].g = palette[index * 3 + 1] << 2;
                colors[index].b = palette[index * 3 + 2] << 2;
                colors[index].a = 255;
            }
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, start, count);
        SDL_BlitSurface(gSdlSurface, nullptr, gSdlTextureSurface, nullptr);
    }
}

// 0x4CB568
void directDrawSetPalette(unsigned char* palette)
{
    if (gSdlSurface != nullptr && gSdlSurface->format->palette != nullptr) {
        SDL_Color colors[256];

        for (int index = 0; index < 256; index++) {
            colors[index].r = palette[index * 3] << 2;
            colors[index].g = palette[index * 3 + 1] << 2;
            colors[index].b = palette[index * 3 + 2] << 2;
            colors[index].a = 255;
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);
        SDL_BlitSurface(gSdlSurface, nullptr, gSdlTextureSurface, nullptr);
    }
}

// 0x4CB68C
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

// 0x4CB850
void _GNW95_ShowRect(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    blitBufferToBuffer(src + srcPitch * srcY + srcX, srcWidth, srcHeight, srcPitch, (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + destX, gSdlSurface->pitch);

    SDL_Rect srcRect;
    srcRect.x = destX;
    srcRect.y = destY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    SDL_Rect destRect;
    destRect.x = destX;
    destRect.y = destY;
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
}

// Clears drawing surface.
//
// 0x4CBBC8
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

    SDL_BlitSurface(gSdlSurface, nullptr, gSdlTextureSurface, nullptr);
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

    if (!gInterfaceBarMode) {
        windowBottomMargin = INTERFACE_BAR_HEIGHT;
    }
    return screenGetHeight() - windowBottomMargin;
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

    // A new renderer is a new GL context, born with its own swap interval.
    vsync_dirty = true;

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

void handleWindowSizeChanged()
{
    destroyRenderer();
    createRenderer(screenGetWidth(), screenGetHeight());
}

void renderPresent()
{
    trace_alive(); // the hang watchdog's pulse (rt/game/game_trace.cc)

    SDL_UpdateTexture(gSdlTexture, nullptr, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, nullptr, nullptr);

    // The renderer's GL context is current here -- it has just drawn -- which makes this the one
    // safe place to change its swap interval.
    if (vsync_dirty) {
        svga_vsync_apply();
        vsync_suspend_applied = false;
    }

    // Loading: present at once, as without VSync. Back on with the first present after it.
    if (vsync_interval > 0 && vsync_suspend_applied != vsync_suspended) {
        SDL_GL_SetSwapInterval(vsync_suspended ? 0 : vsync_interval);
        vsync_suspend_applied = vsync_suspended;
        vsync_last_present = 0;
        vsync_acc_ms = 0.0;
        vsync_acc_frames = 0;
    }

    SDL_RenderPresent(gSdlRenderer);

    if (vsync_interval > 0 && !vsync_suspended) {
        // Wait for the swap itself, so the driver never queues frames ahead of the display.
        if (vsync_gl_finish != nullptr) {
            vsync_gl_finish();
        }
        svga_vsync_check();
    }
}

} // namespace fallout
