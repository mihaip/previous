#pragma once

#ifndef __ND_SDL_H__
#define __ND_SDL_H__

#include <SDL.h>

#include "config.h"

#ifdef __cplusplus

class NDSDL {
    int           slot;
    uint32_t*     vram;
    SDL_Window*   ndWindow;
    SDL_Renderer* ndRenderer;
    SDL_Texture*  ndTexture;
    SDL_atomic_t  blitNDFB;

#ifdef ENABLE_RENDERING_THREAD
    volatile bool doRepaint;
    SDL_Thread*   repaintThread;
    static int    repainter(void *_this);
    int           repainter(void);
#endif
public:
    NDSDL(int slot, uint32_t* vram);
    void    repaint(void);
    void    init(void);
    void    uninit(void);
    void    destroy(void);
    void    pause(bool pause);
    void    resize(float scale);
};

extern "C" {
#endif /* __cplusplus */
#ifndef ENABLE_RENDERING_THREAD
    void nd_sdl_repaint(void);
#endif
    void nd_sdl_resize(float scale);
    void nd_sdl_show(void);
    void nd_sdl_hide(void);
    void nd_sdl_destroy(void);
#ifdef __cplusplus
}
#endif
    
#endif /* __ND_SDL_H__ */
