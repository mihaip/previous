#pragma once

#ifndef __ND_SDL_H__
#define __ND_SDL_H__

#include <SDL.h>

#ifdef __cplusplus

class NDSDL {
    int           slot;
    SDL_Window*   ndWindow;
    SDL_Renderer* ndRenderer;
    SDL_Texture*  ndTexture;
    SDL_atomic_t  blitNDFB;

    uint32_t*     vram;
public:
    static volatile bool ndVBLtoggle;
    static volatile bool ndVideoVBLtoggle;

    NDSDL(int slot, uint32_t* vram);
    void    repaint(void);
    void    init(void);
    void    uninit(void);
    void    pause(bool pause);
    void    resize(float scale);
    void    destroy(void);
    void    start_interrupts();
};

extern "C" {
#endif
    extern const int DISPLAY_VBL_MS;
    extern const int VIDEO_VBL_MS;
    extern const int BLANK_MS;
    
    void nd_sdl_repaint(void);
    void nd_vbl_state_handler(bool state);
    void nd_vbl_handler(void);
    void nd_video_vbl_handler(void);
    void nd_sdl_resize(float scale);
    void nd_sdl_show(void);
    void nd_sdl_hide(void);
    void nd_sdl_destroy(void);
#ifdef __cplusplus
}
#endif
    
#endif /* __ND_SDL_H__ */
