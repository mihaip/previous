/*
  Hatari - hatari-glue.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This file contains some code to glue the UAE CPU core to the rest of the
  emulator and Hatari's "illegal" opcodes.
*/
const char HatariGlue_fileid[] = "Hatari hatari-glue.c : " __DATE__ " " __TIME__;


#include <stdio.h>

#include "main.h"
#include "configuration.h"
#include "video.h"

#include "sysdeps.h"
#include "maccess.h"
#include "memory.h"
#include "newcpu.h"
#include "hatari-glue.h"


struct uae_prefs currprefs, changed_prefs;

/**
 * Initialize 680x0 emulation
 */
int Init680x0(void) {
	switch (ConfigureParams.System.nCpuLevel) {
		case 3: currprefs.cpu_model = changed_prefs.cpu_model = 68030; break;
		case 4: currprefs.cpu_model = changed_prefs.cpu_model = 68040; break;
		default: fprintf (stderr, "Init680x0() : Error, CPU level not supported (%i)\n",ConfigureParams.System.nCpuLevel);
	}
    
    switch (ConfigureParams.System.n_FPUType) {
        case 68881: currprefs.fpu_revision = changed_prefs.fpu_revision = 0x1f; break;
        case 68882: currprefs.fpu_revision = changed_prefs.fpu_revision = 0x20; break;
        case 68040:
            if (ConfigureParams.System.bTurbo)
                currprefs.fpu_revision = changed_prefs.fpu_revision = 0x41;
            else
                currprefs.fpu_revision = changed_prefs.fpu_revision = 0x40;
            break;
		default: fprintf (stderr, "Init680x0() : Error, fpu_model unknown\n");
    }
    
    currprefs.fpu_model = changed_prefs.fpu_model = ConfigureParams.System.n_FPUType;
	currprefs.cpu_compatible = changed_prefs.cpu_compatible = ConfigureParams.System.bCompatibleCpu;
	currprefs.fpu_strict = changed_prefs.fpu_strict = ConfigureParams.System.bCompatibleFPU;
    currprefs.mmu_model = changed_prefs.mmu_model = ConfigureParams.System.bMMU?changed_prefs.cpu_model:0;

   	write_log("Init680x0() called\n");

	init_m68k();

	return true;
}


/**
 * Deinitialize 680x0 emulation
 */
void Exit680x0(void)
{
	memory_uninit();

	free(table68k);
	table68k = NULL;
}


TCHAR* buf_out (TCHAR *buffer, int *bufsize, const TCHAR *format, ...)
{
    va_list parms;
    int count;
    
    if (buffer == NULL)
    {
        return NULL;
    }
    
    va_start (parms, format);
    vsnprintf (buffer, (*bufsize) - 1, format, parms);
    va_end (parms);
    
    count = _tcslen (buffer);
    *bufsize -= count;
    
    return buffer + count;
}

void error_log(const TCHAR *format, ...)
{
    va_list parms;
    
    va_start(parms, format);
    vfprintf(stderr, format, parms);
    va_end(parms);
    
    if (format[strlen(format) - 1] != '\n')
    {
        fputc('\n', stderr);
    }
}
