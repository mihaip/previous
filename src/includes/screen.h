/*
  Hatari - screen.h

  This file is distributed under the GNU Public License, version 2 or at your
  option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_SCREEN_H
#define HATARI_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <SDL.h>

extern volatile bool bGrabMouse;
extern volatile bool bInFullScreen;
extern struct SDL_Window *sdlWindow;
extern SDL_Surface *sdlscrn;

void Screen_Init(void);
void Screen_UnInit(void);
void Screen_Pause(bool pause);
void Screen_EnterFullScreen(void);
void Screen_ReturnFromFullScreen(void);
void Screen_SizeChanged(void);
void Screen_ModeChanged(void);
void Screen_StatusbarChanged(void);
bool Update_StatusBar(void);
void Screen_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects);
void Screen_UpdateRect(SDL_Surface *screen, int32_t x, int32_t y, int32_t w, int32_t h);
void blitDimension(uint32_t* vram, SDL_Texture* tex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* ifndef HATARI_SCREEN_H */
