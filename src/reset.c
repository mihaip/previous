/*
  Hatari

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Reset emulation state.
*/
const char Reset_fileid[] = "Hatari reset.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "host.h"
#include "configuration.h"
#include "cycInt.h"
#include "m68000.h"
#include "reset.h"
#include "screen.h"
#include "tmc.h"
#include "video.h"
#include "debugcpu.h"
#include "scsi.h"
#include "mo.h"
#include "sysReg.h"
#include "rtcnvram.h"
#include "scc.h"
#include "ethernet.h"
#include "floppy.h"
#include "snd.h"
#include "printer.h"
#include "dsp.h"
#include "kms.h"
#include "NextBus.hpp"

/*-----------------------------------------------------------------------*/
/**
 * Reset NEXT emulator states, chips, interrupts and registers.
 */
static const char* Reset_NeXT(bool bCold)
{
	if (bCold) {
		const char* error_str;
		error_str=memory_init(ConfigureParams.Memory.nMemoryBankSize);
		if (error_str!=NULL) {
			return error_str;
		}
	}

	host_reset();                 /* Reset host related timing vars */

	M68000_Reset(bCold);          /* Reset CPU */
	CycInt_Reset();               /* Reset interrupts */
	Main_SpeedReset();            /* Reset speed reporting system */
	Video_Reset();                /* Reset video */
	TMC_Reset();				  /* Reset TMC Registers */
	SCR_Reset();                  /* Reset System Control Registers */
	RTC_Reset();                  /* Reset RTC and NVRAM */
	SCSI_Reset();                 /* Reset SCSI disks */
	MO_Reset();                   /* Reset MO disks */
	Floppy_Reset();               /* Reset Floppy disks */
	SCC_Reset();                  /* Reset SCC */
	Ethernet_Reset(true);         /* Reset Ethernet */
	KMS_Reset();                  /* Reset KMS */
	Sound_Reset();                /* Reset Sound */
	Printer_Reset();              /* Reset Printer */
	DSP_Reset();                  /* Reset DSP */
	NextBus_Reset();              /* Reset NextBus */
	Screen_ModeChanged();         /* Reset Screen Mode */
	DebugCpu_SetDebugging();      /* Reset debugging flag if needed */

	return NULL;
}


/*-----------------------------------------------------------------------*/
/**
 * Cold reset ST (reset memory, all registers and reboot)
 */
const char* Reset_Cold(void)
{
	return Reset_NeXT(true);
}


/*-----------------------------------------------------------------------*/
/**
 * Warm reset ST (reset registers, leave in same state and reboot)
 */
const char* Reset_Warm(void)
{
	return Reset_NeXT(false);
}
