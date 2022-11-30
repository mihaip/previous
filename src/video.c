/*
  Hatari - video.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

*/
const char Video_fileid[] = "Hatari video.c";

#include "main.h"
#include "host.h"
#include "configuration.h"
#include "cycInt.h"
#include "ioMem.h"
#include "screen.h"
#include "shortcut.h"
#include "video.h"
#include "dma.h"
#include "sysReg.h"
#include "tmc.h"
#include "nd_sdl.hpp"


#define NEXT_VBL_FREQ 68

/*-----------------------------------------------------------------------*/
/**
 * Start VBL interrupt.
 */
void Video_Reset(void) {
	CycInt_AddRelativeInterruptUs(1000, 0, INTERRUPT_VIDEO_VBL);
}

/*-----------------------------------------------------------------------*/
/**
 * Generate vertical video retrace interrupt.
 */
static void Video_Interrupt(void) {
	if (ConfigureParams.System.bTurbo) {
		tmc_video_interrupt();
	} else if (ConfigureParams.System.bColor) {
		color_video_interrupt();
	} else {
		dma_video_interrupt();
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Check if it is time for vertical video retrace interrupt.
 */
void Video_InterruptHandler(void) {
	static bool bBlankToggle = false;

	CycInt_AcknowledgeInterrupt();
	if (bBlankToggle) {
		Video_Interrupt();
		host_blank(0, MAIN_DISPLAY, true);
	} else {
		Main_SendSpecialEvent(MAIN_REPAINT);
	}
	nd_vbl_state_handler(bBlankToggle);
	bBlankToggle = !bBlankToggle;
	CycInt_AddRelativeInterruptUs((1000*1000)/(2*NEXT_VBL_FREQ), 0, INTERRUPT_VIDEO_VBL);
}
