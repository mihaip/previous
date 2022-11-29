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

/*--------------------------------------------------------------*/
/* Local functions prototypes                                   */
/*--------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/**
 * Reset video chip and start VBL interrupt
 */

#define NEXT_VBL_FREQ 68

void Video_Reset(void) {
	CycInt_AddRelativeInterruptUs((1000*1000)/NEXT_VBL_FREQ, 0, INTERRUPT_VIDEO_VBL);
}

/**
 * Generate vertical video retrace interrupt
 */
static void Video_InterruptHandler(void) {
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
 * VBL interrupt : set new interrupts, draw screen, generate sound,
 * reset counters, ...
 */
void Video_InterruptHandler_VBL(void) {
	static bool bBlankToggle = false;

	CycInt_AcknowledgeInterrupt();
	bBlankToggle = !bBlankToggle;
	if (bBlankToggle) {
		Video_InterruptHandler();
		host_blank(0, MAIN_DISPLAY, true);
	} else {
		Main_SendSpecialEvent(MAIN_REPAINT);
	}
	nd_vbl_state_handler(bBlankToggle);
	CycInt_AddRelativeInterruptUs((1000*1000)/(2*NEXT_VBL_FREQ), 0, INTERRUPT_VIDEO_VBL);
}
