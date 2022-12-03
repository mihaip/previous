/*
  Hatari - screen.c

  This file is distributed under the GNU Public License, version 2 or at your
  option any later version. Read the file gpl.txt for details.

 (SC) Simon Schubiger - most of it rewritten for Previous NeXT emulator
*/

const char Screen_fileid[] = "Previous fast_screen.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "host.h"
#include "configuration.h"
#include "log.h"
#include "dimension.hpp"
#include "nd_sdl.hpp"
#include "nd_mem.hpp"
#include "paths.h"
#include "screen.h"
#include "statusbar.h"
#include "video.h"
#include "m68000.h"

#include <SDL.h>


SDL_Window*   sdlWindow;
SDL_Surface*  sdlscrn = NULL;        /* The SDL screen surface */
int           nWindowWidth;          /* Width of SDL window in physical pixels */
int           nWindowHeight;         /* Height of SDL window in physical pixels */
float         dpiFactor;             /* Factor to convert physical pixels to logical pixels on high-dpi displays */

/* extern for shortcuts */
volatile bool bGrabMouse    = false; /* Grab the mouse cursor in the window */
volatile bool bInFullScreen = false; /* true if in full screen */

static const int NeXT_SCRN_WIDTH  = 1120;
static const int NeXT_SCRN_HEIGHT = 832;
int width;   /* guest framebuffer */
int height;  /* guest framebuffer */

static SDL_Renderer* sdlRenderer;
static SDL_Texture*  uiTexture;
static SDL_Texture*  fbTexture;
static SDL_atomic_t  blitFB;
static SDL_atomic_t  blitUI;
static bool          doUIblit;
static SDL_Rect      statusBar;
static SDL_Rect      screenRect;
static SDL_Rect      saveWindowBounds; /* Window bounds before going fullscreen. Used to restore window size & position. */
static MONITORTYPE   saveMonitorType;  /* Save monitor type to restore on return from fullscreen */
static uint32_t      mask;             /* green screen mask for transparent UI areas */
static void*         uiBuffer;         /* uiBuffer used for user interface texture */
static SDL_SpinLock  uiBufferLock;     /* Lock for concurrent access to UI buffer between m68k thread and repainter */


static uint32_t BW2RGB[0x400];
static uint32_t COL2RGB[0x10000];

static uint32_t bw2rgb(SDL_PixelFormat* format, int bw) {
	switch(bw & 3) {
		case 3:  return SDL_MapRGB(format, 0,   0,   0);
		case 2:  return SDL_MapRGB(format, 85,  85,  85);
		case 1:  return SDL_MapRGB(format, 170, 170, 170);
		case 0:  return SDL_MapRGB(format, 255, 255, 255);
		default: return 0;
	}
}

static uint32_t col2rgb(SDL_PixelFormat* format, int col) {
	int r = col & 0xF000; r >>= 12; r |= r << 4;
	int g = col & 0x0F00; g >>= 8;  g |= g << 4;
	int b = col & 0x00F0; b >>= 4;  b |= b << 4;
	return SDL_MapRGB(format, r,   g,   b);
}

/*
 BW format is 2bit per pixel
 */
static void blitBW(SDL_Texture* tex) {
	void* pixels;
	int   d;
	int   pitch = (NeXT_SCRN_WIDTH + (ConfigureParams.System.bTurbo ? 0 : 32)) / 4;
	SDL_LockTexture(tex, NULL, &pixels, &d);
	uint32_t* dst = (uint32_t*)pixels;
	for(int y = 0; y < NeXT_SCRN_HEIGHT; y++) {
		int src     = y * pitch;
		for(int x = 0; x < NeXT_SCRN_WIDTH/4; x++, src++) {
			int idx = NEXTVideo[src] * 4;
			*dst++  = BW2RGB[idx+0];
			*dst++  = BW2RGB[idx+1];
			*dst++  = BW2RGB[idx+2];
			*dst++  = BW2RGB[idx+3];
		}
	}
	SDL_UnlockTexture(tex);
}

/*
 Color format is 4bit per pixel, big-endian: RGBx
 */
static void blitColor(SDL_Texture* tex) {
	void* pixels;
	int   d;
	int pitch = NeXT_SCRN_WIDTH + (ConfigureParams.System.bTurbo ? 0 : 32);
	SDL_LockTexture(tex, NULL, &pixels, &d);
	uint32_t* dst = (uint32_t*)pixels;
	for(int y = 0; y < NeXT_SCRN_HEIGHT; y++) {
		uint16_t* src = (uint16_t*)NEXTVideo + (y*pitch);
		for(int x = 0; x < NeXT_SCRN_WIDTH; x++) {
			*dst++ = COL2RGB[*src++];
		}
	}
	SDL_UnlockTexture(tex);
}

/*
 Dimension format is 8bit per pixel, big-endian: RRGGBBAA
 */
void Screen_BlitDimension(uint32_t* vram, SDL_Texture* tex) {
#if ND_STEP
	uint32_t* src = &vram[0];
#else
	uint32_t* src = &vram[16];
#endif
	int       d;
	uint32_t  format;
	SDL_QueryTexture(tex, &format, &d, &d, &d);
	if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		/* Add big-endian accelerated blit loops as needed here */
		switch (format) {
			default: {
				void*   pixels;
				SDL_LockTexture(tex, NULL, &pixels, &d);
				uint32_t* dst = (uint32_t*)pixels;

				/* fallback to SDL_MapRGB */
				SDL_PixelFormat* pformat = SDL_AllocFormat(format);
				for(int y = NeXT_SCRN_HEIGHT; --y >= 0;) {
					for(int x = NeXT_SCRN_WIDTH; --x >= 0;) {
						uint32_t v = *src++;
						*dst++ = SDL_MapRGB(pformat, (v >> 8) & 0xFF, (v>>16) & 0xFF, (v>>24) & 0xFF);
					}
					src += 32;
				}
				SDL_FreeFormat(pformat);
				SDL_UnlockTexture(tex);
				break;
			}
		}
	} else {
		/* Add little-endian accelerated blit loops as needed here */
		switch (format) {
			case SDL_PIXELFORMAT_ARGB8888: {
				SDL_UpdateTexture(tex, NULL, src, (NeXT_SCRN_WIDTH+32)*4);
				break;
			}
			default: {
				void*   pixels;
				SDL_LockTexture(tex, NULL, &pixels, &d);
				uint32_t* dst = (uint32_t*)pixels;

				/* fallback to SDL_MapRGB */
				SDL_PixelFormat* pformat = SDL_AllocFormat(format);
				for(int y = NeXT_SCRN_HEIGHT; --y >= 0;) {
					for(int x = NeXT_SCRN_WIDTH; --x >= 0;) {
						uint32_t v = *src++;
						*dst++ = SDL_MapRGB(pformat, (v >> 16) & 0xFF, (v>>8) & 0xFF, (v>>0) & 0xFF);
					}
					src += 32;
				}
				SDL_FreeFormat(pformat);
				SDL_UnlockTexture(tex);
				break;
			}
		}
	}
}

/*
 Blit NeXT framebuffer to texture.
 */
static bool blitScreen(SDL_Texture* tex) {
	if (ConfigureParams.Screen.nMonitorType==MONITOR_TYPE_DIMENSION) {
		uint32_t* vram = nd_vram_for_slot(ND_SLOT(ConfigureParams.Screen.nMonitorNum));
		if (vram) {
			Screen_BlitDimension(vram, tex);
			return true;
		}
	} else {
		if (NEXTVideo) {
			if (ConfigureParams.System.bColor) {
				blitColor(tex);
			} else {
				blitBW(tex);
			}
			return true;
		}
	}
	return false;
}

/*
 Blits the NeXT framebuffer to the fbTexture, blends with the GUI surface and shows it.
 */
void Screen_Repaint(void) {
	bool updateFB = false;

	// Blit the NeXT framebuffer to texture
	if (bEmulationActive) {
		updateFB = blitScreen(fbTexture);
	}

	// Copy UI surface to texture
	if (SDL_AtomicSet(&blitUI, 0)) {
		// update full UI texture
		SDL_UpdateTexture(uiTexture, NULL, uiBuffer, sdlscrn->pitch);
		updateFB = true;
	}

	if (updateFB) {
		SDL_RenderClear(sdlRenderer);
		// Render NeXT framebuffer texture
		SDL_RenderCopy(sdlRenderer, fbTexture, NULL, &screenRect);
		SDL_RenderCopy(sdlRenderer, uiTexture, NULL, &screenRect);
		SDL_RenderPresent(sdlRenderer);
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Pause Screen, pauses or resumes drawing of NeXT framebuffer
 */
void Screen_Pause(bool pause) {
}

/*-----------------------------------------------------------------------*/
/**
 * Init Screen, creates window, renderer and textures
 */
void Screen_Init(void) {
	uint32_t format, r, g, b, a;
	int      d;

	/* Set initial window resolution */
	width  = NeXT_SCRN_WIDTH;
	height = NeXT_SCRN_HEIGHT;
	bInFullScreen = false;

	/* Statusbar */
	Statusbar_SetHeight(width, height);
	statusBar.x = 0;
	statusBar.y = height;
	statusBar.w = width;
	statusBar.h = Statusbar_GetHeight();
	/* Grow to fit statusbar */
	height += Statusbar_GetHeight();

	/* Screen */
	screenRect.x = 0;
	screenRect.y = 0;
	screenRect.h = height;
	screenRect.w = width;

	/* Set new video mode */
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	fprintf(stderr, "SDL screen request: %d x %d (%s)\n", width, height, bInFullScreen ? "fullscreen" : "windowed");

	int x = SDL_WINDOWPOS_UNDEFINED;
	if(ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
		for(int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
			SDL_Rect r;
			SDL_GetDisplayBounds(i, &r);
			if(r.w >= width * 2) {
				x = r.x + width + ((r.w - width * 2) / 2);
				break;
			}
			if(r.x >= 0 && SDL_GetNumVideoDisplays() == 1) x = r.x + 8;
		}
	}
	sdlWindow  = SDL_CreateWindow(PROG_NAME, x, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (!sdlWindow) {
		fprintf(stderr,"Failed to create window: %s!\n", SDL_GetError());
		exit(-1);
	}

	sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
	if (!sdlRenderer) {
		fprintf(stderr,"Failed to create renderer: %s!\n", SDL_GetError());
		exit(-1);
	}

	SDL_GetWindowSizeInPixels(sdlWindow, &nWindowWidth, &nWindowHeight);
	if (nWindowWidth > 0) {
		dpiFactor = (float)width / nWindowWidth;
		fprintf(stderr,"SDL screen scale: %.3f\n", dpiFactor);
	} else {
		dpiFactor = 1.0;
		fprintf(stderr,"Failed to set screen scale\n");
	}

	SDL_RenderSetLogicalSize(sdlRenderer, width, height);

	uiTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, width, height);
	SDL_SetTextureBlendMode(uiTexture, SDL_BLENDMODE_BLEND);

	fbTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, width, height);
	SDL_SetTextureBlendMode(fbTexture, SDL_BLENDMODE_NONE);

	SDL_QueryTexture(uiTexture, &format, &d, &d, &d);
	SDL_PixelFormatEnumToMasks(format, &d, &r, &g, &b, &a);

	sdlscrn = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, r, g, b, a);

	/* Exit if we can not open a screen */
	if (!sdlscrn) {
		fprintf(stderr, "Could not set video mode:\n %s\n", SDL_GetError() );
		SDL_Quit();
		exit(-2);
	}

	/* Clear UI with mask */
	mask = g | a;
	SDL_FillRect(sdlscrn, NULL, mask);

	/* Allocate buffers for copy routines */
	uiBuffer = malloc(sdlscrn->h * sdlscrn->pitch);

	/* Initialize statusbar */
	Statusbar_Init(sdlscrn);

	/* Setup lookup tables */
	SDL_PixelFormat* pformat = SDL_AllocFormat(format);
	/* initialize BW lookup table */
	for(int i = 0; i < 0x100; i++) {
		BW2RGB[i*4+0] = bw2rgb(pformat, i>>6);
		BW2RGB[i*4+1] = bw2rgb(pformat, i>>4);
		BW2RGB[i*4+2] = bw2rgb(pformat, i>>2);
		BW2RGB[i*4+3] = bw2rgb(pformat, i>>0);
	}
	/* initialize color lookup table */
	for(int i = 0; i < 0x10000; i++)
		COL2RGB[SDL_BYTEORDER == SDL_BIG_ENDIAN ? i : SDL_Swap16(i)] = col2rgb(pformat, i);

	/* Configure some SDL stuff: */
	SDL_ShowCursor(SDL_DISABLE);
	Main_SetMouseGrab(bGrabMouse);

	if (ConfigureParams.Screen.bFullScreen) {
		Screen_EnterFullScreen();
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Free screen bitmap and allocated resources
 */
void Screen_UnInit(void) {
	nd_sdl_destroy();
	SDL_DestroyTexture(uiTexture);
	SDL_DestroyTexture(fbTexture);
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(sdlWindow);
}

/*-----------------------------------------------------------------------*/
/**
 * Enter Full screen mode
 */
void Screen_EnterFullScreen(void) {
	bool bWasRunning;

	if (!bInFullScreen) {
		/* Hold things... */
		bWasRunning = Main_PauseEmulation(false);
		bInFullScreen = true;

		SDL_GetWindowPosition(sdlWindow, &saveWindowBounds.x, &saveWindowBounds.y);
		SDL_GetWindowSize(sdlWindow, &saveWindowBounds.w, &saveWindowBounds.h);
		SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_Delay(100);                  /* To give monitor time to change to new resolution */

		/* If using multiple screen windows, save and go to single window mode */
		saveMonitorType = ConfigureParams.Screen.nMonitorType;
		if (ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
			ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_CPU;
			Screen_ModeChanged();
		}

		if (bWasRunning) {
			/* And off we go... */
			Main_UnPauseEmulation();
		}

		/* Always grab mouse pointer in full screen mode */
		Main_SetMouseGrab(true);

		/* Make sure screen is painted in case emulation is paused */
		SDL_AtomicSet(&blitUI, 1);
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Return from Full screen mode back to a window
 */
void Screen_ReturnFromFullScreen(void) {
	bool bWasRunning;

	if (bInFullScreen) {
		/* Hold things... */
		bWasRunning = Main_PauseEmulation(false);
		bInFullScreen = false;

		SDL_SetWindowFullscreen(sdlWindow, 0);
		SDL_Delay(100);                /* To give monitor time to switch resolution */
		SDL_SetWindowPosition(sdlWindow, saveWindowBounds.x, saveWindowBounds.y);
		SDL_SetWindowSize(sdlWindow, saveWindowBounds.w, saveWindowBounds.h);

		/* Return to windowed monitor mode */
		if (saveMonitorType == MONITOR_TYPE_DUAL) {
			ConfigureParams.Screen.nMonitorType = saveMonitorType;
			Screen_ModeChanged();
		}

		if (bWasRunning) {
			/* And off we go... */
			Main_UnPauseEmulation();
		}

		/* Go back to windowed mode mouse grab settings */
		Main_SetMouseGrab(bGrabMouse);

		/* Make sure screen is painted in case emulation is paused */
		SDL_AtomicSet(&blitUI, 1);
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Force things associated with changing screen size
 */
void Screen_SizeChanged(void) {
	float scale;

	if (!bInFullScreen) {
		SDL_RenderGetScale(sdlRenderer, &scale, &scale);
		SDL_SetWindowSize(sdlWindow, width*scale*dpiFactor, height*scale*dpiFactor);

		nd_sdl_resize(scale*dpiFactor);
	}

	/* Make sure screen is painted in case emulation is paused */
	SDL_AtomicSet(&blitUI, 1);
}


/*-----------------------------------------------------------------------*/
/**
 * Force things associated with changing between fullscreen/windowed
 */
void Screen_ModeChanged(void) {
	if (!sdlscrn) {
		/* screen not yet initialized */
		return;
	}

	/* Do not use multiple windows in full screen mode */
	if (ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL && bInFullScreen) {
		saveMonitorType = ConfigureParams.Screen.nMonitorType;
		ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_CPU;
	}
	if (ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL && !bInFullScreen) {
		nd_sdl_show();
	} else {
		nd_sdl_hide();
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Force things associated with changing statusbar visibility
 */
void Screen_StatusbarChanged(void) {
	float scale;

	if (!sdlscrn) {
		/* screen not yet initialized */
		return;
	}

	/* Get new heigt for our window */
	height = NeXT_SCRN_HEIGHT + Statusbar_SetHeight(NeXT_SCRN_WIDTH, NeXT_SCRN_HEIGHT);

	if (bInFullScreen) {
		scale = (float)saveWindowBounds.w / NeXT_SCRN_WIDTH;
		saveWindowBounds.h = height * scale;
		SDL_RenderSetLogicalSize(sdlRenderer, width, height);
	} else {
		SDL_RenderGetScale(sdlRenderer, &scale, &scale);
		SDL_SetWindowSize(sdlWindow, width*scale*dpiFactor, height*scale*dpiFactor);
		SDL_RenderSetLogicalSize(sdlRenderer, width, height);
		SDL_RenderSetScale(sdlRenderer, scale, scale);
	}

	/* Make sure screen is painted in case emulation is paused */
	SDL_AtomicSet(&blitUI, 1);
}


/*-----------------------------------------------------------------------*/
/**
 * Draw screen to window/full-screen - (SC) Just status bar updates. Screen redraw is done in repaint thread.
 */
static void statusBarUpdate(void) {
	SDL_LockSurface(sdlscrn);
	SDL_AtomicLock(&uiBufferLock);
	memcpy(&((uint8_t*)uiBuffer)[statusBar.y*sdlscrn->pitch], &((uint8_t*)sdlscrn->pixels)[statusBar.y*sdlscrn->pitch], statusBar.h * sdlscrn->pitch);
	SDL_AtomicSet(&blitUI, 1);
	SDL_AtomicUnlock(&uiBufferLock);
	SDL_UnlockSurface(sdlscrn);
}

/*
 Copy UI SDL surface to uiBuffer and replace mask pixels with transparent pixels for
 UI blending with framebuffer texture.
*/
static void uiUpdate(void) {
	SDL_LockSurface(sdlscrn);
	int     count = sdlscrn->w * sdlscrn->h;
	uint32_t* dst = (uint32_t*)uiBuffer;
	uint32_t* src = (uint32_t*)sdlscrn->pixels;
	SDL_AtomicLock(&uiBufferLock);
	// poor man's green-screen - would be nice if SDL had more blending modes...
	for(int i = count; --i >= 0; src++)
		*dst++ = *src == mask ? 0 : *src;
	SDL_AtomicSet(&blitUI, 1);
	SDL_AtomicUnlock(&uiBufferLock);
	SDL_UnlockSurface(sdlscrn);
}

void Screen_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects) {
	while(numrects--) {
		if(rects->y < NeXT_SCRN_HEIGHT) {
			uiUpdate();
			doUIblit = true;
		} else {
			if(doUIblit) {
				uiUpdate();
				doUIblit = false;
			} else {
				statusBarUpdate();
			}
		}
	}
	if (!bEmulationActive) {
		Screen_Repaint();
	}
}

void Screen_UpdateRect(SDL_Surface *screen, int32_t x, int32_t y, int32_t w, int32_t h) {
	SDL_Rect rect = { x, y, w, h };
	Screen_UpdateRects(screen, 1, &rect);
}
