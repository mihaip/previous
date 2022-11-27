/*
  Hatari - main.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_MAIN_H
#define HATARI_MAIN_H


/* Name and version for window title: */
#define PROG_NAME "Previous 2.6"

/* Messages for window title: */
#define MOUSE_LOCK_MSG "Mouse is locked. Ctrl-click to release."


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#if __GNUC__ >= 3
# define likely(x)      __builtin_expect (!!(x), 1)
# define unlikely(x)    __builtin_expect (!!(x), 0)
#else
# define likely(x)      (x)
# define unlikely(x)    (x)
#endif

/* avoid warnings with variables used only in asserts */
#ifdef NDEBUG
# define ASSERT_VARIABLE(x) (void)(x)
#else
# define ASSERT_VARIABLE(x) assert(x)
#endif

#ifdef WIN32
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

#define CALL_VAR(func)  { ((void(*)(void))func)(); }

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (int)(sizeof(x)/sizeof(x[0]))
#endif

/* 68000 operand sizes */
#define SIZE_BYTE  1
#define SIZE_WORD  2
#define SIZE_LONG  4

enum {
	PAUSE_NONE,
	PAUSE_EMULATION,
	UNPAUSE_EMULATION,
	HALT_EMULATION
};

/* Flag for pausing m68k thread (used by i860 debugger) */
extern volatile int mainPauseEmulation;

extern volatile bool bQuitProgram;
extern volatile bool bEmulationActive;

extern void Main_HandleBlankEvent(void);
extern bool Main_PauseEmulation(bool visualize);
extern bool Main_UnPauseEmulation(void);
extern void Main_RequestQuit(bool confirm);
extern void Main_WarpMouse(int x, int y);
extern void Main_SetMouseGrab(bool grab);
extern void Main_EventHandler(void);
extern void Main_EventHandlerInterrupt(void);
extern void Main_UpdateStatusbar(void);
extern void Main_SetTitle(const char *title);
extern void Main_SpeedReset(void);
extern const char* Main_SpeedMsg(void);

#endif /* ifndef HATARI_MAIN_H */
