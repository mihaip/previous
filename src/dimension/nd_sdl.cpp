#include "main.h"
#include "nd_sdl.hpp"
#include "configuration.h"
#include "dimension.hpp"
#include "screen.h"
#include "host.h"
#include "cycInt.h"
#include "NextBus.hpp"

#include <SDL.h>


#ifdef ENABLE_RENDERING_THREAD
NDSDL::NDSDL(int slot, uint32_t* vram) : slot(slot), vram(vram), ndWindow(NULL), ndRenderer(NULL), ndTexture(NULL), doRepaint(true), repaintThread(NULL) {}

int NDSDL::repainter(void *_this) {
    return ((NDSDL*)_this)->repainter();
}

int NDSDL::repainter(void) {
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_NORMAL);

    while (doRepaint) {
        if (SDL_AtomicGet(&blitNDFB)) {
            Screen_BlitDimension(vram, ndTexture);
            SDL_RenderClear(ndRenderer);
            SDL_RenderCopy(ndRenderer, ndTexture, NULL, NULL);
            SDL_RenderPresent(ndRenderer);
        } else {
            host_sleep_ms(100);
        }
    }

    return 0;
}
#else // !ENABLE_RENDERING_THREAD
NDSDL::NDSDL(int slot, uint32_t* vram) : slot(slot), vram(vram), ndWindow(NULL), ndRenderer(NULL), ndTexture(NULL) {}

void NDSDL::repaint(void) {
    Screen_BlitDimension(vram, ndTexture);
    SDL_RenderClear(ndRenderer);
    SDL_RenderCopy(ndRenderer, ndTexture, NULL, NULL);
    SDL_RenderPresent(ndRenderer);
}
#endif // !ENABLE_RENDERING_THREAD

void NDSDL::init(void) {
    int x, y, w, h;
    char title[32], name[32];
    SDL_Rect r = {0,0,1120,832};

#ifdef ENABLE_RENDERING_THREAD
    SDL_RendererFlags vsync_flag = SDL_RENDERER_PRESENTVSYNC;
#else
    SDL_RendererFlags vsync_flag = 0;
#endif

    if (!ndWindow) {
        SDL_GetWindowPosition(sdlWindow, &x, &y);
        SDL_GetWindowSize(sdlWindow, &w, &h);
        h = (w * 832) / 1120;
        snprintf(title, sizeof(title), "NeXTdimension (Slot %i)", slot);
        ndWindow = SDL_CreateWindow(title, x+14*slot, y+14*slot, w, h, SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
        
        if (!ndWindow) {
            fprintf(stderr,"[ND] Slot %i: Failed to create window! (%s)\n", slot, SDL_GetError());
            exit(-1);
        }
    }
    
    if (ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
        if (!ndRenderer) {
            ndRenderer = SDL_CreateRenderer(ndWindow, -1, SDL_RENDERER_ACCELERATED | vsync_flag);
            if (!ndRenderer) {
                fprintf(stderr,"[ND] Slot %i: Failed to create accelerated renderer! (%s)\n", slot, SDL_GetError());
                ndRenderer = SDL_CreateRenderer(ndWindow, -1, vsync_flag);
                if (!ndRenderer) {
                    fprintf(stderr,"[ND] Slot %i: Failed to create renderer! (%s)\n", slot, SDL_GetError());
                    exit(-1);
                }
            }
            SDL_RenderSetLogicalSize(ndRenderer, r.w, r.h);
            ndTexture = SDL_CreateTexture(ndRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, r.w, r.h);
#ifdef ENABLE_RENDERING_THREAD

            snprintf(name, sizeof(name), "[Previous] Screen at slot %d", slot);
            SDL_AtomicSet(&blitNDFB, 0);
            repaintThread = SDL_CreateThread(NDSDL::repainter, name, this);
#endif
        }

        SDL_ShowWindow(ndWindow);
    } else {
        SDL_HideWindow(ndWindow);
    }
}

void NDSDL::uninit(void) {
    SDL_HideWindow(ndWindow);
}

void NDSDL::destroy(void) {
#ifdef ENABLE_RENDERING_THREAD
    doRepaint = false; // stop repaint thread
    int s;
    SDL_WaitThread(repaintThread, &s);
#endif
    SDL_DestroyTexture(ndTexture);
    SDL_DestroyRenderer(ndRenderer);
    SDL_DestroyWindow(ndWindow);
    uninit();
}

void NDSDL::pause(bool pause) {
#ifdef ENABLE_RENDERING_THREAD
    if (!pause && ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
        SDL_AtomicSet(&blitNDFB, 1);
    } else {
        SDL_AtomicSet(&blitNDFB, 0);
    }
#endif
}

void NDSDL::resize(float scale) {
    if (ndWindow) {
        SDL_SetWindowSize(ndWindow, 1120*scale, 832*scale);
    }
}

#ifndef ENABLE_RENDERING_THREAD
void nd_sdl_repaint(void) {
    FOR_EACH_SLOT(slot) {
        IF_NEXT_DIMENSION(slot, nd) {
            nd->sdl.repaint();
        }
    }
}
#endif

void nd_sdl_resize(float scale) {
    FOR_EACH_SLOT(slot) {
        IF_NEXT_DIMENSION(slot, nd) {
            nd->sdl.resize(scale);
        }
    }
}

void nd_sdl_show(void) {
    FOR_EACH_SLOT(slot) {
        IF_NEXT_DIMENSION(slot, nd) {
            nd->sdl.init();
        }
    }
}

void nd_sdl_hide(void) {
    FOR_EACH_SLOT(slot) {
        IF_NEXT_DIMENSION(slot, nd) {
            nd->sdl.pause(true);
            nd->sdl.uninit();
        }
    }
}

void nd_sdl_destroy(void) {
    FOR_EACH_SLOT(slot) {
        IF_NEXT_DIMENSION(slot, nd) {
            nd->sdl.destroy();
        }
    }
}
