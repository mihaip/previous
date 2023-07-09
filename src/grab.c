/*
  Previous - grab.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Grab screen and save it as PNG file.
*/
const char Grab_fileid[] = "Previous grab.c";

#include "main.h"
#include "configuration.h"
#include "log.h"
#include "dimension.hpp"
#include "paths.h"
#include "statusbar.h"
#include "m68000.h"
#include "grab.h"


#if HAVE_LIBPNG
#include "file.h"
#include <png.h>

#define NEXT_SCREEN_HEIGHT 832
#define NEXT_SCREEN_WIDTH  1120


/* Convert framebuffer data to RGBA and fill buffer */
static bool Grab_FillBuffer(uint8_t* buf) {
	uint8_t* fb;
	int i, j;
	
	if (ConfigureParams.Screen.nMonitorType==MONITOR_TYPE_DIMENSION) {
		fb = (uint8_t*)(nd_vram_for_slot(ND_SLOT(ConfigureParams.Screen.nMonitorNum)));
		if (fb) {
#if ND_STEP
			j = 0;
#else
			j = 16*4;
#endif
			for (i = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=4, j+=4) {		
				if (i && (i%(NEXT_SCREEN_WIDTH*4))==0)
					j+=32*4;

				buf[i+0] = fb[j+2]; // r
				buf[i+1] = fb[j+1]; // g
				buf[i+2] = fb[j+0]; // b
				buf[i+3] = 0xff;    // a
			}
			return true;
		}
	} else {
		if (NEXTVideo) {
			fb = NEXTVideo;
			j = 0;
			if (ConfigureParams.System.bColor) {
				for (i = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=4, j+=2) {
					if (!ConfigureParams.System.bTurbo && i && (i%(NEXT_SCREEN_WIDTH*4))==0)
						j+=32*2;
					
					buf[i+0] = (fb[j+0]&0xF0) | ((fb[j+0]&0xF0)>>4); // r
					buf[i+1] = (fb[j+0]&0x0F) | ((fb[j+0]&0x0F)<<4); // g
					buf[i+2] = (fb[j+1]&0xF0) | ((fb[j+1]&0xF0)>>4); // b
					buf[i+3] = 0xff;                                 // a
				}
			} else {
				for (i = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=16, j++) {
					if (!ConfigureParams.System.bTurbo && i && (i%(NEXT_SCREEN_WIDTH*4))==0)
						j+=32/4;
					
					buf[i+ 0] = buf[i+ 1] = buf[i+ 2] = (~(fb[j] >> 6) & 3) * 0x55; buf[i+ 3] = 0xff; // rgba
					buf[i+ 4] = buf[i+ 5] = buf[i+ 6] = (~(fb[j] >> 4) & 3) * 0x55; buf[i+ 7] = 0xff; // rgba
					buf[i+ 8] = buf[i+ 9] = buf[i+10] = (~(fb[j] >> 2) & 3) * 0x55; buf[i+11] = 0xff; // rgba
					buf[i+12] = buf[i+13] = buf[i+14] = (~(fb[j] >> 0) & 3) * 0x55; buf[i+15] = 0xff; // rgba
				}
			}
			return true;
		}
	}
	return false;
}

/* Create PNG file */
static bool Grab_MakePNG(FILE* fp) {
	png_structp png_ptr  = NULL;
	png_infop   info_ptr = NULL;
	png_text    pngtext;
	
	char        key[]    = "Title";
	char        text[]   = "Previous Screen Grab";

	int         y        = 0;
	bool        result   = false;
	
	off_t       start    = 0;
	uint8_t*    src_ptr  = NULL;
	uint8_t*    buf      = malloc(NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4);
	
	if (buf) {
		if (Grab_FillBuffer(buf)) {
			/* Create and initialize the png_struct with error handler functions. */
			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (png_ptr) {
				/* Allocate/initialize the image information data. */
				info_ptr = png_create_info_struct(png_ptr);
				if (info_ptr) {
					/* libpng ugliness: Set error handling when not supplying own
					 * error handling functions in the png_create_write_struct() call.
					 */
					if (!setjmp(png_jmpbuf(png_ptr))) {
						/* store current pos in fp (could be != 0 for avi recording) */
						start = ftello(fp);
						
						/* initialize the png structure */
						png_init_io(png_ptr, fp);
						
						/* image data properties */
						png_set_IHDR(png_ptr, info_ptr, NEXT_SCREEN_WIDTH, NEXT_SCREEN_HEIGHT, 8, PNG_COLOR_TYPE_RGB_ALPHA,
									 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
									 PNG_FILTER_TYPE_DEFAULT);
						
						/* image info */
						pngtext.key = key;
						pngtext.text = text;
						pngtext.compression = PNG_TEXT_COMPRESSION_NONE;
#ifdef PNG_iTXt_SUPPORTED
						pngtext.lang = NULL;
#endif
						png_set_text(png_ptr, info_ptr, &pngtext, 1);
						
						/* write the file header information */
						png_write_info(png_ptr, info_ptr);
						
						for (y = 0; y < NEXT_SCREEN_HEIGHT; y++)
						{		
							src_ptr = buf + y * NEXT_SCREEN_WIDTH * 4;
							
							png_write_row(png_ptr, src_ptr);
						}
						
						/* write the additional chunks to the PNG file */
						png_write_end(png_ptr, info_ptr);
						
						result = true;
					}
				}
				/* handles info_ptr being NULL */
				png_destroy_write_struct(&png_ptr, &info_ptr);
			}
		}
		free(buf);
	}	
	return result;
}

/* Open file and save PNG data to it */
static void Grab_SaveFile(char* szPathName) {
	FILE *fp = NULL;

	fp = File_Open(szPathName, "wb");
	if (!fp) {
		Log_Printf(LOG_WARN, "[Grab] Error: Could not open file %s", szPathName);
		return;
	}
	
	if (Grab_MakePNG(fp)) {
		Statusbar_AddMessage("Saving screen grab", 0);
	} else {
		Log_Printf(LOG_WARN, "[Grab] Error: Could not create PNG file");
	}
	
	File_Close(fp);
}

/* Grab screen */
void Grab_Screen(void) {
	int i;
	static char *szPathName = NULL;
	char *szFileName = malloc(FILENAME_MAX);
	
	if (!szFileName)  return;
	
	if (File_DirExists(ConfigureParams.Printer.szPrintToFileName)) {
		for (i = 0; i < 1000; i++) {
			snprintf(szFileName, FILENAME_MAX, "next_screen_%03d", i);
			szPathName = File_MakePath(ConfigureParams.Printer.szPrintToFileName, szFileName, ".png");
			
			if (File_Exists(szPathName)) {
				continue;
			}
			
			Grab_SaveFile(szPathName);
			break;
		}
		
		if (i >= 1000) {
			Log_Printf(LOG_WARN, "[Grab] Error: Maximum screen grab count exceeded (%d)", i);
		}
	}

	free(szFileName);
}
#else
void Grab_Screen(void) {
	Log_Printf(LOG_WARN, "[Grab] Screen grab not supported (libpng missing)");
}
#endif
