/*
  video.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

*/
const char Video_fileid[] = "Previous video.c : " __DATE__ " " __TIME__;

#include <stdbool.h>

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
static bool statusBarToggle;
void Video_InterruptHandler_VBL ( void ) {
    CycInt_AcknowledgeInterrupt();
    if (ConfigureParams.Screen.nMonitorType != MONITOR_TYPE_DIMENSION) {
        Screen_CopyBuffer(NEXTVideo, ConfigureParams.System.bColor?(2*1024*1024):(256*1024));
    }
    host_blank(0, MAIN_DISPLAY, true);
    Video_InterruptHandler();
    CycInt_AddRelativeInterruptUs((1000*1000)/NEXT_VBL_FREQ, 0, INTERRUPT_VIDEO_VBL);
}
