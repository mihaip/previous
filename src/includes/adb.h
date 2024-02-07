/*
  Previous - adb.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#ifndef PREV_ADB_H
#define PREV_ADB_H

#include <SDL.h>

extern uint32_t adb_lget(uint32_t addr);
extern uint16_t adb_wget(uint32_t addr);
extern uint8_t  adb_bget(uint32_t addr);

extern void adb_lput(uint32_t addr, uint32_t l);
extern void adb_wput(uint32_t addr, uint16_t w);
extern void adb_bput(uint32_t addr, uint8_t  b);

extern void ADB_Reset(void);

extern void ADB_KeyDown(const SDL_Keysym *sdlkey);
extern void ADB_KeyUp(const SDL_Keysym *sdlkey);
extern void ADB_MouseMove(int dx, int dy);
extern void ADB_MouseButton(int left, int down);

#endif /* PREV_ADB_H */
