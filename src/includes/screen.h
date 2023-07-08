/*
  Hatari - screen.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_SCREEN_H
#define HATARI_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <SDL.h>

extern volatile bool bGrabMouse;
extern volatile bool bInFullScreen;
extern SDL_Window *sdlWindow;
extern SDL_Surface *sdlscrn;

extern void Screen_Init(void);
extern void Screen_UnInit(void);
extern void Screen_Pause(bool pause);
extern void Screen_EnterFullScreen(void);
extern void Screen_ReturnFromFullScreen(void);
extern void Screen_Grab(void);
extern void Screen_ShowMainWindow(void);
extern void Screen_SizeChanged(void);
extern void Screen_ModeChanged(void);
extern void Screen_StatusbarChanged(void);
extern void Screen_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects);
extern void Screen_UpdateRect(SDL_Surface *screen, int32_t x, int32_t y, int32_t w, int32_t h);
extern void Screen_BlitDimension(uint32_t* vram, SDL_Texture* tex);
extern void Screen_Repaint(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* ifndef HATARI_SCREEN_H */
