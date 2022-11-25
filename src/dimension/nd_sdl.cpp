#include "main.h"
#include "nd_sdl.hpp"
#include "configuration.h"
#include "dimension.hpp"
#include "screen.h"
#include "host.h"
#include "cycInt.h"
#include "NextBus.hpp"

#include <SDL.h>


volatile bool NDSDL::ndVBLtoggle;
volatile bool NDSDL::ndVideoVBLtoggle;

NDSDL::NDSDL(int slot) : slot(slot), ndWindow(NULL), ndRenderer(NULL), ndTexture(NULL) {}

void NDSDL::repaint(void) {
    if (SDL_AtomicSet(&blitNDFB, 0)) {
        blitDimension(buffer, &bufferLock, ndTexture);
        SDL_RenderClear(ndRenderer);
        SDL_RenderCopy(ndRenderer, ndTexture, NULL, NULL);
        SDL_RenderPresent(ndRenderer);
    }
}

void NDSDL::init(void) {
    int x, y, w, h;
    char title[32], name[32];
    SDL_Rect r = {0,0,1120,832};

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
            ndRenderer = SDL_CreateRenderer(ndWindow, -1, SDL_RENDERER_ACCELERATED);
            
            if (!ndRenderer) {
                fprintf(stderr,"[ND] Slot %i: Failed to create renderer! (%s)\n", slot, SDL_GetError());
                exit(-1);
            }
            SDL_RenderSetLogicalSize(ndRenderer, r.w, r.h);
            ndTexture = SDL_CreateTexture(ndRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, r.w, r.h);
        }

        SDL_ShowWindow(ndWindow);
    } else {
        SDL_HideWindow(ndWindow);
    }
}

void NDSDL::start_interrupts(void) {
    CycInt_AddRelativeInterruptUs(1000, 0, INTERRUPT_ND_VBL);
    CycInt_AddRelativeInterruptUs(1000, 0, INTERRUPT_ND_VIDEO_VBL);
}

// called from m68k thread
void NDSDL::copy(uint8_t* vram) {
    SDL_AtomicLock(&bufferLock);
    memcpy(buffer, vram, ND_VBUF_SIZE);
    SDL_AtomicSet(&blitNDFB, 1);
    SDL_AtomicUnlock(&bufferLock);
}

// called from 68k thread
void nd_vbl_handler(void)       {
    CycInt_AcknowledgeInterrupt();

    FOR_EACH_SLOT(slot) {
        IF_NEXT_DIMENSION(slot, nd) {
            if (NDSDL::ndVBLtoggle) {
                if (ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
                    nd->sdl.copy(nd->vram);
                } else if (ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DIMENSION) {
                    if (ConfigureParams.Screen.nMonitorNum == ND_NUM(slot)) {
                        Screen_CopyBuffer(nd->vram, ND_VBUF_SIZE);
                    }
                }
            }
            host_blank(nd->slot, ND_DISPLAY, NDSDL::ndVBLtoggle);
            nd->i860.i860cycles = (1000*1000*33)/136;
        }
    }
    NDSDL::ndVBLtoggle = !NDSDL::ndVBLtoggle;

    // 136Hz with toggle gives 68Hz, blank time is 1/2 frame time
    CycInt_AddRelativeInterruptUs((1000*1000)/136, 0, INTERRUPT_ND_VBL);
}

// called from m68k thread
void nd_video_vbl_handler(void) {
    CycInt_AcknowledgeInterrupt();

    FOR_EACH_SLOT(slot) {
        IF_NEXT_DIMENSION(slot, nd) {
            host_blank(slot, ND_VIDEO, NDSDL::ndVideoVBLtoggle);
            nd->i860.i860cycles = nd->i860.i860cycles; // make compiler happy
        }
    }
    NDSDL::ndVideoVBLtoggle = !NDSDL::ndVideoVBLtoggle;

    // 120Hz with toggle gives 60Hz NTSC, blank time is 1/2 frame time
    CycInt_AddRelativeInterruptUs((1000*1000)/120, 0, INTERRUPT_ND_VIDEO_VBL);
}

void NDSDL::uninit(void) {
    SDL_HideWindow(ndWindow);
}

void NDSDL::resize(float scale) {
    if (ndWindow) {
        SDL_SetWindowSize(ndWindow, 1120*scale, 832*scale);
    }
}

void nd_sdl_repaint(void) {
    FOR_EACH_SLOT(slot) {
        IF_NEXT_DIMENSION(slot, nd) {
            nd->sdl.repaint();
        }
    }
}

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

void NDSDL::destroy(void) {
    SDL_DestroyTexture(ndTexture);
    SDL_DestroyRenderer(ndRenderer);
    SDL_DestroyWindow(ndWindow);
    uninit();
}
