/*
  Previous - statusbar.h
  
  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/
#ifndef PREV_STATUSBAR_H
#define PREV_STATUSBAR_H

#include <SDL.h>

typedef enum {
	DEVICE_LED_ENET,
	DEVICE_LED_OD,
	DEVICE_LED_SCSI,
    DEVICE_LED_FD,
    NUM_DEVICE_LEDS
} drive_index_t;

typedef enum {
	LED_STATE_OFF,
	LED_STATE_ON,
	LED_STATE_ON_BUSY,
	MAX_LED_STATE
} drive_led_t;


extern int Statusbar_SetHeight(int ScreenWidth, int ScreenHeight);
extern int Statusbar_GetHeightForSize(int width, int height);
extern int Statusbar_GetHeight(void);
extern void Statusbar_BlinkLed(drive_index_t drive);
extern void Statusbar_SetSystemLed(bool state);
extern void Statusbar_SetDspLed(bool state);
extern void Statusbar_SetNdLed(int state);

extern void Statusbar_Init(SDL_Surface *screen);
extern void Statusbar_UpdateInfo(void);
extern void Statusbar_AddMessage(const char *msg, uint32_t msecs);
extern void Statusbar_OverlayBackup(SDL_Surface *screen);
extern void Statusbar_Update(SDL_Surface *screen);
extern void Statusbar_OverlayRestore(SDL_Surface *screen);

#endif /* PREV_STATUSBAR_H */
