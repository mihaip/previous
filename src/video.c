/*
  Previous - video.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

*/
const char Video_fileid[] = "Previous video.c";

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
 * Return true if video output is enabled.
 */
bool Video_Enabled(void) {
	if (ConfigureParams.Boot.bVisible) {
		return true;
	} else if (ConfigureParams.System.bTurbo) {
		return tmc_video_enabled();
	} else if (ConfigureParams.System.bColor) {
		return color_video_enabled();
	} else {
		return brighness_video_enabled();
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Check if it is time for vertical video retrace interrupt.
 */
void Video_InterruptHandler(void) {
#ifdef ENABLE_RENDERING_THREAD
	CycInt_AcknowledgeInterrupt();
	host_blank_count(MAIN_DISPLAY, true);
	Main_CheckStatusbarUpdate();
	Video_Interrupt();
	CycInt_AddRelativeInterruptUs((1000*1000)/NEXT_VBL_FREQ, 0, INTERRUPT_VIDEO_VBL);
#else
	static bool bBlankToggle = false;

	CycInt_AcknowledgeInterrupt();
	host_blank_count(MAIN_DISPLAY, bBlankToggle);
	if (bBlankToggle) {
		Video_Interrupt();
	} else if (ConfigureParams.Screen.nMonitorType != MONITOR_TYPE_DIMENSION) {
		Main_SendSpecialEvent(MAIN_REPAINT);
	}
	bBlankToggle = !bBlankToggle;
	CycInt_AddRelativeInterruptUs((1000*1000)/(2*NEXT_VBL_FREQ), 0, INTERRUPT_VIDEO_VBL);
#endif
}
